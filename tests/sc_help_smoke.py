"""Parse shipped help and render its actual example synth bodies through scsynth."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import numpy as np
from sc_identity_nrt import bundle,message,samples
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--plugins',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
p.add_argument('--engines',help='Optional comma-separated helpfile class names')
p.add_argument('--extensions',type=Path,help='Use staged classes/help instead of repository sources')
p.add_argument('--resources',type=Path,default=Path('/Applications/SuperCollider.app/Contents/Resources'))
a=p.parse_args();dest=a.out.resolve();dest.mkdir(parents=True,exist_ok=False);defs=dest/'defs';defs.mkdir()
r=a.resources.resolve();sclang=r.parent/'MacOS/sclang'
if not sclang.exists():sclang=r/'sclang'
language_paths=(r/'SCClassLibrary',a.extensions.resolve()) if a.extensions else (r/'SCClassLibrary',ROOT/'sc/Classes',ROOT/'modules/rubberband')
config=dest/'sclang.yaml';config.write_text('includePaths:\n'+''.join('  - '+json.dumps(str(x))+'\n' for x in language_paths)+'excludePaths: []\npostInlineWarnings: false\n')
helpfiles=sorted(h for h in a.extensions.resolve().rglob('*.schelp') if h.parent.name == 'Classes') if a.extensions else sorted((ROOT/'sc/HelpSource/Classes').glob('*.schelp'))
if a.engines:
    selected=a.engines.split(',');helpfiles=[h for h in helpfiles if h.stem in selected]
    assert len(helpfiles)==len(set(selected))
else:assert len(helpfiles)==15
commands=[];cases=[]
for path in helpfiles:
    text=path.read_text();bodies=re.findall(r'x = \{(.*?)\n    \}\.play;',text,re.S);assert len(bodies)==3,path
    commands.append('if(SCDoc.parseFileFull('+json.dumps(str(path))+').isNil) { Error("Help parse failed").throw };')
    for i,body in enumerate(bodies):
        name=path.stem+str(i);lines=body.strip().splitlines();lines[-1]='Out.ar(0, '+lines[-1].strip()+');'
        commands.append('SynthDef(\\'+name+', {\n'+'\n'.join(lines)+'\n}).writeDefFile('+json.dumps(str(defs))+');')
        cases.append((name,path))
script=dest/'compile.scd';script.write_text('(\n'+ '\n'.join(commands)+'\n"HELP_COMPILE_OK".postln;\n0.exit;\n)\n')
run=subprocess.run([str(sclang),'-a','-l',str(config),str(script)],capture_output=True,text=True,timeout=90)
log=run.stdout+run.stderr;(dest/'compile.log').write_text(log)
assert run.returncode==0 and 'HELP_COMPILE_OK' in log and 'ERROR' not in log,log
assert len(list(defs.glob('*.scsyndef')))==3*len(helpfiles)
results=[]
for name,path in cases:
    score=dest/(name+'.osc');score.write_bytes(bundle(0,message('/d_recv',(defs/(name+'.scsyndef')).read_bytes(),message('/s_new',name,1000,0,0)))+bundle(4,message('/c_set',0,0)))
    output=dest/(name+'.wav')
    run=subprocess.run([str(r/'scsynth'),'-D','0','-i','0','-o','2','-z','64','-m','65536','-U',str(r/'plugins')+':'+str(a.plugins.resolve()),'-N',str(score),'_',str(output),'48000','WAV','float'],capture_output=True,text=True,timeout=45)
    log=run.stdout+run.stderr;(dest/(name+'.log')).write_text(log)
    assert run.returncode==0 and not any(s in log for s in ('FAILURE','ERROR','exception','not installed')),log
    audio=np.asarray(samples(output),dtype=np.float32);assert np.isfinite(audio).all()
    peak=float(np.max(abs(audio)));rms=float(np.sqrt(np.mean(audio[24000:144000].astype(float)**2)))
    assert peak<1 and rms>1e-5,(name,peak,rms)
    assert np.max(abs(audio[170000:]))==0,name # Env doneAction frees each node.
    results.append(dict(example=name,peak=peak,rms=rms,help_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),wav_sha256=hashlib.sha256(output.read_bytes()).hexdigest()))
(dest/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print(f'{len(helpfiles)} SCDoc documents parsed; {len(cases)} example SynthDefs compiled/rendered with finite audible output and automatic node cleanup')
