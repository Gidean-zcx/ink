import json
from pathlib import Path
p = Path(__file__).resolve().parents[1]
new = [
    {"id":"AUD18-203", "severity":"高", "area":"关机 / 音频 / 持久化",
     "title":"录音未停时关机，写入仍被音频延后，5 秒后直接断电丢失已接受的数据",
     "evidence_type":["复现","代码核查"],
     "locations":["system/services/power/power_service.c:125","system/services/power/power_service.c:277","system/services/audio/audio_service.c:565","system/services/storage/storage_service.c:403"],
     "impact":"未注入故障：录音中接受一条 INK_DB_LAZY 记录，再正常调用 system.shutdown。请求断电时 recording=1 capture=1 db_idle=0 storage_pending=1 durable=1 latest=2；用同一 Flash 镜像重启，记录返回 -5（不存在）。关机只发 flush 并等待最多 5 s，音频写入 hold 则可延后 30 s；没有先停止采集、结束录音或暂停媒体。低电量路径也走同一写入超时逻辑，且仍显示东西都存好了。后者为代码核查。",
     "recommendation":"建立统一关机收尾流程：停止新采集和编辑、收尾录音、释放音频写入 hold、等待持久化回执，再画关机图；普通关机遇失败应可重试或明确告知。低电量必须有受电量约束的收尾与失败表述。",
     "reproduce":"shutdown_audio_probe.c 两轮运行，共用 build-audit/audit-shutdown.img 和 audit-shutdown-sd；第二轮设置 INK_AUDIT_REOPEN=1。完整命令和输出见 experiments/shutdown-audio.log、shutdown-audio-reboot.log。"},
    {"id":"AUD18-204", "severity":"高", "area":"耳机断连 / 音频路由",
     "title":"耳机断开先切外放、后异步暂停，已缓冲的媒体会外放",
     "evidence_type":["复现","代码核查"],
     "locations":["system/services/audio/audio_service.c:645","system/services/audio/audio_service.c:679","system/services/audio/media_service.c:435","system/services/bt/bt_service.c:56"],
     "impact":"媒体执行器暂忙时，模拟耳机离线触发真实 BT 服务切回 speaker；音频任务立即继续播放缓冲帧，暂停仍在媒体队列里。复现额外外放 990 ms，media_state=playing。现有 integration-media 在已等到 PAUSED 后才记录外放基线，不能验证断连到暂停之间没有外放，设计文档给出的保证不成立。",
     "recommendation":"切到 speaker 之前同步停止或静音媒体输出并清理在途输出；将暂停位置和通知交给媒体执行器。测试从耳机断开前开始记录外放样本，并覆盖媒体队列忙的情况。",
     "reproduce":"python3 docs/audits/2026-11-05-codex-services-s3/experiments/run_probe.py docs/audits/2026-11-05-codex-services-b/experiments/headset_probe.c --template sim-media --script docs/audits/2026-11-05-codex-services-b/experiments/audio.script；输出 experiments/headset.log。"},
    {"id":"AUD18-205", "severity":"中", "area":"音频焦点 / 响铃",
     "title":"来电覆盖闹钟后，闹钟继续响但不再持有焦点，录音可以在其下启动",
     "evidence_type":["复现","代码核查"],
     "locations":["system/services/audio/audio_service.c:931","system/services/audio/audio_service.c:1113","system/services/notify/interrupt.c:79","system/services/notify/interrupt.c:96"],
     "impact":"闹钟响铃持有 ALERT；CALL acquire 将其删除。来电和剩余闹钟均 ringing 时，打断服务不再调用 ink_audio_ring(true)，旧 ring_focus 没有恢复。结束来电后 interrupt=alarm、focus=0，500 ms 内仍播放 495 ms 响铃；audio.record 返回 done 并开始采集。闹钟对低档音频的阻挡失效。",
     "recommendation":"让闹钟焦点在 CALL 下暂停并恢复，或让 ring 管理者在焦点丢失和恢复时重新取得有效 token；不能仅用 ring_on 的布尔变化维护焦点生命周期。",
     "reproduce":"python3 docs/audits/2026-11-05-codex-services-s3/experiments/run_probe.py docs/audits/2026-11-05-codex-services-b/experiments/audio_focus_probe.c --template sim-media --script docs/audits/2026-11-05-codex-services-b/experiments/audio.script；输出 experiments/audio-focus.log。"}
]
f=p/'findings.json'
old=json.loads(f.read_text())
ids={x['id'] for x in new}
f.write_text(json.dumps([x for x in old if x['id'] not in ids]+new,ensure_ascii=False,indent=2)+'\n')
f=p/'report.md'
s=f.read_text()
at=s.index('## 修复核查')
body=''
for x in new:
    body+='### '+x['id']+'【'+x['severity']+'】'+x['title']+'\n\n【复现】【代码核查】'+x['impact']+'\n\n位置：'+'；'.join('`'+y+'`' for y in x['locations'])+'。建议：'+x['recommendation']+'\n\n复现：'+x['reproduce']+'\n\n'
f.write_text(s[:at]+body+s[at:])
