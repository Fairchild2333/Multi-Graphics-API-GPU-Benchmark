import json,subprocess,xml.etree.ElementTree as ET,signal
from pathlib import Path
root=Path(__file__).resolve().parent
cmd=['xcrun','xctrace','export','--input',str(root/'a15-limiters-final.trace'),'--xpath','/trace-toc/run[@number="1"]/data/table[@schema="metal-gpu-counter-intervals"]']
with (root/'counter-sample-export.log').open('w') as err:
 p=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=err,text=True)
 lines=[];count=0
 try:
  for line in p.stdout:
   lines.append(line);count+=line.count('</row>')
   if count>=1000:break
 finally:
  if p.poll() is None:p.send_signal(signal.SIGINT)
  p.stdout.close()
  try:p.wait(timeout=20)
  except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
s=''.join(lines)
if count<1000:raise SystemExit(f'Expected1000 sample rows, got{count}')
s=s[:s.rfind('</row>')+len('</row>')]+'</node></trace-query-result>'
tree=ET.fromstring(s);refs={};samples=[]
for row in tree.findall('.//row'):
 for x in row.iter():
  if 'id' in x.attrib:refs[x.attrib['id']]=(x.text,x.attrib.get('fmt'))
 def get(x):return refs[x.attrib['ref']] if 'ref' in x.attrib else (x.text,x.attrib.get('fmt'))
 xs=list(row);samples.append({'start_ns':int(get(xs[0])[0]),'duration_ns':int(get(xs[1])[0]),'counter_id':int(get(xs[2])[0]),'name':get(xs[3])[1],'value':float(get(xs[5])[0])})
(root/'counter-sample.json').write_text(json.dumps({'scope':'First1000 intervals; collection proof only, includes background GPU activity before benchmark and is not a bandwidth result','rows':samples},indent=2))
print('Hardware sample',len(samples),'rows,',len({x['name'] for x in samples}),'counter names')
