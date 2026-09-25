#pragma once

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

namespace QuarkTV {

Q_NAMESPACE

enum class PlaybackState {
    Stopped,
    Opening,
    Buffering,
    Playing,
    Paused,
    Ended,
    Error,
};
Q_ENUM_NS(PlaybackState)

enum class TrackType {
    Video,
    Audio,
    Subtitle,
    Attachment,
    Unknown,
};
Q_ENUM_NS(TrackType)

struct TrackInfo {
    int streamIndex = -1;
    TrackType type = TrackType::Unknown;
    QString codec;
    QString language;
    QString title;
    QString disposition;
    qint64 bitrate = 0;
    int width = 0;
    int height = 0;
    int channelCount = 0;
    int sampleRate = 0;
    bool isDefault = false;
    bool isForced = false;

    [[nodiscard]] QVariantMap toVariantMap() const
    {
        return {
            {QStringLiteral("streamIndex"), streamIndex},
            {QStringLiteral("type"), static_cast<int>(type)},
            {QStringLiteral("codec"), codec},
            {QStringLiteral("language"), language},
            {QStringLiteral("title"), title},
            {QStringLiteral("disposition"), disposition},
            {QStringLiteral("bitrate"), bitrate},
            {QStringLiteral("width"), width},
            {QStringLiteral("height"), height},
            {QStringLiteral("channelCount"), channelCount},
            {QStringLiteral("sampleRate"), sampleRate},
            {QStringLiteral("isDefault"), isDefault},
            {QStringLiteral("isForced"), isForced},
        };
    }
};

struct MediaInfo {
    QUrl source;
    QString title;
    QString formatName;
    QString formatLongName;
    QString metadataTitle;
    qint64 durationMs = 0;
    qint64 bitrate = 0;
    int videoStream = -1;
    int audioStream = -1;
    int subtitleStream = -1;
    QVector<TrackInfo> tracks;
};

} // namespace QuarkTV

Q_DECLARE_METATYPE(QuarkTV::PlaybackState)
Q_DECLARE_METATYPE(QuarkTV::TrackType)
Q_DECLARE_METATYPE(QuarkTV::TrackInfo)
Q_DECLARE_METATYPE(QuarkTV::MediaInfo)
