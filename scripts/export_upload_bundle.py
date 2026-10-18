# -*- coding: utf-8 -*-
"""导出上传包：
  1) 10.14/10.15/10.16 三天 MDU 舞蹈动作片段 -> 每天合并成一个 CSV（pelvis/torso 各一份）
  2) 跨周原始记录 7天数据.trc -> CSV（CAN FD 原始帧 + A3 解码）
"""
import glob
import os
import struct
import time
import numpy as np

OUT = r"C:\Users\wxw\Desktop\QT\TASoftware\forensen_combnitation\output_navdata\mdu_upload_20261016"
TRC = r"C:\Users\wxw\Desktop\7天数据.trc"

ROOTS = {
    "2026.10.14": r"C:\Users\wxw\Desktop\logs\mdu_imulog\2026.10.14",
    "2026.10.15": r"C:\Users\wxw\Desktop\logs\mdu_imulog\2026.10.15\mdu_imulog",
    "2026.10.16": r"C:\Users\wxw\Desktop\logs\mdu_imulog\2026.10.16\mdu_imulog",
}

# ── 舞蹈检测参数 ──
WIN = 1.0      # 滑窗(s)
THR = 0.6      # 窗内 roll/pitch 标准差阈值(deg)
GAP = 20.0     # 小于该间隔的两段运动合并(s)
MIN_LEN = 8.0  # 最短有效运动段(s)
PAD = 1.0      # 两端外扩(s)

IDS = {0x272: "A3-1", 0x200: "A3-2", 0x28F: "A3-3", 0x06F: "A3-4"}
WRAP_MS = 604800000


def hhmmss(ns):
    return time.strftime("%H:%M:%S", time.localtime(ns / 1e9))


# ══════════════════════ 1. 舞蹈片段 ══════════════════════
def load_pp(path):
    """快速读取 stamp_ns / roll_deg / pitch_deg 三列。"""
    with open(path, "r", encoding="utf-8-sig") as f:
        f.readline()
        rows = []
        for line in f:
            p = line.split(",")
            if len(p) < 9:
                continue
            rows.append((float(p[1]), float(p[7]), float(p[8])))
    a = np.array(rows, dtype=float)
    if a.size == 0:
        return np.zeros(0), np.zeros(0), np.zeros(0)
    return a[:, 0], a[:, 1], a[:, 2]


def detect(ts, roll, pitch):
    n = ts.size
    dur = (ts[-1] - ts[0]) / 1e9
    fs = (n - 1) / dur
    W = max(1, int(round(WIN * fs)))
    k = np.ones(W) / W
    pm = np.convolve(pitch, k, "same")
    rm = np.convolve(roll, k, "same")
    pstd = np.sqrt(np.maximum(np.convolve(pitch * pitch, k, "same") - pm * pm, 0))
    rstd = np.sqrt(np.maximum(np.convolve(roll * roll, k, "same") - rm * rm, 0))
    act = np.maximum(pstd, rstd) > THR
    idx = np.where(act)[0]
    if idx.size == 0:
        return []
    segs, s, p = [], idx[0], idx[0]
    for i in idx[1:]:
        if (ts[i] - ts[p]) / 1e9 > GAP:
            segs.append((s, p))
            s = i
        p = i
    segs.append((s, p))
    out = []
    for s, e in segs:
        if (ts[e] - ts[s]) / 1e9 < MIN_LEN:
            continue
        a = max(0, s - int(round(PAD * fs)))
        b = min(n - 1, e + int(round(PAD * fs)))
        out.append((float(ts[a]), float(ts[b])))
    return out


def extract_day(day, root):
    bags = sorted(glob.glob(os.path.join(root, "*", "pelvis_imu.csv")))
    bags = [b for b in bags if os.path.getsize(b) > 100]
    ts_l, r_l, p_l = [], [], []
    for b in bags:
        t, r, p = load_pp(b)
        ts_l.append(t); r_l.append(r); p_l.append(p)
    ts = np.concatenate(ts_l); roll = np.concatenate(r_l); pitch = np.concatenate(p_l)
    o = np.argsort(ts, kind="stable")
    ts, roll, pitch = ts[o], roll[o], pitch[o]
    ts, u = np.unique(ts, return_index=True)      # 去重（包之间有重叠）
    roll, pitch = roll[u], pitch[u]
    iv = detect(ts, roll, pitch)
    print("  %s  舞蹈段 %d 个:" % (day, len(iv)))
    for a, b in iv:
        print("      %s ~ %s   %.1f s" % (hhmmss(a), hhmmss(b), (b - a) / 1e9))

    for topic in ("pelvis", "torso"):
        outcsv = os.path.join(OUT, "%s_dance_%s.csv" % (day, topic))
        hdr = None
        kept = 0
        last = -1.0
        with open(outcsv, "w", newline="", encoding="utf-8-sig") as fo:
            for b in bags:
                f = os.path.join(os.path.dirname(b), "%s_imu.csv" % topic)
                if not os.path.isfile(f) or os.path.getsize(f) < 100:
                    continue
                with open(f, "r", encoding="utf-8-sig") as fi:
                    head = fi.readline()
                    if hdr is None:
                        hdr = head
                        fo.write(head)
                    for line in fi:
                        c = line.find(",")
                        c2 = line.find(",", c + 1)
                        try:
                            s = float(line[c + 1:c2])
                        except ValueError:
                            continue
                        if s <= last:
                            continue
                        if not any(a <= s <= bb for a, bb in iv):
                            continue
                        fo.write(line)
                        last = s
                        kept += 1
        print("      -> %s  (%d 行)" % (os.path.basename(outcsv), kept))


# ══════════════════════ 2. 跨周 trc -> CSV ══════════════════════
def f32(d, o): return struct.unpack_from("<f", d, o)[0]
def u16(d, o): return struct.unpack_from("<H", d, o)[0]
def u32(d, o): return struct.unpack_from("<I", d, o)[0]


def trc_to_csv():
    outcsv = os.path.join(OUT, "7day_wrap_raw.csv")
    start = None
    with open(TRC, "r", encoding="utf-8", errors="replace") as fi:
        for line in fi:
            if line.startswith(";   Start time:"):
                start = line.split(":", 1)[1].strip()[:-2]   # 去掉结尾 ".0"
                break
        fi.seek(0)
        import datetime as dt
        t0 = dt.datetime.strptime(start, "%Y/%m/%d %H:%M:%S.%f")
        cols = ["序号", "绝对时间", "时间偏移ms", "类型", "CAN_ID", "方向", "DLC", "数据hex",
                "cycle", "GPS周", "周内毫秒", "陀螺X(deg/s)", "陀螺Y(deg/s)", "陀螺Z(deg/s)",
                "加表X(m/s2)", "加表Y(m/s2)", "加表Z(m/s2)", "温度(C)",
                "Q0", "Q1", "Q2", "Q3", "|q|", "RunStatus", "尾部字节50-59", "CRC32"]
        nframe = nde = 0
        with open(outcsv, "w", newline="", encoding="utf-8-sig") as fo:
            import csv as _csv
            w = _csv.writer(fo)
            w.writerow(cols)
            for line in fi:
                line = line.strip()
                if not line or line.startswith(";"):
                    continue
                t = line.split()
                if len(t) < 5:
                    continue
                n, off, typ = t[0], t[1], t[2]
                try:
                    ms = float(off)
                except ValueError:
                    continue
                abst = (t0 + dt.timedelta(milliseconds=ms)).strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
                if typ == "ST":
                    data = t[4:]
                    w.writerow([n, abst, off, typ, "", t[3], len(data),
                                " ".join(data)] + [""] * 18)
                    nframe += 1
                    continue
                cid = int(t[3], 16)
                dlc = int(t[5])
                raw = t[6:6 + dlc]
                extra = [""] * 18
                if cid in IDS and dlc == 64 and len(raw) == 64:
                    try:
                        d = bytes(int(x, 16) for x in raw)
                        q = [f32(d, 33 + 4 * i) for i in range(4)]
                        qn = (q[0] ** 2 + q[1] ** 2 + q[2] ** 2 + q[3] ** 2) ** 0.5
                        extra = [d[0], u16(d, 1), u32(d, 3),
                                 "%.6f" % f32(d, 7), "%.6f" % f32(d, 11), "%.6f" % f32(d, 15),
                                 "%.6f" % f32(d, 19), "%.6f" % f32(d, 23), "%.6f" % f32(d, 27),
                                 "%.2f" % (struct.unpack_from("<h", d, 31)[0] * 0.01),
                                 "%.7f" % q[0], "%.7f" % q[1], "%.7f" % q[2], "%.7f" % q[3],
                                 "%.6f" % qn, d[49],
                                 " ".join("%02x" % x for x in d[50:60]), u32(d, 60)]
                        nde += 1
                    except Exception:
                        pass
                w.writerow([n, abst, off, typ, "0x%03X" % cid, t[4], dlc,
                            " ".join(raw)] + extra)
                nframe += 1
    print("  trc 共 %d 帧（其中 A3 有效解码 %d 帧）-> %s" % (nframe, nde, os.path.basename(outcsv)))


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    print("=" * 70)
    for day, root in ROOTS.items():
        extract_day(day, root)
    print("=" * 70)
    trc_to_csv()
    print("完成 ->", OUT)
