import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QuarkTV 1.0

import "components"

ApplicationWindow {
    id: window

    width: 1280
    height: 780
    minimumWidth: 880
    minimumHeight: 560
    visible: true
    color: "#080b0d"
    title: player.title.length > 0 ? player.title + " - qPlay" : "qPlay"

    property bool libraryOpen: true
    property bool playlistOpen: false
    property bool controlsVisible: true
    property bool playerPage: false

    function primaryColor() {
        return "#3dd6b5"
    }

    function openFileDialog() {
        openDialog.open()
    }

    function toggleFullscreen() {
        window.visibility = window.visibility === Window.FullScreen
                ? Window.Windowed
                : Window.FullScreen
    }

    Shortcut {
        sequence: StandardKey.Open
        context: Qt.ApplicationShortcut
        onActivated: window.openFileDialog()
    }

    Connections {
        target: player
        function onSourceChanged() {
            if (player.source && player.source.toString().length > 0)
                window.playerPage = true
        }
    }

    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        onActivated: player.togglePlayback()
    }

    Shortcut {
        sequence: "Left"
        context: Qt.ApplicationShortcut
        onActivated: player.seekRelative(-5000)
    }

    Shortcut {
        sequence: "Right"
        context: Qt.ApplicationShortcut
        onActivated: player.seekRelative(5000)
    }

    Shortcut {
        sequence: "M"
        context: Qt.ApplicationShortcut
        onActivated: player.toggleMute()
    }

    Shortcut {
        sequence: "F"
        context: Qt.ApplicationShortcut
        onActivated: window.toggleFullscreen()
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: window.visibility === Window.FullScreen
        onActivated: window.visibility = Window.Windowed
    }

    Rectangle {
        anchors.fill: parent
        color: "#080b0d"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 62
            color: "#0c1114"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: "#1d282c"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    radius: 7
                    color: window.primaryColor()

                    Text {
                        anchors.centerIn: parent
                        text: "Q"
                        color: "#04110e"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: 164
                    spacing: 0

                    Text {
                        text: "qPlay"
                        color: "#edf5f3"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1.1
                    }

                    Text {
                        text: player.videoBackend
                        color: "#6f8587"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                ToolButton {
                    text: "\u2261"
                    font.pixelSize: 20
                    Accessible.name: "Toggle library"
                    ToolTip.visible: hovered
                    ToolTip.text: "Library"
                    onClicked: window.libraryOpen = !window.libraryOpen

                    background: Rectangle {
                        radius: 6
                        color: parent.down ? "#243136" : parent.hovered ? "#182226" : "transparent"
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                ToolButton {
                    text: "Home"
                    visible: window.playerPage
                    onClicked: window.playerPage = false
                }

                Button {
                    text: "\u25B6  Open"
                    Accessible.name: "Open media"
                    onClicked: window.openFileDialog()

                    contentItem: Text {
                        text: parent.text
                        color: "#06110e"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        implicitWidth: 96
                        implicitHeight: 34
                        radius: 6
                        color: parent.down ? "#2eb59a" : parent.hovered ? "#4be3c1" : window.primaryColor()
                    }
                }

                ToolButton {
                    text: "\u2263"
                    font.pixelSize: 19
                    Accessible.name: "Toggle playlist"
                    ToolTip.visible: hovered
                    ToolTip.text: "Playlist"
                    onClicked: window.playlistOpen = !window.playlistOpen

                    background: Rectangle {
                        radius: 6
                        color: window.playlistOpen
                               ? "#243136"
                               : parent.down ? "#243136" : parent.hovered ? "#182226" : "transparent"
                    }
                }

                ToolButton {
                    text: "\u2699"
                    font.pixelSize: 18
                    Accessible.name: "Settings"
                    ToolTip.visible: hovered
                    ToolTip.text: "Settings"
                    onClicked: settingsDialog.open()

                    background: Rectangle {
                        radius: 6
                        color: parent.down ? "#243136" : parent.hovered ? "#182226" : "transparent"
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: window.playerPage && window.libraryOpen ? 286 : 0
                visible: Layout.preferredWidth > 0
                color: "#0c1114"
                clip: true

                LibraryPanel {
                    anchors.fill: parent
                    engine: player
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    width: 1
                    color: "#1d282c"
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                PosterWall {
                    anchors.fill: parent
                    visible: !window.playerPage
                    engine: player
                    onPlayRequested: function(source) {
                        player.open(source)
                        window.playerPage = true
                    }
                }

                MpvVideoItem {
                    anchors.fill: parent
                    visible: window.playerPage
                    session: player.renderSession
                }

                MouseArea {
                    anchors.fill: parent
                    visible: window.playerPage
                    acceptedButtons: Qt.LeftButton
                    onClicked: window.controlsVisible = !window.controlsVisible
                    onDoubleClicked: window.toggleFullscreen()
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 70
                    visible: window.playerPage && window.controlsVisible && player.title.length > 0
                    color: "#99080b0d"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: player.title
                                color: "#f4faf8"
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: player.formatName.length > 0
                                      ? player.formatName.toUpperCase()
                                      : "READY"
                                color: "#829598"
                                font.pixelSize: 10
                                font.letterSpacing: 0.8
                                elide: Text.ElideRight
                            }
                        }

                        Text {
                            visible: player.buffering
                            text: "BUFFERING"
                            color: window.primaryColor()
                            font.pixelSize: 10
                            font.letterSpacing: 1
                        }
                    }
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: window.playerPage && (player.playbackState === PlaybackState.Opening || player.buffering)
                    visible: running
                    width: 48
                    height: 48
                    palette.highlight: window.primaryColor()
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: playerControls.top
                    anchors.bottomMargin: 18
                    width: Math.min(parent.width - 48, statusText.implicitWidth + 28)
                    height: statusText.implicitHeight + 20
                    radius: 7
                    color: "#d91a2023"
                    visible: window.playerPage && player.statusMessage.length > 0

                    Text {
                        id: statusText
                        anchors.centerIn: parent
                        width: parent.width - 24
                        text: player.statusMessage
                        color: "#f4d4ca"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }
                }

                PlayerControls {
                    id: playerControls
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    visible: window.playerPage && window.controlsVisible
                    engine: player

                    onOpenRequested: window.openFileDialog()
                    onTracksRequested: trackMenu.open()
                    onPlaylistRequested: window.playlistOpen = !window.playlistOpen
                    onSettingsRequested: settingsDialog.open()
                }
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: window.playerPage && window.playlistOpen ? 324 : 0
                visible: Layout.preferredWidth > 0
                color: "#0c1114"
                clip: true

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: "#1d282c"
                }

                PlaylistPanel {
                    anchors.fill: parent
                    engine: player
                }
            }
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: {
            if (drop.hasUrls && drop.urls.length > 0) {
                player.open(drop.urls[0])
                window.playerPage = true
            }
        }
    }

    FileDialog {
        id: openDialog
        title: "Open media"
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "Media files (*.mp4 *.mkv *.webm *.mov *.avi *.m4v *.ts *.m2ts *.mp3 *.flac *.opus *.aac *.wav)",
            "All files (*)"
        ]
        onAccepted: {
            player.open(selectedFile)
            window.playerPage = true
        }
    }

    TrackMenu {
        id: trackMenu
        engine: player
    }

    SettingsDialog {
        id: settingsDialog
        engine: player
    }
}
