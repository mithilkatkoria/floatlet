from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urlparse,unquote
import json
root=Path(__file__).resolve().parents[1]/'dist'
class Page(HTMLParser):
 def __init__(self):
  super().__init__();self.h1=0;self.meta={};self.links=[];self.canonical='';self.ids=set();self.title='';self.in_title=False;self.schema=[];self.in_schema=False;self.buffer=''
 def handle_starttag(self,tag,attrs):
  a=dict(attrs)
  if tag=='h1':self.h1+=1
  if 'id' in a:self.ids.add(a['id'])
  if tag=='meta':self.meta[a.get('name',a.get('property',''))]=a.get('content','')
  if tag=='link' and a.get('rel')=='canonical':self.canonical=a['href']
  if tag=='a' and 'href' in a:self.links.append(a['href'])
  if tag=='title':self.in_title=True
  if tag=='script' and a.get('type')=='application/ld+json':self.in_schema=True;self.buffer=''
 def handle_data(self,data):
  if self.in_title:self.title+=data
  if self.in_schema:self.buffer+=data
 def handle_endtag(self,tag):
  if tag=='title':self.in_title=False
  if tag=='script' and self.in_schema:self.schema.append(json.loads(self.buffer));self.in_schema=False
pages={};errors=[]
for file in root.rglob('*.html'):
 p=Page();p.feed(file.read_text(encoding='utf-8'));pages[file.resolve()]=p
 if p.h1!=1:errors.append(f'{file}: expected one h1')
 for name in ['description','robots','og:title','og:description','og:image','twitter:card']:
  if not p.meta.get(name):errors.append(f'{file}: missing {name}')
 if not p.canonical.startswith('https://floatlet.vercel.app/'):errors.append(f'{file}: wrong canonical')
 if not p.schema:errors.append(f'{file}: missing schema')
 if not (root/urlparse(p.meta.get('og:image','')).path.lstrip('/')).is_file():errors.append(f'{file}: missing social image')
for file,p in pages.items():
 for link in p.links:
  u=urlparse(link)
  if u.scheme or u.netloc:continue
  target=(root/u.path.lstrip('/') if u.path.startswith('/') else file.parent/u.path) if u.path else file
  if target.is_dir():target=target/'index.html'
  target=target.resolve()
  if not target.exists():errors.append(f'{file.name}: broken link {link}')
  elif u.fragment and target in pages and unquote(u.fragment) not in pages[target].ids:errors.append(f'{file.name}: missing anchor {link}')
assert len({p.title for p in pages.values()})==len(pages),'Duplicate page titles'
assert not errors,'\n'.join(errors)
print(f'PASS: {len(pages)} pages, unique titles, metadata, canonical URLs, JSON-LD, social images and all internal links/anchors.')
