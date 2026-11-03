# Daymark 现行品牌资产 · A-20261103

用户确认的 A 比例母版：D 形记录轮廓、左下斜向留白、右上短笔，配原创小写 `daymark` 字标。含义为「记下事情，给一天留下标记」；这是品牌意图，不声称所有人都能直接识别，也不表示任务完成或同步状态。

唯一几何源为 [geometry.json](source/geometry.json)。原图只作形状参考，没有把参考位图、字体或材质嵌入产品。主体、留白和短笔作为整组等比归一；短笔/主体宽约36.6%，中部内白/主体宽约49.5%。禁止单独拉长短笔或扩大内白。

`master/` 为11个标准矢量，`micro/` 为16/20/24物理像素单色微型，`app/` 为10个浅深载体，`png/` 为91张含sRGB的栅格。`manifest.json`记录源与导出文件SHA256。完整规范、模板和PDF见 [VI](../../../resource/VI/README.md)，产品映射见 [接入契约](../../../docs/design/brand-identity-integration.md)。

作者环境使用 Python、Pillow 12.2.0、CairoSVG 2.9.0；仅用于生成已提交资源，不增加客户端依赖。安装依赖后，Windows PowerShell 与 macOS shell 均可运行：

```sh
python assets/brand/current/source/build_identity.py --runtime
```

这会同步 PNG/ICO/ICNS、托盘和 QML 原生路径；不要手改生成路径。运行时无需 Python、CairoSVG、字体包或 QtSvg。UI操作图标仍由 IconPaths.qml 管理。品牌四色不替代功能色，字标下方不添加四色点缀。
