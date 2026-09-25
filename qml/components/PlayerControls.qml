import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QuarkTV 1.0

Item {
    id: root

    property var engine: null
    signal openRequested()
    signal tracksRequested()
    signal playlistRequested()
    signal settingsRequested()

    implicitHeight: 96

    function isPlaying() {
        return engine
                && (engine.playbackState === PlaybackState.Playing
                    || engine.playbackState === PlaybackState.Buffering)
    }

    function positionText() {
        return engine ? engine.formatTime(engine.position) : "0:00"
    }

    function durationText() {
        return engine ? engine.formatTime(engine.duration) : "0:00"
    }

    Rectangle {
        anchors.fill: parent
        color: "#ed05080a"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        anchors.topMargin: 8
        anchors.bottomMargin: 10
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                Layout.preferredWidth: 48
                text: root.positionText()
                color: "#d7e2df"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
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
                    height: 3
                    radius: 2
                    color: "#334247"

                    Rectangle {
                        width: progress.visualPosition * parent.width
                        height: parent.height
                        radius: parent.radius
                        color: "#3dd6b5"
                    }
                }

                handle: Rectangle {
                    x: progress.leftPadding + progress.visualPosition * (progress.availableWidth - width)
                    y: progress.topPadding + progress.availableHeight / 2 - height / 2
                    width: progress.pressed || progress.hovered ? 15 : 12
                    height: width
                    radius: width / 2
                    color: "#f0f7f5"
                    border.color: "#3dd6b5"
                    border.width: 2
                }
            }

            Text {
                Layout.preferredWidth: 48
                text: root.durationText()
                color: "#93a5a7"
                font.pixelSize: 11
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 6

            ToolButton {
                text: "\uE000"
                visible: false
            }

            ToolButton {
                text: "\u2263"
                font.pixelSize: 18
                Accessible.name: "Playlist"
                ToolTip.visible: hovered
                ToolTip.text: "Playlist"
                onClicked: root.playlistRequested()

                background: Rectangle {
                    radius: 6
                    color: parent.down ? "#263438" : parent.hovered ? "#1a2529" : "transparent"
                }
            }

            ToolButton {
                text: "\u00AB"
                font.pixelSize: 22
                Accessible.name: "Previous"
                onClicked: root.engine.previous()

                background: Rectangle {
                    radius: 6
                    color: parent.down ? "#263438" : parent.hovered ? "#1a2529" : "transparent"
                }
            }

            ToolButton {
                text: "\u2212"
                font.pixelSize: 22
                Accessible.name: "Seek backward five seconds"
                onClicked: root.engine.seekRelative(-5000)

                background: Rectangle {
                    radius: 6
                    color: parent.down ? "#263438" : parent.hovered ? "#1a2529" : "transparent"
                }
            }

            RoundButton {
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                radius: 21
                text: root.isPlaying() ? "\u2161" : "\u25B6"
                font.pixelSize: root.isPlaying() ? 20 : 18
                Accessible.name: root.isPlaying() ? "Pause" : "Play"
                onClicked: root.engine.togglePlayback()

                background: Rectangle {
                    radius: parent.radius
                    color: parent.down ? "#32bea1" : parent.hovered ? "#4be3c1" : "#3dd6b5"
                }

                contentItem: Text {
                    text: parent.text
                    color: "#07110f"
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            ToolButton {
                text: "+"
                font.pixelSize: 20
                Accessible.name: "Seek forward five seconds"
                onClicked: root.engine.seekRelative(5000)

                background: Rectangle {
                    radius: 6
                    color: parent.down ? "#263438" : parent.hovered ? "#1a2529" : "transparent"
                }
            }

            ToolButton {
                text: "\u00BB"
                font.pixelSize: 22
                Accessible.name: "Next"
                onClicked: root.engine.next()

                background: Rectangle {
                    radius: 6
                    color: parent.down ? "#263438" : parent.hovered ? "#1a2529" : "transparent"
                }
            }

            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                text: "\u25A3"
                font.pixelSize: 17
                Accessible.name: "Tracks"
                ToolTip.visible: hovered
                ToolTip.text: "Audio and subtitle tracks"
                onClicked: root.tracksRequested()

                background: Rectangle {
                    radius: 6
                    color: parent.down ? "#263438" : parent.hovered ? "#1a2529" : "transparent"
                }
            }

            ToolButton {
                text: "\u266B"
                font.pixelSize: 18
                Accessible.name: root.engine && root.engine.muted ? "Unmute" : "Mute"
                onClicked: root.engine.toggleMute()

                background: Rectangle {
                    radius: 6
                    color: parent.down ? "#263438" : parent.hovered ? "#1a2529" : "transparent"
                }
            }

            Slider {
                id: volume
                Layout.preferredWidth: 112
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
                    color: "#334247"

                    Rectangle {
                        width: volume.visualPosition * parent.width
                        height: parent.height
                        color: "#b8c7c5"
                    }
                }

                handle: Rectangle {
                    x: volume.leftPadding + volume.visualPosition * (volume.availableWidth - width)
                    y: volume.topPadding + volume.availableHeight / 2 - height / 2
                    width: 11
                    height: 11
                    radius: 6
                    color: "#edf5f3"
                }
            }

            ComboBox {
                Layout.preferredWidth: 88
                model: ["0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x"]
                currentIndex: {
                    if (!root.engine) {
                        return 2
                    }
                    return Math.max(0, model.indexOf(root.engine.playbackRate.toFixed(2).replace(/0$/, "") + "x"))
                }
                onActivated: {
                    const rates = [0.5, 0.75, 1.0, 1.25, 1.5, 2.0]
                    root.engine.playbackRate = rates[index]
                }
            }

            ToolButton {
                text: "\u2699"
                font.pixelSize: 18
                Accessible.name: "Settings"
                onClicked: root.settingsRequested()

                background: Rectangle {
                    radius: 6
                    color: parent.down ? "#263438" : parent.hovered ? "#1a2529" : "transparent"
                }
            }
        }
    }
}
