# 更新器源码依赖

仅更新SDK/原生宿主使用，正常构建不联网下载。

| 组件 | 版本/固定上游提交 | 许可/用途 |
|---|---|---|
| nlohmann/json | 3.12.0 / `55f93686c01528224f448c19128836e7df245f72` | MIT；严格JSON解析回调、字段/深度/重复key检查 |
| miniz | 3.1.2 / `77d0dce8627735138c51770d1799a1ef48f2117d` | 上游MIT LICENSE，原始源文件亦保留其许可声明；只编译读取/解压，不编译写ZIP/deflateAPI |

来源：[json提交](https://github.com/nlohmann/json/commit/55f93686c01528224f448c19128836e7df245f72)、[miniz提交](https://github.com/richgel999/miniz/commit/77d0dce8627735138c51770d1799a1ef48f2117d)。SOURCES.json锁定每个上游文件实际字节SHA256，json.hpp还与上游Release公布的`aaf127c0…fc5de63`完整散列核对一致。原始源码未修改；`miniz_export.h`是本项目对静态构建生成的空导出宏头，不冒充上游原文件。

Windows使用系统CNG/WinHTTP，不再分发OpenSSL。原生宿主不依赖Qt；Daymark Qt主程序仍沿已有LGPL分发策略。托管更新包根目录附update-licenses的原许可证与来源清单。

2026-11-05核对上游发布与安全入口：miniz3.1.2包含中央目录偏移溢出和inflate死循环修复；json安全入口没有已发布advisory。该检查不等于完整漏洞审计或未来不会出现漏洞；畸形ZIP/JSON和资源上限仍由本项目测试守卫，后续依赖更新须重新锁散列与验证。未经此过程不浮动升级标签或拉取develop。
