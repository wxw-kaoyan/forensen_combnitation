#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""X50 子设备 YModem OTA 协议 V1.0 —— BL 合规性检测工具

两种模式：
  monitor  双串口旁路监听（主板 -> 模组 / 模组 -> 主板），逐帧按协议校验，退出时出结论
  emulate  模拟主板发送端，主动测 BL；支持注入 CRC 错 / 块号错 / 重复块

接线（monitor）：
  适配器 A 的 RX <- 主板 TX，适配器 B 的 RX <- 模组 TX，两适配器与主板共地
  （只接一个方向也行，缺失的方向会被标记为"未观测"）

用法：
  python ota_bl_check.py monitor --host-port COM5 --bl-port COM6 --baud 115200
  python ota_bl_check.py monitor --bl-port COM6 --baud 115200 --idle-stop 20
  python ota_bl_check.py emulate --port COM6 --baud 115200 --fw firmware.bin
  python ota_bl_check.py emulate --port COM6 --baud 115200 --fw firmware.bin --corrupt-block 3
  python ota_bl_check.py --selftest
"""
import argparse
import csv
import os
import sys
import threading
import time

import serial

# ── 协议常量 ──
SOH, STX, EOT = 0x01, 0x02, 0x04
ACK, NAK, CCC, CAN = 0x06, 0x15, 0x43, 0x18
ENTER_OTA = bytes([0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF])
VER_QUERY = bytes([0xFF, 0xFF, 0x74, 0x00, 0x00, 0x00, 0x00, 0x72])
BLOCK = 1024
TIMEOUT_C = 10.0
TIMEOUT_ACK = 10.0


def crc16(data):
    """CRC-16/CCITT-FALSE: poly 0x1021, init 0x0000, 不反转不异或"""
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def cmd_checksum(b):
    return sum(b[:7]) & 0xFF


def build_block(seq, payload, head=STX):
    n = 128 if head == SOH else 1024
    body = payload[:n] + bytes(n - len(payload[:n]))
    c = crc16(body)
    return bytes([head, seq & 0xFF, (0xFF - seq) & 0xFF]) + body + bytes([c >> 8, c & 0xFF])


def build_block0(name, length):
    body = name.encode() + b"\x00" + str(length).encode() + b"\x00"
    return build_block(0, body[:128], SOH)


def build_end():
    return build_block(0, bytes(128), SOH)


def hexs(b, n=24):
    s = " ".join("%02X" % x for x in b[:n])
    return s + (" ..." if len(b) > n else "")


# ══════════════════════ 帧解析 ══════════════════════
def frame_len(b):
    """返回 (长度, 类型)；长度 0 表示数据还不够"""
    if not b:
        return 0, None
    h = b[0]
    if h == SOH:
        return 3 + 128 + 2, "SOH"
    if h == STX:
        return 3 + 1024 + 2, "STX"
    if h == 0xFF:
        if len(b) < 2:
            return 0, None
        if b[1] == 0xFF:
            return 8, "CMD"
        if b[1] == 0x00:
            return 8, "VER"
        return 1, "RAW"
    if h in (EOT, ACK, NAK, CCC, CAN):
        return 1, "CTL"
    return 1, "RAW"


class Frame:
    __slots__ = ("dir", "kind", "raw", "t0", "t1", "ok", "text", "seq")

    def __init__(self, d, kind, raw, t0, t1):
        self.dir, self.kind, self.raw, self.t0, self.t1 = d, kind, raw, t0, t1
        self.ok = True
        self.text = ""
        self.seq = None


# ══════════════════════ monitor ══════════════════════
class Reader(threading.Thread):
    def __init__(self, ser, tag):
        super().__init__(daemon=True)
        self.ser, self.tag = ser, tag
        self.buf, self.ts = bytearray(), []
        self.lock = threading.Lock()
        self.stop = False

    def run(self):
        while not self.stop:
            try:
                d = self.ser.read(self.ser.in_waiting or 1)
            except Exception:
                break
            if d:
                t = time.perf_counter()
                with self.lock:
                    self.buf.extend(d)
                    self.ts.extend([t] * len(d))

    def drain(self):
        with self.lock:
            b, t = bytes(self.buf), list(self.ts)
            self.buf.clear()
            self.ts.clear()
        return b, t


class Checker:
    def __init__(self):
        self.checks = []

    def add(self, name, ok, detail=""):
        self.checks.append((name, ok, detail))
        print("  [%s] %-34s %s" % ("PASS" if ok else "FAIL", name, detail))

    def warn(self, name, detail=""):
        self.checks.append((name, None, detail))
        print("  [WARN] %-34s %s" % (name, detail))

    def result(self):
        bad = [c for c in self.checks if c[1] is False]
        return len(bad) == 0, bad


def run_monitor(a):
    ser_h = serial.Serial(a.host_port, a.baud, timeout=0) if a.host_port else None
    ser_b = serial.Serial(a.bl_port, a.baud, timeout=0) if a.bl_port else None
    if not ser_h and not ser_b:
        sys.exit("至少给一个 --host-port 或 --bl-port")

    readers = []
    if ser_h:
        readers.append(Reader(ser_h, "H>B"))
    if ser_b:
        readers.append(Reader(ser_b, "B>H"))
    for r in readers:
        r.start()

    pend = {r.tag: [bytearray(), []] for r in readers}
    frames = []
    last_any = time.time()

    # 会话状态
    st = {
        "enter": None, "c_lat": None, "ver": None, "ver_req": None,
        "block0": None, "declared": None, "seen": set(), "blk_nak": 0,
        "seq_err": 0, "crc_err": 0, "retx": 0,
        "eot": None, "end": None, "acked": 0, "raw": 0,
        "b0_pending": False, "b0_ack": False,
        "blk_pending": False, "blk_ack": 0,
        "eot_pending": False, "eot_ack": False,
        "end_pending": False, "end_ack": False,
    }
    ctx = {"host_end": None}

    print("监听中（Ctrl+C 结束）...")
    try:
        while True:
            got = False
            for r in readers:
                d, t = r.drain()
                if d:
                    got = True
                    last_any = time.time()
                    pend[r.tag][0].extend(d)
                    pend[r.tag][1].extend(t)
            if not got:
                if a.idle_stop and time.time() - last_any > a.idle_stop and frames:
                    print("\n空闲 %.0fs，结束监听" % a.idle_stop)
                    break
                time.sleep(0.005)
                continue

            for r in readers:
                buf, tbuf = pend[r.tag]
                while True:
                    L, kind = frame_len(buf)
                    if L == 0 or len(buf) < L:
                        break
                    raw = bytes(buf[:L])
                    f = Frame(r.tag, kind, raw, tbuf[0], tbuf[L - 1])
                    del buf[:L]
                    del tbuf[:L]
                    frames.append(f)
                    process(f, st, ctx)
    except KeyboardInterrupt:
        print("\n停止监听")

    for r in readers:
        r.stop = True
    for s in (ser_h, ser_b):
        if s:
            s.close()

    report(frames, st, a)


def process(f, st, ctx):
    """逐帧校验 + 打印"""
    lat = ""
    if f.dir == "B>H" and ctx["host_end"] is not None:
        lat = "lat=%.2fms" % ((f.t0 - ctx["host_end"]) * 1000)
    if f.dir == "H>B":
        ctx["host_end"] = f.t1

    if f.kind == "CMD":
        ok = cmd_checksum(f.raw) == f.raw[7]
        f.ok = ok
        name = {0x01: "ENTER_OTA", 0x74: "VER_QUERY"}.get(f.raw[2], "CMD_%02X" % f.raw[2])
        f.text = "%s  %s  sum=%s" % (name, hexs(f.raw[:4]), "OK" if ok else "BAD")
        if f.raw[2] == 0x01:
            st["enter"] = f
        elif f.raw[2] == 0x74:
            st["ver_req"] = f
    elif f.kind == "VER":
        ok = cmd_checksum(f.raw) == f.raw[7]
        v1, v2 = f.raw[5], f.raw[6]
        f.ok = ok
        f.text = "VER_REPLY  V1=0x%02X V2=0x%02X -> 在线版本 %d  sum=%s" % (
            v1, v2, v1 * 10 + v2, "OK" if ok else "BAD")
        st["ver"] = (v1, v2, v1 * 10 + v2, ok)
    elif f.kind in ("SOH", "STX"):
        seq, nseq = f.raw[1], f.raw[2]
        data = f.raw[3:-2]
        c = (f.raw[-2] << 8) | f.raw[-1]
        seq_ok = nseq == (0xFF - seq)
        crc_ok = c == crc16(data)
        f.seq = seq
        f.ok = seq_ok and crc_ok
        f.text = "%s seq=%d ~seq=%02X %s %s" % (
            f.kind, seq, nseq, "seqOK" if seq_ok else "seqBAD",
            "crcOK" if crc_ok else "crcBAD")
        if f.dir == "H>B":
            if seq == 0 and seq_ok and crc_ok:
                if data == bytes(len(data)):
                    st["end"] = f
                    st["end_pending"] = True
                    f.text = "END_BLOCK  " + f.text
                else:
                    s = data.split(b"\x00")
                    name = s[0].decode("ascii", "replace")
                    ln = None
                    try:
                        ln = int(s[1].decode())
                    except Exception:
                        pass
                    st["block0"] = (name, ln, crc_ok)
                    st["b0_pending"] = True
                    f.text = "BLOCK0 name=%s len=%s  %s" % (name, ln, f.text)
            elif f.kind == "SOH":
                st["raw"] += 1
            else:
                if not seq_ok:
                    st["seq_err"] += 1
                if not crc_ok:
                    st["crc_err"] += 1
                if seq in st["seen"]:
                    st["retx"] += 1
                if crc_ok and seq_ok:
                    st["seen"].add(seq)
                    st["blk_pending"] = True
    elif f.kind == "CTL":
        b = f.raw[0]
        nm = {EOT: "EOT", ACK: "ACK", NAK: "NAK", CCC: "'C'", CAN: "CAN"}.get(b, "%02X" % b)
        f.text = nm
        if b == NAK:
            st["blk_nak"] += 1
        elif b == ACK:
            st["acked"] += 1
            if st["b0_pending"]:
                st["b0_ack"], st["b0_pending"] = True, False
            elif st["blk_pending"]:
                st["blk_ack"] += 1
                st["blk_pending"] = False
            elif st["eot_pending"]:
                st["eot_ack"], st["eot_pending"] = True, False
            elif st["end_pending"]:
                st["end_ack"], st["end_pending"] = True, False
        elif b == CCC and st["enter"] is not None and st["c_lat"] is None:
            st["c_lat"] = f.t0 - st["enter"].t1
        elif b == EOT:
            st["eot"] = f
            st["eot_pending"] = True
    else:
        f.ok = False
        f.text = "UNEXPECTED %02X" % f.raw[0]
        st["raw"] += 1

    print("[%8.3f] %s %-42s %s" % (f.t0, f.dir, f.text, lat))


def report(frames, st, a):
    print("\n" + "=" * 72)
    print("X50 子设备 YModem OTA 协议 V1.0 —— BL 合规性判定")
    print("=" * 72)
    ck = Checker()
    n = len(frames)

    if not any(f.kind == "CMD" and f.raw[2] == 0x01 for f in frames):
        ck.warn("进入升级指令", "未观测到（只监听了模组方向？）")
    else:
        e = [f for f in frames if f.kind == "CMD" and f.raw[2] == 0x01][0]
        ck.add("进入升级指令格式/校验和", e.ok, hexs(e.raw))
        if st["c_lat"] is None:
            ck.add("进入升级后回 'C'", False, "未收到")
        else:
            ms = st["c_lat"] * 1000
            ck.add("进入升级后回 'C'", ms <= TIMEOUT_C * 1000,
                   "延迟 %.1f ms（建议≤500，上限 10000）" % ms)

    if st["ver"]:
        v1, v2, v, ok = st["ver"]
        ck.add("版本回复帧格式/校验和", ok, "在线版本 %d（V1=0x%02X V2=0x%02X）" % (v, v1, v2))
        if a.expect_version:
            want = a.expect_version
            ck.add("在线版本与包内版本比较", v != want,
                   "在线=%d 包内=%d -> %s" % (v, want, "需升级" if v != want else "跳过"))
    elif a.bl_port:
        ck.warn("版本查询回复", "未收到回复帧")

    if st["block0"]:
        name, ln, ok = st["block0"]
        ck.add("Block0 格式/CRC/文件名/长度", ok and ln is not None,
               "name=%s len=%s" % (name, ln))
        ck.add("Block0 -> ACK", st["b0_ack"], "收到 ACK" if st["b0_ack"] else "未收到 ACK")
        if ln:
            want = -(-ln // BLOCK)
            got = len(st["seen"])
            if want <= 256:
                ck.add("数据块数量 = ceil(len/1024)", got == want,
                       "收到 %d / 期望 %d" % (got, want))
            else:
                ck.add("块号回绕覆盖（>256 块）", got == 256,
                       "收到 %d 个块号 / 期望覆盖 256（总块数 %d）" % (got, want))
    elif a.host_port:
        ck.warn("Block0", "未观测到")

    if st["seen"] or st["crc_err"] or st["seq_err"]:
        ck.add("数据块 CRC", st["crc_err"] == 0, "CRC 错 %d 块" % st["crc_err"])
        ck.add("数据块块号/反码", st["seq_err"] == 0, "块号错 %d 块" % st["seq_err"])
        ck.add("每个数据块都有 ACK", st["blk_ack"] >= len(st["seen"]) and len(st["seen"]) > 0,
               "ACK %d / 块 %d" % (st["blk_ack"], len(st["seen"])))

    if st["eot"] is not None:
        ck.add("EOT -> ACK", st["eot_ack"], "收到 ACK" if st["eot_ack"] else "未收到 ACK")
    if st["end"] is not None:
        ck.add("结束块格式/CRC/全零", st["end"].ok, hexs(st["end"].raw[:8]))
        ck.add("结束块 -> ACK", st["end_ack"], "收到 ACK" if st["end_ack"] else "未收到 ACK")
    elif a.host_port and st["block0"]:
        ck.warn("结束块", "未观测到")

    if st["retx"]:
        ck.warn("重发块", "%d 次（协议允许）" % st["retx"])
    if st["blk_nak"]:
        ck.warn("NAK", "%d 次" % st["blk_nak"])
    if st["raw"]:
        ck.warn("协议外字节", "%d 个" % st["raw"])

    ok, bad = ck.result()
    print("-" * 72)
    print("帧总数 %d    结论：%s" % (n, "PASS" if ok else "FAIL"))
    if bad:
        for name, _, detail in bad:
            print("  未通过：%s  %s" % (name, detail))

    # 落盘
    out = a.out_dir or os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "output_navdata",
        "ota_check_%s" % time.strftime("%Y%m%d_%H%M%S"))
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "frames.csv"), "w", newline="", encoding="utf-8-sig") as fh:
        w = csv.writer(fh)
        w.writerow(["t_s", "方向", "类型", "SEQ", "校验", "内容"])
        for f in frames:
            w.writerow(["%.4f" % f.t0, f.dir, f.kind, "" if f.seq is None else f.seq,
                        "OK" if f.ok else "BAD", f.text])
    with open(os.path.join(out, "verdict.txt"), "w", encoding="utf-8") as fh:
        fh.write("结论: %s\n" % ("PASS" if ok else "FAIL"))
        for name, o, detail in ck.checks:
            fh.write("[%s] %s  %s\n" % ("PASS" if o else ("WARN" if o is None else "FAIL"),
                                        name, detail))
    print("报告 ->", os.path.abspath(out))


# ══════════════════════ emulate ══════════════════════
def read_n(ser, n, timeout):
    buf = bytearray()
    t0 = time.time()
    while len(buf) < n and time.time() - t0 < timeout:
        d = ser.read(n - len(buf))
        if d:
            buf.extend(d)
        else:
            time.sleep(0.002)
    return bytes(buf)


def wait_ctrl(ser, timeout, want=None):
    t0 = time.time()
    while time.time() - t0 < timeout:
        b = ser.read(1)
        if not b:
            time.sleep(0.002)
            continue
        if want is None or b[0] in want:
            return b[0]
    return None


def drain(ser):
    try:
        ser.reset_input_buffer()
    except Exception:
        pass


def emulate_send(ser, payload, seq, label, fault=None, corrupt=False):
    if corrupt:
        body = payload[:BLOCK] + bytes(BLOCK - len(payload[:BLOCK]))
        c = (crc16(body) ^ 0xFFFF) & 0xFFFF          # 故意错 CRC
        frame = bytes([STX, seq & 0xFF, (0xFF - seq) & 0xFF]) + body + bytes([c >> 8, c & 0xFF])
    elif fault == "seq":
        frame = bytes([STX, (seq + 1) & 0xFF, (0xFF - seq) & 0xFF]) + \
                payload[:BLOCK] + bytes(BLOCK - len(payload[:BLOCK]))
        c = crc16(frame[3:])
        frame = frame + bytes([c >> 8, c & 0xFF])
    else:
        frame = build_block(seq, payload[:BLOCK], STX)
    ser.write(frame)
    ser.flush()
    return frame


def run_emulate(a):
    fw = open(a.fw, "rb").read()
    ln = len(fw)
    nblk = -(-ln // BLOCK)
    ser = serial.Serial(a.port, a.baud, timeout=0)
    drain(ser)
    print("固件 %s  %d 字节  %d 块" % (os.path.basename(a.fw), ln, nblk))

    t0 = time.time()
    ser.write(ENTER_OTA); ser.flush()
    r = wait_ctrl(ser, TIMEOUT_C)
    if r != CCC:
        print("FAIL: 进入升级未回 'C'（收到 %s）" % (None if r is None else "%02X" % r))
        ser.close(); return
    print("PASS: 进入升级 -> 'C'  (%.1f ms)" % ((time.time() - t0) * 1000))

    ser.write(VER_QUERY); ser.flush()
    v = read_n(ser, 8, 1.0)
    if len(v) == 8 and v[0] == 0xFF and v[2] == 0x84:
        print("PASS: 版本回复 %s -> 在线版本 %d" % (hexs(v, 8), v[5] * 10 + v[6]))
    else:
        print("WARN: 版本回复异常 %s" % hexs(v))

    ser.write(build_block0("sensor", ln)); ser.flush()
    r = wait_ctrl(ser, 5.0, (ACK, NAK))
    if r != ACK:
        print("FAIL: Block0 未 ACK")
        ser.close(); return
    print("PASS: Block0 -> ACK")

    ok = 0
    for i in range(1, nblk + 1):
        seq = i & 0xFF
        chunk = fw[(i - 1) * BLOCK:i * BLOCK]
        if a.delay_ms:
            time.sleep(a.delay_ms / 1000.0)
        inject = (a.corrupt_block == i)
        if inject:
            emulate_send(ser, chunk, seq, "blk%d" % i, corrupt=True)
            r = wait_ctrl(ser, 3.0, (ACK, NAK))
            print("  注入: 块 %d 坏 CRC -> %s" % (i, "NAK(正确)" if r == NAK else "未回 NAK(异常)"))
        if a.bad_seq_block == i:
            emulate_send(ser, chunk, seq, "blk%d" % i, fault="seq")
            r = wait_ctrl(ser, 3.0, (ACK, NAK))
            print("  注入: 块 %d 块号错 -> %s" % (i, "NAK(正确)" if r == NAK else "未回 NAK(异常)"))
        emulate_send(ser, chunk, seq, "blk%d" % i)
        r = wait_ctrl(ser, TIMEOUT_ACK, (ACK, NAK))
        if a.dup_block == i:
            emulate_send(ser, chunk, seq, "blk%d" % i)
            r2 = wait_ctrl(ser, TIMEOUT_ACK, (ACK, NAK))
            print("  注入: 块 %d 重复发送 -> %s" % (i, "ACK" if r2 == ACK else "异常"))
        if r == ACK:
            ok += 1
        else:
            print("FAIL: 块 %d 未 ACK（%s）" % (i, "NAK" if r == NAK else "超时"))
            break
        if i % 10 == 0 or i == nblk:
            print("  进度 %d/%d" % (i, nblk))

    if ok != nblk:
        print("FAIL: 仅 %d/%d 块成功" % (ok, nblk))
        ser.close(); return

    ser.write(bytes([EOT])); ser.flush()
    r = wait_ctrl(ser, TIMEOUT_ACK, (ACK, NAK))
    print("%s: EOT -> %s" % ("PASS" if r == ACK else "FAIL", "ACK" if r == ACK else r))

    ser.write(build_end()); ser.flush()
    r = wait_ctrl(ser, TIMEOUT_ACK, (ACK, NAK))
    print("%s: 结束块 -> %s" % ("PASS" if r == ACK else "FAIL", "ACK" if r == ACK else r))
    print("耗时 %.1f s" % (time.time() - t0))
    ser.close()


def selftest():
    # 注意：协议文档写 "CRC-16/CCITT-FALSE" 但初始值给 0x0000，
    # 实际是 CRC-16/XMODEM（标准 YModem 用法）。"123456789" -> 0x31C3。
    # 若 BL 按真正的 CCITT-FALSE（init=0xFFFF）实现 -> 0x29B1，两者不兼容。
    assert crc16(b"123456789") == 0x31C3, hex(crc16(b"123456789"))
    assert cmd_checksum(ENTER_OTA) == 0xFF
    assert cmd_checksum(VER_QUERY) == 0x72
    f = build_block0("sensor", 42240)
    assert len(f) == 133 and f[0] == SOH and f[1] == 0 and f[2] == 0xFF
    assert crc16(f[3:-2]) == ((f[-2] << 8) | f[-1])
    assert len(build_end()) == 133
    assert len(build_block(1, b"x" * 1024, STX)) == 1029
    print("selftest OK  (CRC16/校验和/Block0/结束块/STX 帧长 均正确)")


def main():
    ap = argparse.ArgumentParser(description="X50 子设备 YModem OTA BL 合规性检测")
    ap.add_argument("mode", nargs="?", choices=["monitor", "emulate"], default=None)
    ap.add_argument("--host-port", help="接主板 TX 的串口")
    ap.add_argument("--bl-port", help="接模组 TX 的串口")
    ap.add_argument("--port", help="emulate 模式：接模组的串口")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--fw", help="emulate 模式：固件文件")
    ap.add_argument("--expect-version", type=int, help="包内版本值（major*100+minor*10+patch）")
    ap.add_argument("--idle-stop", type=float, default=20.0, help="空闲多少秒后自动出结论，0=不自动停")
    ap.add_argument("--out-dir")
    ap.add_argument("--corrupt-block", type=int, default=0, help="emulate：该块先发坏 CRC")
    ap.add_argument("--bad-seq-block", type=int, default=0, help="emulate：该块发错块号")
    ap.add_argument("--dup-block", type=int, default=0, help="emulate：该块重复发送")
    ap.add_argument("--delay-ms", type=float, default=0, help="emulate：每块前延时")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()

    if a.selftest:
        selftest(); return
    if a.mode == "monitor":
        run_monitor(a)
    elif a.mode == "emulate":
        if not a.port or not a.fw:
            sys.exit("emulate 需要 --port 和 --fw")
        run_emulate(a)
    else:
        ap.print_help()


if __name__ == "__main__":
    main()
