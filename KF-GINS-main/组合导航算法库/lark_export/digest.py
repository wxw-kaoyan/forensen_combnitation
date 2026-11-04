# -*- coding: utf-8 -*-
"""过滤低信号消息, 抽取工作项, 输出精简摘要"""
import json, io, os, re, collections

EXPORT = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(EXPORT, 'messages.jsonl')
OUT = os.path.join(EXPORT, 'digest.md')
ME = '吴湘伟'

# ---------- 低信号判定 ----------
GREET = {'好','好的','收到','嗯','ok','OK','Ok','谢谢','感谢','哈哈','行','可以','是的','对',
         '明白','了解','辛苦了','没事','不要紧','是的呢','嗯嗯','好的好的','好嘞','了解了解',
         '有','没有','在','在的','不是','不知道','等着','稍等','等等','马上','看看','好的谢谢'}
EMOJI_RE = re.compile(r'^[\[\]\s（）()、,，。.!！?？~～\-—_…\U0001F300-\U0001FAFF\u2600-\u27BF]+$')
BRACKET_RE = re.compile(r'^(\[[^\]]{1,8}\])+$')

WORK_KW = ['测试','试验','固件','版本','问题','报告','数据','需求','提测','复现','bug','Bug','BUG',
           '方案','标定','对标','精度','误差','航向','航迹','罗经','组合导航','IMU','GNSS','RTK','PPK',
           '里程计','天线','模块','硬件','烧录','升级','发布','评审','审批','排期','计划','文档','SOP',
           '物料','采购','返工','工厂','产线','客户','丰疆','中远','海兰信','南航','推土机','挖掘机',
           '真值','解算','滤波','卡尔曼','陀螺','加计','零偏','基线','双天线','PPS','串口','CAN',
           '验证','分析','排查','定位','联调','对接','需求评审','性能','指标','测试报告','演示']

def is_low(content, msg_type):
    c = (content or '').strip()
    if not c:
        return True
    if re.fullmatch(r'<[^>]+>', c):          # 纯资源占位(无文件名)
        return True
    if EMOJI_RE.match(c) or BRACKET_RE.match(c):
        return True
    if c in GREET:
        return True
    if len(c) <= 3 and not re.search(r'[A-Za-z0-9]', c):
        return True
    return False

def has_kw(c):
    return any(k in c for k in WORK_KW)

def clean(c):
    c = re.sub(r'\s+', ' ', c or '')
    return c.strip()

FILE_RE = re.compile(r'name="([^"]+)"')
LINK_RE = re.compile(r'(https?://[^\s\)\]]+)')

def main():
    msgs = []
    with io.open(SRC, encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line:
                msgs.append(json.loads(line))

    print('总消息:', len(msgs))

    by_chat = collections.defaultdict(list)
    for m in msgs:
        by_chat[m['chat_name']].append(m)

    # 统计
    keep = []
    for m in msgs:
        c = clean(m['content'])
        if is_low(c, m['msg_type']):
            continue
        m['_c'] = c
        keep.append(m)
    print('过滤后:', len(keep))

    lines = []
    lines.append('# 飞书消息精简摘要\n')
    lines.append(f'- 会话数：{len(by_chat)}\n- 原始消息：{len(msgs)}\n- 过滤后：{len(keep)}\n')

    # 时间范围
    times = sorted([m['time'] for m in msgs if m.get('time')])
    if times:
        lines.append(f"- 时间范围：{times[0]} ~ {times[-1]}\n")

    # ---- 1. 会话总览 ----
    lines.append('\n## 一、会话总览\n')
    lines.append('| 会话 | 类型 | 消息数 | 有效 | 时间范围 |')
    lines.append('|---|---|---|---|---|')
    for name, arr in sorted(by_chat.items(), key=lambda x: -len(x[1])):
        ts = sorted([a['time'] for a in arr if a.get('time')])
        eff = sum(1 for a in arr if not is_low(clean(a['content']), a['msg_type']))
        lines.append(f"| {name} | {arr[0]['chat_type']} | {len(arr)} | {eff} | {ts[0][:10]}~{ts[-1][:10]} |")

    # ---- 2. 我发出的文件/文档 ----
    lines.append('\n## 二、我发出的文件/文档\n')
    seen = set()
    for m in msgs:
        if m['sender'] != ME:
            continue
        for fn in FILE_RE.findall(m['content'] or ''):
            k = (m['time'][:10], fn)
            if k in seen:
                continue
            seen.add(k)
            lines.append(f"- {m['time'][:16]} [{m['chat_name']}] `{fn}`")
    if not seen:
        lines.append('- （无）')

    # ---- 3. 我发出的链接 ----
    lines.append('\n## 三、我发出的链接\n')
    seenl = set()
    for m in msgs:
        if m['sender'] != ME:
            continue
        for lk in LINK_RE.findall(m['content'] or ''):
            if lk in seenl:
                continue
            seenl.add(lk)
            lines.append(f"- {m['time'][:16]} [{m['chat_name']}] {lk}")
    if not seenl:
        lines.append('- （无）')

    # ---- 4. 按会话输出工作相关内容 ----
    lines.append('\n## 四、各会话工作内容（已过滤低信号）\n')
    for name, arr in sorted(by_chat.items(), key=lambda x: -len(x[1])):
        eff = [a for a in arr if not is_low(clean(a['content']), a['msg_type'])]
        if not eff:
            continue
        lines.append(f'\n### {name}（{arr[0]["chat_type"]}，{len(arr)} 条，有效 {len(eff)}）\n')
        # 只保留含关键词或较长的
        picked = [a for a in eff if has_kw(clean(a['content'])) or len(clean(a['content'])) >= 18]
        if not picked:
            picked = eff
        for a in picked[:60]:
            c = clean(a['content'])
            if len(c) > 300:
                c = c[:300] + '…'
            lines.append(f"- `{a['time'][5:16]}` **{a['sender']}**: {c}")

    io.open(OUT, 'w', encoding='utf-8').write('\n'.join(lines))
    print('写入:', OUT, os.path.getsize(OUT), 'bytes')

if __name__ == '__main__':
    main()
