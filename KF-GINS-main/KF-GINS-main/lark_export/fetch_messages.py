# -*- coding: utf-8 -*-
"""批量抓取飞书会话消息, 落盘为 jsonl, 供离线过滤/汇总"""
import json, io, os, subprocess, sys, time

EXPORT = os.path.dirname(os.path.abspath(__file__))
CHATS = os.path.join(EXPORT, 'chats.json')
OUT = os.path.join(EXPORT, 'messages.jsonl')

START = '2026-07-01T00:00:00+08:00'
END   = '2026-09-15T00:00:00+08:00'

# 排除的机器人/助手类会话
SKIP_NAMES = {
    '假勤','账号安全中心','飞行社','审批','日历助手','任务助手','云文档助手',
    '智能纪要助手','联系人助手','开发者小助手','吴湘伟的智能伙伴'
}

env = dict(os.environ)
env['LARKSUITE_CLI_NO_UPDATE_NOTIFIER'] = '1'
env['LARKSUITE_CLI_NO_SKILLS_NOTIFIER'] = '1'

def run(args):
    p = subprocess.run(['lark-cli'] + args, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', env=env, shell=True)
    return p.stdout or ''

def main():
    d = json.load(io.open(CHATS, encoding='utf-8-sig'))
    chats = d['data']['chats']
    targets = []
    for c in chats:
        name = c.get('name') or ''
        if name in SKIP_NAMES:
            continue
        targets.append(c)
    print(f'待抓取会话: {len(targets)} / {len(chats)}', flush=True)

    n_total = 0
    with io.open(OUT, 'w', encoding='utf-8') as fo:
        for i, c in enumerate(targets, 1):
            cid = c['chat_id']; cname = c.get('name'); ctype = c.get('chat_mode')
            out = run(['im', '+chat-messages-list', '--as', 'user', '--chat-id', cid,
                       '--start', START, '--end', END, '--page-all', '--page-size', '50',
                       '--no-reactions', '--format', 'json'])
            try:
                j = json.loads(out[out.index('{'):])
            except Exception as e:
                print(f'  [{i}/{len(targets)}] {cname}: PARSE-FAIL {e}', flush=True)
                continue
            if not j.get('ok'):
                print(f'  [{i}/{len(targets)}] {cname}: ERR {json.dumps(j.get("error"),ensure_ascii=False)[:120]}', flush=True)
                continue
            items = j.get('data', {}).get('messages') or j.get('data', {}).get('items') or []
            for it in items:
                s = it.get('sender') or {}
                fo.write(json.dumps({
                    'chat_id': cid, 'chat_name': cname, 'chat_type': ctype,
                    'time': it.get('create_time'),
                    'sender': s.get('name'), 'sender_id': s.get('id'),
                    'msg_type': it.get('msg_type'),
                    'content': (it.get('content') or '').strip(),
                }, ensure_ascii=False) + '\n')
            n_total += len(items)
            print(f'  [{i}/{len(targets)}] {cname}: {len(items)} 条 (累计 {n_total})', flush=True)
            time.sleep(0.15)
    print('DONE total messages:', n_total, flush=True)

if __name__ == '__main__':
    main()
