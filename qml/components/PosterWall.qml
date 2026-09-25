import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var engine: null
    property var entries: []
    property string filterText: ""
    signal playRequested(url source)

    function reload() {
        entries = engine ? engine.libraryMedia(filterText) : []
        if (engine) {
            for (let i = 0; i < entries.length; ++i) {
                if (!entries[i].posterUrl)
                    engine.enrichWithTmdb(entries[i].source, entries[i].title)
            }
        }
    }

    Component.onCompleted: reload()

    Connections {
        target: root.engine
        function onLibraryChanged() { root.reload() }
    }

    Rectangle {
        anchors.fill: parent
        color: "#080b0d"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 34
        anchors.rightMargin: 34
        anchors.topMargin: 28
        anchors.bottomMargin: 26
        spacing: 20

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    text: "YOUR LIBRARY"
                    color: "#f2f6f5"
                    font.pixelSize: 25
                    font.weight: Font.DemiBold
                }
                Text {
                    text: root.entries.length + " titles"
                    color: "#819193"
                    font.pixelSize: 12
                }
            }

            TextField {
                Layout.preferredWidth: 260
                placeholderText: "Search your library"
                color: "#e7efed"
                placeholderTextColor: "#718184"
                selectByMouse: true
                onTextChanged: {
                    root.filterText = text
                    root.reload()
                }
                background: Rectangle {
                    radius: 7
                    color: "#11181b"
                    border.color: parent.activeFocus ? "#3dd6b5" : "#263236"
                }
            }

            Button {
                text: "Scan a folder"
                onClicked: root.engine.chooseAndScanFolder()
                contentItem: Text {
                    text: parent.text
                    color: "#06110e"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: parent.down ? "#2eb59a" : parent.hovered ? "#4be3c1" : "#3dd6b5"
                }
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: 174
            cellHeight: 274
            model: root.entries
            visible: root.entries.length > 0

            delegate: Item {
                required property var modelData
                width: grid.cellWidth
                height: grid.cellHeight

                Column {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.leftMargin: 9
                    anchors.rightMargin: 9
                    spacing: 9

                    Rectangle {
                        width: parent.width
                        height: width * 1.48
                        radius: 8
                        clip: true
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#1d413e" }
                            GradientStop { position: 0.52; color: "#172a30" }
                            GradientStop { position: 1.0; color: "#11181d" }
                        }

                        Text {
                            anchors.centerIn: parent
                            width: parent.width - 24
                            text: modelData.title.length > 0 ? modelData.title : "?"
                            color: "#d7e7e2"
                            opacity: 0.88
                            font.pixelSize: 22
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            maximumLineCount: 4
                            elide: Text.ElideRight
                            visible: !modelData.posterUrl
                        }

                        Image {
                            anchors.fill: parent
                            source: modelData.posterUrl
                            visible: status === Image.Ready
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 3
                            visible: modelData.positionMs > 0 && modelData.durationMs > 0
                            color: "#263538"
                            Rectangle {
                                width: parent.width * Math.min(1, modelData.positionMs / modelData.durationMs)
                                height: parent.height
                                color: "#3dd6b5"
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            color: cardMouse.containsMouse ? "#18000000" : "transparent"
                            border.width: cardMouse.containsMouse ? 1 : 0
                            border.color: "#57dfc0"
                        }

                        MouseArea {
                            id: cardMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.playRequested(modelData.source)
                        }
                    }

                    Text {
                        width: parent.width
                        text: modelData.title
                        color: "#dce7e4"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: modelData.durationMs > 0
                              ? root.engine.formatTime(modelData.durationMs)
                              : modelData.formatName
                        color: "#748589"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
            }

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.entries.length === 0
            Text {
                anchors.centerIn: parent
                width: Math.min(420, parent.width)
                text: "Add a folder containing your movies to build your library."
                color: "#748589"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
