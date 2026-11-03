# UX-70 · A品牌系统的软件接入

需求：统一用户批准的A比例母版，完整更新VI及桌面应用。工程决策见ADR-032。产品需求映射FR的品牌呈现，不新增数据、账号或网络行为。

P-13 / Apple HIG映射：Purpose让任务内容优先（UX-01）；Simplicity只保留一个品牌标记；Craft统一曲线、留白、比例及实际像素（UX-09）；Flexibility沿系统主题、高对比与文本缩放（UX-10/11），可恢复的桌面入口沿UX-13。图形不是完成、编辑或新增按钮，交互说明采用可访问名称与现有标签。

| 位置 | 接口 / 资源 | 行为 |
|---|---|---|
| Windows EXE、标题栏、任务栏 | daymark.ico / BrandIcons::application() | 多尺寸中性载体，48物理px起四色 |
| macOS bundle | daymark.icns | 11个原生表示；Finder/Dock实测另验 |
| 主窗侧栏 | BrandWordmark(ink) | 原创小写路径，274:58等比；不挤占导航 |
| 关于、独立账号窗口 | BrandLockup(ink,monochrome) | 320:72原始组合；不强制文字大写 |
| 悬浮球 | BrandMark(ink,monochrome) | 根据物理像素选微型；保留原点击/悬停/错误反馈 |
| 托盘 / 菜单栏 | BrandIcons::tray(lightSurface) | 始终单色；Windows系统栏主题独立于应用主题；macOS alpha模板 |
| UI功能图标 | IconPaths.qml | 保持原有语义，品牌标志不参与 |

BrandMark以100格绘制；16/20/24微型控制点按1/8物理像素校准，32物理px起使用四色，高对比强制单色。BrandWordmark和BrandMark监听主题变化重建Shape，避免Windows原生渲染的旧颜色残留。Accessible.name为daymark，组合内子标记不重复朗读。静态标记不新增Tab停靠，球的按钮、Return/Space与托盘恢复路径沿原交互；不新增动效。

测试：brand_assets验证资源尺寸/透明/四色、托盘黑白、ICO与ICNS结构及无外链矢量；brand_rendering在临时资料中验证主窗、关于、账号、球的浅深/高对比/200%真实绘制。完整CTest检查既有键盘与桌面交互。Windows原生专项保存独立窗口截图；物理混合DPI、读屏、Shell缓存刷新及macOS不能由离屏或文件结构检查替代。实际结果只记录PROGRESS。

作者源、导出资产与文档生成独立于CMake；正常构建只消费已提交资源。旧日历勾图标生成器改为调用现行作者源，防止下一次再生成旧标识。历史设计不参与生产，也不作为新功能状态图标。数据库/schema保持不变。

品牌路径选择Qt内置CurveRenderer，减少缩放时的折线感并使用其原生抗锯齿；后端不支持时由Qt回退，不增加运行时插件。依据：[Qt 6.10 Shape](https://doc.qt.io/qt-6.10/qml-qtquick-shapes-shape.html#rendererType-prop)。原生截图与离屏测试均在最终渲染器下验证。
