"""Build the complete Daymark VI V2 package and Chinese PDF handbook.

Authoring only; master assets are copied byte-for-byte from the frozen identity.
No client/build configuration, external VI originals or user databases are edited.
"""
from pathlib import Path
import io,json,hashlib,shutil,html,os,re
from PIL import Image,ImageDraw,ImageFont,ImageCms
import cairosvg
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.colors import HexColor
from reportlab.graphics import renderPDF
from reportlab.lib.utils import ImageReader
from svglib.svglib import svg2rlg

ROOT=Path(__file__).resolve().parents[1]
REPO=ROOT.parents[1]
FINAL=REPO/'assets/brand/current'
T=json.loads((ROOT/'source/tokens-source.json').read_text(encoding='utf-8'))
SRGB=ImageCms.ImageCmsProfile(ImageCms.createProfile('sRGB')).tobytes()
INK='#1C1C1E';GREY='#626570';PAPER='#F5F6F8'
W,H=960,600
PAGE_TITLES=['品牌总览','定位与调性','标志系统','比例与保护区','浅深与单色','品牌配色','品牌色与功能色','字体与字标','版式与空间','辅助图形','图标与小尺寸','产品内的品牌','常用模板','正确与错误用法','接入与迁移','文件与验收']


def body(path):
    import xml.etree.ElementTree as ET
    e=ET.parse(path).getroot()
    return ''.join(ET.tostring(x,encoding='unicode') for x in e if x.tag.rsplit('}',1)[-1]!='title')


def svg(content,w,h,title):
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}"><title>{html.escape(title)}</title>{content}</svg>\n'


def raster(s,w,h):
    return Image.open(io.BytesIO(cairosvg.svg2png(bytestring=s.encode(),output_width=w,output_height=h))).convert('RGBA')


def font(n,bold=False):
    return ImageFont.truetype(str(Path(os.environ.get('WINDIR','C:/Windows'))/'Fonts'/('msyhbd.ttc' if bold else 'msyh.ttc')),n)


def assets():
    for folder in ['assets/brand','assets/micro','assets/app','assets/tray','assets/graphics','exports','templates','tokens','implementation','reference','previews','output/pdf']:
        (ROOT/folder).mkdir(parents=True,exist_ok=True)
    sources=[]
    for old,new in [('master','assets/brand'),('micro','assets/micro'),('app','assets/app')]:
        for p in sorted((FINAL/old).glob('*.svg')):
            target=ROOT/new/p.name;shutil.copyfile(p,target)
            sources.append({'source':p.relative_to(REPO).as_posix(),'target':target.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
    for p in (FINAL/'png').rglob('*.png'):
        out=ROOT/'exports'/p.relative_to(FINAL/'png');out.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,out)
    for n in [16,20,24,32]:
        for color in ['black','white']:
            source=FINAL/('micro' if n<32 else 'master')/(f'symbol-{n}-{color}.svg' if n<32 else f'symbol-{color}.svg')
            shutil.copyfile(source,ROOT/'assets/tray'/f'tray-{n}-{color}.svg')
            out=ROOT/'exports/tray'/f'tray-{n}-{color}.png';out.parent.mkdir(exist_ok=True)
            shutil.copyfile(FINAL/'png/symbol'/f'symbol-{color}-{n}.png',out)
    ref=ROOT/'reference/main-ui-current.png'
    native=REPO/'build/brand24/native.png.desktop-brand-main-light.png'
    if native.exists():shutil.copyfile(native,ref)
    if not ref.exists():raise FileNotFoundError('Capture native brand UI before building the handbook.')
    (ROOT/'source/provenance.json').write_text(json.dumps({'masterSource':'assets/brand/current','masterFiles':sources,
        'uiReference':{'source':'reference/main-ui-current.png','sha256':hashlib.sha256(ref.read_bytes()).hexdigest(),'status':'Windows native Qt 6.10.2 / isolated synthetic fixture / unmodified'},
        'runtimeIntegrated':True,'pdfFonts':'Local system font subsets embedded for reading; no standalone font files distributed.'},ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
    (ROOT/'tokens/daymark.tokens.json').write_text(json.dumps(T,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
    css=['/* Brand-only reference; not a runtime UI override. */',':root {',f'  --daymark-brand-ink: {INK};']
    for k,v in T['brand']['lightPalette'].items():css.append(f'  --daymark-brand-{k}: {v};')
    css+=['  --daymark-brand-paper: #ffffff;','  --daymark-brand-canvas: #f5f6f8;','}', '[data-daymark-brand-theme="dark"] {','  --daymark-brand-ink: #f2f2f7;']
    css +=[f'  --daymark-brand-{k}: {v};' for k,v in T['brand']['darkPalette'].items()];css+=['}']
    (ROOT/'tokens/daymark.brand.css').write_text('\n'.join(css)+'\n',encoding='utf-8',newline='\n')
    qrc=['<RCC><qresource prefix="/daymark/vi">']
    for p in sorted((ROOT/'assets').rglob('*.svg')):qrc.append(f'  <file alias="{p.relative_to(ROOT/"assets").as_posix()}">../{p.relative_to(ROOT).as_posix()}</file>')
    qrc.append('</qresource></RCC>');(ROOT/'implementation/daymark_brand.qrc').write_text('\n'.join(qrc)+'\n',encoding='utf-8',newline='\n')


def graphics():
    colors=list(T['brand']['lightPalette'].values())
    ribbon=''.join(f'<polygon points="{i*80-120},800 {i*80-40},800 {i*80+760},0 {i*80+680},0" fill="{c}"/>' for i,c in enumerate(colors))
    (ROOT/'assets/graphics/ribbon-edge.svg').write_text(svg(ribbon,400,800,'Daymark 45-degree color field'),encoding='utf-8',newline='\n')
    grid=''.join(f'<path d="M{x} 0V400" stroke="#E0E1E7" stroke-width="1"/>' for x in range(0,601,40))
    grid+=''.join(f'<path d="M0 {y}H600" stroke="#E0E1E7" stroke-width="1"/>' for y in range(0,401,40))
    grid+='<rect x="80" y="80" width="200" height="40" rx="8" fill="#E8EBEF"/><rect x="80" y="160" width="320" height="40" rx="8" fill="#E8EBEF"/>'
    (ROOT/'assets/graphics/order-grid.svg').write_text(svg(grid,600,400,'Daymark neutral ordering grid'),encoding='utf-8',newline='\n')


def template(name,w,h,texts,accent=False):
    # Logo and graphics are native vector shapes. Copy is editable SVG text.
    logo=body(ROOT/'assets/brand'/('wordmark-black.svg' if accent else 'logo-primary.svg'))
    bw,bh=(274,58) if accent else (320,72)
    lw=320 if w>1200 else 280
    x,y=int(w*.065),int(h*.085)
    art=f'<rect width="{w}" height="{h}" fill="#F5F6F8"/><g transform="translate({x} {y}) scale({lw/bw})">{logo}</g>'
    if accent:
        art+=f'<g transform="translate({w-300} 0) scale(.75 {h/800})">'+body(ROOT/'assets/graphics/ribbon-edge.svg')+'</g>'
    plain=svg(art,w,h,'Daymark editable layout geometry')
    full=art
    for x,y,s,n,c in texts:
        full+=f'<text x="{x}" y="{y+n*.85}" fill="{c}" font-family="Microsoft YaHei, sans-serif" font-size="{n}">{html.escape(s)}</text>'
    (ROOT/'templates'/f'{name}.svg').write_text(svg(full,w,h,'Daymark editable '+name),encoding='utf-8',newline='\n')
    im=raster(plain,w,h);d=ImageDraw.Draw(im)
    for x,y,s,n,c in texts:d.text((x,y),s,font=font(n),fill=c)
    im.save(ROOT/'templates'/f'{name}.png',icc_profile=SRGB,optimize=True)


def templates():
    template('cover-1600x900',1600,900,[(104,330,'把事情记下，',72,INK),(104,430,'把时间理顺。',72,INK),(104,700,'任务与时间安排',30,GREY)])
    template('release-1600x900',1600,900,[(104,335,'版本更新',76,INK),(104,465,'清楚记录，从容安排。',36,GREY),(104,730,'发布说明 / RELEASE NOTES',24,GREY)],True)
    template('square-1080x1080',1080,1080,[(70,365,'每一天，',74,INK),(70,475,'都有清楚的下一步。',62,INK),(70,875,'任务 · 日历 · 提醒',28,GREY)])
    template('document-header-1600x400',1600,400,[(104,245,'产品文档 / DOCUMENTATION',34,INK)])


def register_fonts():
    root=Path(os.environ.get('WINDIR','C:/Windows'))/'Fonts'
    pdfmetrics.registerFont(TTFont('DM-CN',str(root/'msyh.ttc'),subfontIndex=0))
    pdfmetrics.registerFont(TTFont('DM-CN-B',str(root/'msyhbd.ttc'),subfontIndex=0))
    pdfmetrics.registerFont(TTFont('DM-Latin',str(root/'segoeui.ttf')))


def txt(c,x,y,s,n=14,color=INK,bold=False):
    c.setFont('DM-CN-B' if bold else 'DM-CN',n);c.setFillColor(HexColor(color));c.drawString(x,H-y-n,s)


def para(c,x,y,s,width=400,n=14,color=GREY,leading=24):
    lines=[];line=''
    for ch in s:
        if ch=='\n':lines.append(line);line=''
        elif line and pdfmetrics.stringWidth(line+ch,'DM-CN',n)>width:
            if ch in '，。；：、！？）》”':
                lines.append(line[:-2]);line=line[-2:]+ch
            else:
                tail=re.search(r'[A-Za-z0-9_./-]+$',line)
                if ch.isascii() and ch.isalnum() and tail and tail.start()>0:
                    lines.append(line[:tail.start()]);line=tail[0]+ch
                else:lines.append(line);line=ch
        else:line+=ch
    if line:lines.append(line)
    for i,v in enumerate(lines):txt(c,x,y+i*leading,v,n,color)
    return y+len(lines)*leading


def rect(c,x,y,w,h,color='#FFFFFF',radius=12):
    c.setFillColor(HexColor(color));c.roundRect(x,H-y-h,w,h,radius,stroke=0,fill=1)


def art(c,p,x,y,w):
    drawing=svg2rlg(str(p));scale=w/drawing.width
    c.saveState();c.translate(x,H-y-drawing.height*scale);c.scale(scale,scale)
    bounds=c.beginPath();bounds.rect(0,0,drawing.width,drawing.height);c.clipPath(bounds,stroke=0,fill=0)
    renderPDF.draw(drawing,c,0,0);c.restoreState()


def picture(c,p,x,y,w):
    with Image.open(p) as im:h=w*im.height/im.width
    c.drawImage(ImageReader(str(p)),x,H-y-h,width=w,height=h,mask='auto')


def page(c,n,title,dark=False):
    c.setFillColor(HexColor('#1C1C1E' if dark else PAPER));c.rect(0,0,W,H,fill=1,stroke=0)
    txt(c,56,27,'DAYMARK / VISUAL IDENTITY V2',10,'#B7B7C2' if dark else GREY)
    txt(c,56,66,title,30,'#F2F2F7' if dark else INK,True)
    txt(c,56,565,'2026-11-03  /  批准A母版 · 桌面软件接入',9,'#B7B7C2' if dark else GREY)
    txt(c,866,565,f'{n:02d} / 16',9,'#B7B7C2' if dark else GREY)


def manual():
    register_fonts()
    p=ROOT/'output/pdf/Daymark-VI-V2.pdf';c=canvas.Canvas(str(p),pagesize=(W,H),pageCompression=1,invariant=1)
    c.setTitle('Daymark VI V2 - 品牌视觉识别手册');c.setAuthor('Daymark');c.setSubject('D形记录轮廓、四色短笔与定制小写字标的完整VI规范')
    page(c,1,'把事情记下，把时间理顺。')
    art(c,ROOT/'assets/brand/logo-primary.svg',68,210,820)
    txt(c,70,455,'任务与时间安排',22,GREY);txt(c,70,505,'VISUAL IDENTITY SYSTEM / V2.0',12,GREY);c.showPage()

    page(c,2,'清晰、可靠、轻盈，适度亲和。')
    para(c,56,145,'Daymark是个人桌面任务与时间安排工具，把记下、整理、安排和执行连成清楚的工作流。列表与日历查看同一任务，本地优先，桌面组件保持低干扰的在场感。',840,17,INK,29)
    for i,(a,b) in enumerate([('清晰','轮廓集中，重点可辨。'),('可靠','同一名称、同一身份。'),('轻盈','让内容成为主角。'),('亲和','小写字形与克制圆弧。')]):
        x=56+i*218;rect(c,x,305,194,166);txt(c,x+18,327,a,23,bold=True);para(c,x+18,382,b,158,14)
    txt(c,56,506,'品牌意图：事情有记录，下一步清楚，对一天的安排有把握。',15,GREY);c.showPage()

    page(c,3,'一个稳定的标记。')
    rect(c,56,145,360,355);art(c,ROOT/'assets/brand/symbol-primary.svg',118,165,240)
    para(c,480,155,'D形两段主体保持炭黑；左下斜向留白与右上短笔形成记录的笔势。短笔由蓝、绿、橙、紫四个连续色面构成，字标提供明确名称。',410,17,INK,29)
    para(c,480,295,'品牌意图是记下事情，给一天留下标记。含义保持适度抽象，不宣称直接识别；四色不对应固定分类，也不表示任务进度。',410,15)
    para(c,480,402,'标准名称：Daymark。正式字标采用小写daymark原创路径；不另造中文商品名。',410,15)
    c.showPage()

    page(c,4,'比例与保护区。')
    rect(c,56,140,390,375);art(c,ROOT/'assets/brand/symbol-primary.svg',120,175,250)
    c.setStrokeColor(HexColor('#B9BFC8'));c.setDash(4,3);c.rect(90,H-470,310,300,stroke=1,fill=0);c.setDash()
    para(c,500,155,'符号画布100×100。保护区从可见轮廓外缘起，至少12个设计单位；符号显示为100px画布时，留出至少12px。',395,16,INK,28)
    para(c,500,302,'短笔包络宽约为主体的36.6%，中部内白宽约49.5%。横版320×72，字标274×58；整组等比缩放。SVG画布空白不代表可占用的保护区。',395,15)
    txt(c,500,447,'不压扁，不旋转，不挤进其他控件。',16,bold=True);c.showPage()

    page(c,5,'浅深与单色，身份一致。')
    for x,y,bg,mode in [(56,145,'#FFFFFF','primary'),(496,145,'#1C1C1E','dark'),(56,338,'#FFFFFF','black'),(496,338,'#1C1C1E','white')]:
        rect(c,x,y,408,170,bg);art(c,ROOT/f'assets/brand/logo-{mode}.svg',x+24,y+48,360)
    c.showPage()

    page(c,6,'炭黑为骨架，四色向上。')
    for i,(name,light,dark) in enumerate(zip(['蓝','绿','橙','紫'],T['brand']['lightPalette'].values(),T['brand']['darkPalette'].values())):
        x=56+i*218;rect(c,x,155,194,140,light);txt(c,x,315,name+' '+light,17,bold=True);txt(c,x,354,'深色 '+dark,13,GREY)
    rect(c,56,410,194,56,INK);txt(c,274,420,'主体与字标 #1C1C1E',18,bold=True)
    para(c,56,500,'色序固定：从主体向外为蓝→绿→橙→紫。四色仅用于上扬笔画，字标下方不加四色装饰。',850,14);c.showPage()

    page(c,7,'品牌色与功能色各有职责。')
    roles=[('操作强调','accent'),('成功 / 完成反馈','success'),('危险 / 删除错误','danger'),('分类可选色','categoryPurple'),('阅读文字','text')]
    for i,(name,key) in enumerate(roles):
        y=152+i*65;txt(c,56,y+8,name,16,bold=True)
        for j,theme in enumerate(['light','dark']):
            col=T['uiReference']['normal'][theme][key];rect(c,350+j*275,y,70,42,col,7);txt(c,438+j*275,y+10,col,14)
    para(c,56,506,'这些值来自当前DesignTokens.qml；品牌四色不能替代普通文字、危险、成功或焦点色，状态也不能只依赖颜色。',850,13);c.showPage()

    page(c,8,'品牌字标与阅读字体分工。')
    rect(c,56,145,848,110);art(c,ROOT/'assets/brand/wordmark-black.svg',82,168,430)
    txt(c,550,178,'字标为原创路径，不是字体文件。',16,bold=True)
    rows=[('页面标题',28),('普通标题 / 面板',20),('列表任务标题',15),('正文 / 导航 / 菜单',13),('日期 / 备注摘要',12)]
    for i,(role,size) in enumerate(rows):
        y=288+i*43;txt(c,56,y,role,14,GREY);txt(c,300,y-4,'整理下周计划 09:30',size);txt(c,785,y,str(size)+' DIP',13,GREY)
    para(c,56,518,'Windows沿当前中文优先系统字体策略；macOS使用Qt GeneralFont与语言回退。不分发SF、苹方或独立字体文件。',850,12);c.showPage()

    page(c,9,'有节奏的留白，不堆叠装饰。')
    vals=T['layout']['spacing']
    for i,v in enumerate(vals):
        x=56+i*95;rect(c,x,170,70,v*3,'#DDE7F4',4);txt(c,x,345,str(v),16,bold=True)
    para(c,56,410,'界面基准为4 DIP，常用8/12/16/24。控件、行、菜单、面板圆角分别为7/8/10/14；宣传版式采用12列和约6%的外边距。',850,16,INK,27)
    para(c,56,503,'宣传版式字级与产品DIP角色分开，不用营销大标题覆盖紧凑任务界面。',850,14);c.showPage()

    page(c,10,'辅助图形延续笔势。')
    rect(c,56,145,390,350);art(c,ROOT/'assets/graphics/ribbon-edge.svg',166,150,170)
    rect(c,496,145,408,350);art(c,ROOT/'assets/graphics/order-grid.svg',520,185,360)
    txt(c,56,510,'四色辅助图形出现时，版式使用单色标志或字标，避免第二个四色重心。',14,GREY);c.showPage()

    page(c,11,'小号守住轮廓。')
    for i,n in enumerate([16,20,24,32,48,64,128]):
        x=64+i*125;picture(c,ROOT/f'exports/symbol/symbol-primary-{n}.png',x,165,min(n*1.5,120));txt(c,x,310,str(n)+'px源图',13,GREY)
        picture(c,ROOT/f'exports/app/app-light-{n}.png',x,370,min(n*1.5,120))
    para(c,56,516,'符号32px、应用载体48px起使用四色；更小用单色。PDF为源图放大预览，真实像素见exports。系统托盘始终单色。',850,12);c.showPage()

    page(c,12,'品牌进入产品，内容仍是主角。')
    picture(c,ROOT/'reference/main-ui-current.png',56,144,590)
    para(c,690,151,'主窗：品牌入口克制，任务内容优先。',210,16,INK,27)
    para(c,690,263,'小窗与Mini：不额外堆叠大Logo或四色装饰。',210,15)
    para(c,690,373,'悬浮球与托盘：复用同一身份，保留可恢复和可读性。',210,15)
    txt(c,56,531,'Windows Qt 6.10.2原生截图，临时资料；品牌已接入，图像未做标志贴图。',12,GREY);c.showPage()

    page(c,13,'常用版式，直接复用。')
    for x,y,name,w in [(56,145,'cover-1600x900',400),(504,145,'release-1600x900',400),(56,386,'document-header-1600x400',400),(660,384,'square-1080x1080',144)]:
        picture(c,ROOT/f'templates/{name}.png',x,y,w)
    txt(c,56,531,'SVG中的文案可编辑，PNG作排版预览。替换文案后需检查字体回退与换行。',12,GREY);c.showPage()

    page(c,14,'保持清楚的一个重心。')
    rect(c,56,145,408,333);art(c,ROOT/'assets/brand/logo-primary.svg',80,230,360);txt(c,80,393,'正确：一组四色 + 炭黑字标',16,bold=True)
    rect(c,496,145,408,333);art(c,ROOT/'assets/brand/wordmark-black.svg',528,225,340)
    for i,col in enumerate(T['brand']['lightPalette'].values()):rect(c,550+i*70,325,48,6,col,3)
    txt(c,520,393,'禁用：字标下再挂四色装饰',16,'#B92330',True)
    para(c,56,506,'不拉伸、不换色序、不新增材质，不将品牌符号当作完成/新增按钮。',850,14);c.showPage()

    page(c,15,'接入有映射，迁移有边界。')
    entries=[('标志与字标','assets/brand；对应BrandMark、BrandWordmark、BrandLockup'),('应用图标','assets/app、exports/app；平台容器生成与系统显示另验'),('托盘 / 菜单栏','assets/tray；按系统栏实际对比选择黑白模板'),('颜色参数','tokens/daymark.tokens.json；品牌参数与UI参考值分层'),('旧v0.9','日出/地平线、海军蓝/珊瑚橙与QSS示例停止作为现行规范')]
    for i,(a,b) in enumerate(entries):
        y=147+i*73;txt(c,56,y,a,17,bold=True);para(c,295,y,b,600,14)
    txt(c,56,526,'作者源同步QML与平台图标；正常构建消费已提交资产，不运行作者工具。',13,GREY);c.showPage()

    page(c,16,'一套现行VI，一处正式入口。')
    para(c,56,153,'本包以assets/brand/current的批准A母版为唯一生产源。主体、留白与短笔按参考整体校准；历史探索不作为备用主标。',840,18,INK,31)
    for i,(a,b) in enumerate([('可直接使用','SVG母版、PNG、模板、配色参数、完整规范'),('可复核','源文件、母版散列、生成脚本、包清单与校验'),('平台边界','Windows原生绘制有截图；macOS与混合DPI另验')]):
        y=300+i*65;txt(c,56,y,a,17,bold=True);txt(c,295,y,b,15,GREY)
    txt(c,56,520,'设计文件与PDF的校验记录见validation.json；不把静态设计等同平台发布通过。',12,GREY)
    c.showPage();c.save()


def overview():
    im=Image.new('RGB',(1800,1300),PAPER);d=ImageDraw.Draw(im)
    d.text((80,45),'DAYMARK / VI V2',font=font(30),fill=GREY)
    logo=raster((ROOT/'assets/brand/logo-primary.svg').read_text(encoding='utf-8'),1080,243);im.paste(logo,(80,145),logo)
    d.text((80,420),'把事情记下，把时间理顺。',font=font(42),fill=INK)
    d.text((80,496),'标准色 / BRAND COLORS',font=font(24),fill=GREY)
    for i,col in enumerate(T['brand']['lightPalette'].values()):d.rounded_rectangle((80+i*420,540,450+i*420,640),16,fill=col)
    for i,name in enumerate(['cover-1600x900','release-1600x900','square-1080x1080']):
        with Image.open(ROOT/f'templates/{name}.png') as r:
            r=r.convert('RGB');r.thumbnail((510,420));im.paste(r,(80+i*570,730))
    d.text((80,1195),'标志 · 配色 · 排版 · 辅助图形 · 图标 · 应用与传播模板',font=font(30),fill=GREY)
    im.save(ROOT/'Daymark_VI_preview.png',icc_profile=SRGB,optimize=True)


if __name__=='__main__':
    assets();graphics();templates();manual();overview()
    print('VI V2 built: copied frozen identity assets, tokens, 4 editable templates, 16-page PDF and overview.')
