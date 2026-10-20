# qPlay v1.0.3

**类型化片库与首页主题 · 2026-10-21**

qPlay v1.0.3 在 v1.0.2 的基础上加入作品类型管理、可配置的首页主题和集中设置页面，并完善电影多播放源与电视剧详情。

## 主要更新

- **增加作品类型与主题映射**：电影类型绑定到 TMDB 电影，电视剧类型绑定到整部剧。设置页可把 TMDB 类型名称映射为 qPlay 自定义主题名，映射数据保存在 SQLite 中。
- **增加首页布局选择**：在设置页选择随机海报墙，或按作品类型排列的横向主题栏。海报墙会随机打乱作品；主题栏提供平滑滚动按钮。
- **按用户记录生成首页推荐**：没有收藏或观看历史时随机推荐；出现用户记录后，根据收藏和观看过的作品类型偏好推荐，不使用写死的类型。
- **完善影视详情**：详情页显示类型标签；同一电影的多个文件合并为可选播放源；电视剧详情页展示分季和剧集信息。
- **增加集中设置页面**：从首页齿轮打开，可配置首页布局、类型主题、WebDAV 连接和播放选项。
- **提高目录读取上限**：媒体目录单次读取上限提高到 5,000 项。
- **更新版本和安装包名称**：程序版本为 `1.0.3`；DEB 包和独立二进制文件名包含版本、平台和架构。

## 键盘操作

| 按键 | 操作 |
| --- | --- |
| `F11` | 切换应用窗口全屏 |
| `Space` | 播放 / 暂停 |
| `←` / `→` | 快退 / 快进 5 秒 |
| `↑` / `↓` | 调高 / 调低音量 5% |
| `M` | 静音 / 取消静音 |
| 双击视频画面 | 切换沉浸式视频全屏 |
| `Esc` | 退出全屏；否则返回上一级页面 |

## 数据和配置

媒体目录、作品类型和播放进度保存在 `~/.local/share/qPlay/library.sqlite3`。WebDAV 和 TMDB 配置保存在 `~/.config/qPlay/user-config.ini`；海报和背景图缓存位于 `~/.local/share/qPlay/poster` 与 `~/.local/share/qPlay/background`。

## 构建和安装

从源码构建：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/bin/qPlay
```

构建 Debian 安装包和独立二进制文件：

```bash
./scripts/build-deb.sh
```

DEB 包：`qPlay-1.0.3-Linux-x86_64.deb`。安装命令：

```bash
sudo apt install ./qPlay-1.0.3-Linux-x86_64.deb
```

独立动态链接程序：`qPlay-1.0.3-Linux-x86_64.bin`。查看程序版本：

```bash
./build/bin/qPlay --version
```

**Full Changelog**: https://github.com/FPS1024/qPlay/compare/v1.0.2...v1.0.3
