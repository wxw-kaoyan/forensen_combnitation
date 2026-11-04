# -*- coding: utf-8 -*-
"""
data2 数据截断 + 改名

1) 只保留起始时刻之后的数据: 周内毫秒 >= 209718000  (GPS周 2347, 北京时间 2026-09-22 18:15)
2) 文件名加前缀 "挖机俯仰角发散_"
3) imu.bin 是二进制, 不动

各文件时间列:
  614EB_0x0101_IMU.csv    第 0 列  周内毫秒
  614EB_0x0111_GNSS.csv   第 0 列  位置ITOW(ms)
  614EB_0x0131_KF.csv     第 48 列 ITOW(ms)      <- 在最后一列
  parse_result.csv        第 19 列 时间戳
"""
import os

D = r'D:\工作目录\问题记录\丰疆挖机问题\data2'
T0 = 209718000.0          # 周内毫秒起始时刻
PFX = '挖机俯仰角发散_'
NAMES = ('周内毫秒', '位置ITOW(ms)', '时间戳', 'ITOW(ms)')
FILES = ['614EB_0x0101_IMU.csv', '614EB_0x0111_GNSS.csv',
         '614EB_0x0131_KF.csv', 'parse_result.csv']


def main():
    # 清理上次中断留下的临时文件
    for x in os.listdir(D):
        if x.endswith('.tmp'):
            try:
                os.remove(os.path.join(D, x))
                print('清理临时文件:', x)
            except OSError as e:
                print('临时文件清理失败:', x, e)

    for fn in FILES:
        p = os.path.join(D, fn)
        if not os.path.exists(p):
            print('%-24s 原文件不存在, 跳过' % fn)
            continue

        with open(p, encoding='utf-8-sig', newline='') as f:
            hdr = f.readline()
            h = hdr.rstrip('\r\n').split(',')
            tc = -1
            for i, name in enumerate(h):
                if name.strip() in NAMES:
                    tc = i
                    break
            if tc < 0:
                print('%-24s 未找到时间列, 跳过' % fn)
                continue

            n = nk = 0
            keep = []
            t_prev = None
            n_back = 0        # 时间回绕/回退次数
            for line in f:
                c = line.split(',')
                if len(c) <= tc:
                    continue
                try:
                    v = float(c[tc])
                except ValueError:
                    continue
                n += 1
                if t_prev is not None and v < t_prev:
                    n_back += 1
                t_prev = v
                if v >= T0:
                    keep.append(line)
                    nk += 1

        if nk == 0:
            print('%-24s 第%d列(%s)  %d行 -> 0行(无满足条件的数据), 未改动'
                  % (fn, tc, h[tc], n))
            continue

        with open(os.path.join(D, PFX + fn), 'w', encoding='utf-8-sig', newline='') as g:
            g.write(hdr)
            g.writelines(keep)
        os.remove(p)

        t0k, t1k = keep[0].split(',')[tc], keep[-1].split(',')[tc]
        print('%-24s 第%d列(%s)  %d行 -> %d行   首=%.0f 末=%.0f   时间回退次数=%d'
              % (fn, tc, h[tc], n, nk, float(t0k), float(t1k), n_back))

    print('\n目录现状:')
    for x in sorted(os.listdir(D)):
        print('   %-44s %8.2f MB' % (x, os.path.getsize(os.path.join(D, x)) / 1e6))


if __name__ == '__main__':
    main()
