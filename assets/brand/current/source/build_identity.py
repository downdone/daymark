"""Reproducible approved-A assets and native Qt Quick artwork.

Authoring dependencies only: Pillow and CairoSVG. Runtime uses native paths/PNG.
Run from either Windows or macOS; no SDK or machine paths are stored here.
"""
from pathlib import Path
import argparse,io,json,math,re,struct,hashlib
import cairosvg
from PIL import Image,ImageCms

ROOT=Path(__file__).resolve().parents[1]
REPO=ROOT.parents[2]
G=json.loads((ROOT/'source/geometry.json').read_text(encoding='utf-8'))
SRGB=ImageCms.ImageCmsProfile(ImageCms.createProfile('sRGB')).tobytes()

def number(v):return f'{v:.7f}'.rstrip('0').rstrip('.') or '0'
def transform_path(path,scale=1,dx=0,dy=0,grid=None):
    result=[]
    def n(v):return number(round(v/grid)*grid if grid else v)
    for match in re.finditer(r'([A-Za-z])([^A-Za-z]*)',path):
        command=match[1];vals=[float(x) for x in re.findall(r'-?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?',match[2])]
        assert command in 'MLHVCSQTAZ',command
        if command=='H':vals=[v*scale+dx for v in vals]
        elif command=='V':vals=[v*scale+dy for v in vals]
        elif command=='A':
            for i in range(0,len(vals),7):
                vals[i]*=scale;vals[i+1]*=scale;vals[i+5]=vals[i+5]*scale+dx;vals[i+6]=vals[i+6]*scale+dy
        else:vals=[v*scale+(dx if i%2==0 else dy) for i,v in enumerate(vals)]
        # Arc flags/rotation must never be snapped as coordinates.
        formatted=[number(v) if command=='A' and i%7 in [2,3,4] else n(v) for i,v in enumerate(vals)]
        result.append(command+' '.join(formatted))
    return ''.join(result)

T=G['rawTransform'];BODY=[transform_path(p,T['scale'],*T['translate']) for p in G['rawBodyPaths']]
FACES=[transform_path(p,T['scale'],*T['translate']) for p in G['rawColorPaths']]

def raw_cap():
    start,end=G['rawCap']['start'],G['rawCap']['end'];radius=G['rawCap']['radius']
    length=math.dist(start,end);ux=(end[0]-start[0])/length;uy=(end[1]-start[1])/length
    def p(t,n):return f'{number(start[0]+ux*t-uy*n)} {number(start[1]+uy*t+ux*n)}'
    return f'M{p(0,-radius)}L{p(length,-radius)}A{radius} {radius} 0 0 1 {p(length,radius)}L{p(0,radius)}A{radius} {radius} 0 0 1 {p(0,-radius)}Z'
CAP=transform_path(raw_cap(),T['scale'],*T['translate'])
MICRO={str(n):[transform_path(p,n/100,grid=.125) for p in BODY+[CAP]] for n in [16,20,24]}
WORDS=[transform_path(p,dx=offset+(G['markGroupShift'] if i>=3 else 0)) for i,(p,offset) in enumerate(G['wordmarkPathsAndOffsets'])]

def ink(mode):return '#FFFFFF' if mode=='white' else G['bodyDark'] if mode=='dark' else G['bodyLight']
def svg(content,w=100,h=100):
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" role="img" aria-labelledby="title"><title id="title">daymark — approved A</title>{content}</svg>\n'
def symbol_body(mode='primary',size=None):
    paths=MICRO[str(size)] if str(size) in MICRO else BODY+[CAP]
    out=''.join(f'<path d="{p}" fill="{ink(mode)}"/>' for p in paths[:2])
    if size or mode in ['black','white']:return out+f'<path d="{paths[2]}" fill="{ink(mode)}"/>'
    return out+''.join(f'<path d="{p}" fill="{c}"/>' for p,c in zip(FACES,G['colorsDark'] if mode=='dark' else G['colorsLight']))
def symbol(mode='primary',size=None):return svg(symbol_body(mode,size),size or 100,size or 100)
def word_body(mode='black'):
    return f'<g fill="none" stroke="{ink(mode)}" stroke-width="{G["wordmarkStroke"]}" stroke-linecap="round" stroke-linejoin="round">'+''.join(f'<path d="{p}"/>' for p in WORDS)+'</g>'
def word(mode='black'):return svg(word_body(mode),*G['wordmarkCanvas'])
def logo(mode='primary'):
    x,y,s=G['symbolPlacement'];a,b,k=G['wordmarkPlacement']
    return svg(f'<g transform="translate({x} {y}) scale({s})">{symbol_body(mode)}</g><g transform="translate({a} {b}) scale({k})">{word_body(mode)}</g>',*G['lockupCanvas'])
def app(dark=False,size=None):
    n=size if size and size<48 else 100;mode=('white' if dark else 'black') if size and size<48 else ('dark' if dark else 'primary')
    paths=symbol_body(mode,n) if str(n) in MICRO else symbol_body(mode)
    factor=.84 if str(n) in MICRO else n*.0084
    return svg(f'<rect width="{n}" height="{n}" rx="{n*.23}" fill="{"#252527" if dark else "#F6F6F8"}"/><g transform="translate({n*.08} {n*.08}) scale({factor})">{paths}</g>',n,n)
def raster(source,w,h=None):
    if h is None:
        a,b=map(float,re.search(r'viewBox="0 0 ([\d.]+) ([\d.]+)"',source).groups());h=round(w*b/a)
    png=cairosvg.svg2png(bytestring=source.encode(),output_width=w*4,output_height=h*4)
    return Image.open(io.BytesIO(png)).convert('RGBA').resize((w,h),Image.Resampling.LANCZOS)
def save_png(source,path,w,h=None):raster(source,w,h).save(path,icc_profile=SRGB,optimize=True)

def qml_artwork():
    micro={n:[transform_path(p,100/int(n)) for p in paths] for n,paths in MICRO.items()}
    def path_line(path,fill):return f'            ShapePath {{ strokeWidth: -1; fillColor: {fill}; PathSvg {{ path: {json.dumps(path)} }} }}'
    body='\n'.join(path_line(f'PLACEHOLDER{i}', 'root.ink') for i in range(2))
    for i in range(2):body=body.replace(json.dumps(f'PLACEHOLDER{i}'),f'root.artwork[{i}]')
    body+='\n'+path_line('PLACEHOLDER','root.singleColor ? root.ink : "transparent"').replace('"PLACEHOLDER"','root.artwork[2]')
    for i,p in enumerate(FACES):body+='\n'+path_line(p,f'root.singleColor ? "transparent" : root.palette[{i}]')
    mark='''// Generated from assets/brand/current/source/geometry.json; do not hand-edit paths.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
Item {
    id: root
    implicitWidth: 28; implicitHeight: 28
    property color ink: Theme.text
    property bool monochrome: false
    readonly property real physicalSize: Math.min(width,height)*Screen.devicePixelRatio
    readonly property bool singleColor: monochrome || Theme.highContrast || physicalSize<32
    readonly property int artworkSize: physicalSize<18 ? 16 : physicalSize<22 ? 20 : physicalSize<32 ? 24 : 100
    readonly property var artwork: artworkSize===100 ? regularPaths : microPaths[artworkSize]
    readonly property var regularPaths: REGULAR
    readonly property var microPaths: MICRO
    readonly property var palette: Theme.dark ? DARK : LIGHT
    Accessible.role: Accessible.StaticText
    Accessible.name: "daymark"
    property bool paletteReady: false
    function redraw(){if(paletteReady){drawing.active=false;drawing.active=true}}
    Component.onCompleted: paletteReady=true
    onInkChanged: redraw()
    onSingleColorChanged: redraw()
    onPaletteChanged: redraw()
    Loader {id:drawing;sourceComponent:art}
    Component {
        id:art
        Shape {
            width:100;height:100
            x:(root.width-Math.min(root.width,root.height))/2
            y:(root.height-Math.min(root.width,root.height))/2
            transform: Scale {xScale:Math.min(root.width,root.height)/100;yScale:xScale}
            preferredRendererType: Shape.CurveRenderer
BODY
        }
    }
}
'''.replace('REGULAR',json.dumps(BODY+[CAP])).replace('MICRO',json.dumps(micro)).replace('DARK',json.dumps(G['colorsDark'])).replace('LIGHT',json.dumps(G['colorsLight'])).replace('BODY',body)
    letter='\n'.join(f'            PathSvg {{path:{json.dumps(p)}}}' for p in WORDS)
    wordmark='''// Generated from assets/brand/current/source/geometry.json; original custom lettering.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
Item {
    id:root
    property color ink:Theme.text
    implicitWidth:120;implicitHeight:implicitWidth*58/274
    Accessible.role:Accessible.StaticText;Accessible.name:"daymark"
    property bool paletteReady:false
    Component.onCompleted:paletteReady=true
    onInkChanged:if(paletteReady){drawing.active=false;drawing.active=true}
    Loader {id:drawing;sourceComponent:lettering}
    Component {
        id:lettering
        Shape {
            width:274;height:58
            preferredRendererType:Shape.CurveRenderer
            transform:Scale{xScale:root.width/274;yScale:root.height/58}
            ShapePath {
                strokeColor:root.ink;strokeWidth:4.5;fillColor:"transparent"
                capStyle:ShapePath.RoundCap;joinStyle:ShapePath.RoundJoin
PATHS
            }
        }
    }
}
'''.replace('PATHS',letter)
    return mark,wordmark

def runtime():
    icons=REPO/'assets/icons';icons.mkdir(exist_ok=True,parents=True)
    (icons/'daymark.svg').write_text(app(False),encoding='utf-8',newline='\n')
    for n in [16,20,24,32,48,64,128,256,512,1024]:save_png(app(False,n),icons/f'daymark-{n}.png',n)
    # PNG-backed ICO representations and native ICNS chunks, not renamed PNG files.
    largest=Image.open(icons/'daymark-256.png')
    frames=[Image.open(icons/f'daymark-{n}.png') for n in [16,20,24,32,48,64,128]]
    largest.save(icons/'daymark.ico',sizes=[(n,n) for n in [16,20,24,32,48,64,128,256]],append_images=frames)
    chunks=[]
    for kind,n in [('icp4',16),('icp5',32),('icp6',64),('ic07',128),('ic08',256),('ic09',512),('ic10',1024),('ic11',32),('ic12',64),('ic13',256),('ic14',512)]:
        data=(icons/f'daymark-{n}.png').read_bytes();chunks.append(kind.encode()+struct.pack('>I',len(data)+8)+data)
    payload=b''.join(chunks);(icons/'daymark.icns').write_bytes(b'icns'+struct.pack('>I',len(payload)+8)+payload)
    for n in [16,20,24,32]:
        for mode in ['black','white']:save_png(symbol(mode,n if n<32 else None),icons/f'tray-{mode}-{n}.png',n)
    mark,wordmark=qml_artwork()
    (REPO/'src/ui/BrandMark.qml').write_text(mark,encoding='utf-8',newline='\n')
    (REPO/'src/ui/BrandWordmark.qml').write_text(wordmark,encoding='utf-8',newline='\n')

def manifest():
    files=[{'path':p.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(ROOT.rglob('*')) if p.is_file() and p.name!='manifest.json' and '__pycache__' not in p.parts]
    (ROOT/'manifest.json').write_text(json.dumps({'revision':G['revision'],'files':files},ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--runtime',action='store_true');parser.add_argument('--manifest-only',action='store_true');args=parser.parse_args()
    if args.manifest_only:
        manifest();return
    for p in ['master','micro','app','png/symbol','png/logo','png/wordmark','png/app']:(ROOT/p).mkdir(exist_ok=True,parents=True)
    for mode in ['primary','dark','black','white']:
        (ROOT/'master'/f'symbol-{mode}.svg').write_text(symbol(mode),encoding='utf-8',newline='\n')
        (ROOT/'master'/f'logo-{mode}.svg').write_text(logo(mode),encoding='utf-8',newline='\n')
    for mode in ['black','dark','white']:(ROOT/'master'/f'wordmark-{mode}.svg').write_text(word(mode),encoding='utf-8',newline='\n')
    for n in [16,20,24]:
        for mode in ['black','white']:(ROOT/'micro'/f'symbol-{n}-{mode}.svg').write_text(symbol(mode,n),encoding='utf-8',newline='\n')
    for dark in [False,True]:
        name='dark' if dark else 'light';(ROOT/'app'/f'app-{name}.svg').write_text(app(dark),encoding='utf-8',newline='\n')
        for n in [16,20,24,32]:(ROOT/'app'/f'app-{name}-{n}.svg').write_text(app(dark,n),encoding='utf-8',newline='\n')
    for n in [16,20,24,32,48,64,128,256,512,1024]:
        for mode in ['primary','dark','black','white']:
            source=symbol('white' if mode in ['dark','white'] else 'black',n) if n<32 else symbol(mode)
            save_png(source,ROOT/'png/symbol'/f'symbol-{mode}-{n}.png',n)
        for dark in [False,True]:save_png(app(dark,n),ROOT/'png/app'/f'app-{"dark" if dark else "light"}-{n}.png',n)
    for mode in ['primary','dark','black','white']:
        for n in [168,320,640,1280]:save_png(logo(mode),ROOT/'png/logo'/f'logo-{mode}-{n}.png',n)
    for mode in ['black','dark','white']:
        for n in [120,164,274,548,1096]:save_png(word(mode),ROOT/'png/wordmark'/f'wordmark-{mode}-{n}.png',n)
    (ROOT/'source/normalized.json').write_text(json.dumps({'body':BODY,'cap':CAP,'faces':FACES,'micro':MICRO,'word':WORDS,'paletteLight':G['colorsLight'],'paletteDark':G['colorsDark']},ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
    if args.runtime:runtime()
    manifest()
    print('Approved A: 27 SVG, 91 PNG'+', ICO/ICNS, tray and native QML generated.' if args.runtime else 'Approved A artwork generated.')

if __name__=='__main__':main()
