from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urlparse, unquote
import json
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1] / "dist"
origin = "https://floatlet.vercel.app"
private = {"/account/", "/admin/", "/404.html", "/privacy/", "/terms/"}

class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.h1 = 0
        self.meta = {}
        self.links = []
        self.canonical = ""
        self.ids = set()
        self.title = ""
        self.in_title = False
        self.schema = []
        self.in_schema = False
        self.buffer = ""
    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "h1": self.h1 += 1
        if "id" in a: self.ids.add(a["id"])
        if tag == "meta": self.meta[a.get("name", a.get("property", ""))] = a.get("content", "")
        if tag == "link" and a.get("rel") == "canonical": self.canonical = a["href"]
        if tag == "a" and "href" in a: self.links.append(a["href"])
        if tag == "title": self.in_title = True
        if tag == "script" and a.get("type") == "application/ld+json": self.in_schema = True; self.buffer = ""
    def handle_data(self, data):
        if self.in_title: self.title += data
        if self.in_schema: self.buffer += data
    def handle_endtag(self, tag):
        if tag == "title": self.in_title = False
        if tag == "script" and self.in_schema: self.schema.append(json.loads(self.buffer)); self.in_schema = False

pages, errors, indexable = {}, [], set()
for file in root.rglob("*.html"):
    if file.name.startswith("google") and file.read_text(encoding="utf-8").startswith("google-site-verification:"):
        continue
    p = Page()
    p.feed(file.read_text(encoding="utf-8"))
    pages[file.resolve()] = p
    route = "/" + file.relative_to(root).as_posix().removesuffix("index.html")
    if p.h1 != 1: errors.append(f"{route}: expected one h1")
    if not p.title: errors.append(f"{route}: missing title")
    if route in private:
        if "noindex" not in p.meta.get("robots", ""): errors.append(f"{route}: must remain noindex")
        if route in {"/account/", "/admin/"}: continue
    else:
        indexable.add(origin + route)
        if "noindex" in p.meta.get("robots", ""): errors.append(f"{route}: unexpectedly noindex")
    for name in ["description", "robots", "og:title", "og:description", "og:image", "twitter:card"]:
        if not p.meta.get(name): errors.append(f"{route}: missing {name}")
    if p.canonical != origin + ("/404/" if route == "/404.html" else route): errors.append(f"{route}: incorrect canonical")
    if not p.schema: errors.append(f"{route}: missing structured data")
    if route not in private and route != "/" and not any(item.get("@type") == "BreadcrumbList" for schema in p.schema for item in schema.get("@graph", [])):
        errors.append(f"{route}: missing breadcrumb schema")
    if not (root / urlparse(p.meta.get("og:image", "")).path.lstrip("/")).is_file():
        errors.append(f"{route}: missing social image")
for file, p in pages.items():
    for link in p.links:
        u = urlparse(link)
        if u.scheme or u.netloc: continue
        target = (root / u.path.lstrip("/") if u.path.startswith("/") else file.parent / u.path) if u.path else file
        if target.is_dir(): target = target / "index.html"
        target = target.resolve()
        if not target.exists(): errors.append(f"{file.name}: broken link {link}")
        elif u.fragment and target in pages and unquote(u.fragment) not in pages[target].ids:
            errors.append(f"{file.name}: missing anchor {link}")
sitemap = ET.fromstring((root / "sitemap.xml").read_text(encoding="utf-8"))
listed = {item.text for item in sitemap.findall(".//{http://www.sitemaps.org/schemas/sitemap/0.9}loc")}
if listed != indexable: errors.append(f"Sitemap mismatch: {listed ^ indexable}")
if len({p.title for p in pages.values()}) != len(pages): errors.append("Duplicate page titles")
if errors: raise SystemExit("\n".join(errors))
print(f"PASS: {len(pages)} pages, {len(indexable)} indexable URLs, metadata, canonical URLs, structured data, noindex rules and internal links.")