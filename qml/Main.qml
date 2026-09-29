import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QuarkTV 1.0

import "components"

ApplicationWindow {
    id: window

    width: 1360
    height: 820
    minimumWidth: 960
    minimumHeight: 620
    visible: true
    color: "#080808"
    title: player.title.length > 0 && page === 2 ? player.title + " - qPlay" : "qPlay"

    property int page: 0 // 0: browse, 1: details, 2: playback
    property bool controlsVisible: true
    property var selectedMedia: ({})
    property string playbackHeaderTitle: ""

    function playSelected(playbackMedia) {
        const target = playbackMedia || selectedMedia
        if (!target || !target.source)
            return

        let displayTitle = target.title || ""
        if (target.mediaType === "episode") {
            const show = target.seriesTitle || target.title || ""
            let episode = target.episodeTitle || ""
            if (!episode && target.seasonNumber !== undefined && target.episodeNumber !== undefined) {
                const seasonNumber = Number(target.seasonNumber)
                const episodeNumber = Number(target.episodeNumber)
                episode = "S" + (seasonNumber < 10 ? "0" : "") + seasonNumber
                          + "E" + (episodeNumber < 10 ? "0" : "") + episodeNumber
            }
            displayTitle = show && episode ? show + " · " + episode : (episode || show)
        }
        window.playbackHeaderTitle = displayTitle

        if (player.source.toString() === target.source.toString()) {
            if (player.playbackState !== PlaybackState.Playing)
                player.play()
        } else {
            player.openWithTitle(target.source, displayTitle)
        }
        page = 2
        controlsVisible = true
    }

    function goBack() {
        if (page === 2) {
            player.pause()
            page = 1
        } else if (page === 1) {
            page = 0
        }
    }

    function toggleFullscreen() {
        if (visibility === Window.FullScreen) {
            visibility = Window.Windowed
            controlsVisible = true
        } else {
            visibility = Window.FullScreen
            if (page === 2)
                controlsVisible = false
        }
    }

    function bitrateText(rate) {
        if (rate <= 0) return "未知"
        return rate >= 1000000 ? (rate / 1000000).toFixed(2) + " Mbps"
                               : Math.round(rate / 1000) + " kbps"
    }

    Connections {
        target: player
        function onSourceChanged() {
            if (player.source && player.source.toString().length > 0) {
                window.page = 2
                window.playbackHeaderTitle = player.title
            }
        }
    }

    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        enabled: window.page === 2
        onActivated: player.togglePlayback()
    }

    Shortcut {
        sequence: "Left"
        context: Qt.ApplicationShortcut
        enabled: window.page === 2
        onActivated: player.seekRelative(-5000)
    }

    Shortcut {
        sequence: "Right"
        context: Qt.ApplicationShortcut
        enabled: window.page === 2
        onActivated: player.seekRelative(5000)
    }

    Shortcut {
        sequence: "Up"
        context: Qt.ApplicationShortcut
        enabled: window.page === 2
        onActivated: {
            player.muted = false
            player.volume = Math.min(2.0, player.volume + 0.05)
        }
    }

    Shortcut {
        sequence: "Down"
        context: Qt.ApplicationShortcut
        enabled: window.page === 2
        onActivated: {
            player.muted = false
            player.volume = Math.max(0.0, player.volume - 0.05)
        }
    }

    Shortcut {
        sequence: "M"
        context: Qt.ApplicationShortcut
        enabled: window.page === 2
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
        onActivated: {
            if (window.visibility === Window.FullScreen)
                window.toggleFullscreen()
            else if (window.page > 0)
                window.goBack()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#080808"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            visible: !(window.page === 2 && window.visibility === Window.FullScreen)
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            color: "#0b0b0b"
            z: 2

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 42
                anchors.rightMargin: 42
                spacing: 28

                Button {
                    id: backButton
                    visible: window.page > 0
                    text: "‹  返回"
                    onClicked: window.goBack()
                    background: Item {}
                    contentItem: Text {
                        text: backButton.text
                        color: "#e5e5e5"
                        font.pixelSize: 15
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Text {
                    text: "qPlay"
                    color: "#e50914"
                    font.pixelSize: 27
                    font.weight: Font.Black
                    font.letterSpacing: 0.4
                }

                Text {
                    visible: window.page === 0
                    text: "首页"
                    color: "#f5f5f1"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }

                Item { Layout.fillWidth: true }

                TextField {
                    id: searchField
                    visible: window.page === 0
                    Layout.preferredWidth: 250
                    Layout.preferredHeight: 38
                    placeholderText: "搜索影片"
                    color: "#f5f5f1"
                    placeholderTextColor: "#8a8a8a"
                    leftPadding: 38
                    selectByMouse: true
                    onTextChanged: homeView.filterText = text

                    background: Rectangle {
                        radius: 4
                        color: "#171717"
                        border.color: searchField.activeFocus ? "#777777" : "#343434"
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: "⌕"
                        color: "#a7a7a7"
                        font.pixelSize: 22
                    }
                }

                Button {
                    id: settingsButton
                    text: "⚙"
                    Accessible.name: "设置"
                    onClicked: settingsDialog.open()
                    background: Item {}
                    contentItem: Text {
                        text: settingsButton.text
                        color: settingsButton.hovered ? "#ffffff" : "#bdbdbd"
                        font.pixelSize: 21
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            PosterWall {
                id: homeView
                anchors.fill: parent
                visible: window.page === 0
                engine: player
                onDetailsRequested: function(media) {
                    window.selectedMedia = media
                    window.page = 1
                }
            }

            TitleDetails {
                anchors.fill: parent
                visible: window.page === 1
                media: window.selectedMedia
                engine: player
                onBackRequested: window.page = 0
                onPlayRequested: function(media) { window.playSelected(media) }
            }

            Item {
                id: playbackPage
                anchors.fill: parent
                visible: window.page === 2

                MpvVideoItem {
                    anchors.fill: parent
                    session: player.renderSession
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: window.controlsVisible = !window.controlsVisible
                    onDoubleClicked: window.toggleFullscreen()
                }

                ToolButton {
                    id: playbackInfoButton
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 16
                    anchors.rightMargin: 22
                    z: 4
                    text: "ⓘ 视频信息"
                    hoverEnabled: true
                    contentItem: Text {
                        text: playbackInfoButton.text
                        color: playbackInfoButton.hovered ? "#ffffff" : "#eeeeee"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 5
                        color: playbackInfoButton.hovered ? "#dd242424" : "#aa111111"
                        border.color: playbackInfoButton.hovered ? "#e50914" : "#66555555"
                    }
                    ToolTip.visible: hovered
                    ToolTip.delay: 0
                    ToolTip.text: "当前视频流\n"
                                   + "编码：" + (player.videoCodec || "未知") + "\n"
                                   + "分辨率：" + (player.videoResolution || "未知") + "\n"
                                   + "视频码率：" + window.bitrateText(player.videoBitrate)
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 66
                    visible: window.controlsVisible && player.title.length > 0
                    color: "#99000000"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 28
                        anchors.rightMargin: 28

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                Layout.fillWidth: true
                                text: window.playbackHeaderTitle || player.title
                                color: "#f5f5f1"
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: player.formatName.toUpperCase()
                                color: "#aaa"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                        }

                        Text {
                            visible: player.buffering
                            text: "正在缓冲…"
                            color: "#e50914"
                            font.pixelSize: 12
                        }
                    }
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: player.playbackState === PlaybackState.Opening || player.buffering
                    visible: running
                    width: 48
                    height: 48
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: playerControls.top
                    anchors.bottomMargin: 18
                    width: Math.min(parent.width - 48, statusText.implicitWidth + 28)
                    height: statusText.implicitHeight + 20
                    radius: 5
                    color: "#d91a2023"
                    visible: player.statusMessage.length > 0
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
                    visible: window.controlsVisible
                    engine: player
                }
            }
        }
    }

    SettingsDialog {
        id: settingsDialog
        engine: player
    }

}
