import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var media: ({})
    property var engine: null
    property int imageCacheRevision: engine ? engine.imageCacheRevision : 0
    property var selectedEpisode: null
    property var selectedSeason: null
    property int episodePageIndex: 0
    property real contentSideMargin: Math.max(36, width * 0.075)
    readonly property bool isSeries: media && media.mediaType === "series"

    readonly property var episodePages: buildEpisodePages()
    readonly property var visibleEpisodes: selectedSeason && selectedSeason.episodes
                                          ? selectedSeason.episodes.slice(episodePageIndex * 10,
                                                                         episodePageIndex * 10 + 10)
                                          : []

    signal backRequested()
    signal playRequested(var media)

    function buildEpisodePages() {
        const episodes = selectedSeason && selectedSeason.episodes ? selectedSeason.episodes : []
        const pages = []
        for (let start = 0; start < episodes.length; start += 10) {
            const end = Math.min(start + 10, episodes.length)
            pages.push({
                "start": start,
                "end": end,
                "label": (start + 1) + "–" + end + " 集"
            })
        }
        return pages
    }

    function episodeForSeason(season) {
        if (!season || !season.episodes || season.episodes.length === 0)
            return null
        const resume = root.media && root.media.resumeEpisode
        if (resume && Object.keys(resume).length > 0
                && resume.seasonNumber === season.seasonNumber) {
            for (let i = 0; i < season.episodes.length; ++i) {
                if (season.episodes[i].source.toString() === resume.source.toString())
                    return season.episodes[i]
            }
        }
        return season.episodes[0]
    }

    function selectSeason(season) {
        selectedSeason = season
        episodePageIndex = 0
        selectedEpisode = episodeForSeason(season)
    }

    onMediaChanged: {
        selectedSeason = null
        selectedEpisode = null
        episodePageIndex = 0
        if (!media || !media.seasons || media.seasons.length === 0)
            return

        // Start with season one even if a later season has a saved resume point.
        selectedSeason = media.seasons[0]
        for (let i = 0; i < media.seasons.length; ++i) {
            if (media.seasons[i].seasonNumber === 1) {
                selectedSeason = media.seasons[i]
                break
            }
        }
        selectedEpisode = episodeForSeason(selectedSeason)
    }

    Rectangle {
        anchors.fill: parent
        color: "#080808"
    }

    Image {
        anchors.fill: parent
        source: {
            const revision = root.imageCacheRevision
            const url = root.media && (root.media.backdropUrl || root.media.posterUrl)
            return root.engine && url
                    ? root.engine.cachedImageSource(url, root.media.backdropUrl ? "background" : "poster")
                    : ""
        }
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        opacity: 0.44
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#55080808" }
            GradientStop { position: 0.55; color: "#88080808" }
            GradientStop { position: 1.0; color: "#bb080808" }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.75; color: "transparent" }
            GradientStop { position: 0.9; color: "#99080808" }
            GradientStop { position: 1.0; color: "#e6080808" }
        }
    }

    Flickable {
        id: detailsScroll
        anchors.fill: parent
        contentWidth: width
        contentHeight: pageContent.implicitHeight + 44
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: pageContent
            width: detailsScroll.width
            spacing: 30

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: root.contentSideMargin
                Layout.rightMargin: root.contentSideMargin
                Layout.topMargin: 38
                spacing: Math.max(28, root.width * 0.04)

                ColumnLayout {
                    Layout.preferredWidth: Math.min(root.isSeries ? 170 : 290,
                                                    (root.width - 2 * root.contentSideMargin)
                                                    * (root.isSeries ? 0.20 : 0.29))
                    Layout.alignment: Qt.AlignTop
                    spacing: 12

                    Rectangle {
                        id: detailPoster
                        Layout.fillWidth: true
                        Layout.preferredHeight: width * 1.47
                        radius: 5
                        color: "#1e1e1e"
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: {
                                const revision = root.imageCacheRevision
                                return root.engine && root.media && root.media.posterUrl
                                        ? root.engine.cachedImageSource(root.media.posterUrl, "poster") : ""
                            }
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                        }

                        Text {
                            anchors.centerIn: parent
                            width: parent.width - 30
                            text: root.media && root.media.title ? root.media.title : "qPlay"
                            color: "#e5e5e5"
                            font.pixelSize: 22
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            visible: !root.media || !root.media.posterUrl
                        }
                    }

                    ComboBox {
                        id: seasonPicker
                        visible: root.media && root.media.mediaType === "series"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        model: root.media && root.media.seasons ? root.media.seasons : []
                        textRole: "displayName"
                        currentIndex: {
                            if (!root.selectedSeason || !root.media || !root.media.seasons) return 0
                            for (let i = 0; i < root.media.seasons.length; ++i) {
                                if (root.media.seasons[i].seasonNumber === root.selectedSeason.seasonNumber)
                                    return i
                            }
                            return 0
                        }
                        onActivated: function(index) {
                            root.selectSeason(root.media.seasons[index])
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 16

                    Text {
                        text: "影片详情"
                        color: "#e50914"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        font.letterSpacing: 1.4
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.media && (root.media.seriesTitle || root.media.title)
                              ? (root.media.seriesTitle || root.media.title) : "未知影片"
                        color: "#ffffff"
                        font.pixelSize: Math.min(48, Math.max(30, root.width * 0.04))
                        font.weight: Font.Black
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        spacing: 12
                        Text {
                            visible: root.media && root.media.rating > 0
                            text: root.media && root.media.rating > 0
                                  ? "★ " + Number(root.media.rating).toFixed(1) : ""
                            color: "#f5c518"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: root.media && root.media.releaseDate
                                  ? root.media.releaseDate.substring(0, 4) : ""
                            color: "#d4d4d4"
                            font.pixelSize: 14
                        }
                        Text {
                            visible: root.media && root.media.durationMs > 0
                            text: root.media && root.media.durationMs > 0 && root.engine
                                  ? root.engine.formatTime(root.media.durationMs) : ""
                            color: "#d4d4d4"
                            font.pixelSize: 14
                        }
                        Text {
                            visible: root.media && root.media.width > 0
                            text: root.media && root.media.width > 0
                                  ? root.media.width + " × " + root.media.height : ""
                            color: "#d4d4d4"
                            font.pixelSize: 14
                        }
                    }

                    Text {
                        visible: root.media && root.media.mediaType === "episode"
                        text: root.media && root.media.mediaType === "episode"
                              ? (root.media.seriesTitle || root.media.title) + "  ·  S"
                                + (root.media.seasonNumber < 10 ? "0" : "") + root.media.seasonNumber + "E"
                                + (root.media.episodeNumber < 10 ? "0" : "") + root.media.episodeNumber
                                + (root.media.episodeTitle ? "  ·  " + root.media.episodeTitle : "")
                              : ""
                        color: "#d4d4d4"
                        font.pixelSize: 14
                        wrapMode: Text.Wrap
                    }

                    Text {
                        visible: root.media && root.media.mediaType === "series"
                        text: root.media && root.media.episodeCount > 0
                              ? "剧集 · " + root.media.seasonCount + " 季 · " + root.media.episodeCount + " 集"
                              : "电视剧"
                        color: "#d4d4d4"
                        font.pixelSize: 14
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 720
                        text: root.media && root.media.mediaType !== "series" && root.media.overview
                              ? root.media.overview
                              : (root.media && root.media.mediaType === "series"
                                 ? "选择下方剧集查看单集简介。" : "暂无剧情简介。")
                        color: "#dddddd"
                        font.pixelSize: 15
                        lineHeight: 1.45
                        wrapMode: Text.Wrap
                        maximumLineCount: 5
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.topMargin: 4
                        spacing: 12

                        Button {
                            id: playButton
                            visible: root.media && root.media.mediaType !== "series"
                                     || root.selectedEpisode !== null
                            text: root.media && root.media.mediaType === "series"
                                  ? (root.selectedEpisode && root.selectedEpisode.positionMs > 0
                                     ? "▶  继续播放第 " + root.selectedEpisode.episodeNumber + " 集"
                                     : "▶  播放当前集")
                                  : (root.media && root.media.positionMs > 0 ? "▶  继续播放" : "▶  播放")
                            onClicked: root.playRequested(root.selectedEpisode || root.media)
                            contentItem: Text {
                                text: playButton.text
                                color: "#111111"
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                implicitWidth: 178
                                implicitHeight: 46
                                radius: 4
                                color: playButton.down ? "#d6d6d6" : playButton.hovered ? "#ffffff" : "#eeeeee"
                            }
                        }

                        Button {
                            id: favoriteButton
                            text: root.media && root.media.favorite ? "✓  已收藏" : "+  收藏"
                            onClicked: {
                                if (root.engine && root.media) {
                                    const nextFavorite = !root.media.favorite
                                    root.engine.setFavorite(root.media.source, nextFavorite)
                                    const updatedMedia = Object.assign({}, root.media)
                                    updatedMedia.favorite = nextFavorite
                                    root.media = updatedMedia
                                }
                            }
                            contentItem: Text {
                                text: favoriteButton.text
                                color: "#ffffff"
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                implicitWidth: 144
                                implicitHeight: 46
                                radius: 4
                                color: favoriteButton.down ? "#333333" : favoriteButton.hovered ? "#292929" : "#202020"
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                visible: root.media && root.media.mediaType === "series"
                Layout.fillWidth: true
                Layout.leftMargin: root.contentSideMargin
                Layout.rightMargin: root.contentSideMargin
                Layout.bottomMargin: 38
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            text: "分集"
                            color: "#ffffff"
                            font.pixelSize: 22
                            font.weight: Font.Bold
                        }
                        Text {
                            text: root.selectedSeason
                                  ? root.selectedSeason.displayName + " · "
                                    + root.selectedSeason.episodes.length + " 集"
                                  : "暂无剧集"
                            color: "#bdbdbd"
                            font.pixelSize: 13
                        }
                    }

                    ToolButton {
                        text: "‹"
                        Accessible.name: "上一组剧集"
                        enabled: root.episodePageIndex > 0
                        onClicked: root.episodePageIndex--
                    }

                    ComboBox {
                        id: episodeRangePicker
                        Layout.preferredWidth: 150
                        model: root.episodePages
                        textRole: "label"
                        currentIndex: root.episodePageIndex
                        enabled: model.length > 0
                        onActivated: function(index) { root.episodePageIndex = index }
                    }

                    ToolButton {
                        text: "›"
                        Accessible.name: "下一组剧集"
                        enabled: root.episodePageIndex < root.episodePages.length - 1
                        onClicked: root.episodePageIndex++
                    }
                }

                GridLayout {
                    id: episodeGrid
                    Layout.fillWidth: true
                    columns: Math.max(1, Math.min(5,
                        Math.floor((root.width - 2 * root.contentSideMargin + 14) / 194)))
                    rowSpacing: 14
                    columnSpacing: 14

                    Repeater {
                        model: root.visibleEpisodes
                        delegate: Rectangle {
                            id: episodeCard
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: width * 0.5625 + 112
                            radius: 5
                            color: episodeMouse.containsMouse ? "#303030" : "#1b1b1b"
                            border.width: episodeMouse.containsMouse ? 1 : 0
                            border.color: "#666666"
                            clip: true

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 0

                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: episodeCard.width * 0.5625
                                    clip: true

                                    Image {
                                        id: episodeStill
                                        anchors.fill: parent
                                        source: {
                                            const revision = root.imageCacheRevision
                                            return root.engine && episodeCard.modelData.stillUrl
                                                    ? root.engine.cachedImageSource(episodeCard.modelData.stillUrl,
                                                                                   "episode") : ""
                                        }
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        cache: true
                                    }

                                    Rectangle {
                                        anchors.fill: parent
                                        color: "#292929"
                                        visible: episodeStill.status !== Image.Ready
                                        Text {
                                            anchors.centerIn: parent
                                            text: episodeStill.status === Image.Loading
                                                  ? "正在载入剧照…" : "暂无剧照"
                                            color: "#888888"
                                            font.pixelSize: 13
                                        }
                                    }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.bottom: parent.bottom
                                        anchors.margins: 8
                                        radius: 3
                                        color: "#cc000000"
                                        implicitWidth: episodeNumberLabel.implicitWidth + 14
                                        implicitHeight: episodeNumberLabel.implicitHeight + 8
                                        Text {
                                            id: episodeNumberLabel
                                            anchors.centerIn: parent
                                            text: "第 " + episodeCard.modelData.episodeNumber + " 集"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 10
                                    Layout.rightMargin: 10
                                    Layout.topMargin: 8
                                    text: episodeCard.modelData.episodeTitle
                                          ? episodeCard.modelData.episodeTitle
                                          : "第 " + episodeCard.modelData.episodeNumber + " 集"
                                    color: "#ffffff"
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 10
                                    Layout.rightMargin: 10
                                    Layout.topMargin: 4
                                    Layout.bottomMargin: 9
                                    text: episodeCard.modelData.overview || "暂无单集简介。"
                                    color: "#bdbdbd"
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 3
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignTop
                                }
                            }

                            MouseArea {
                                id: episodeMouse
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.selectedEpisode = episodeCard.modelData
                                    root.playRequested(episodeCard.modelData)
                                }
                            }

                            Accessible.role: Accessible.Button
                            Accessible.name: "播放第 " + modelData.episodeNumber + " 集 "
                                             + (modelData.episodeTitle || "")
                        }
                    }
                }

                Text {
                    visible: root.selectedSeason && root.selectedSeason.episodes.length === 0
                    text: "本季暂无可播放剧集。"
                    color: "#bdbdbd"
                    font.pixelSize: 14
                }
            }
        }
    }
}
