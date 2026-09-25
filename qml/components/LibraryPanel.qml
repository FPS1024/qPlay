import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var engine: null
    property var entries: []

    function reload() {
        entries = engine ? engine.recentMedia(100) : []
    }

    function durationText(milliseconds) {
        return engine ? engine.formatTime(milliseconds) : "0:00"
    }

    Component.onCompleted: reload()

    Connections {
        target: root.engine
        function onLibraryChanged() {
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
                text: "LIBRARY"
                color: "#e5efed"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.letterSpacing: 1.2
            }

            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                text: "\u21BB"
                font.pixelSize: 16
                Accessible.name: "Refresh media library"
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
            text: "Files you open will appear here."
            color: "#728689"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        ListView {
            id: mediaList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 8
            clip: true
            model: root.entries
            spacing: 2

            delegate: Rectangle {
                id: mediaItem
                required property var modelData
                required property int index

                width: mediaList.width
                height: 66
                color: itemMouse.containsMouse ? "#172024" : "transparent"

                Rectangle {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: parent.color === "transparent" ? 0 : 34
                    radius: 2
                    color: "#3dd6b5"
                }

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.right: removeButton.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3

                    Text {
                        width: parent.width
                        text: modelData.title
                        color: "#dce8e5"
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: root.durationText(modelData.durationMs)
                              + (modelData.positionMs > 0
                                 ? "  /  " + root.durationText(modelData.positionMs)
                                 : "")
                        color: "#718588"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }

                ToolButton {
                    id: removeButton
                    anchors.right: parent.right
                    anchors.rightMargin: 7
                    anchors.verticalCenter: parent.verticalCenter
                    width: 30
                    height: 30
                    text: "\u00D7"
                    font.pixelSize: 16
                    visible: itemMouse.containsMouse
                    Accessible.name: "Remove from recent media"
                    onClicked: root.engine.removeRecent(modelData.source)

                    background: Rectangle {
                        radius: 5
                        color: parent.down ? "#354347" : parent.hovered ? "#243035" : "transparent"
                    }
                }

                MouseArea {
                    id: itemMouse
                    anchors.fill: parent
                    anchors.rightMargin: 36
                    hoverEnabled: true
                    onClicked: root.engine.open(modelData.source)
                }
            }
        }

        Button {
            Layout.fillWidth: true
            Layout.margins: 12
            text: "Clear history"
            visible: root.entries.length > 0
            onClicked: root.engine.clearRecent()

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
                border.width: 1
            }
        }
    }
}
