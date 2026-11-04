# -*- coding: utf-8 -*-
"""data2 挖机俯仰角发散 分析 step1: 数据质量 + 姿态时间序列"""
import csv, os, statistics as st

D = r'D:\工作目录\问题记录\丰疆挖机问题\data2'
IMU = os.path.join(D, '挖机俯仰角发散_614EB_0x0101_IMU.csv')


def open_smart(p):
    for enc in ('utf-8-sig', 'gbk', 'utf-8'):
        try:
            f = open(p, encoding=enc)
            f.readline(); f.seek(0); return f, enc
        except UnicodeDecodeError:
            continue
    raise RuntimeError(p)


def bj(ms):
    s = ms / 1000.0
    d = int(s // 86400)
    x = (s - d * 86400 + 8 * 3600) % 86400
    return '周%d %02d:%02d:%05.2f' % (d, int(x // 3600), int(x % 3600 // 60), x % 60)


f, enc = open_smart(IMU)
r = csv.reader(f)
h = [x.strip() for x in next(r)]
print('IMU.csv 列(%d):' % len(h))
for i, c in enumerate(h):
    print('   %2d %s' % (i, c))

idx = {h[i]: i for i in range(len(h))}
rows = []
for c in r:
    if len(c) < len(h):
        continue
    try:
        rows.append([float(x) for x in c])
    except ValueError:
        continue
f.close()

t = [x[0] for x in rows]
print('\n行数=%d  时间 %.0f ~ %.0f  (%.0f ms = %.1f s)' % (len(rows), t[0], t[-1], t[-1] - t[0], (t[-1] - t[0]) / 1000))
print('  首行: %s   末行: %s' % (bj(t[0]), bj(t[-1])))
d = [t[i + 1] - t[i] for i in range(len(t) - 1)]
neg = sum(1 for x in d if x < 0)
print('  dt: 最小=%.0f 中位=%.0f 最大=%.0f ms   回退次数=%d  零间隔=%d'
      % (min(d), st.median(d), max(d), neg, sum(1 for x in d if x == 0)))
print('  -> 等效采样率 %.2f Hz' % (1000.0 / st.median(d)))

# 时间序列: 18:15 ~ 18:21
T0, T1 = 209718000.0, 209718000.0 + 360000.0
sel = [x for x in rows if T0 <= x[0] <= T1]
print('\n窗口 %s ~ %s  共 %d 行' % (bj(T0), bj(T1), len(sel)))

cols = ['零速标志', '陀螺X(deg/s)', '陀螺Y(deg/s)', '陀螺Z(deg/s)',
        '加表X(g)', '加表Y(g)', '加表Z(g)', '温度(C)',
        'QEKF横滚(deg)', 'QEKF俯仰(deg)', 'QEKF航向(deg)',
        'KF横滚(deg)', 'KF俯仰(deg)', 'KF航向(deg)']
ci = [idx[c] for c in cols]

print('\n%-14s %4s %8s %8s %8s %9s %9s %9s %7s %8s %8s %8s %8s'
      % ('北京时', '零速', '陀X', '陀Y', '陀Z', '加X', '加Y', '加Z', '温度',
         'QE俯仰', 'KF俯仰', 'QE横滚', 'KF横滚'))
step = max(1, len(sel) // 45)
for k in range(0, len(sel), step):
    x = sel[k]
    print('%-14s %4.0f %8.3f %8.3f %8.3f %9.4f %9.4f %9.4f %7.1f %8.3f %8.3f %8.3f %8.3f'
          % (bj(x[0]), x[idx['零速标志']], x[idx['陀螺X(deg/s)']], x[idx['陀螺Y(deg/s)']],
             x[idx['陀螺Z(deg/s)']], x[idx['加表X(g)']], x[idx['加表Y(g)']], x[idx['加表Z(g)']],
             x[idx['温度(C)']], x[idx['QEKF俯仰(deg)']], x[idx['KF俯仰(deg)']],
             x[idx['QEKF横滚(deg)']], x[idx['KF横滚(deg)']]))

print('\n窗口内统计:')
for name in cols:
    v = [x[idx[name]] for x in sel]
    print('   %-16s 均值=%+10.4f  标准差=%9.4f  最小=%+10.4f  最大=%+10.4f'
          % (name, st.mean(v), st.pstdev(v), min(v), max(v)))
