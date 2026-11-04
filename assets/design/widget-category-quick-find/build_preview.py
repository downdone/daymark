from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageCms
import html,io
import cairosvg
ROOT=Path(__file__).resolve().parent;W,H=1820,1000
im=Image.new('RGB',(W,H),'#F4F5F8');d=ImageDraw.Draw(im);svg=[]
INK='#1C1C1E';SUB='#626570';BLUE='#0063CF';SEL='#E9F1FE';BORDER='#DFE1E7'
def text(x,y,s,n=20,c=INK,bold=False):
 f=ImageFont.truetype('C:/Windows/Fonts/'+('msyhbd.ttc' if bold else 'msyh.ttc'),n);d.text((x,y),s,font=f,fill=c);svg.append(f'<text x="{x}" y="{y+n*.88}" font-family="Microsoft YaHei,PingFang SC,sans-serif" font-size="{n}" fill="{c}" font-weight="{600 if bold else 400}">{html.escape(s)}</text>')
def box(x,y,w,h,c='#FFFFFF',r=12,stroke=None):
 d.rounded_rectangle((x,y,x+w,y+h),r,fill=c,outline=stroke,width=2 if stroke else 1);svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{c}"'+(f' stroke="{stroke}" stroke-width="2"' if stroke else '')+'/>')
def line(x,y,x2,y2,c=BORDER,w=1):
 d.line((x,y,x2,y2),fill=c,width=w);svg.append(f'<path d="M{x} {y}L{x2} {y2}" fill="none" stroke="{c}" stroke-width="{w}"/>')
def circle(x,y,r,c='#FFFFFF',stroke=SUB):
 d.ellipse((x-r,y-r,x+r,y+r),fill=c,outline=stroke,width=2);svg.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{c}" stroke="{stroke}" stroke-width="2"/>')
def check(x,y,on=True):
 box(x,y,24,24,BLUE if on else '#FFFFFF',5,BLUE if on else BORDER)
 if on:line(x+5,y+12,x+10,y+17,'#FFFFFF',2);line(x+10,y+17,x+19,y+7,'#FFFFFF',2)
def pinicon(x,y):
 path='<path d="M8.5 3.5h7l-1 6 3.5 4h-12l3.5-4z M12 13.5v7" fill="none" stroke="'+BLUE+'" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/>'
 data='<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">'+path+'</svg>'
 icon=Image.open(io.BytesIO(cairosvg.svg2png(bytestring=data.encode()))).convert('RGBA');im.paste(icon,(x,y),icon);svg.append(f'<g transform="translate({x} {y})">'+path+'</g>')
text(44,30,'DAYMARK / 小窗分类快查',30,bold=True);text(44,80,'设计评审稿 · 范围规则已确认，功能尚未实现',18,SUB)
text(44,140,'01  常用分类，一点切换',23,bold=True)
box(44,198,512,640,stroke=BORDER);box(45,199,86,638,'#F3F4F7',12)
for i,(s,count) in enumerate([('今天','2'),('运营','6'),('产品','3'),('个人','1')]):
 y=226+i*86
 if i==1:box(53,y-4,70,74,SEL,8);line(54,y+5,54,y+53,BLUE,3)
 text(65,y+6,s,20,BLUE if i==1 else INK,i==1);text(80,y+35,count,16,SUB)
text(65,602,'分类',18,SUB);text(77,636,'…',28,SUB)
text(154,222,'运营',28,bold=True);text(221,231,'6',18,SUB)
box(433,215,40,40,SEL,10);pinicon(441,223);text(490,219,'…',28,SUB)
text(154,268,'全部未完成 · 含子分类',17,SUB)
for i,(title,meta) in enumerate([('整理活动方案','今天 · 活动策划'),('确认项目排期','11月6日 · 日常运营'),('准备例会材料','未安排 · 日常运营')]):
 y=332+i*105;circle(167,y+18,12);text(194,y,title,22);text(194,y+37,meta,16,SUB);line(194,y+88,530,y+88)
box(542,330,4,190,'#C6C8CE',2)
line(148,758,538,758);text(159,782,'＋ 添加到「运营」',21)
text(48,868,'正文宽度保持；快捷栏随停靠方向放在内侧。',17,SUB)
text(612,140,'02  全部分类，就近选择',23,bold=True)
box(612,198,546,640,stroke=BORDER);text(638,223,'选择分类',26,bold=True)
box(638,277,493,46,'#F3F4F7',9);text(654,287,'搜索分类名称或路径',18,SUB)
text(640,343,'分类',16,SUB);text(1020,343,'任务',16,SUB);text(1082,343,'常用',16,SUB)
rows=[('运营',6,True,0,True),('活动策划',3,False,1,False),('日常运营',3,False,1,False),('产品',3,True,0,False),('个人',1,True,0,False)]
for i,(s,count,pin,depth,active) in enumerate(rows):
 y=382+i*71
 if active:box(628,y-2,514,58,SEL,8)
 text(643+depth*24,y+10,s,21,BLUE if active else INK,active);text(1032,y+12,str(count),18,SUB);check(1091,y+10,pin)
text(638,779,'点名称切换；“常用”只管理快捷入口。',17,SUB)
text(616,868,'一级包含二级任务；二级仅看自己。',17,SUB)
text(1212,140,'03  分类范围，在设置统一选择',23,bold=True)
box(1212,198,564,640,stroke=BORDER);text(1240,224,'桌面组件 / 分类检索',25,bold=True)
text(1240,290,'分类任务范围',21,bold=True)
box(1238,334,509,76,SEL,10);circle(1264,371,10,BLUE,BLUE);circle(1264,371,4,'#FFFFFF','#FFFFFF');text(1290,349,'全部未完成',21,bold=True);text(1290,378,'默认，含未安排和将来任务',16,SUB)
circle(1264,451,10);text(1290,433,'仅今天',21);text(1290,464,'与主窗口的“今天”一致',16,SUB)
line(1240,515,1747,515);text(1240,544,'显示分类快捷栏',21);box(1684,543,59,32,BLUE,16);circle(1726,559,12,'#FFFFFF','#FFFFFF')
text(1240,603,'常用分类与顺序',21,bold=True)
for i,s in enumerate(['运营','产品','个人']):
 y=650+i*44;text(1244,y,s,19);text(1713,y,'≡',21,SUB)
text(1240,794,'“今天”入口始终看今天，不受此设置影响。',16,SUB)
text(44,945,'统一上下文：小窗 / Mini 共用当前分类；悬浮球保留轻量入口。新建、排序与计数都跟随当前范围。',20,SUB)
(root:=ROOT/'preview.png').parent.mkdir(exist_ok=True,parents=True);im.save(root,icc_profile=ImageCms.ImageCmsProfile(ImageCms.createProfile('sRGB')).tobytes(),optimize=True)
(ROOT/'preview.svg').write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}"><title>Daymark widget category quick find — proposal, not implemented</title><rect width="{W}" height="{H}" fill="#F4F5F8"/>'+''.join(svg)+'</svg>\n',encoding='utf-8',newline='\n')
