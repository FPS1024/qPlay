# qPlay

qPlay is a native Linux desktop media-library application. The interface is
Qt Quick/QML, the local catalogue and watch progress are stored in SQLite, and
playback is handled by embedded libmpv through its OpenGL render API. It does
not use a browser or HTML video element.

The home screen scans local movie folders into a poster wall. TMDB movie and
series metadata can be matched with a locally stored v3 API key. Selecting a
title opens the in-app player, which supports mpv seeking, audio/subtitle track
selection, playback speed, volume, and resume position.

## Dependencies

Ubuntu packages:

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-declarative-dev qml6-module-qtquick \
  qml6-module-qtquick-controls qml6-module-qtquick-dialogs \
  qml6-module-qtquick-layouts libqt6sql6-sqlite libmpv-dev
```

## Build and run

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/bin/qPlay
```

Use **Add folder** to scan a local movie directory. Add a TMDB v3 API key in
Settings to fetch posters and descriptions. The key is saved in the app's local
settings and is not written into the source tree.

The Quark Drive account and direct cloud playback source are not yet connected
to this app; the current catalogue scans local folders and accepts playable
URLs through the native file-open flow.
