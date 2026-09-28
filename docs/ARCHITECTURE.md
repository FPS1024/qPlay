# Architecture

qPlay is a native Qt desktop application. QML provides the browse home, title
details, playback controls, and settings. Selecting a poster opens details;
only the details page starts playback. `PlayerEngine` exposes the application
model to QML and stores catalogue entries, TMDB metadata, favorites, and resume
positions in SQLite.

`MpvSession` owns a libmpv client and translates mpv events and commands into
application state. The video item embeds libmpv's OpenGL render API in a
`QQuickFramebufferObject`; audio decoding, synchronization, seeking, and codec
handling remain inside mpv.

An external AI agent skill owns catalog scanning and TMDB matching. It reads
the WebDAV root and TMDB key from `~/.config/qPlay/user-config.ini`, then
writes metadata and WebDAV playback URLs into the shared SQLite catalog.
qPlay does not scan directories or make TMDB requests. It reads the catalog,
stores playback progress, and passes WebDAV credentials to libmpv only for
media URLs below the configured WebDAV root. The shared catalog contract is in
[`MEDIA_CATALOG.md`](MEDIA_CATALOG.md).

TMDB poster and background URLs are cached as files in
`~/.local/share/qPlay/poster`, `~/.local/share/qPlay/background`, and
`~/.local/share/qPlay/episode`. On a cache miss, qPlay displays the remote image
while downloading it; once saved, the image source switches to the local file.
Later launches use the cached file.

The user-facing name, executable, Qt application name, and organization are
`qPlay`. The `QuarkTV` C++ namespace and QML module remain internal identifiers.
On first launch after the rename, the media database is copied from earlier
application-data locations to `~/.local/share/qPlay`.
