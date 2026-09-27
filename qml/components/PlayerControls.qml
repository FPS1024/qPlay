import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QuarkTV 1.0

Item {
    id: root

    property var engine: null
    signal tracksRequested()

    implicitHeight: 100

    function isPlaying() {
        return engine && (engine.playbackState === PlaybackState.Playing
                          || engine.playbackState === PlaybackState.Buffering)
    }

    Rectangle {
        anchors.fill: parent
        color: "#e6000000"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 30
        anchors.rightMargin: 30
        anchors.topMargin: 7
        anchors.bottomMargin: 10
        spacing: 1

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: root.engine ? root.engine.formatTime(root.engine.position) : "0:00"
                color: "#eeeeee"
                font.pixelSize: 11
            }

            Slider {
                id: progress
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, root.engine ? root.engine.duration : 0)
                value: root.engine ? Math.min(root.engine.position, to) : 0
                enabled: root.engine && root.engine.duration > 0
                onMoved: root.engine.seek(value)

                background: Rectangle {
                    x: progress.leftPadding
                    y: progress.topPadding + progress.availableHeight / 2 - height / 2
                    width: progress.availableWidth
                    height: progress.hovered || progress.pressed ? 5 : 3
                    radius: 3
                    color: "#666666"
                    Rectangle {
                        width: progress.visualPosition * parent.width
                        height: parent.height
                        radius: parent.radius
                        color: "#e50914"
                    }
                }

                handle: Rectangle {
                    x: progress.leftPadding + progress.visualPosition * (progress.availableWidth - width)
                    y: progress.topPadding + progress.availableHeight / 2 - height / 2
                    width: progress.pressed || progress.hovered ? 14 : 0
                    height: width
                    radius: width / 2
                    color: "#ffffff"
                }
            }

            Text {
                text: root.engine ? root.engine.formatTime(root.engine.duration) : "0:00"
                color: "#bbbbbb"
                font.pixelSize: 11
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Item { Layout.fillWidth: true }

            ToolButton {
                id: seekBackButton
                text: "−10"
                Accessible.name: "快退十秒"
                onClicked: root.engine.seekRelative(-10000)
                contentItem: Text {
                    text: seekBackButton.text
                    color: seekBackButton.hovered ? "#ffffff" : "#dddddd"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
            }

            RoundButton {
                id: playPauseButton
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                radius: 21
                text: root.isPlaying() ? "Ⅱ" : "▶"
                Accessible.name: root.isPlaying() ? "暂停" : "播放"
                onClicked: root.engine.togglePlayback()
                contentItem: Text {
                    text: playPauseButton.text
                    color: "#ffffff"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: playPauseButton.radius
                    color: playPauseButton.down ? "#bd0710" : playPauseButton.hovered ? "#f6121d" : "#e50914"
                }
            }

            ToolButton {
                id: seekForwardButton
                text: "+10"
                Accessible.name: "快进十秒"
                onClicked: root.engine.seekRelative(10000)
                contentItem: Text {
                    text: seekForwardButton.text
                    color: seekForwardButton.hovered ? "#ffffff" : "#dddddd"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                id: tracksButton
                text: "字幕 / 音轨"
                Accessible.name: "字幕和音轨"
                onClicked: root.tracksRequested()
                contentItem: Text {
                    text: tracksButton.text
                    color: tracksButton.hovered ? "#ffffff" : "#dddddd"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
            }

            ToolButton {
                id: muteButton
                text: root.engine && root.engine.muted ? "静音" : "音量"
                Accessible.name: root.engine && root.engine.muted ? "取消静音" : "静音"
                onClicked: root.engine.toggleMute()
                contentItem: Text {
                    text: muteButton.text
                    color: muteButton.hovered ? "#ffffff" : "#dddddd"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
            }

            Slider {
                id: volume
                Layout.preferredWidth: 92
                from: 0
                to: 1
                value: root.engine ? root.engine.volume : 1
                onMoved: root.engine.volume = value
                background: Rectangle {
                    x: volume.leftPadding
                    y: volume.topPadding + volume.availableHeight / 2 - height / 2
                    width: volume.availableWidth
                    height: 3
                    radius: 2
                    color: "#666666"
                    Rectangle {
                        width: volume.visualPosition * parent.width
                        height: parent.height
                        color: "#dddddd"
                    }
                }
                handle: Rectangle {
                    x: volume.leftPadding + volume.visualPosition * (volume.availableWidth - width)
                    y: volume.topPadding + volume.availableHeight / 2 - height / 2
                    width: 10
                    height: 10
                    radius: 5
                    color: "#ffffff"
                }
            }
        }
    }
}
