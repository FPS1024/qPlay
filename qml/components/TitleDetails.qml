import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var media: ({})
    property var engine: null
    property int imageCacheRevision: engine ? engine.imageCacheRevision : 0

    signal backRequested()
    signal playRequested()

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
        opacity: 0.28
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#080808" }
            GradientStop { position: 0.55; color: "#c9080808" }
            GradientStop { position: 1.0; color: "#f2080808" }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#44080808" }
            GradientStop { position: 1.0; color: "#080808" }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Math.max(42, parent.width * 0.075)
        anchors.rightMargin: Math.max(42, parent.width * 0.075)
        anchors.topMargin: 44
        anchors.bottomMargin: 54
        spacing: Math.max(36, parent.width * 0.055)

        Rectangle {
            id: detailPoster
            property real posterWidth: Math.min(310, parent.width * 0.28)
            Layout.preferredWidth: posterWidth
            Layout.preferredHeight: posterWidth * 1.47
            Layout.alignment: Qt.AlignVCenter
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

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 18

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
                    text: root.media && root.media.rating > 0 ? "★ " + Number(root.media.rating).toFixed(1) : ""
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
                Layout.fillWidth: true
                Layout.maximumWidth: 680
                text: root.media && root.media.overview
                      ? root.media.overview : "暂无剧情简介。"
                color: "#dddddd"
                font.pixelSize: 15
                lineHeight: 1.45
                wrapMode: Text.Wrap
                maximumLineCount: 6
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.topMargin: 8
                spacing: 12

                Button {
                    id: playButton
                    text: root.media && root.media.positionMs > 0 ? "▶  继续播放" : "▶  播放"
                    onClicked: root.playRequested()
                    contentItem: Text {
                        text: playButton.text
                        color: "#111111"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        implicitWidth: 158
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
}
