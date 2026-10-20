import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var engine: null
    property var entries: []
    property string filterText: ""
    property string catalogSection: "home"
    property string homeLayout: "genreRows"
    property var continueEntries: []
    property var genreSections: []
    property var featured: null
    property bool featuredInitialized: false
    property int imageCacheRevision: engine ? engine.imageCacheRevision : 0
    readonly property int railControlWidth: 58
    readonly property real railControlHeight: 172 * 1.43
    readonly property real railStep: 172 + 14

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
        initializeFeatured()
        if (catalogSection === "series")
            entries = catalog.filter(function(item) { return item.mediaType === "series" })
        else if (catalogSection === "movies")
            entries = catalog.filter(function(item) { return item.mediaType === "movie" })
        else if (catalogSection === "favorites")
            entries = catalog.filter(function(item) { return item.favorite })
        else
            entries = catalog
        if (catalogSection === "home" && filterText.trim().length === 0
                && homeLayout === "posterWall") {
            entries = shuffledEntries(entries)
        }
        continueEntries = entries.filter(function(item) { return item.positionMs > 0 })
                                   .map(function(item) {
            const continueItem = Object.assign({}, item)
            continueItem.inContinueList = true
            return continueItem
        })
        genreSections = buildGenreSections(entries)
    }

    function mediaWorkKey(item) {
        const workType = item.mediaType === "movie" ? "movie" : "tv"
        const tmdbId = Number(item.tmdbId || 0)
        if (tmdbId > 0)
            return workType + ":" + tmdbId
        const title = (item.seriesTitle || item.title || "").trim().toLocaleLowerCase()
        const year = workType === "movie" ? (item.releaseDate || "").substring(0, 4) : ""
        return workType + ":" + title + ":" + year
    }

    function hasUserActivity(item) {
        return !!item.favorite || Number(item.playCount || 0) > 0
                || Number(item.lastOpenedMs || 0) > 0 || Number(item.positionMs || 0) > 0
    }

    function chooseFeatured(items) {
        if (items.length === 0)
            return null

        const activeWorks = Object.create(null)
        const genreWeights = Object.create(null)
        let hasActivity = false
        for (let i = 0; i < items.length; ++i) {
            const item = items[i]
            if (!hasUserActivity(item))
                continue

            hasActivity = true
            activeWorks[mediaWorkKey(item)] = true
            const weight = (item.favorite ? 3 : 0)
                    + (Number(item.playCount || 0) > 0 || Number(item.lastOpenedMs || 0) > 0
                       || Number(item.positionMs || 0) > 0 ? 1 : 0)
            const countedGenres = Object.create(null)
            const genres = item.genres || []
            for (let j = 0; j < genres.length; ++j) {
                const genreName = (genres[j].name || genres[j].sourceName || "").trim()
                if (!genreName)
                    continue
                const genreKey = "$" + genreName.toLocaleLowerCase()
                if (countedGenres[genreKey])
                    continue
                countedGenres[genreKey] = true
                genreWeights[genreKey] = (genreWeights[genreKey] || 0) + weight
            }
        }

        let pool = []
        if (hasActivity) {
            for (let i = 0; i < items.length; ++i) {
                const item = items[i]
                if (activeWorks[mediaWorkKey(item)])
                    continue
                let score = 0
                const countedGenres = Object.create(null)
                const genres = item.genres || []
                for (let j = 0; j < genres.length; ++j) {
                    const genreName = (genres[j].name || genres[j].sourceName || "").trim()
                    if (!genreName)
                        continue
                    const genreKey = "$" + genreName.toLocaleLowerCase()
                    if (countedGenres[genreKey])
                        continue
                    countedGenres[genreKey] = true
                    score += genreWeights[genreKey] || 0
                }
                if (score > 0)
                    pool.push({ item: item, weight: score })
            }
            if (pool.length === 0) {
                pool = items.filter(function(item) { return !activeWorks[mediaWorkKey(item)] })
                            .map(function(item) { return { item: item, weight: 1 } })
            }
        } else {
            pool = items.map(function(item) { return { item: item, weight: 1 } })
        }

        if (pool.length === 0)
            pool = items.map(function(item) { return { item: item, weight: 1 } })

        // Keep the hero changing between launches, including while category preferences are active.
        if (engine && pool.length > 1) {
            const previousKey = engine.lastFeaturedKey
            const alternatives = pool.filter(function(candidate) {
                return mediaWorkKey(candidate.item) !== previousKey
            })
            if (alternatives.length > 0)
                pool = alternatives
            else if (hasActivity) {
                const unseenAlternatives = items.filter(function(item) {
                    return !activeWorks[mediaWorkKey(item)] && mediaWorkKey(item) !== previousKey
                }).map(function(item) { return { item: item, weight: 1 } })
                if (unseenAlternatives.length > 0)
                    pool = unseenAlternatives
            }
        }

        const totalWeight = pool.reduce(function(total, candidate) { return total + candidate.weight }, 0)
        let randomWeight = Math.random() * totalWeight
        for (let i = 0; i < pool.length; ++i) {
            randomWeight -= pool[i].weight
            if (randomWeight < 0)
                return pool[i].item
        }
        return pool[pool.length - 1].item
    }

    function initializeFeatured() {
        if (featuredInitialized || !engine)
            return
        const catalog = engine.libraryMedia()
        if (!catalog || catalog.length === 0)
            return
        featured = chooseFeatured(catalog)
        if (!featured)
            return
        featuredInitialized = true
        engine.lastFeaturedKey = mediaWorkKey(featured)
    }

    function shuffledEntries(items) {
        const result = items.slice()
        for (let i = result.length - 1; i > 0; --i) {
            const j = Math.floor(Math.random() * (i + 1))
            const temporary = result[i]
            result[i] = result[j]
            result[j] = temporary
        }
        return result
    }

    function buildGenreSections(items) {
        if (catalogSection !== "home" || homeLayout !== "genreRows" || filterText.trim().length > 0)
            return []

        const groups = Object.create(null)
        for (let i = 0; i < items.length; ++i) {
            const item = items[i]
            const workType = item.mediaType === "movie" ? "movie" : "tv"
            const tmdbId = Number(item.tmdbId || 0)
            const fallbackTitle = (item.seriesTitle || item.title || "").trim().toLocaleLowerCase()
            const workKey = workType + ":" + (tmdbId > 0 ? tmdbId : fallbackTitle)
            const genres = item.genres || []

            for (let j = 0; j < genres.length; ++j) {
                const genre = genres[j]
                const title = (genre.name || genre.sourceName || "").trim()
                if (!title)
                    continue

                const groupKey = "$" + title.toLocaleLowerCase()
                if (!groups[groupKey]) {
                    groups[groupKey] = { title: title, items: [], seen: Object.create(null) }
                }
                if (!groups[groupKey].seen[workKey]) {
                    groups[groupKey].seen[workKey] = true
                    groups[groupKey].items.push(item)
                }
            }
        }

        const sections = Object.keys(groups).map(function(key) { return groups[key] })
                                             .filter(function(section) { return section.items.length >= 2 })
        sections.forEach(function(section) {
            section.items.sort(function(a, b) {
                const ratingDifference = Number(b.rating || 0) - Number(a.rating || 0)
                if (ratingDifference !== 0)
                    return ratingDifference
                const playDifference = Number(b.playCount || 0) - Number(a.playCount || 0)
                if (playDifference !== 0)
                    return playDifference
                return (a.seriesTitle || a.title || "").localeCompare(b.seriesTitle || b.title || "", "zh-CN")
            })
        })
        sections.sort(function(a, b) {
            if (a.items.length !== b.items.length)
                return b.items.length - a.items.length
            return a.title.localeCompare(b.title, "zh-CN")
        })
        return sections
    }

    Component.onCompleted: reload()

    Connections {
        target: root.engine
        function onLibraryChanged() { root.reload() }
        function onHomeLayoutChanged() { root.reload() }
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

            NumberAnimation {
                id: verticalWheelScroll
                target: pageFlickable
                property: "contentY"
                duration: 120
                easing.type: Easing.OutCubic
            }

            WheelHandler {
                target: null
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: function(event) {
                    const delta = event.pixelDelta.y !== 0
                            ? event.pixelDelta.y * 1.5
                            : event.angleDelta.y / 120 * 64
                    const maxY = Math.max(0, pageFlickable.contentHeight - pageFlickable.height)
                    const pendingY = verticalWheelScroll.running
                            ? verticalWheelScroll.to : pageFlickable.contentY
                    verticalWheelScroll.stop()
                    verticalWheelScroll.from = pageFlickable.contentY
                    verticalWheelScroll.to = Math.max(0, Math.min(maxY, pendingY - delta))
                    verticalWheelScroll.start()
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

                        Item {
                            width: parent.width
                            height: 326

                            ListView {
                                id: continueList
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.leftMargin: root.railControlWidth
                                anchors.rightMargin: root.railControlWidth
                                orientation: ListView.Horizontal
                                spacing: 14
                                clip: true
                                model: root.continueEntries
                                delegate: posterCard
                                boundsBehavior: Flickable.StopAtBounds
                            }

                            NumberAnimation {
                                id: continueRailScroll
                                target: continueList
                                property: "contentX"
                                duration: 320
                                easing.type: Easing.OutCubic
                            }

                            ToolButton {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                width: root.railControlWidth
                                height: root.railControlHeight
                                z: 2
                                visible: continueList.contentWidth > continueList.width + 1
                                         && continueList.contentX > 1
                                enabled: visible
                                text: "‹"
                                onClicked: {
                                    continueRailScroll.stop()
                                    continueRailScroll.from = continueList.contentX
                                    continueRailScroll.to = Math.max(0, continueList.contentX - root.railStep)
                                    continueRailScroll.start()
                                }
                                contentItem: Text {
                                    text: "‹"
                                    color: "#ffffff"
                                    font.pixelSize: 46
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 5
                                    color: parent.hovered ? "#dd242424" : "#bb111111"
                                }
                            }

                            ToolButton {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                width: root.railControlWidth
                                height: root.railControlHeight
                                z: 2
                                visible: continueList.contentWidth > continueList.width + 1
                                         && continueList.contentX < continueList.contentWidth - continueList.width - 1
                                enabled: continueList.contentX < continueList.contentWidth - continueList.width - 1
                                text: "›"
                                onClicked: {
                                    continueRailScroll.stop()
                                    continueRailScroll.from = continueList.contentX
                                    continueRailScroll.to = Math.min(continueList.contentWidth - continueList.width,
                                                                     continueList.contentX + root.railStep)
                                    continueRailScroll.start()
                                }
                                contentItem: Text {
                                    text: "›"
                                    color: "#ffffff"
                                    font.pixelSize: 46
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 5
                                    color: parent.hovered ? "#dd242424" : "#bb111111"
                                }
                            }
                        }
                    }

                    Repeater {
                        model: root.genreSections

                        delegate: Column {
                            required property var modelData
                            width: sectionStack.width
                            spacing: 14

                            Text {
                                text: modelData.title
                                color: "#f5f5f1"
                                font.pixelSize: 21
                                font.weight: Font.DemiBold
                            }

                            Item {
                                width: parent.width
                                height: 326

                                ListView {
                                    id: genreList
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.leftMargin: root.railControlWidth
                                    anchors.rightMargin: root.railControlWidth
                                    orientation: ListView.Horizontal
                                    spacing: 14
                                    clip: true
                                    model: modelData.items
                                    delegate: posterCard
                                    boundsBehavior: Flickable.StopAtBounds
                                }

                                NumberAnimation {
                                    id: genreRailScroll
                                    target: genreList
                                    property: "contentX"
                                    duration: 320
                                    easing.type: Easing.OutCubic
                                }

                                ToolButton {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    width: root.railControlWidth
                                    height: root.railControlHeight
                                    z: 2
                                    visible: genreList.contentWidth > genreList.width + 1
                                             && genreList.contentX > 1
                                    enabled: visible
                                    text: "‹"
                                    onClicked: {
                                        genreRailScroll.stop()
                                        genreRailScroll.from = genreList.contentX
                                        genreRailScroll.to = Math.max(0, genreList.contentX - root.railStep)
                                        genreRailScroll.start()
                                    }
                                    contentItem: Text {
                                        text: "‹"
                                        color: "#ffffff"
                                        font.pixelSize: 46
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 5
                                        color: parent.hovered ? "#dd242424" : "#bb111111"
                                    }
                                }

                                ToolButton {
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    width: root.railControlWidth
                                    height: root.railControlHeight
                                    z: 2
                                    visible: genreList.contentWidth > genreList.width + 1
                                             && genreList.contentX < genreList.contentWidth - genreList.width - 1
                                    enabled: genreList.contentX < genreList.contentWidth - genreList.width - 1
                                    text: "›"
                                    onClicked: {
                                        genreRailScroll.stop()
                                        genreRailScroll.from = genreList.contentX
                                        genreRailScroll.to = Math.min(genreList.contentWidth - genreList.width,
                                                                      genreList.contentX + root.railStep)
                                        genreRailScroll.start()
                                    }
                                    contentItem: Text {
                                        text: "›"
                                        color: "#ffffff"
                                        font.pixelSize: 46
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 5
                                        color: parent.hovered ? "#dd242424" : "#bb111111"
                                    }
                                }
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 14
                        visible: root.entries.length > 0
                                 && (root.catalogSection !== "home"
                                     || root.filterText.trim().length > 0
                                     || root.homeLayout === "posterWall")

                        Text {
                            visible: root.catalogSection !== "home" || root.filterText.trim().length > 0
                            text: root.filterText.trim().length > 0 ? "搜索结果"
                                  : root.catalogSection === "series" ? "电视剧"
                                  : root.catalogSection === "movies" ? "电影"
                                  : root.catalogSection === "favorites" ? "我的收藏"
                                  : ""
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
                                 || (root.catalogSection === "home"
                                     && root.homeLayout === "genreRows"
                                     && root.filterText.trim().length === 0
                                     && root.genreSections.length === 0)
                        Text {
                            anchors.centerIn: parent
                            text: root.entries.length > 0 ? "还没有足够作品组成主题栏目"
                                  : root.filterText.length > 0 ? "没有找到相关影片"
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
            width: ListView.view ? 172 : (GridView.view ? GridView.view.cellWidth - 12 : 172)
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
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton && modelData.inContinueList) {
                                watchedMenu.popup()
                            } else if (mouse.button === Qt.LeftButton) {
                                root.detailsRequested(modelData)
                            }
                        }

                        Menu {
                            id: watchedMenu
                            MenuItem {
                                text: "标记为已观看"
                                onTriggered: {
                                    if (root.engine) root.engine.markWatched(modelData.source)
                                }
                            }
                        }
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
                    text: modelData.rating > 0
                          ? "★ " + Number(modelData.rating).toFixed(1)
                            + (modelData.releaseDate ? "  ·  " + modelData.releaseDate.substring(0, 4) : "")
                          : (modelData.releaseDate || modelData.formatName || "影片")
                    color: "#929292"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }
    }
}
