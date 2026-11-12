from pathlib import Path
import json, shlex, subprocess, shutil, re
root=Path(__file__).resolve().parents[2];build=root/'build-s31';area=build/'ime-baseline';area.mkdir(parents=True,exist_ok=True)
db=json.loads((build/'compile_commands.json').read_text());units=['system/services/text/pinyin.c','system/services/text/text_service.c'];objects=[]
for unit in units:
 source=root/unit;entry=next(e for e in db if Path(e['file'])==source);args=shlex.split(entry['command']);new=area/source.name;new.write_bytes(subprocess.check_output(['git','show','36473cc4933e6fea2f2a78a4fbec16186d3be3f4:'+unit]));obj=area/(source.name+'.obj');objects.append(str(obj));args[args.index('-o')+1]=str(obj);args[args.index(str(source))]=str(new)
 if '-MF' in args:args[args.index('-MF')+1]=str(obj)+'.d'
 args+=['-I'+str(source.parent)];subprocess.run(args,cwd=entry['directory'],check=True)
compiler=Path(shlex.split(next(e for e in db if e['file'].endswith('text_service.c'))['command'])[0]);ar=compiler.with_name(compiler.name.replace('gcc','ar'))
archive=area/'libink_system.a';shutil.copyfile(build/'esp-idf/ink_system/libink_system.a',archive)
subprocess.run([str(ar),'r',str(archive),*objects],check=True)
link=shlex.split((build/'CMakeFiles/ink-s31.elf.dir/link.txt').read_text());link[link.index('-o')+1]=str(area/'baseline.elf');link=[str(archive) if x.endswith('/ink_system/libink_system.a') else x.replace('-Wl,-Map=ink-s31.map','-Wl,-Map='+str(area/'baseline.map')) for x in link]
subprocess.run(link,cwd=build,check=True,capture_output=True,text=True)
size=compiler.with_name(compiler.name.replace('gcc','size'));rows={}
for name,elf in [('baseline',area/'baseline.elf'),('new',build/'ink-s31.elf')]:
 out=subprocess.check_output([str(size),str(elf)],text=True);print(name,out);parts=out.splitlines()[-1].split();rows[name]={'text':int(parts[0]),'data':int(parts[1]),'bss':int(parts[2])}
rows['delta']={k:rows['new'][k]-rows['baseline'][k] for k in rows['new']};(area/'delta.json').write_text(json.dumps(rows,indent=2)+'\n');print(rows)
