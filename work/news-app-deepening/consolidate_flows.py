import csv
import json
from pathlib import Path

root = Path('/home/workspace/reader')
batches = root / 'docs/research/interaction-library/batches'

def read(name):
    with (batches / name).open(newline='') as fp:
        return list(csv.DictReader(fp))

def write(name, rows):
    with (batches / name).open('w', newline='') as fp:
        writer = csv.DictWriter(fp, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

old = read('news-coverage.csv')
for row in old:
    if row['flow_id'] == 'F-M045-02':
        row.update(depth='D2', evidence_ids='E-ESPR-01;E-ESPR-02;E-ESD-02',
                   known_branches='机构指南 Settings 到 For you；不证明改每日短报',
                   gaps='选项、确认、生效与当前平台构建')
    elif row['flow_id'] == 'F-M045-03':
        row.update(evidence_ids='E-ESPR-02;E-ESD-01',
                   known_branches='静态图可见阅读时间与音频入口、时长；翻译为能力声明',
                   gaps='播放结果、暂停结束、精确返回恢复与语言入口',
                   visual_status='static_official_images_only')
    elif row['flow_id'] == 'F-M046-02':
        row.update(evidence_ids='E-GRND-02;E-GND-14',
                   known_branches='Saved Stories 回访；Web/App 均不支持离线浏览',
                   gaps='取消收藏、同步冲突、断网恢复、原文删除与滚动位置')
write('news-coverage.csv', old)

new = read('news-app-deepening-coverage.csv')
redundant = {'F-M045-05', 'F-M045-06', 'F-M046-14'}
new = [row for row in new if row['flow_id'] not in redundant]
assert len(new) == 12
write('news-app-deepening-coverage.csv', new)
print(json.dumps({'new_tasks': len(new), 'existing_tasks_refined': 3,
                  'd1_to_d2': ['F-M045-02'], 'hands_on': False}))
