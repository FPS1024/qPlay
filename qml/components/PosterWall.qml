import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var engine: null
    property var entries: []
    property string filterText: ""
    property string catalogSection: "home"
    property var continueEntries: []
    property var featured: entries.length > 0 ? entries[0] : null
    property int imageCacheRevision: engine ? engine.imageCacheRevision : 0

    function imageSource(url, kind) {
        const revision = imageCacheRevision
        return engine && url ? engine.cachedImageSource(url, kind) : ""
    }

    signal detailsRequested(var media)

    onFilterTextChanged: reload()
    onCatalogSectionChanged: {
        reload()
        pageFlickable.contentY = 0
    }

    function reload() {
        const catalog = engine ? engine.libraryMedia(filterText) : []
        if (catalogSection === "series")
            entries = catalog.filter(function(item) { return item.mediaType === "series" })
        else if (catalogSection === "movies")
            entries = catalog.filter(function(item) { return item.mediaType === "movie" })
        else if (catalogSection === "favorites")
            entries = catalog.filter(function(item) { return item.favorite })
        else
            entries = catalog
        continueEntries = entries.filter(function(item) { return item.positionMs > 0 })
    }

    Component.onCompleted: reload()

    Connections {
        target: root.engine
        function onLibraryChanged() { root.reload() }
    }

    Rectangle {
        anchors.fill: parent
        color: "#080808"
    }

    ScrollView {
        id: pageScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        Flickable {
            id: pageFlickable
            anchors.fill: parent
            clip: true
            contentWidth: width
            contentHeight: content.implicitHeight
            maximumFlickVelocity: 5000
            boundsBehavior: Flickable.StopAtBounds

            WheelHandler {
                target: null
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: function(event) {
                    const delta = event.pixelDelta.y !== 0
                            ? event.pixelDelta.y
                            : event.angleDelta.y / 8
                    const maxY = Math.max(0, pageFlickable.contentHeight - pageFlickable.height)
                    pageFlickable.contentY = Math.max(0, Math.min(maxY,
                                                                   pageFlickable.contentY - delta * 2))
                    event.accepted = true
                }
            }

            Column {
                id: content
                width: pageFlickable.width
                spacing: 0

            Item {
                width: content.width
                height: Math.min(520, Math.max(390, width * 0.39))
                clip: true

                Image {
                    anchors.fill: parent
                    source: root.featured
                            ? root.imageSource(root.featured.backdropUrl || root.featured.posterUrl,
                                               root.featured.backdropUrl ? "background" : "poster")
                            : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: true
                    opacity: 0.42
                }

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 0.75; color: "transparent" }
                        GradientStop { position: 0.9; color: "#b3080808" }
                        GradientStop { position: 1.0; color: "#f2080808" }
                    }
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Math.max(36, content.width * 0.055)
                    anchors.rightMargin: 40
                    anchors.bottomMargin: 52
                    width: Math.min(620, content.width * 0.60)
                    spacing: 14

                    Text {
                        text: root.featured && root.featured.mediaType === "series" ? "剧集精选" : "影片精选"
                        color: "#e50914"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        font.letterSpacing: 1.5
                        visible: root.featured !== null
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.featured
                              ? (root.featured.seriesTitle || root.featured.title)
                              : "精彩影片，即刻开启"
                        color: "#ffffff"
                        font.pixelSize: Math.min(52, Math.max(34, content.width * 0.043))
                        font.weight: Font.Black
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: root.featured !== null
                        text: root.featured
                              ? (root.featured.overview.length > 0
                                 ? root.featured.overview
                                 : root.featured.releaseDate)
                              : "在下方浏览影片，打开详情后即可播放。"
                        color: "#e5e5e5"
                        font.pixelSize: 15
                        lineHeight: 1.35
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }

                    Button {
                        id: featuredDetailsButton
                        visible: root.featured !== null
                        text: "ⓘ  影片详情"
                        onClicked: root.detailsRequested(root.featured)
                        contentItem: Text {
                            text: featuredDetailsButton.text
                            color: "#111"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitWidth: 148
                            implicitHeight: 42
                            radius: 4
                            color: featuredDetailsButton.down ? "#d6d6d6" : featuredDetailsButton.hovered ? "#ffffff" : "#eeeeee"
                        }
                    }
                }
            }

            Item {
                width: content.width
                height: sectionStack.implicitHeight + 64

                Column {
                    id: sectionStack
                    anchors.fill: parent
                    anchors.leftMargin: Math.max(28, content.width * 0.045)
                    anchors.rightMargin: Math.max(28, content.width * 0.045)
                    anchors.topMargin: 4
                    anchors.bottomMargin: 48
                    spacing: 40

                    Column {
                        width: parent.width
                        spacing: 14
                        visible: root.continueEntries.length > 0

                        Text {
                            text: "继续观看"
                            color: "#f5f5f1"
                            font.pixelSize: 21
                            font.weight: Font.DemiBold
                        }

                        ListView {
                            width: parent.width
                            height: 326
                            orientation: ListView.Horizontal
                            spacing: 14
                            clip: true
                            model: root.continueEntries
                            delegate: posterCard
                            boundsBehavior: Flickable.StopAtBounds
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 14
                        visible: root.entries.length > 0

                        Text {
                            text: root.filterText.length > 0 ? "搜索结果"
                                  : root.catalogSection === "series" ? "电视剧"
                                  : root.catalogSection === "movies" ? "电影"
                                  : root.catalogSection === "favorites" ? "我的收藏"
                                  : "为你推荐"
                            color: "#f5f5f1"
                            font.pixelSize: 21
                            font.weight: Font.DemiBold
                        }

                        GridView {
                            id: allTitles
                            width: parent.width
                            height: Math.max(280, contentHeight)
                            cellWidth: Math.max(154, Math.min(198, Math.floor(width / Math.max(2, Math.floor(width / 180)))))
                            cellHeight: 334
                            interactive: false
                            model: root.entries
                            delegate: posterCard
                        }
                    }

                    Item {
                        width: parent.width
                        height: 180
                        visible: root.entries.length === 0
                        Text {
                            anchors.centerIn: parent
                            text: root.filterText.length > 0 ? "没有找到相关影片"
                                  : root.catalogSection === "favorites" ? "还没有收藏的影视"
                                  : root.catalogSection === "series" ? "还没有识别到电视剧"
                                  : root.catalogSection === "movies" ? "还没有识别到电影"
                                  : "影片内容将在这里呈现"
                            color: "#929292"
                            font.pixelSize: 16
                        }
                    }
                }
            }
            }
        }
    }

    Component {
        id: posterCard

        Item {
            required property var modelData
            width: allTitles.cellWidth - 12
            height: width * 1.43 + 50

            Column {
                anchors.fill: parent
                spacing: 8

                Rectangle {
                    id: posterFrame
                    width: parent.width
                    height: parent.width * 1.43
                    radius: 4
                    color: "#202020"
                    clip: true

                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 24
                        text: modelData.title || "qPlay"
                        color: "#dddddd"
                        font.pixelSize: 19
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                        visible: posterImage.status !== Image.Ready
                    }

                    Image {
                        id: posterImage
                        anchors.fill: parent
                        source: root.imageSource(modelData.posterUrl || "", "poster")
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: true
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: cardMouse.containsMouse ? "#26000000" : "transparent"
                        border.width: cardMouse.containsMouse ? 2 : 0
                        border.color: "#eeeeee"
                    }

                    MouseArea {
                        id: cardMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.detailsRequested(modelData)
                    }
                }

                Text {
                    width: parent.width
                    text: modelData.seriesTitle || modelData.title
                    color: "#e5e5e5"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: modelData.mediaType === "episode"
                          ? "S" + (modelData.seasonNumber < 10 ? "0" : "") + modelData.seasonNumber
                            + "E" + (modelData.episodeNumber < 10 ? "0" : "") + modelData.episodeNumber
                            + (modelData.episodeTitle ? "  ·  " + modelData.episodeTitle : "")
                          : modelData.mediaType === "series"
                            ? modelData.episodeCount + " 集"
                          : (modelData.rating > 0
                             ? "★ " + Number(modelData.rating).toFixed(1)
                               + (modelData.releaseDate ? "  ·  " + modelData.releaseDate.substring(0, 4) : "")
                             : (modelData.releaseDate || modelData.formatName || "影片"))
                    color: "#929292"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }
    }
}
