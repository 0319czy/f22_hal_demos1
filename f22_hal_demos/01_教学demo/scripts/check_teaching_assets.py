#!/usr/bin/env python3
"""Check lesson links, zero CSV schemas, C telemetry counts and Intel HEX integrity."""
from pathlib import Path
import csv, json, re
root=Path(__file__).resolve().parents[1]
for p in root.rglob('*.md'):
    if '.pio' in p.parts or 'build' in p.parts: continue
    for target in re.findall(r'\]\(([^)]+)\)',p.read_text()):
        target=target.strip('<>')
        if '://' in target or target.startswith('#'):continue
        target=target.split('#')[0]
        assert (p.parent/target).exists(),f'broken link: {p}: {target}'
expected={'demo_vofa':20,'demo03_motor':5,'demo04_hover_logic':20,'demo05_tof':12,'demo06_flow':16,'demo07_integration':28}
for name,count in expected.items():
    p=root/'src/demo'/name/'vofa.csv'
    rows=list(csv.reader(p.open()))
    assert len(rows)==2 and len(rows[0])==len(rows[1])==count,p
    assert len(set(rows[0]))==count and all(float(x)==0 for x in rows[1]),p
    if name!='demo_vofa':
        source=(p.parent/'main.c').read_text()
        initializer=re.search(r'float ch\[\]\s*=\s*\{(.*?)\};',source,re.S).group(1)
        depth=0; columns=1
        for char in initializer:
            if char in '([{':depth+=1
            elif char in ')]}':depth-=1
            elif char==',' and depth==0:columns+=1
        assert columns==count,f'{name} sends {columns}, schema says {count}'
        assert re.search(r'Lesson_SendFloats\(ch,\s*'+str(count)+r'U?\)',source),name
for name,fields in json.loads((root/'docs/telemetry_schema.json').read_text()).items():
    assert next(csv.reader((root/'src/demo'/name/'vofa.csv').open()))==fields,name
assert (root/'docs/vofa.csv').read_bytes()==(root/'src/demo/demo_vofa/vofa.csv').read_bytes()
end=False; total=0
for line in (root/'remote/tle100/tle100.hex').read_text().splitlines():
    assert line.startswith(':')
    record=bytes.fromhex(line[1:])
    assert len(record)==record[0]+5 and sum(record)%256==0, line
    assert not end
    if record[3]==0:
        address=(record[1]<<8)|record[2]
        assert address+record[0]<=8192
        total+=record[0]
    if record[3]==1:end=True
assert end and total>0
print(f'teaching links, 6 zero CSV templates/channel order, and TLE100 Intel HEX passed ({total} program bytes)')
