#!/usr/bin/env python3
"""Bundle critical resources so index.html works from file:// and on GitHub Pages.
Run after editing templates/*.html, style.css, app.js or source files.
Video and report downloads stay in their relative companion folders.
"""
from pathlib import Path
import base64, json, mimetypes, re
ROOT=Path(__file__).resolve().parent
css=(ROOT/'style.css').read_text()
js=(ROOT/'app.js').read_text()
files=json.loads((ROOT/'source-index.json').read_text())
for entry in files: entry['content']=(ROOT/entry['path']).read_text(errors='replace')
source_json=json.dumps(files,ensure_ascii=True).replace('<','\\u003c')
def inline_image(match):
 key,path=match.groups()
 p=ROOT/path
 if not p.is_file(): raise FileNotFoundError(path)
 mime=mimetypes.guess_type(path)[0] or 'application/octet-stream'
 return f'{key}="data:{mime};base64,{base64.b64encode(p.read_bytes()).decode()}"'
for name in ['index.html','function.html','wisdom.html']:
 html=(ROOT/'templates'/name).read_text()
 html=re.sub(r'<link rel="stylesheet" href="style.css">','<style>'+css+'</style>',html)
 html=html.replace('<script src="app.js" defer></script>','')
 html=re.sub(r'(src|poster|data-lightbox)="(assets/[^\"]+\.(?:jpg|jpeg|png|webp|svg))"',inline_image,html)
 html=html.replace('href="assets/favicon.svg"','href="data:image/svg+xml;base64,'+base64.b64encode((ROOT/'assets/favicon.svg').read_bytes()).decode()+'"')
 if name=='index.html':
  html=html.replace('</body>', '<script type="application/json" id="bundled-sources">'+source_json+'</script><script>'+js+'</script></body>')
 (ROOT/name).write_text(html)
 print(name,round((ROOT/name).stat().st_size/1024),'KiB')
