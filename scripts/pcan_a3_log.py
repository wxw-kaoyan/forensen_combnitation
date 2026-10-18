#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PCAN CAN FD 四路 IMU (0x272 / 0x200 / 0x28F / 0x06F) 连续采集 + A3 解码

用法:
    python pcan_a3_log.py [-m 40] [-o 输出.csv]

输出 CSV 每行一帧（4 个 ID 混在一起，用 can_id 区分）：
    本地时间, 设备时间戳us, CAN_ID, cycle, GPS周, 周内毫秒, dt(ms),
    陀螺X/Y/Z(deg/s), 加表X/Y/Z(m/s^2), 温度(C),
    四元数w/x/y/z, |q|, 四元数增量(deg), 陀螺积分预期(deg), 比值, 运行状态,
    尾部字节50-59, CRC32, 标记

★ 核心判据（看积分是否发散）：
    用相邻两帧的四元数算增量角 Δq = 2·acos(|q1·q2|)，
    再用陀螺原始值算预期 Δω = |ω|·dt。
    正常时 两者同量级（比值≈1）；
    若固件在周内毫秒回绕时算错 dt，会出现 Δq 巨大而 Δω 正常 —— 即"积分发散"。
    该帧在终端打 ***发散*** 并在 CSV 标记列写 DIVERGE。
"""
import argparse, ctypes, csv, math, os, struct, sys, time
from datetime import datetime

DLL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "package", "PCANBasic.dll")
DLL = os.path.normpath(DLL)
PCAN_USBBUS1 = 0x51
IDS = [0x272, 0x200, 0x28F, 0x06F]
NAMES = {0x272: "A3-1", 0x200: "A3-2", 0x28F: "A3-3", 0x06F: "A3-4"}
# 实测可用：标称 1 Mbit/s(75%) / 数据 5 Mbit/s(75%)
BITRATE = b"f_clock=80000000,nom_brp=10,nom_tseg1=5,nom_tseg2=2,nom_sjw=1," \
          b"data_brp=2,data_tseg1=5,data_tseg2=2,data_sjw=1"
ERR_OK, ERR_EMPTY = 0x00000, 0x00020
WRAP_MS = 604800000


class TPCANMsgFD(ctypes.Structure):
    _fields_ = [("ID", ctypes.c_uint32), ("MSGTYPE", ctypes.c_uint8),
                ("DLC", ctypes.c_uint8), ("DATA", ctypes.c_uint8 * 64)]


def f32(d, o): return struct.unpack_from("<f", d, o)[0]
def u16(d, o): return struct.unpack_from("<H", d, o)[0]
def u32(d, o): return struct.unpack_from("<I", d, o)[0]


COLS = ["本地时间", "设备时间戳us", "CAN_ID", "cycle", "GPS周", "周内毫秒", "dt(ms)",
        "陀螺X(deg/s)", "陀螺Y(deg/s)", "陀螺Z(deg/s)",
        "加表X(m/s2)", "加表Y(m/s2)", "加表Z(m/s2)", "温度(C)",
        "四元数w", "四元数x", "四元数y", "四元数z", "|q|",
        "四元数增量(deg)", "陀螺积分预期(deg)", "比值",
        "运行状态", "尾部字节50-59", "CRC32", "标记"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-m", "--minutes", type=float, default=40.0, help="采集时长(分钟)")
    ap.add_argument("-o", "--out", default=None, help="输出 CSV")
    ap.add_argument("--dll", default=DLL)
    a = ap.parse_args()

    out = a.out or os.path.join(os.path.expanduser("~"), "Desktop", "数据",
                                f"a3_log_{datetime.now():%Y%m%d_%H%M%S}.csv")
    os.makedirs(os.path.dirname(out), exist_ok=True)

    p = ctypes.WinDLL(a.dll)
    p.CAN_InitializeFD.argtypes = [ctypes.c_uint16, ctypes.c_char_p]
    p.CAN_InitializeFD.restype = ctypes.c_uint32
    p.CAN_ReadFD.argtypes = [ctypes.c_uint16, ctypes.POINTER(TPCANMsgFD),
                             ctypes.POINTER(ctypes.c_uint64)]
    p.CAN_ReadFD.restype = ctypes.c_uint32
    p.CAN_Uninitialize.argtypes = [ctypes.c_uint16]
    p.CAN_Reset.argtypes = [ctypes.c_uint16]
    p.CAN_GetErrorText.argtypes = [ctypes.c_uint32, ctypes.c_uint16, ctypes.c_char_p]
    p.CAN_GetErrorText.restype = ctypes.c_uint32

    def et(c):
        b = ctypes.create_string_buffer(256)
        p.CAN_GetErrorText(c, 0, b)
        return b.value.decode("gbk", "replace")

    st = p.CAN_InitializeFD(PCAN_USBBUS1, BITRATE)
    print(f"CAN_InitializeFD -> 0x{st:05X} {et(st)}")
    if st != ERR_OK:
        sys.exit(1)
    print(f"已连 PCAN_USBBUS1  CAN FD 1M/5M    监控 ID: "
          + " ".join(f"0x{i:03X}" for i in IDS))
    print(f"输出: {out}")
    print(f"时长: {a.minutes:.1f} 分钟\n")

    fh = open(out, "w", newline="", encoding="utf-8-sig")
    w = csv.writer(fh)
    w.writerow(COLS)
    fh.flush()

    msg = TPCANMsgFD()
    ts = ctypes.c_uint64()
    counts = {i: 0 for i in IDS}
    prev = {i: None for i in IDS}          # (周内毫秒, 四元数)
    wrapped = {i: 0 for i in IDS}
    ndiv = {i: 0 for i in IDS}
    errs = {}
    t0 = time.time()
    t_stat = t0
    while time.time() - t0 < a.minutes * 60:
        st = p.CAN_ReadFD(PCAN_USBBUS1, ctypes.byref(msg), ctypes.byref(ts))
        if st == ERR_EMPTY:
            time.sleep(0.0005)
            continue
        if st != ERR_OK:
            errs[st] = errs.get(st, 0) + 1
            if errs[st] % 200 == 0:
                p.CAN_Reset(PCAN_USBBUS1)
            continue
        if msg.ID not in counts:
            continue
        d = bytes(msg.DATA)
        if len(d) < 64:
            continue
        ms = u32(d, 3)
        q = [f32(d, 33 + 4 * i) for i in range(4)]
        qn = math.sqrt(sum(x * x for x in q))
        gx, gy, gz = f32(d, 7), f32(d, 11), f32(d, 15)
        tag = ""
        dtm = dq = dth = float("nan")
        ratio = float("nan")
        pv = prev[msg.ID]
        if pv is not None:
            dtm = ms - pv[0]
            if dtm < 0:                        # 周内毫秒回绕
                dtm += WRAP_MS
                wrapped[msg.ID] += 1
                tag = "WRAP"
            wm = math.sqrt(gx * gx + gy * gy + gz * gz)      # deg/s
            dth = wm * dtm / 1000.0                          # 陀螺积分预期增量角
            dot = min(1.0, max(-1.0, abs(sum(a * b for a, b in zip(q, pv[1])))))
            dq = math.degrees(2.0 * math.acos(dot))          # 四元数实际增量角
            ratio = dq / dth if dth > 1e-9 else float("nan")
            if dq > max(3.0 * dth + 2.0, 5.0):
                tag = (tag + "|DIVERGE").lstrip("|")
                ndiv[msg.ID] += 1
                if ndiv[msg.ID] <= 6:
                    print(f"\n*** 积分发散 {NAMES[msg.ID]} 0x{msg.ID:03X} *** "
                          f"周内ms {pv[0]} -> {ms}  dt={dtm:.0f}ms  "
                          f"四元数增量 {dq:.2f}°  vs 陀螺预期 {dth:.3f}°  "
                          f"比值 {ratio:.1f}   ({datetime.now():%H:%M:%S})")
        if abs(qn - 1.0) > 0.01 and "DIVERGE" not in tag:
            tag = (tag + "||q|!=1").lstrip("|")
        prev[msg.ID] = (ms, q)
        w.writerow([f"{datetime.now():%Y-%m-%d %H:%M:%S.%f}"[:-3], ts.value,
                    f"0x{msg.ID:03X}", d[0], u16(d, 1), ms, f"{dtm:.1f}",
                    f"{gx:.6f}", f"{gy:.6f}", f"{gz:.6f}",
                    f"{f32(d,19):.6f}", f"{f32(d,23):.6f}", f"{f32(d,27):.6f}",
                    f"{struct.unpack_from('<h', d, 31)[0]*0.01:.2f}",
                    f"{q[0]:.7f}", f"{q[1]:.7f}", f"{q[2]:.7f}", f"{q[3]:.7f}",
                    f"{qn:.6f}", f"{dq:.4f}", f"{dth:.4f}", f"{ratio:.3f}",
                    d[49], " ".join(f"{b:02x}" for b in d[50:60]),
                    u32(d, 60), tag])
        counts[msg.ID] += 1

        now = time.time()
        if now - t_stat >= 5.0:
            t_stat = now
            el = now - t0
            line = "  ".join(f"{NAMES[i]}:{counts[i]:>6}" for i in IDS)
            ms_txt = "  ".join(f"{NAMES[i]}:{prev[i][0] if prev[i] else 0:>10}" for i in IDS)
            mx = max([prev[i][0] for i in IDS if prev[i]] or [0])
            print(f"[{el/60:5.1f}min] {line} | 周内ms {ms_txt}"
                  f" | 距604800000 {WRAP_MS - mx:>9} ms"
                  f" | 发散 {sum(ndiv.values())}"
                  + (f" | 总线错误 {sum(errs.values())}" if errs else ""))
            fh.flush()

    fh.close()
    p.CAN_Uninitialize(PCAN_USBBUS1)
    print(f"\n采集结束，共 {(time.time()-t0)/60:.1f} 分钟")
    for i in IDS:
        print(f"  {NAMES[i]} 0x{i:03X}: {counts[i]} 帧 = {counts[i]/(time.time()-t0):.2f} Hz"
              f"   回绕 {wrapped[i]} 次   发散 {ndiv[i]} 次")
    if errs:
        print("  总线错误: " + ", ".join(f"0x{k:05X}({et(k)})×{v}" for k, v in errs.items()))
    print(f"  CSV: {out}")


if __name__ == "__main__":
    main()
