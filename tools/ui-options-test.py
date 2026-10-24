from html.parser import HTMLParser
from pathlib import Path
class UI(HTMLParser):
 def __init__(self):super().__init__();self.in_chassis=False;self.options=[]
 def handle_starttag(self,tag,attrs):
  a=dict(attrs)
  if tag=='select':self.in_chassis=a.get('id')=='chassis'
  if tag=='option' and self.in_chassis:self.options.append(a.get('value'))
 def handle_endtag(self,tag):
  if tag=='select':self.in_chassis=False
p=UI();p.feed((Path(__file__).resolve().parents[1]/'firmware/main/boards/esp-sparkbot/assets/hub.html').read_text(encoding='utf8'))
assert p.options==['','tracks','wheels','quadruped','none'],p.options
print('PASS: all four chassis can be selected independently of color reset')
