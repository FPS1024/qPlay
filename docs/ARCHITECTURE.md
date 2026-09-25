# Architecture

qPlay is a native Qt desktop application. QML provides the library, poster
grid, playback controls, and settings. `PlayerEngine` exposes the application
model to QML and stores catalogue entries, TMDB metadata, favorites, and resume
positions in SQLite.

`MpvSession` owns a libmpv client and translates mpv events and commands into
application state. The video item embeds libmpv's OpenGL render API in a
`QQuickFramebufferObject`; audio decoding, synchronization, seeking, and codec
handling remain inside mpv.

TMDB lookups are queued and performed one at a time. The v3 API key is stored
in local application settings. Poster files are fetched by Qt Quick's image
loader and cached by Qt.

The current source provider scans local directories. Cloud-account discovery
and refreshable Quark playback URLs are not yet implemented, so cloud file IDs
and expiring stream URLs are not treated as durable library paths.

The user-facing name and executable are `qPlay`. The `QuarkTV` C++ namespace,
QML module, and Qt settings identity remain stable internal identifiers so the
existing media catalogue and TMDB key keep using their current storage paths.
