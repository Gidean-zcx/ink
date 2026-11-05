"""Generate independent audit-only copies; never alter product or test sources."""
import hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[4]
out=root/'build-audit/audit-reverse'
out.mkdir(parents=True,exist_ok=True)
src=(root/'system/services/audio/media_service.c').read_text()
src=src.replace('#include "ogg.h"', '#include "'+str(root/'system/services/audio/ogg.h')+'"')
old='''    if (!M.stream) { /* paused: where the pause was */
        ink_svc_lock();
        uint32_t at = M.st.position_ms;
        ink_svc_unlock();
        return at;
    }
'''
assert src.count(old)==1
copies={'media_pause.c':src.replace(old,''),
        'media_member.c':src.replace('    if (strcmp(ink_member_current()->id, REC.member)) return; /* someone else has it now, the end is on its way */','    /* audit reverse: accept frames after a member switch */')}
assert copies['media_member.c']!=src
for name,text in copies.items():
    path=out/name
    path.write_text(text)
    print(name,hashlib.sha256(path.read_bytes()).hexdigest())
interrupt=(root/'system/services/notify/interrupt.c').read_text()
assert interrupt.count('if (IT.applying) {')==1
interrupt=interrupt.replace('if (IT.applying) {','if (false) { /* audit reverse: no single-applier guard */')
interrupt=interrupt.replace('#include "notify_internal.h"','#include "'+str(root/'system/services/notify/notify_internal.h')+'"')
(out/'interrupt.c').write_text(interrupt)
wrapper=(root/'docs/audits/2026-11-04-codex-services/experiments/interrupt_boundary.c').read_text()
wrapper=wrapper.replace('../../../../system/services/notify/interrupt.c',str(out/'interrupt.c'))
(out/'interrupt_boundary.c').write_text(wrapper)
print('interrupt.c',hashlib.sha256((out/'interrupt.c').read_bytes()).hexdigest())
