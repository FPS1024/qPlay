import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var engine: null
    property var entries: []

    function reload() {
        entries = engine ? engine.playlist : []
    }

    Component.onCompleted: reload()

    Connections {
        target: root.engine
        function onPlaylistChanged() {
            root.reload()
        }
        function onTitleChanged() {
            root.reload()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#0c1114"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            Layout.leftMargin: 16
            Layout.rightMargin: 12

            Text {
                text: "PLAYLIST"
                color: "#e5efed"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.letterSpacing: 1.2
            }

            Text {
                text: root.entries.length
                color: "#708487"
                font.pixelSize: 11
            }

            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                text: "\u21BB"
                font.pixelSize: 16
                Accessible.name: "Refresh playlist"
                onClicked: root.reload()

                background: Rectangle {
                    radius: 6
                    color: parent.down ? "#263438" : parent.hovered ? "#1a2529" : "transparent"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            height: 1
            color: "#1d282c"
        }

        Text {
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            visible: root.entries.length === 0
            text: "The playlist is empty."
            color: "#728689"
            font.pixelSize: 12
        }

        ListView {
            id: playlist
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 8
            clip: true
            model: root.entries
            spacing: 2

            delegate: Rectangle {
                id: queueItem
                required property var modelData
                required property int index

                width: playlist.width
                height: 52
                color: root.engine && index === root.engine.playlistIndex
                       ? "#18302b"
                       : queueMouse.containsMouse ? "#172024" : "transparent"

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    visible: root.engine && index === root.engine.playlistIndex
                    color: "#3dd6b5"
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.right: removeButton.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: (index + 1) + "  " + modelData.title
                    color: root.engine && index === root.engine.playlistIndex ? "#e9f7f3" : "#c9d6d3"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                ToolButton {
                    id: removeButton
                    anchors.right: parent.right
                    anchors.rightMargin: 7
                    anchors.verticalCenter: parent.verticalCenter
                    width: 30
                    height: 30
                    text: "\u00D7"
                    font.pixelSize: 15
                    visible: queueMouse.containsMouse
                    Accessible.name: "Remove playlist item"
                    onClicked: root.engine.removePlaylistItem(index)

                    background: Rectangle {
                        radius: 5
                        color: parent.down ? "#354347" : parent.hovered ? "#243035" : "transparent"
                    }
                }

                MouseArea {
                    id: queueMouse
                    anchors.fill: parent
                    anchors.rightMargin: 36
                    hoverEnabled: true
                    onClicked: root.engine.playPlaylistItem(index)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 8

            Button {
                Layout.fillWidth: true
                text: "Previous"
                onClicked: root.engine.previous()

                contentItem: Text {
                    text: parent.text
                    color: "#aebfbd"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    implicitHeight: 34
                    radius: 6
                    color: parent.down ? "#263438" : parent.hovered ? "#1a2529" : "#11181b"
                    border.color: "#263438"
                }
            }

            Button {
                Layout.fillWidth: true
                text: "Next"
                onClicked: root.engine.next()

                contentItem: Text {
                    text: parent.text
                    color: "#aebfbd"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    implicitHeight: 34
                    radius: 6
                    color: parent.down ? "#263438" : parent.hovered ? "#1a2529" : "#11181b"
                    border.color: "#263438"
                }
            }

            ToolButton {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 34
                text: "\u2327"
                font.pixelSize: 15
                Accessible.name: "Clear playlist"
                ToolTip.visible: hovered
                ToolTip.text: "Clear playlist"
                onClicked: root.engine.clearPlaylist()

                background: Rectangle {
                    radius: 6
                    color: parent.down ? "#3a2929" : parent.hovered ? "#302020" : "#11181b"
                    border.color: "#3a2929"
                }
            }
        }
    }
}
