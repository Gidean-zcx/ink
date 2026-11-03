#!/usr/bin/env python3
"""Compare emitted samples, not just elapsed duration; outputs stay under the audit directory."""
import hashlib,json,struct,wave
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
def samples(name):
    p=ROOT/'build-audit/r8-probes'/name/'speaker.wav'
    with wave.open(str(p),'rb') as w:
        assert (w.getnchannels(),w.getframerate(),w.getsampwidth())==(1,16000,2)
        b=w.readframes(w.getnframes())
    return list(struct.unpack('<'+'h'*(len(b)//2),b)),hashlib.sha256(b).hexdigest()
a,ha=samples('r8_audio_control');b,hb=samples('r8_audio_tones')
i=j=0;removed=[]
while i<len(a):
    if a[i]==b[j]:i+=1;j+=1;continue
    assert b[j+960:j+1088]==a[i:i+128],(i,j)
    removed.append(dict(control_sample=i,tone_sample=j,samples=960))
    j+=960
assert i==48000 and j==52800 and len(removed)==5
tone=b[removed[0]['tone_sample']:removed[0]['tone_sample']+960]
c,hc=samples('r8_audio_fault_cancel');d,hd=samples('r8_audio_fault_replace')
result=dict(control_samples=len(a),with_tones_samples=len(b),insertions=removed,all_48000_voice_samples_identical=True,
            fault_cancel_tail_is_tone=c[-960:]==tone,
            fault_replace_tail_is_new_stream=d[-4800:]==a[:4800],
            hashes=dict(control=ha,tones=hb,cancel=hc,replace=hd),
            note='WAV from simulator codec; not acoustic quality or real Opus validation. Fault WAV includes clock-loss silence; played_ms is separate.')
(HERE/'audio-integrity.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
