#include "mpv_session.h"

#include <QFileInfo>
#include <algorithm>
#include <clocale>
#include <cstdint>

namespace QuarkTV::Mpv {
namespace {

QString errorText(int code)
{
    return QString::fromUtf8(mpv_error_string(code));
}

QString valueString(const mpv_node *node, const char *key)
{
    if (!node || node->format != MPV_FORMAT_NODE_MAP) {
        return {};
    }
    for (int i = 0; i < node->u.list->num; ++i) {
        const char *name = node->u.list->keys[i];
        if (name && qstrcmp(name, key) == 0) {
            const mpv_node &value = node->u.list->values[i];
            if (value.format == MPV_FORMAT_STRING && value.u.string) {
                return QString::fromUtf8(value.u.string);
            }
        }
    }
    return {};
}

int valueInt(const mpv_node *node, const char *key, int fallback = -1)
{
    if (!node || node->format != MPV_FORMAT_NODE_MAP) {
        return fallback;
    }
    for (int i = 0; i < node->u.list->num; ++i) {
        const char *name = node->u.list->keys[i];
        if (name && qstrcmp(name, key) == 0) {
            const mpv_node &value = node->u.list->values[i];
            if (value.format == MPV_FORMAT_INT64) {
                return static_cast<int>(value.u.int64);
            }
        }
    }
    return fallback;
}

bool valueBool(const mpv_node *node, const char *key)
{
    if (!node || node->format != MPV_FORMAT_NODE_MAP) {
        return false;
    }
    for (int i = 0; i < node->u.list->num; ++i) {
        const char *name = node->u.list->keys[i];
        if (name && qstrcmp(name, key) == 0) {
            const mpv_node &value = node->u.list->values[i];
            return value.format == MPV_FORMAT_FLAG && value.u.flag != 0;
        }
    }
    return false;
}

} // namespace

MpvSession::MpvSession(QObject *parent, const QMap<QString, QString> &overrides)
    : QObject(parent)
{
    // libmpv rejects creation when LC_NUMERIC uses a locale with decimal
    // commas. Keep the process numeric locale in C for mpv's lifetime.
    if (!std::setlocale(LC_NUMERIC, "C")) {
        initializationError_ = tr("Could not set LC_NUMERIC to C before creating libmpv.");
        qWarning().noquote() << initializationError_;
        emit errorOccurred(initializationError_);
        return;
    }

    handle_ = mpv_create();
    if (!handle_) {
        initializationError_ = tr("mpv_create() could not create the libmpv player.");
        qWarning().noquote() << initializationError_;
        emit errorOccurred(initializationError_);
        return;
    }

    const auto option = [this](const char *name, const char *value) {
        const int result = mpv_set_option_string(handle_, name, value);
        if (result < 0) {
            emit warningOccurred(tr("mpv option %1 was rejected: %2")
                                     .arg(QString::fromLatin1(name), errorText(result)));
        }
    };
    option("vo", "libmpv");
    option("terminal", "no");
    option("input-default-bindings", "no");
    option("osc", "no");
    option("keep-open", "no");
    // Avoid probing CUDA on machines where the NVIDIA runtime/driver is not
    // available. Software decoding is the reliable baseline; hardware decode
    // can be enabled later when a usable VAAPI/NVDEC device is detected.
    option("hwdec", "no");
    option("sub-auto", "fuzzy");
    option("audio-file-auto", "fuzzy");
    option("volume-max", "200");
    option("force-window", "no");
    option("pause", "yes");
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it)
        option(it.key().toUtf8().constData(), it.value().toUtf8().constData());

    const int result = mpv_initialize(handle_);
    if (result < 0) {
        initializationError_ = tr("mpv_initialize() failed: %1").arg(errorText(result));
        qWarning().noquote() << initializationError_;
        emit errorOccurred(initializationError_);
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
        return;
    }

    mpv_observe_property(handle_, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 2, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 3, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(handle_, 4, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(handle_, 5, "track-list", MPV_FORMAT_NODE);
    mpv_request_log_messages(handle_, "warn");
    pollTimer_.setInterval(30);
    connect(&pollTimer_, &QTimer::timeout, this, &MpvSession::pollEvents);
    pollTimer_.start();
    emit videoBackendChanged(QStringLiteral("libmpv"));
}

MpvSession::~MpvSession()
{
    shutdown();
}

void MpvSession::openUrl(const QUrl &url, bool autoplay,
                         const QString &httpUsername, const QString &httpPassword)
{
    if (!handle_) {
        emit errorOccurred(tr("libmpv is not initialized."));
        return;
    }
    currentUrl_ = url;
    currentHttpUsername_ = httpUsername;
    currentHttpPassword_ = httpPassword;
    hasPendingOpen_ = true;
    Q_UNUSED(autoplay)
    // The libmpv VO cannot open a video until the Qt OpenGL render context
    // has been attached. The QML page becomes visible asynchronously, so
    // wait for MpvVideoItem to finish creating that context before loadfile.
    if (!renderContextReady_) return;
    loadCurrentUrl();
}

void MpvSession::setRenderContextReady()
{
    renderContextReady_ = true;
    if (hasPendingOpen_) loadCurrentUrl();
}

void MpvSession::loadCurrentUrl()
{
    if (!handle_ || !hasPendingOpen_) return;
    // Set the HTTP header immediately before loadfile. This API path works on
    // older system libmpv versions that do not support loadfile per-file
    // options. Clearing it for other sources prevents credential reuse.
    const QString fields = currentHttpUsername_.isEmpty()
        ? QString()
        : QStringLiteral("Authorization: Basic ")
              + QString::fromLatin1((currentHttpUsername_ + QLatin1Char(':')
                                      + currentHttpPassword_).toUtf8().toBase64());
    const QByteArray headers = fields.toUtf8();
    const int headerResult = mpv_set_property_string(handle_, "http-header-fields", headers.constData());
    if (headerResult < 0)
        emit warningOccurred(tr("Could not configure WebDAV authorization headers: %1")
                                 .arg(errorText(headerResult)));

    qInfo() << "WebDAV authentication attached to current playback:" << !currentHttpUsername_.isEmpty();
    const QByteArray encoded = (currentUrl_.isLocalFile()
                                    ? QUrl::fromLocalFile(currentUrl_.toLocalFile()).toEncoded(QUrl::FullyEncoded)
                                    : currentUrl_.toEncoded(QUrl::FullyEncoded));
    hasPendingOpen_ = false;
    const char *command[] = {"loadfile", encoded.constData(), "replace", nullptr};
    const int result = mpv_command(handle_, command);
    if (result < 0) {
        emit errorOccurred(tr("Unable to open media: %1").arg(errorText(result)));
    }
}

void MpvSession::play()
{
    int value = 0;
    setProperty("pause", MPV_FORMAT_FLAG, &value);
}

void MpvSession::pause()
{
    int value = 1;
    setProperty("pause", MPV_FORMAT_FLAG, &value);
}

void MpvSession::stop()
{
    if (!handle_) return;
    const char *command[] = {"stop", nullptr};
    mpv_command(handle_, command);
}

void MpvSession::seek(qint64 positionMs)
{
    if (!handle_) return;
    const double seconds = qMax<qint64>(0, positionMs) / 1000.0;
    const char *command[] = {"seek", nullptr, "absolute+exact", nullptr};
    const QByteArray value = QByteArray::number(seconds, 'f', 3);
    command[1] = value.constData();
    mpv_command(handle_, command);
}

void MpvSession::setVolume(double volume)
{
    double value = std::clamp(volume * 100.0, 0.0, 200.0);
    setProperty("volume", MPV_FORMAT_DOUBLE, &value);
}

void MpvSession::setMuted(bool muted)
{
    int value = muted ? 1 : 0;
    setProperty("mute", MPV_FORMAT_FLAG, &value);
}

void MpvSession::setPlaybackRate(double rate)
{
    double value = std::clamp(rate, 0.25, 4.0);
    setProperty("speed", MPV_FORMAT_DOUBLE, &value);
}

void MpvSession::selectVideoTrack(int id)
{
    if (!handle_) return;
    if (id < 0) {
        mpv_set_property_string(handle_, "vid", "no");
        return;
    }
    int64_t value = id;
    setProperty("vid", MPV_FORMAT_INT64, &value);
}

void MpvSession::selectAudioTrack(int id)
{
    if (!handle_) return;
    if (id < 0) {
        mpv_set_property_string(handle_, "aid", "no");
        return;
    }
    int64_t value = id;
    setProperty("aid", MPV_FORMAT_INT64, &value);
}

void MpvSession::selectSubtitleTrack(int id)
{
    if (!handle_) return;
    if (id < 0) {
        mpv_set_property_string(handle_, "sid", "no");
        return;
    }
    int64_t value = id;
    setProperty("sid", MPV_FORMAT_INT64, &value);
}

void MpvSession::shutdown()
{
    if (shuttingDown_) return;
    shuttingDown_ = true;
    pollTimer_.stop();
    if (handle_) {
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
    }
}

void MpvSession::setProperty(const char *name, mpv_format format, void *value)
{
    if (!handle_) return;
    const int result = mpv_set_property(handle_, name, format, value);
    if (result < 0) {
        emit warningOccurred(tr("Could not set mpv property %1: %2")
                                 .arg(QString::fromLatin1(name), errorText(result)));
    }
}

void MpvSession::pollEvents()
{
    if (!handle_) return;
    while (true) {
        mpv_event *event = mpv_wait_event(handle_, 0.0);
        if (!event || event->event_id == MPV_EVENT_NONE) break;
        switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED:
            handleFileLoaded();
            break;
        case MPV_EVENT_END_FILE: {
            auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (end && end->reason == MPV_END_FILE_REASON_EOF) emit playbackFinished();
            else if (end && end->reason == MPV_END_FILE_REASON_ERROR)
                emit errorOccurred(tr("Playback failed: %1").arg(errorText(end->error)));
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto *property = static_cast<mpv_event_property *>(event->data);
            if (!property || !property->name) break;
            if (qstrcmp(property->name, "time-pos") == 0
                && property->format == MPV_FORMAT_DOUBLE && property->data) {
                emit positionChanged(qMax<qint64>(0, qRound64(*static_cast<double *>(property->data) * 1000.0)));
            } else if (qstrcmp(property->name, "duration") == 0
                       && property->format == MPV_FORMAT_DOUBLE && property->data) {
                emit durationChanged(qMax<qint64>(0, qRound64(*static_cast<double *>(property->data) * 1000.0)));
            } else if (qstrcmp(property->name, "paused-for-cache") == 0
                       && property->format == MPV_FORMAT_FLAG && property->data) {
                const bool state = *static_cast<int *>(property->data) != 0;
                if (buffering_ != state) { buffering_ = state; emit bufferingChanged(state); }
            } else if (qstrcmp(property->name, "track-list") == 0) {
                refreshTracks();
            }
            break;
        }
        case MPV_EVENT_LOG_MESSAGE: {
            auto *message = static_cast<mpv_event_log_message *>(event->data);
            if (message && message->log_level <= MPV_LOG_LEVEL_WARN)
                emit warningOccurred(QString::fromUtf8(message->text).trimmed());
            break;
        }
        default:
            break;
        }
    }
    refreshPlaybackProperties();
}

void MpvSession::refreshPlaybackProperties()
{
    if (!handle_) return;
    double position = 0.0;
    if (mpv_get_property(handle_, "time-pos", MPV_FORMAT_DOUBLE, &position) >= 0)
        emit positionChanged(qMax<qint64>(0, qRound64(position * 1000.0)));
}

void MpvSession::handleFileLoaded()
{
    if (!handle_) return;
    MediaInfo media;
    media.source = currentUrl_;
    media.title = currentUrl_.isLocalFile()
        ? QFileInfo(currentUrl_.toLocalFile()).fileName()
        : currentUrl_.fileName();
    if (media.title.isEmpty()) media.title = currentUrl_.toString();
    char *metadataTitle = mpv_get_property_string(handle_, "media-title");
    media.metadataTitle = metadataTitle ? QString::fromUtf8(metadataTitle) : QString();
    mpv_free(metadataTitle);
    char *formatName = mpv_get_property_string(handle_, "file-format");
    media.formatName = formatName ? QString::fromUtf8(formatName) : QString();
    mpv_free(formatName);
    double durationSeconds = 0.0;
    if (mpv_get_property(handle_, "duration", MPV_FORMAT_DOUBLE, &durationSeconds) >= 0)
        media.durationMs = qRound64(durationSeconds * 1000.0);
    currentMedia_ = media;
    refreshTracks();
    media = currentMedia_;
    emit durationChanged(media.durationMs);
    emit mediaOpened(media);
}

void MpvSession::refreshTracks()
{
    if (!handle_) return;
    mpv_node tracks{};
    if (mpv_get_property(handle_, "track-list", MPV_FORMAT_NODE, &tracks) < 0
        || tracks.format != MPV_FORMAT_NODE_ARRAY || !tracks.u.list) {
        mpv_free_node_contents(&tracks);
        return;
    }

    currentMedia_.source = currentUrl_;
    currentMedia_.tracks.clear();
    currentMedia_.videoStream = -1;
    currentMedia_.audioStream = -1;
    currentMedia_.subtitleStream = -1;
    const int count = tracks.u.list->num;
    for (int i = 0; i < count; ++i) {
        const mpv_node &entry = tracks.u.list->values[i];
        const QString type = valueString(&entry, "type");
        TrackInfo track;
        track.streamIndex = valueInt(&entry, "id");
        track.title = valueString(&entry, "title");
        track.language = valueString(&entry, "lang");
        track.codec = valueString(&entry, "codec");
        track.isDefault = valueBool(&entry, "default");
        if (type == QLatin1String("video")) track.type = TrackType::Video;
        else if (type == QLatin1String("audio")) track.type = TrackType::Audio;
        else if (type == QLatin1String("sub")) track.type = TrackType::Subtitle;
        else continue;
        currentMedia_.tracks.append(track);
        if (valueBool(&entry, "selected")) {
            if (track.type == TrackType::Video) currentMedia_.videoStream = track.streamIndex;
            if (track.type == TrackType::Audio) currentMedia_.audioStream = track.streamIndex;
            if (track.type == TrackType::Subtitle) currentMedia_.subtitleStream = track.streamIndex;
        }
    }
    mpv_free_node_contents(&tracks);
    emit tracksChanged(currentMedia_);
    emit audioTrackChanged(currentMedia_.audioStream);
    emit subtitleTrackChanged(currentMedia_.subtitleStream);
}

} // namespace QuarkTV::Mpv
