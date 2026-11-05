import importlib.util
import json
from pathlib import Path
root=Path(__file__).resolve().parents[4]
spec=importlib.util.spec_from_file_location('old',root/'docs/audits/2026-11-04-codex-services-s2/experiments/reverse_controls.py')
m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
cases=[
 ('closed-space', 'sim-listening', 'system/services/ai/ai_service.c', lambda t:m.replace_once(t,'ink_state_watch("member.authed", on_space_changed, NULL, INK_EXEC_UI);','(void)on_space_changed;'), 'listening', {'profile':'metalio-plus-handsfree'}),
 ('resource-write-error', 'sim-update', 'system/services/update/update_service.c', lambda t:m.replace_once(t,'if (o.r != 0) ok = false;','(void)o.r;'), 'update', {}),
 ('resource-hash', 'sim-update', 'system/services/update/update_service.c', lambda t:m.replace_once(t,'&& check_sections(path, p) == PKG_OK) {','&& (true || check_sections(path, p) == PKG_OK)) {'), 'update', {}),
 m.cases[-1],
]
results=[]
for name,target,source,transform,test,kw in cases:
    results.append(m.run_case(name,target,source,transform,test,**kw))
    (root/'build-audit/reverse-controls/results-more.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
