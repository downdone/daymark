# ADR-032：批准A母版与跨平台品牌资源

状态：现行，2026-11-03；用户明确批准比例校正后的A并要求完整接入与交付。

将几何、定制小写路径和浅深品牌色冻结于assets/brand/current/source/geometry.json。Python作者工具生成独立SVG、sRGB PNG、PNG帧ICO、原生多表示ICNS以及Qt Quick Shape路径。生成物随源码提交，运行时不引入新依赖，正常构建不运行Python。

理由：文件图标、QIcon、QML及VI必须从同一源生成，避免此前多轮试稿串用。单纯替换一张PNG无法覆盖系统小号、深色托盘及高对比。Qt自带Shape和PNG能力满足要求，无需给客户端增加QtSvg依赖。16/20/24提供单色微型，应用载体48起四色；托盘始终单色，并按系统栏实际主题选色。

作者工具依赖Pillow（HPND）、CairoSVG（LGPLv3）；PDF手册另用ReportLab（BSD）、svglib（LGPLv3）。它们只在作者环境使用，不随便携程序分发；版本列于VI source/requirements.txt。手册嵌入本机字体子集，不分发独立字体。品牌轮廓和字标路径由项目独立维护，参考图仅提供形状方向，没有复制参考位图或专有素材包。不据此声称已做商标排查。

弃选：每个平台手画一份（会漂移）；只使用SVG加载（增加图标插件依赖）；全尺寸四色（微型噪声）；把配色强制覆盖功能色（破坏状态语义）。macOS图标结构可检查，实际Finder/Dock和系统模板显示仍需本机验收。
