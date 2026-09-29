# qPlay

qPlay is a native Linux desktop media-library application. The interface is
Qt Quick/QML, the local catalogue and watch progress are stored in SQLite, and
playback is handled by embedded libmpv through its OpenGL render API. It does
not use a browser or HTML video element.

The home screen presents a cinematic feature area and poster rows. Selecting a
poster opens a title details page; playback starts only from that page. TMDB
metadata can be matched with a locally stored v3 API key. The in-app player
supports mpv seeking, playback speed, volume, and resume position.

## Dependencies

Ubuntu packages:

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-declarative-dev qml6-module-qtquick \
  qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts libqt6sql6-sqlite libmpv-dev
```

## Build and run

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/bin/qPlay
```

## Debian package

Build Debian packages and a standalone dynamically linked binary artifact with
version, platform, and architecture in the filenames:

```bash
./scripts/build-deb.sh
```

Artifacts are written to `dist/`, for example
`qPlay-1.0.0-Linux-x86_64.deb` and `qPlay-1.0.0-Linux-x86_64.bin`. Install the
Debian package with `sudo apt install ./dist/qPlay-1.0.0-Linux-x86_64.deb`.
The package declares shared-library dependencies and the required Qt Quick QML
modules in its Debian metadata.

The library catalog is prepared by a separate AI agent skill. It reads the
configured WebDAV root, matches movies and episodes with TMDB, and writes
metadata and playback URLs to SQLite. qPlay only displays that catalog and
plays the saved WebDAV URLs. Enter the WebDAV root and login in Settings and
choose **Save WebDAV settings**; credentials are sent to libmpv only for URLs
under that WebDAV root.

The TMDB key and WebDAV settings are stored in
`~/.config/qPlay/user-config.ini` with owner-only file permissions. This file
is outside the repository and is also excluded by `.gitignore` if copied into
the project. It has `[tmdb] apiKey=...` and `apiToken=...` plus `[webdav] url=...,
username=..., password=...` entries. Settings edits the WebDAV entries; the
separate scraper skill reads the TMDB credentials. The catalog schema is documented in
[`docs/MEDIA_CATALOG.md`](docs/MEDIA_CATALOG.md).

On Linux, playback history and the catalogue are stored in
`~/.local/share/qPlay/library.sqlite3`. Existing data from earlier application
directories is copied there on first launch; the original database is kept.
TMDB poster, background, and episode still images are downloaded on first
display and cached under `~/.local/share/qPlay/poster`,
`~/.local/share/qPlay/background`, and `~/.local/share/qPlay/episode`. Later
launches use those local files instead of fetching the same images again.
