# -*- coding: utf-8 -*-
"""data2 step2: 看设备 KF 内部的协方差/新息/零偏, 定位俯仰收敛慢的原因"""
import csv, os, math

D = r'D:\工作目录\问题记录\丰疆挖机问题\data2'
KF = os.path.join(D, '挖机俯仰角发散_614EB_0x0131_KF.csv')


def open_smart(p):
    for enc in ('utf-8-sig', 'gbk', 'utf-8'):
        try:
            f = open(p, encoding=enc); f.readline(); f.seek(0); return f
        except UnicodeDecodeError:
            continue
    raise RuntimeError(p)


def bj(ms):
    s = ms / 1000.0
    d = int(s // 86400)
    x = (s - d * 86400 + 8 * 3600) % 86400
    return '%02d:%02d:%04.1f' % (int(x // 3600), int(x % 3600 // 60), x % 60)


f = open_smart(KF)
r = csv.reader(f)
h = [x.strip() for x in next(r)]
idx = {h[i]: i for i in range(len(h))}
rows = []
for c in r:
    if len(c) < len(h):
        continue
    try:
        v = [float(x) for x in c]
    except ValueError:
        continue
    if 209717000.0 <= v[-1] <= 209718000.0 + 330000.0:   # -1s 起, 覆盖转折点
        rows.append(v)
f.close()
print('KF.csv 行数(窗口内)=%d   时间 %.0f ~ %.0f' % (len(rows), rows[0][-1], rows[-1][-1]))

print('\n列索引参考: 协方差0-20 = [位置3 速度3 姿态3 陀螺零偏3 加计零偏3 陀螺标度3 加计标度3]')
print('            姿态: 协方差6=横滚 7=俯仰 8=航向')

print('\n%-10s %8s %8s %8s | %8s %8s %8s | %8s %8s %8s | %9s %9s'
      % ('北京时', '俯仰', '横滚', '航向',
         'sd姿态R', 'sd姿态P', 'sd姿态Y',
         '新息3', '新息4', '新息5',
         '陀零偏Y', '加零偏Y'))

sel = [x for x in rows if x[-1] >= 209718000.0 + 130000.0]   # 18:17:10 起
step = max(1, len(sel) // 40)
for k in range(0, len(sel), step):
    x = sel[k]
    sd = lambda i: math.sqrt(max(0.0, x[i]))
    print('%-10s %8.3f %8.3f %8.3f | %8.2e %8.2e %8.2e | %8.4f %8.4f %8.4f | %9.4f %9.5f'
          % (bj(x[-1]), x[idx['俯仰(deg)']], x[idx['横滚(deg)']], x[idx['航向(deg)']],
             sd(6), sd(7), sd(8), x[22], x[23], x[24],
             x[idx['陀螺零偏Y(deg/s)']], x[idx['加表零偏Y(g)']]))

print('\n静止段(18:17:15~18:22)统计:')
st = [x for x in rows if x[-1] >= 209718000.0 + 135000.0]
for name in ['俯仰(deg)', '横滚(deg)', '陀螺零偏X(deg/s)', '陀螺零偏Y(deg/s)', '陀螺零偏Z(deg/s)',
             '加表零偏X(g)', '加表零偏Y(g)', '加表零偏Z(g)']:
    v = [x[idx[name]] for x in st]
    print('   %-20s 首=%+10.6f 末=%+10.6f 变化=%+10.6f' % (name, v[0], v[-1], v[-1] - v[0]))
for i in [6, 7, 8]:
    v = [math.sqrt(max(0.0, x[i])) for x in st]
    print('   sd姿态%d             首=%10.3e 末=%10.3e' % (i - 6, v[0], v[-1]))
for i in [22, 23, 24]:
    v = [x[i] for x in st]
    print('   新息%d               首=%+10.4f 末=%+10.4f 均值=%+10.4f' % (i - 22, v[0], v[-1], sum(v) / len(v)))
