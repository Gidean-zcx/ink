from pathlib import Path
import sys
b=Path(__file__).parent/'src'
for arg in sys.argv[1:]:
 name,ranges=arg.split(':',1);p=next(b.rglob(name));s=p.read_text().splitlines();print('\nFILE',p)
 for r in ranges.split(','):
  lo,hi=map(int,r.split('-'))
  for i in range(lo-1,min(hi,len(s))):print(f'{i+1}: {s[i]}')
