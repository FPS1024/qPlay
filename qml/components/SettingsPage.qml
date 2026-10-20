import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var engine: null
    property int selectedCategory: 0
    property var genreBindingRows: []
    property string genreBindingStatus: ""

    readonly property var categories: [
        {
            title: "首页布局",
            description: "选择首页用海报墙展示，或按你绑定的主题分成横向作品栏。"
        },
        {
            title: "类型与主题",
            description: "把 TMDB 原始类型整理成 qPlay 的推荐主题。"
        },
        {
            title: "片库连接",
            description: "配置 qPlay 连接媒体库使用的 WebDAV 信息。"
        },
        {
            title: "播放",
            description: "调整音量、静音状态和播放速度。"
        }
    ]

    function reloadGenreBindings() {
        genreBindingRows = root.engine ? root.engine.genreBindings() : []
        genreBindingStatus = ""
    }

    function saveGenreBinding(binding, displayName) {
        if (!root.engine) return
        const saved = root.engine.setGenreBinding(binding.workType, binding.genreId, displayName)
        if (saved) {
            reloadGenreBindings()
            genreBindingStatus = "类型词条已保存。"
        } else {
            genreBindingStatus = "保存失败，请重试。"
        }
    }

    Component.onCompleted: reloadGenreBindings()
    onVisibleChanged: if (visible) reloadGenreBindings()

    Connections {
        target: root.engine
        function onLibraryChanged() { root.reloadGenreBindings() }
    }

    Rectangle {
        anchors.fill: parent
        color: "#080808"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 48
        anchors.rightMargin: 48
        anchors.topMargin: 34
        anchors.bottomMargin: 42
        spacing: 0

        ColumnLayout {
            Layout.minimumWidth: 180
            Layout.preferredWidth: 180
            Layout.maximumWidth: 180
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignTop
            spacing: 10

            Text {
                text: "设置分类"
                color: "#888888"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                Layout.bottomMargin: 8
            }

            Repeater {
                model: root.categories

                delegate: Button {
                    id: categoryButton
                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    text: modelData.title
                    onClicked: root.selectedCategory = index

                    background: Rectangle {
                        radius: 5
                        color: root.selectedCategory === categoryButton.index
                               ? "#242424" : categoryButton.hovered ? "#171717" : "transparent"
                        border.width: root.selectedCategory === categoryButton.index ? 1 : 0
                        border.color: "#383838"

                        Rectangle {
                            width: 3
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            radius: 2
                            color: "#e50914"
                            visible: root.selectedCategory === categoryButton.index
                        }
                    }

                    contentItem: Text {
                        leftPadding: 14
                        text: categoryButton.text
                        color: root.selectedCategory === categoryButton.index ? "#ffffff" : "#b8b8b8"
                        font.pixelSize: 14
                        font.weight: root.selectedCategory === categoryButton.index
                                     ? Font.DemiBold : Font.Normal
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.leftMargin: 28
            Layout.rightMargin: 34
            width: 1
            color: "#252525"
        }

        ScrollView {
            id: settingsScroll
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                width: settingsScroll.availableWidth
                spacing: 22

                Text {
                    text: root.categories[root.selectedCategory].title
                    color: "#ffffff"
                    font.pixelSize: 30
                    font.weight: Font.Bold
                }

                Text {
                    Layout.fillWidth: true
                    text: root.categories[root.selectedCategory].description
                    color: "#a8a8a8"
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                }

                StackLayout {
                    Layout.fillWidth: true
                    currentIndex: root.selectedCategory

                    ColumnLayout {
                        spacing: 16

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 220
                            radius: 7
                            color: "#111111"
                            border.color: "#292929"

                            ColumnLayout {
                                id: recommendationCard
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                anchors.topMargin: 18
                                spacing: 8

                                Text {
                                    text: "选择首页布局"
                                    color: "#eeeeee"
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "海报墙会随机打散本地作品；主题横向栏会按设置中的 qPlay 词条分组。选择会自动保存。"
                                    color: "#a8a8a8"
                                    font.pixelSize: 13
                                    wrapMode: Text.Wrap
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    Button {
                                        id: posterWallOption
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 92
                                        property bool selectedLayout: root.engine && root.engine.homeLayout === "posterWall"
                                        onClicked: if (root.engine) root.engine.homeLayout = "posterWall"

                                        background: Rectangle {
                                            radius: 6
                                            color: posterWallOption.selectedLayout ? "#252020" : posterWallOption.hovered ? "#1b1b1b" : "#151515"
                                            border.width: 1
                                            border.color: posterWallOption.selectedLayout ? "#e50914" : "#343434"
                                        }

                                        contentItem: Column {
                                            anchors.fill: parent
                                            anchors.margins: 12
                                            spacing: 7

                                            Text {
                                                text: (posterWallOption.selectedLayout ? "●  " : "○  ") + "海报墙"
                                                color: "#f5f5f1"
                                                font.pixelSize: 14
                                                font.weight: Font.DemiBold
                                            }
                                            Text {
                                                width: parent.width
                                                text: "随机排列全部本地作品"
                                                color: "#a8a8a8"
                                                font.pixelSize: 12
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }

                                    Button {
                                        id: genreRowsOption
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 92
                                        property bool selectedLayout: root.engine && root.engine.homeLayout === "genreRows"
                                        onClicked: if (root.engine) root.engine.homeLayout = "genreRows"

                                        background: Rectangle {
                                            radius: 6
                                            color: genreRowsOption.selectedLayout ? "#252020" : genreRowsOption.hovered ? "#1b1b1b" : "#151515"
                                            border.width: 1
                                            border.color: genreRowsOption.selectedLayout ? "#e50914" : "#343434"
                                        }

                                        contentItem: Column {
                                            anchors.fill: parent
                                            anchors.margins: 12
                                            spacing: 7

                                            Text {
                                                text: (genreRowsOption.selectedLayout ? "●  " : "○  ") + "主题横向栏"
                                                color: "#f5f5f1"
                                                font.pixelSize: 14
                                                font.weight: Font.DemiBold
                                            }
                                            Text {
                                                width: parent.width
                                                text: "按 qPlay 词条分组横向浏览"
                                                color: "#a8a8a8"
                                                font.pixelSize: 12
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 16

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 120
                            radius: 7
                            color: "#111111"
                            border.color: "#292929"

                            ColumnLayout {
                                id: mappingCard
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                anchors.topMargin: 18
                                spacing: 8

                                Text {
                                    text: "TMDB 类型与 qPlay 主题"
                                    color: "#eeeeee"
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "每行代表一个 TMDB 原始类型。填写 qPlay 展示词条并保存；同名词条可以把多个原始类型归到一起。留空或点“恢复原词条”即可使用 TMDB 原名。"
                                    color: "#a8a8a8"
                                    font.pixelSize: 13
                                    wrapMode: Text.Wrap
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.genreBindingStatus
                            color: root.genreBindingStatus.indexOf("失败") >= 0 ? "#ff7777" : "#82c791"
                            font.pixelSize: 13
                            visible: root.genreBindingStatus.length > 0
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.genreBindingRows.length === 0
                            text: "媒体库中还没有可绑定的 TMDB 类型。"
                            color: "#a8a8a8"
                            font.pixelSize: 13
                        }

                        Repeater {
                            model: root.genreBindingRows

                            delegate: Rectangle {
                                id: genreBindingRow
                                required property var modelData

                                Layout.fillWidth: true
                                implicitHeight: 88
                                radius: 7
                                color: "#111111"
                                border.color: "#292929"

                                RowLayout {
                                    id: genreBindingLayout
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 14
                                    anchors.topMargin: 14
                                    spacing: 12

                                    ColumnLayout {
                                        Layout.preferredWidth: 160
                                        spacing: 4

                                        Text {
                                            Layout.fillWidth: true
                                            text: genreBindingRow.modelData.workType === "movie" ? "电影类型" : "电视剧类型"
                                            color: "#8f8f8f"
                                            font.pixelSize: 11
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: genreBindingRow.modelData.sourceName
                                            color: "#eeeeee"
                                            font.pixelSize: 14
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: genreBindingRow.modelData.workCount + " 部作品"
                                            color: "#8f8f8f"
                                            font.pixelSize: 11
                                        }
                                    }

                                    TextField {
                                        id: qplayTermField
                                        Layout.fillWidth: true
                                        placeholderText: "qPlay 展示词条"
                                        text: genreBindingRow.modelData.displayName
                                    }

                                    Button {
                                        text: "保存"
                                        onClicked: root.saveGenreBinding(genreBindingRow.modelData, qplayTermField.text)
                                    }

                                    ToolButton {
                                        text: "恢复原词条"
                                        onClicked: root.saveGenreBinding(genreBindingRow.modelData, "")
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 16

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 250
                            radius: 7
                            color: "#111111"
                            border.color: "#292929"

                            ColumnLayout {
                                id: webDavCard
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                anchors.topMargin: 18
                                spacing: 14

                                Text {
                                    text: "WebDAV 媒体库"
                                    color: "#eeeeee"
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                }

                                TextField {
                                    id: webDavUrlField
                                    Layout.fillWidth: true
                                    placeholderText: "WebDAV 地址"
                                    text: root.engine ? root.engine.webDavUrl : ""
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    TextField {
                                        id: webDavUserField
                                        Layout.fillWidth: true
                                        placeholderText: "用户名"
                                        text: root.engine ? root.engine.webDavUsername : ""
                                    }

                                    TextField {
                                        id: webDavPasswordField
                                        Layout.fillWidth: true
                                        placeholderText: "密码"
                                        echoMode: TextInput.Password
                                        text: root.engine ? root.engine.webDavPassword : ""
                                    }
                                }

                                Button {
                                    text: "保存媒体库设置"
                                    onClicked: {
                                        if (!root.engine) return
                                        root.engine.webDavUrl = webDavUrlField.text
                                        root.engine.webDavUsername = webDavUserField.text
                                        root.engine.webDavPassword = webDavPasswordField.text
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 16

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 280
                            radius: 7
                            color: "#111111"
                            border.color: "#292929"

                            ColumnLayout {
                                id: playbackCard
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                anchors.topMargin: 18
                                spacing: 18

                                Text {
                                    text: "音量"
                                    color: "#eeeeee"
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Slider {
                                        id: volumeSlider
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 1
                                        value: root.engine ? root.engine.volume : 1
                                        onMoved: if (root.engine) root.engine.volume = value
                                    }
                                    Text {
                                        Layout.preferredWidth: 48
                                        text: Math.round(volumeSlider.value * 100) + "%"
                                        color: "#d4d4d4"
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }

                                Text {
                                    text: "播放速度"
                                    color: "#eeeeee"
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Slider {
                                        id: speedSlider
                                        Layout.fillWidth: true
                                        from: 0.25
                                        to: 4.0
                                        stepSize: 0.25
                                        value: root.engine ? root.engine.playbackRate : 1
                                        onMoved: if (root.engine) root.engine.playbackRate = value
                                    }
                                    Text {
                                        Layout.preferredWidth: 48
                                        text: speedSlider.value.toFixed(2) + "x"
                                        color: "#d4d4d4"
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }

                                Switch {
                                    text: "静音"
                                    checked: root.engine ? root.engine.muted : false
                                    onToggled: if (root.engine) root.engine.muted = checked
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
