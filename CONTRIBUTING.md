# 参与 Daymark

感谢参与。本项目使用 C++20 + Qt 6.10.2，以本地优先、离线可用为核心约束。开始前请读 [AGENTS.md](AGENTS.md)、[文档导航](docs/README.md) 与[工程原则](docs/principles.md)。

## 环境

| 依赖 | 版本 |
|---|---|
| Qt | **6.10.2**（CMake 使用 `EXACT` 校验） |
| CMake | ≥ 3.24 |
| Ninja | 任意近期版本 |
| 编译器 | Windows：MinGW GCC 13.1 或 MSVC 2022；macOS：Apple Clang |
| Python | 3（发布守卫与辅助脚本） |
| PowerShell | 7（Windows 打包与测试脚本） |

## 构建与测试

```bash
# Windows：DAYMARK_QT 指向 6.10.2/mingw_64，DAYMARK_MINGW 指向 Tools/mingw1310_64
./scripts/build-windows.ps1
./scripts/test-windows.ps1

# macOS：DAYMARK_QT 指向 6.10.2/macos
bash scripts/build-macos.sh macos-release
```

不要提交本机绝对路径、Qt SDK、构建产物、日志、数据库或凭据。路径一律通过环境变量或忽略的 `CMakeUserPresets.json` 注入。

## 提交流程

1. 先开 issue 或用现有条目说明问题与期望行为。
2. 一个提交只做一件事；行为变化必须同时更新对应测试与文档。
3. 提交前本地运行：

   ```bash
   python -m unittest discover -s tests -p test_release_metadata.py -v
   python scripts/publish-source-release.py --check-current
   pwsh -NoProfile -File scripts/check-docs.ps1
   ```

4. 代码标识用英文，开发文档与用户说明用中文（见工程原则 P-16）。

## 分层约束

- QML 只做界面；任务判定集中在 C++ `QueryService`/服务层；SQLite 访问集中在 Repository；平台原生调用隔离于 `src/platform`。
- 自动测试一律使用 `QTemporaryDir`，不读写真实用户数据库。
- 数据模式变化必须新增 migration，不覆盖历史迁移。

## 许可

提交即表示你同意以本项目的 [LGPL-3.0](LICENSE) 许可贡献代码。请勿加入许可不兼容的第三方代码或资源。
