import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Menu {
    id: root

    property var engine: null
    width: 300
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Text {
        text: "Audio"
        color: "#708487"
        font.pixelSize: 10
        font.letterSpacing: 1
        leftPadding: 14
        topPadding: 8
        bottomPadding: 4
    }

    MenuItem {
        text: "No audio tracks"
        enabled: false
        visible: !root.engine || root.engine.audioTracks.length === 0
    }

    Instantiator {
        model: root.engine ? root.engine.audioTracks : []
        delegate: MenuItem {
            required property var modelData

            text: {
                const title = modelData.title || modelData.language || modelData.codec || "Audio"
                const details = modelData.codec ? "  (" + modelData.codec + ")" : ""
                return title + details
            }
            checkable: true
            checked: root.engine
                     && modelData.streamIndex === root.engine.currentAudioTrack
            onTriggered: {
                root.engine.selectAudioTrack(modelData.streamIndex)
                root.close()
            }
        }
    }

    MenuSeparator {}

    Text {
        text: "Subtitles"
        color: "#708487"
        font.pixelSize: 10
        font.letterSpacing: 1
        leftPadding: 14
        topPadding: 8
        bottomPadding: 4
    }

    MenuItem {
        text: "Off"
        checkable: true
        checked: root.engine && root.engine.currentSubtitleTrack < 0
        onTriggered: {
            root.engine.selectSubtitleTrack(-1)
            root.close()
        }
    }

    Instantiator {
        model: root.engine ? root.engine.subtitleTracks : []
        delegate: MenuItem {
            required property var modelData

            text: {
                const title = modelData.title || modelData.language || modelData.codec || "Subtitle"
                const forced = modelData.isForced ? "  [forced]" : ""
                return title + forced
            }
            checkable: true
            checked: root.engine
                     && modelData.streamIndex === root.engine.currentSubtitleTrack
            onTriggered: {
                root.engine.selectSubtitleTrack(modelData.streamIndex)
                root.close()
            }
        }
    }
}
