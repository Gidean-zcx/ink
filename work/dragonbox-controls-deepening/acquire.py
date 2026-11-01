import importlib.util,json
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
s=importlib.util.spec_from_file_location('base_acquire','work/ground-controls-deepening/acquire.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.W=Path('work/dragonbox-controls-deepening')
jobs=[('learning-index','https://support.kahoot.com/hc/en-us/sections/4468203653523-Learning-apps'),('profiles','https://support.kahoot.com/hc/en-us/articles/4815253732371-How-to-manage-profiles-in-learning-apps'),('dashboard','https://support.kahoot.com/hc/en-us/articles/39935017652125-Child-Activity-Dashboard-your-parent-dashboard-for-Kahoot-Kids'),('subscriptions','https://support.kahoot.com/hc/en-us/articles/4815480526227-How-to-manage-DragonBox-and-Poio-subscriptions'),('googleplay','https://play.google.com/store/apps/details?hl=en_US&id=com.kahoot.numbers'),('privacy','https://dragonbox.com/privacy-policy')]
with ThreadPoolExecutor(max_workers=3) as pool:
 results=list(pool.map(lambda j:m.get(*j),jobs))
(m.W/'sources.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
for r in results:print({k:r[k] for k in ['name','status','bytes','error'] if k in r})
