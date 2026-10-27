#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 AimRT（MCAP）录包中导出 IMU 数据为 CSV，并给出丢包（丢帧）统计。

用法：
    python aimrt_imu_export.py <单个 .mcap 文件> [-o 输出目录]
                               [-t 话题 ...] [--check-rate 500]

输出：
    <输出目录>/<包名>/<话题短名>.csv    每个话题一个 CSV
    stdout 先打印人类可读统计，再在一对标记之间打印 JSON 摘要（供上位机解析）：
        AIMRT_SUMMARY_BEGIN
        { ... }
        AIMRT_SUMMARY_END

话题选择：不指定 -t 时，自动导出包内所有 schema 为 sensor_msgs/msg/Imu 的话题
（不同版本机器人的话题命名不同，如 /body_drive/torso_imu/data、
/aima/hal/imu/torso/state，因此不写死话题名）。

CSV 每行一条消息：bag 记录时间、消息 header 时间、四元数、由四元数换算的欧拉角
（度；roll [-180,180]、pitch [-90,90]、yaw [0,360)）、角速度、线加速度。
丢包统计按 --check-rate（默认 500Hz）检查 header.stamp 的连续性，估算丢帧数。

依赖：mcap、mcap-ros2-support（随脚本放在 _pylibs/ 下即可，无需 pip 安装）
"""

import argparse
import csv
import json
import math
import os
import sys

# 允许直接使用随脚本放置的依赖目录（若存在）
_HERE = os.path.dirname(os.path.abspath(__file__))
_LOCAL_LIBS = os.path.join(_HERE, "_pylibs")
if os.path.isdir(_LOCAL_LIBS):
    sys.path.insert(0, _LOCAL_LIBS)

from mcap.reader import make_reader  # noqa: E402
from mcap_ros2.decoder import DecoderFactory  # noqa: E402

COLUMNS = [
    "log_time_ns",             # bag 记录该消息的时间（纳秒）
    "stamp_ns",                # 消息 header.stamp（纳秒）
    "frame_id",                # 消息 header.frame_id
    "orientation_x",
    "orientation_y",
    "orientation_z",
    "orientation_w",
    "roll_deg",                # 由四元数换算，[-180,180]
    "pitch_deg",               # [-90,90]
    "yaw_deg",                 # [0,360)
    "angular_velocity_x",
    "angular_velocity_y",
    "angular_velocity_z",
    "linear_acceleration_x",
    "linear_acceleration_y",
    "linear_acceleration_z",
]

SUMMARY_BEGIN = "AIMRT_SUMMARY_BEGIN"
SUMMARY_END = "AIMRT_SUMMARY_END"


def topic_short(topic):
    parts = topic.strip("/").split("/")
    return parts[-2] if len(parts) >= 2 else parts[-1]


def topic_to_filename(topic):
    # 文件名用话题短名（CSV 已放在与 mcap 同名的文件夹下）
    return topic_short(topic) + ".csv"


def discover_imu_topics(reader, schema_name="sensor_msgs/msg/Imu"):
    """从包内自动发现所有 IMU 话题（按 schema 判定，不依赖话题命名）。

    不同版本机器人的话题前缀不同（旧包 /body_drive/xxx_imu/data、
    新包 /aima/hal/imu/torso/state），因此不写死话题名。
    """
    summary = reader.get_summary()
    if summary is None:
        return []
    imu_schema_ids = {sid for sid, sc in summary.schemas.items()
                      if sc.name == schema_name}
    if not imu_schema_ids:
        return []
    topics = [ch.topic for ch in summary.channels.values()
              if ch.schema_id in imu_schema_ids]
    return sorted(set(topics))


def quat_to_euler_deg(x, y, z, w):
    """四元数 (x,y,z,w) 换算为欧拉角，单位：度（ZYX 内旋）。"""
    roll = math.atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y))
    sin_pitch = max(-1.0, min(1.0, 2.0 * (w * y - z * x)))
    pitch = math.asin(sin_pitch)
    yaw = math.atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z))
    return math.degrees(roll), math.degrees(pitch), math.degrees(yaw) % 360.0


def imu_row(log_time, msg):
    stamp = msg.header.stamp
    o = msg.orientation
    roll, pitch, yaw = quat_to_euler_deg(o.x, o.y, o.z, o.w)
    return [
        log_time,
        stamp.sec * 1_000_000_000 + stamp.nanosec,
        msg.header.frame_id,
        o.x, o.y, o.z, o.w,
        roll, pitch, yaw,
        msg.angular_velocity.x,
        msg.angular_velocity.y,
        msg.angular_velocity.z,
        msg.linear_acceleration.x,
        msg.linear_acceleration.y,
        msg.linear_acceleration.z,
    ]


def rate_stats(stamps, exp_ns):
    """按固定间隔 exp_ns 检查连续性，返回统计字典（exp_ns=0 表示不检查）。"""
    n = len(stamps)
    st = {
        "rows": n,
        "hz": 0.0,
        "intervalMedianUs": 0.0,
        "intervalMinUs": 0.0,
        "intervalMaxUs": 0.0,
        "devMinUs": 0.0,
        "devMaxUs": 0.0,
        "missed": 0,
        "maxGapUs": 0.0,
        "maxGapAtNs": 0,
        "firstStampNs": stamps[0] if n else 0,
        "lastStampNs": stamps[-1] if n else 0,
        "durationS": 0.0,
    }
    if n < 2:
        return st

    diffs = [b - a for a, b in zip(stamps, stamps[1:])]
    span_s = (stamps[-1] - stamps[0]) / 1e9
    st["hz"] = (n - 1) / span_s if span_s > 0 else 0.0
    st["durationS"] = span_s
    st["intervalMedianUs"] = sorted(diffs)[n // 2] / 1e3
    st["intervalMinUs"] = min(diffs) / 1e3
    st["intervalMaxUs"] = max(diffs) / 1e3

    gap = max(diffs)
    st["maxGapUs"] = gap / 1e3
    st["maxGapAtNs"] = stamps[diffs.index(gap) + 1]

    if exp_ns > 0:
        st["devMinUs"] = (min(diffs) - exp_ns) / 1e3
        st["devMaxUs"] = (max(diffs) - exp_ns) / 1e3
        st["missed"] = sum(max(0, int(round(d / exp_ns)) - 1) for d in diffs)
    return st


def main():
    ap = argparse.ArgumentParser(description="导出 AimRT bag 中的 IMU 数据为 CSV")
    ap.add_argument("mcap", help="单个 .mcap 文件")
    ap.add_argument("-o", "--out-dir",
                    default=None,
                    help="CSV 输出目录（默认：mcap 同级目录下的 imu_csv）")
    ap.add_argument("-t", "--topics",
                    nargs="+", default=None,
                    help="要导出的话题（默认：自动发现包内所有 sensor_msgs/msg/Imu 话题）")
    ap.add_argument("--check-rate", type=float, default=500.0,
                    help="按该频率（Hz）检查采样是否丢帧，0 表示不检查")
    args = ap.parse_args()

    path = args.mcap
    if not os.path.isfile(path):
        sys.exit("未找到 .mcap 文件：%s" % path)
    if not path.lower().endswith(".mcap"):
        sys.exit("只支持 .mcap 文件：%s" % path)

    out_dir = args.out_dir or os.path.join(os.path.dirname(os.path.abspath(path)), "imu_csv")
    stem = os.path.splitext(os.path.basename(path))[0]
    out_sub = os.path.join(out_dir, stem)
    os.makedirs(out_sub, exist_ok=True)

    check = args.check_rate > 0
    exp_ns = int(round(1_000_000_000 / args.check_rate)) if check else 0

    counts, stamps, frame_ids, first_log, last_log = {}, {}, {}, {}, {}
    writes = {}
    handles, writers = {}, {}
    try:
        decoder_factory = DecoderFactory()
        with open(path, "rb") as f:
            reader = make_reader(f, decoder_factories=[decoder_factory])

            # 未显式指定话题时，按 schema 自动发现包内全部 IMU 话题
            if args.topics:
                topics = list(args.topics)
            else:
                topics = discover_imu_topics(reader)
                if not topics:
                    sys.exit("包内未找到 sensor_msgs/msg/Imu 话题，可用 -t 手动指定")

            for t in topics:
                counts[t] = 0
                stamps[t] = []
                frame_ids[t] = ""
                first_log[t] = 0
                last_log[t] = 0
                writes[t] = os.path.join(out_sub, topic_to_filename(t))
                fh = open(writes[t], "w", newline="", encoding="utf-8")
                w = csv.writer(fh)
                w.writerow(COLUMNS)
                handles[t] = fh
                writers[t] = w

            for _schema, channel, message, decoded in reader.iter_decoded_messages(
                topics=topics, log_time_order=True
            ):
                row = imu_row(message.log_time, decoded)
                writers[channel.topic].writerow(row)
                counts[channel.topic] += 1
                stamps[channel.topic].append(row[1])
                if not frame_ids[channel.topic]:
                    frame_ids[channel.topic] = row[2]
                if counts[channel.topic] == 1:
                    first_log[channel.topic] = row[0]
                last_log[channel.topic] = row[0]
    finally:
        for fh in handles.values():
            fh.close()

    # ── 控制台统计（给人看的） ──
    print("文件: %s" % os.path.basename(path))
    for t in topics:
        st = rate_stats(stamps[t], exp_ns)
        print("  %-36s n=%-8d %.2fHz  丢帧%d  -> %s"
              % (t, st["rows"], st["hz"], st["missed"], writes[t]))

    # ── JSON 摘要（给上位机解析的） ──
    summary = {
        "ok": True,
        "file": os.path.abspath(path),
        "fileSize": os.path.getsize(path),
        "stem": stem,
        "outDir": os.path.abspath(out_sub),
        "checkRate": args.check_rate if check else 0,
        "topics": [],
    }
    total_rows = 0
    total_missed = 0
    for t in topics:
        st = rate_stats(stamps[t], exp_ns)
        st.update({
            "topic": t,
            "name": t.split("/")[-2] if t.count("/") >= 2 else t.strip("/"),
            "csv": writes[t],
            "frameId": frame_ids[t],
            "firstLogNs": first_log[t],
            "lastLogNs": last_log[t],
        })
        total_rows += st["rows"]
        total_missed += st["missed"]
        summary["topics"].append(st)
    summary["totalRows"] = total_rows
    summary["totalMissed"] = total_missed

    print(SUMMARY_BEGIN)
    print(json.dumps(summary, ensure_ascii=False))
    print(SUMMARY_END)
    print("\n导出完成 -> %s" % os.path.abspath(out_sub))


if __name__ == "__main__":
    main()
