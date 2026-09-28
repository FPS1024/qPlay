# qPlay v1.0.0

**首个正式版 · 2026-09-28**

qPlay v1.0.0 是首个正式版本。qPlay 是一款面向 Linux 桌面的原生影视播放器，使用 Qt Quick 构建界面，并通过嵌入式 libmpv 播放媒体，不依赖浏览器或 HTML 视频组件。

## 主要功能

- 海报墙浏览和影片详情页面。
- 播放配置媒体目录中保存的 WebDAV 地址；认证信息保存在本地配置中。
- 支持进度定位、快进/快退、播放/暂停、播放速度、音量和续播。
- 播放时双击画面进入沉浸式全屏，按 `Esc` 退出全屏。
- 音量最高可调至 200%。高音量可能造成失真。
- TMDB 海报和背景图下载后缓存在本机。
- 使用 SQLite 保存媒体目录和播放进度。

## 快捷键

| 按键 | 操作 |
| --- | --- |
| `Space` | 播放 / 暂停 |
| `←` / `→` | 快退 / 快进 5 秒 |
| `↑` / `↓` | 音量增加 / 减少 5% |
| `M` | 静音 / 取消静音 |
| 双击视频画面 | 进入 / 退出全屏 |
| `Esc` | 全屏时退出全屏；否则返回上一级页面 |

## 数据和配置

媒体目录及播放记录位于 `~/.local/share/qPlay/library.sqlite3`。媒体目录扫描和 TMDB 匹配由独立的目录工具完成；qPlay 负责读取目录、展示影片信息并播放保存的地址。

WebDAV 和 TMDB 配置保存在 `~/.config/qPlay/user-config.ini`。海报和背景缓存分别保存在 `~/.local/share/qPlay/poster` 与 `~/.local/share/qPlay/background`。

## 构建和运行

安装 README 中列出的 Qt 6、CMake、Ninja、SQLite 驱动和 libmpv 开发依赖后运行：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/bin/qPlay
```

查看版本：

```bash
./build/bin/qPlay --version
```
