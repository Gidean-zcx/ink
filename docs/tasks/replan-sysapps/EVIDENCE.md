# 复核证据和命令

只进行文档和源码取证检查，没有构建或运行候选应用、主工程/ASan/S31/gcccheck，也没有真机或模拟器GUI实测。源文件功能判断为【实现】，文档检查命令结果为【实测】，交互/排名和墨水屏适配为【估算】。

## 获取方式

- 项目：`git status --short`、`git log -6 --oneline`、`git rev-parse HEAD`，起点与指定基线一致、工作树干净，已有专用分支。
- 外部：`git clone --filter=blob:none --no-checkout --depth 1 <公开仓库URL> work/replan-sysapps/repos/<owner>__<repo>`；`git ls-tree -r --name-only HEAD`取目录，`git show <固定提交>:<文本路径>`只读选定源码。克隆命令带 `GIT_TERMINAL_PROMPT=0`，不读取或复制凭据。没有检出/获取固件镜像blob。
- Brookesia历史对照：在临时克隆内 `git fetch --depth 1 --filter=blob:none origin tag v0.4.2`，读取B03固定提交；没有移动主仓库或任何已有上游副本的HEAD。
- 社区：公开GET官方固件目录，52条中筛选9条；只保留题目相关字段。源码ZIP只尝试HTTP Range读取目录，两次均返回501，没有读取/下载整包。没有以ZIP文件名冒称审过包内应用。
- GitHub连接器用于仓库/相关Issue/PR查找；tags端点被工具400拒绝后，用公开HTTPS GET取得版本元数据。按paouke搜GitHub用户返回422，不能当作作者没有源码的证据；另做公开关键词检索，结论限“未找到可验证归属”。交互决策板工具返回不可访问，没有同步新选择。

## 文档检查

下列只读Python检查验证14节、每节2～3候选/建议/墨水屏四项、源码提交/路径/行号和SHA-256、本地Markdown链接、写入范围及文件大小。它依赖本次临时克隆；不注册ctest、不要求CI有历史或网络，不引入产品测试框架。

临时源码不存在时，可按SOURCES每条仓库URL新建同样的部分克隆，再 `git fetch --depth 1 --filter=blob:none origin <固定提交>`；无需检出或运行任何东西。源码目录只作复核，不提交。

从仓库根目录执行：

```bash
python3 - <<'PY'
import hashlib,json,pathlib,re,subprocess
ROOT=pathlib.Path.cwd();TASK=ROOT/'docs/tasks/replan-sysapps';BASE='d09b79195e2dc57908e33219216048e529e082c6'
def git(*a):return subprocess.check_output(['git',*a],text=True).strip()
manifest=json.loads((TASK/'sources.json').read_text())
ids={s['id'].lower() for s in manifest['sources']};assert len(ids)==len(manifest['sources'])
seen=set();ranges=0
for source in manifest['sources']:
 assert re.fullmatch('[0-9a-f]{40}',source['commit'])
 clone=ROOT/'work/replan-sysapps/repos'/source['repository'].replace('/','__')
 for item in source['files']:
  key=(source['repository'],source['commit'],item['path']);ranges+=1
  data=subprocess.check_output(['git','-C',str(clone),'show',source['commit']+':'+item['path']])
  assert hashlib.sha256(data).hexdigest()==item['sha256'],key
  assert len(data.splitlines())==item['line_count'],key
  assert 1<=item['start']<=item['end']<=item['line_count'],key
  seen.add(key)
apps=(TASK/'APPS.md').read_text();parts=re.split(r'^## (A\d\d) ',apps,flags=re.M);counts={}
for i in range(1,len(parts),2):
 ident,body=parts[i],parts[i+1]
 rows=re.findall(r'^\| [123] \|.+$',body,flags=re.M);counts[ident]=len(rows)
 assert 2<=len(rows)<=3,(ident,len(rows))
 assert '建议：**' in body,ident
 for row in rows:
  assert all(k in row for k in ['动：','色：','触：','刷：']),(ident,row)
assert len(counts)==14 and sum(counts.values())==38
links=0
for p in TASK.glob('*.md'):
 for label,url in re.findall(r'\[([^\]]*)\]\(([^)]+)\)',p.read_text()):
  if url.startswith(('http://','https://')):continue
  path,sep,frag=url.partition('#'); target=(p.parent/path).resolve()
  assert target.is_file(),(p.name,url)
  if frag.startswith('L'):
   m=re.fullmatch(r'L(\d+)(?:-L(\d+))?',frag);assert m,(p.name,url)
   lo=int(m[1]);hi=int(m[2] or m[1]);assert 1<=lo<=hi<=len(target.read_bytes().splitlines()),(p.name,url)
  elif frag:
   text=target.read_text();heads=[h.lower().strip() for h in re.findall(r'^##?\s+(.+)$',text,flags=re.M)]
   assert frag.lower() in heads,(p.name,url)
  links+=1
files=[p for p in TASK.iterdir() if p.is_file()]
assert all(p.stat().st_size<5_000_000 for p in files)
changed=git('diff','--name-only',BASE).splitlines()
assert changed and all(p.startswith('docs/tasks/replan-sysapps/') for p in changed),changed
for p in files:
 if p.name.endswith('.json'):json.loads(p.read_text())
print('DOC_CHECK_OK')
print('applications=14 candidate_positions=38 candidates_each='+','.join(str(v) for v in counts.values()))
print(f'pinned_entries={len(ids)} repositories={len({s["repository"] for s in manifest["sources"]})} unique_source_files={len(seen)} source_ranges={ranges}')
print(f'local_links={links} task_files={len(files)} scope_only_task_docs=true product_lines=0 test_lines=0')
PY
```

实际输出在下方收尾记录中。

## Git范围和隐私

收尾按总规则对指定基线至HEAD的差异执行空白检查、范围统计和原隐私表达式扫描；结果在下方记录。隐私表达式不原样放进提交，以免其自身路径文字成为匹配。

## 口袋源码包元数据

只读固定克隆的 `git ls-tree -r -l HEAD firmware-pocket`：

| 源码包 | Git blob | 字节数 | 本次展开 |
| --- | --- | --- | --- |
| firmware-pocket-1.0.7-s3.20-source.zip | `cae78c4863dc554f7b5faa1ff260a0ee3e94053e` | 1566126 | 无；Range请求501 |
| firmware-pocket-plus-1.0.7-plus.16-source.zip | `8dd5841d4c39aa1a6e4844274c3eb0d947970039` | 2018663 | 无；Range请求501 |

文件名/大小证明交付包存在，不证明包内包含某个小应用。

## 收尾记录

【实测】文档/源码检查退出0，输出：

```text
DOC_CHECK_OK
applications=14 candidate_positions=38 candidates_each=3,3,3,2,2,2,3,3,3,3,3,3,2,3
pinned_entries=41 repositories=40 unique_source_files=106 source_ranges=133
local_links=134 task_files=8 scope_only_task_docs=true product_lines=0 test_lines=0
```

【实测】`git diff --cached --check`退出0。一次文档检查发现home.c引用终点越界，已改为实际28行；一次空白检查发现SOURCES末尾多空行，已修正并复核。没有改任何产品测试来通过检查。

最终提交后再核对基线至HEAD范围与隐私，结果另记；外部HTTP链接没有逐个发HEAD请求，检查的是固定Git对象中的文件/行号和本地链接，不虚报全部网页可访问。

构建/测试原话：**未运行主工程构建；未运行全量ctest；未运行ASan；未编译S31；未运行gcccheck。** 本任务为纯调研，任务书豁免构建/全量测试；没有产品修复和反向对照适用项。
