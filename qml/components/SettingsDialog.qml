import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property var engine: null

    modal: true
    focus: true
    width: Math.min(520, Overlay.overlay ? Overlay.overlay.width - 40 : 520)
    height: 650
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    title: "Playback settings"

    contentItem: ColumnLayout {
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 7
            Text {
                text: "WEBDAV MEDIA LIBRARY"
                color: "#708487"
                font.pixelSize: 10
                font.letterSpacing: 1
            }
            TextField {
                id: webDavUrlField
                Layout.fillWidth: true
                placeholderText: "WebDAV collection URL"
                text: root.engine ? root.engine.webDavUrl : ""
            }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: webDavUserField
                    Layout.fillWidth: true
                    placeholderText: "Username"
                    text: root.engine ? root.engine.webDavUsername : ""
                }
                TextField {
                    id: webDavPasswordField
                    Layout.fillWidth: true
                    placeholderText: "Password"
                    echoMode: TextInput.Password
                    text: root.engine ? root.engine.webDavPassword : ""
                }
            }
            Button {
                text: "Save WebDAV settings"
                onClicked: {
                    if (!root.engine) return
                    root.engine.webDavUrl = webDavUrlField.text
                    root.engine.webDavUsername = webDavUserField.text
                    root.engine.webDavPassword = webDavPasswordField.text
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "VOLUME"
                color: "#708487"
                font.pixelSize: 10
                font.letterSpacing: 1
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Slider {
                    id: volumeSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    value: root.engine ? root.engine.volume : 1
                    onMoved: root.engine.volume = value
                }

                Text {
                    Layout.preferredWidth: 44
                    text: Math.round(volumeSlider.value * 100) + "%"
                    color: "#dce8e5"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "PLAYBACK SPEED"
                color: "#708487"
                font.pixelSize: 10
                font.letterSpacing: 1
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Slider {
                    id: speedSlider
                    Layout.fillWidth: true
                    from: 0.25
                    to: 4.0
                    stepSize: 0.25
                    value: root.engine ? root.engine.playbackRate : 1
                    onMoved: root.engine.playbackRate = value
                }

                Text {
                    Layout.preferredWidth: 44
                    text: speedSlider.value.toFixed(2) + "x"
                    color: "#dce8e5"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        Switch {
            Layout.fillWidth: true
            text: "Muted"
            checked: root.engine ? root.engine.muted : false
            onToggled: root.engine.muted = checked
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            radius: 7
            color: "#11181b"
            border.color: "#263438"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 3

                Text {
                    text: "VIDEO PROCESSING"
                    color: "#708487"
                    font.pixelSize: 10
                    font.letterSpacing: 1
                }

                Text {
                    Layout.fillWidth: true
                    text: root.engine ? root.engine.videoBackend : ""
                    color: "#c5d3d0"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: "Close"
                onClicked: root.close()

                contentItem: Text {
                    text: parent.text
                    color: "#111111"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    implicitWidth: 88
                    implicitHeight: 34
                    radius: 6
                    color: parent.down ? "#bd0710" : parent.hovered ? "#f6121d" : "#e50914"
                }
            }
        }
    }

    background: Rectangle {
        radius: 8
        color: "#11181b"
        border.color: "#2b383d"
        border.width: 1
    }
}
