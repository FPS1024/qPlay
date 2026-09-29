#include "player_engine.h"

#include "mpv/mpv_session.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QMetaObject>
#include <QSettings>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace QuarkTV::App {

namespace {

QString userConfigPath()
{
    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString directory = QDir(configRoot).filePath(QStringLiteral("qPlay"));
    QDir().mkpath(directory);
    const QString path = QDir(directory).filePath(QStringLiteral("user-config.ini"));
    if (!QFileInfo::exists(path)) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) file.close();
    }
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return path;
}

QSettings userConfig()
{
    return QSettings(userConfigPath(), QSettings::IniFormat);
}

void secureUserConfig(QSettings &settings)
{
    settings.sync();
    QFile::setPermissions(settings.fileName(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QString displayNameFor(const QUrl &source)
{
    if (source.isLocalFile()) {
        return QFileInfo(source.toLocalFile()).fileName();
    }
    return source.fileName().isEmpty() ? source.toString() : source.fileName();
}

bool sameSource(const QUrl &left, const QUrl &right)
{
    return left.toString(QUrl::FullyEncoded) == right.toString(QUrl::FullyEncoded);
}

} // namespace

PlayerEngine::PlayerEngine(QObject *parent)
    : QObject(parent)
    , session_(new QuarkTV::Mpv::MpvSession())
{
    imageNetwork_ = new QNetworkAccessManager(this);
    QSettings settings = userConfig();
    webDavUrl_ = settings.value(QStringLiteral("webdav/url")).toString();
    QUrl configuredWebDavUrl(webDavUrl_);
    if (configuredWebDavUrl.isValid() && !configuredWebDavUrl.path().endsWith(QLatin1Char('/'))) {
        configuredWebDavUrl.setPath(configuredWebDavUrl.path() + QLatin1Char('/'));
        webDavUrl_ = configuredWebDavUrl.toString();
    }
    webDavUsername_ = settings.value(QStringLiteral("webdav/username")).toString();
    webDavPassword_ = settings.value(QStringLiteral("webdav/password")).toString();
    if (settings.value(QStringLiteral("tmdb/apiKey")).toString().isEmpty()) {
        const QList<QPair<QString, QString>> legacySettingsIds{
            {QStringLiteral("qPlay"), QStringLiteral("qPlay")},
            {QStringLiteral("QuarkTV"), QStringLiteral("qPlay")},
            {QStringLiteral("QuarkTV"), QStringLiteral("Quark TV")},
        };
        for (const auto &[organization, application] : legacySettingsIds) {
            QSettings legacySettings(organization, application);
            const QString apiKey = legacySettings.value(QStringLiteral("tmdb/apiKey")).toString();
            if (!apiKey.isEmpty()) {
                settings.setValue(QStringLiteral("tmdb/apiKey"), apiKey);
                secureUserConfig(settings);
                break;
            }
        }
    }
    if (!session_->initialized()) {
        setStatusMessage(session_->initializationError().isEmpty()
                             ? tr("libmpv could not be initialized; check the application log.")
                             : session_->initializationError());
    }

    QString libraryError;
    if (!library_.initialize(&libraryError)) {
        setStatusMessage(tr("Media library is unavailable: %1").arg(libraryError));
    }

    connect(session_, &QuarkTV::Mpv::MpvSession::mediaOpened,
            this, &PlayerEngine::handleMediaOpened);
    connect(session_, &QuarkTV::Mpv::MpvSession::playbackStatsChanged, this,
            [this](const QString &codec, int width, int height, qint64 bitrate) {
        const QString resolution = width > 0 && height > 0
            ? QStringLiteral("%1 × %2").arg(width).arg(height) : QString();
        if (videoCodec_ == codec && videoResolution_ == resolution && videoBitrate_ == bitrate)
            return;
        videoCodec_ = codec;
        videoResolution_ = resolution;
        videoBitrate_ = bitrate;
        emit playbackStatsChanged();
    });
    connect(session_, &QuarkTV::Mpv::MpvSession::positionChanged,
            this, &PlayerEngine::handlePositionChanged);
    connect(session_, &QuarkTV::Mpv::MpvSession::durationChanged, this, [this](qint64 durationMs) {
        durationMs_ = qMax<qint64>(0, durationMs);
        emit durationChanged();
    });
    connect(session_, &QuarkTV::Mpv::MpvSession::tracksChanged, this, [this](const MediaInfo &media) {
        videoTracks_ = tracksByType(media, TrackType::Video);
        audioTracks_ = tracksByType(media, TrackType::Audio);
        subtitleTracks_ = tracksByType(media, TrackType::Subtitle);
        currentVideoTrack_ = media.videoStream;
        currentAudioTrack_ = media.audioStream;
        currentSubtitleTrack_ = media.subtitleStream;
        emit tracksChanged();
    });
    connect(session_, &QuarkTV::Mpv::MpvSession::playbackFinished,
            this, &PlayerEngine::handlePlaybackFinished);
    connect(session_, &QuarkTV::Mpv::MpvSession::bufferingChanged, this, [this](bool buffering) {
        if (buffering_ == buffering) {
            return;
        }
        buffering_ = buffering;
        emit bufferingChanged();
    });
    connect(session_, &QuarkTV::Mpv::MpvSession::errorOccurred, this, [this](const QString &message) {
        setStatusMessage(message);
        setPlaybackState(PlaybackState::Error);
        saveProgress();
    });
    connect(session_, &QuarkTV::Mpv::MpvSession::warningOccurred, this, [this](const QString &message) {
        QString safeMessage = message;
        safeMessage.replace(QRegularExpression(QStringLiteral("https?://[^\\s]+")),
                            QStringLiteral("[media URL redacted]"));
        qWarning().noquote() << "libmpv:" << safeMessage;
    });
    connect(session_, &QuarkTV::Mpv::MpvSession::videoBackendChanged, this, [this](const QString &backend) {
        if (videoBackend_ == backend) {
            return;
        }
        videoBackend_ = backend;
        emit videoBackendChanged();
    });
    connect(session_, &QuarkTV::Mpv::MpvSession::audioTrackChanged, this, [this](int streamIndex) {
        currentAudioTrack_ = streamIndex;
        emit tracksChanged();
    });
    connect(session_, &QuarkTV::Mpv::MpvSession::subtitleTrackChanged, this, [this](int streamIndex) {
        currentSubtitleTrack_ = streamIndex;
        emit tracksChanged();
    });

    progressTimer_.setInterval(5000);
    progressTimer_.setTimerType(Qt::CoarseTimer);
    connect(&progressTimer_, &QTimer::timeout, this, [this] {
        if (playbackState_ == PlaybackState::Playing) {
            saveProgress();
        }
    });
    progressTimer_.start();
}

PlayerEngine::~PlayerEngine()
{
    progressTimer_.stop();
    QMetaObject::invokeMethod(session_, "shutdown", Qt::DirectConnection);
    delete session_;
    session_ = nullptr;
}

PlaybackState PlayerEngine::playbackState() const noexcept
{
    return playbackState_;
}

QUrl PlayerEngine::source() const
{
    return source_;
}

QString PlayerEngine::title() const
{
    return title_;
}

QString PlayerEngine::formatName() const
{
    return mediaInfo_.formatName;
}

QString PlayerEngine::videoCodec() const { return videoCodec_; }
QString PlayerEngine::videoResolution() const { return videoResolution_; }
qint64 PlayerEngine::videoBitrate() const noexcept { return videoBitrate_; }

qint64 PlayerEngine::duration() const noexcept
{
    return durationMs_;
}

qint64 PlayerEngine::position() const noexcept
{
    return positionMs_;
}

double PlayerEngine::volume() const noexcept
{
    return volume_;
}

void PlayerEngine::setVolume(double volume)
{
    const double bounded = std::clamp(volume, 0.0, 2.0);
    if (std::abs(volume_ - bounded) < 0.0001) {
        return;
    }
    volume_ = bounded;
    QMetaObject::invokeMethod(session_, "setVolume", Qt::QueuedConnection, Q_ARG(double, volume_));
    emit volumeChanged();
}

bool PlayerEngine::muted() const noexcept
{
    return muted_;
}

void PlayerEngine::setMuted(bool muted)
{
    if (muted_ == muted) {
        return;
    }
    muted_ = muted;
    QMetaObject::invokeMethod(session_, "setMuted", Qt::QueuedConnection, Q_ARG(bool, muted_));
    emit mutedChanged();
}

double PlayerEngine::playbackRate() const noexcept
{
    return playbackRate_;
}

void PlayerEngine::setPlaybackRate(double rate)
{
    const double bounded = std::clamp(rate, 0.25, 4.0);
    if (std::abs(playbackRate_ - bounded) < 0.0001) {
        return;
    }
    playbackRate_ = bounded;
    QMetaObject::invokeMethod(session_, "setPlaybackRate",
                              Qt::QueuedConnection,
                              Q_ARG(double, playbackRate_));
    emit playbackRateChanged();
}

bool PlayerEngine::buffering() const noexcept
{
    return buffering_;
}

QString PlayerEngine::statusMessage() const
{
    return statusMessage_;
}

QString PlayerEngine::videoBackend() const
{
    return videoBackend_;
}

QObject *PlayerEngine::renderSession() const
{
    return session_;
}

QVariantList PlayerEngine::videoTracks() const
{
    return videoTracks_;
}

QVariantList PlayerEngine::audioTracks() const
{
    return audioTracks_;
}

QVariantList PlayerEngine::subtitleTracks() const
{
    return subtitleTracks_;
}

int PlayerEngine::currentVideoTrack() const noexcept
{
    return currentVideoTrack_;
}

int PlayerEngine::currentAudioTrack() const noexcept
{
    return currentAudioTrack_;
}

int PlayerEngine::currentSubtitleTrack() const noexcept
{
    return currentSubtitleTrack_;
}

QVariantList PlayerEngine::playlist() const
{
    return playlist_;
}

int PlayerEngine::playlistIndex() const noexcept
{
    return playlistIndex_;
}

int PlayerEngine::imageCacheRevision() const noexcept
{
    return imageCacheRevision_;
}

void PlayerEngine::open(const QUrl &source)
{
    openInternal(source, {});
}

void PlayerEngine::openWithTitle(const QUrl &source, const QString &displayTitle)
{
    openInternal(source, displayTitle);
}

void PlayerEngine::openInternal(const QUrl &source, const QString &displayTitle)
{
    if (!source.isValid()) {
        setStatusMessage(tr("The selected media URL is invalid."));
        return;
    }

    saveProgress();
    playbackTitleOverride_ = displayTitle.trimmed();
    source_ = source;
    title_ = playbackTitleOverride_.isEmpty() ? displayNameFor(source) : playbackTitleOverride_;
    durationMs_ = 0;
    positionMs_ = 0;
    mediaInfo_ = {};
    videoCodec_.clear();
    videoResolution_.clear();
    videoBitrate_ = 0;
    mediaInfo_.source = source;
    mediaInfo_.title = title_;
    videoTracks_.clear();
    audioTracks_.clear();
    subtitleTracks_.clear();
    currentVideoTrack_ = -1;
    currentAudioTrack_ = -1;
    currentSubtitleTrack_ = -1;
    buffering_ = false;
    lastSavedPositionMs_ = -1;
    setPlaybackState(PlaybackState::Opening);
    appendPlaylistEntry(source, title_);

    const QUrl webDavRoot(webDavUrl_);
    QString httpUsername;
    QString httpPassword;
    if (source.host().compare(webDavRoot.host(), Qt::CaseInsensitive) == 0
        && source.scheme() == webDavRoot.scheme()
        && source.port(-1) == webDavRoot.port(-1)
        && source.path().startsWith(webDavRoot.path())) {
        httpUsername = webDavUsername_;
        httpPassword = webDavPassword_;
    }
    emit sourceChanged();
    emit titleChanged();
    emit mediaInfoChanged();
    emit playbackStatsChanged();
    emit durationChanged();
    emit positionChanged();
    emit tracksChanged();
    emit bufferingChanged();
    QMetaObject::invokeMethod(session_, "openUrl",
                              Qt::QueuedConnection,
                              Q_ARG(QUrl, source),
                              Q_ARG(bool, false),
                              Q_ARG(QString, httpUsername),
                              Q_ARG(QString, httpPassword));
}

void PlayerEngine::openPath(const QString &path)
{
    open(path.contains(QStringLiteral("://"))
             ? QUrl(path)
             : QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));
}

void PlayerEngine::play()
{
    if (!source_.isValid() || playbackState_ == PlaybackState::Opening) {
        return;
    }
    if (playbackState_ == PlaybackState::Ended) {
        positionMs_ = 0;
        emit positionChanged();
    }
    setPlaybackState(PlaybackState::Playing);
    QMetaObject::invokeMethod(session_, "play", Qt::QueuedConnection);
}

void PlayerEngine::pause()
{
    if (playbackState_ != PlaybackState::Playing
        && playbackState_ != PlaybackState::Buffering) {
        return;
    }
    setPlaybackState(PlaybackState::Paused);
    QMetaObject::invokeMethod(session_, "pause", Qt::QueuedConnection);
    saveProgress();
}

void PlayerEngine::togglePlayback()
{
    if (playbackState_ == PlaybackState::Playing
        || playbackState_ == PlaybackState::Buffering) {
        pause();
    } else {
        play();
    }
}

void PlayerEngine::stop()
{
    saveProgress();
    positionMs_ = 0;
    setPlaybackState(PlaybackState::Stopped);
    emit positionChanged();
    QMetaObject::invokeMethod(session_, "stop", Qt::QueuedConnection);
}

void PlayerEngine::seek(qint64 positionMs)
{
    const qint64 bounded = durationMs_ > 0
        ? std::clamp<qint64>(positionMs, 0, durationMs_)
        : qMax<qint64>(0, positionMs);
    positionMs_ = bounded;
    emit positionChanged();
    QMetaObject::invokeMethod(session_, "seek", Qt::QueuedConnection, Q_ARG(qint64, bounded));
}

void PlayerEngine::seekRelative(qint64 offsetMs)
{
    seek(positionMs_ + offsetMs);
}

void PlayerEngine::selectVideoTrack(int streamIndex)
{
    currentVideoTrack_ = streamIndex;
    emit tracksChanged();
    QMetaObject::invokeMethod(session_, "selectVideoTrack",
                              Qt::QueuedConnection, Q_ARG(int, streamIndex));
}

void PlayerEngine::selectAudioTrack(int streamIndex)
{
    currentAudioTrack_ = streamIndex;
    emit tracksChanged();
    QMetaObject::invokeMethod(session_, "selectAudioTrack",
                              Qt::QueuedConnection, Q_ARG(int, streamIndex));
}

void PlayerEngine::selectSubtitleTrack(int streamIndex)
{
    currentSubtitleTrack_ = streamIndex;
    emit tracksChanged();
    QMetaObject::invokeMethod(session_, "selectSubtitleTrack",
                              Qt::QueuedConnection, Q_ARG(int, streamIndex));
}

void PlayerEngine::toggleMute()
{
    setMuted(!muted_);
}

void PlayerEngine::addToPlaylist(const QUrl &source, const QString &title)
{
    if (!source.isValid()) {
        return;
    }
    appendPlaylistEntry(source, title.isEmpty() ? displayNameFor(source) : title);
}

void PlayerEngine::removePlaylistItem(int index)
{
    if (index < 0 || index >= playlist_.size()) {
        return;
    }
    playlist_.removeAt(index);
    if (playlistIndex_ == index) {
        playlistIndex_ = -1;
    } else if (playlistIndex_ > index) {
        --playlistIndex_;
    }
    emit playlistChanged();
}

void PlayerEngine::clearPlaylist()
{
    playlist_.clear();
    playlistIndex_ = -1;
    emit playlistChanged();
}

void PlayerEngine::playPlaylistItem(int index)
{
    if (index < 0 || index >= playlist_.size()) {
        return;
    }
    const QVariantMap item = playlist_.at(index).toMap();
    playlistIndex_ = index;
    emit playlistChanged();
    open(item.value(QStringLiteral("source")).toUrl());
}

void PlayerEngine::next()
{
    if (playlist_.isEmpty()) {
        return;
    }
    playPlaylistItem((playlistIndex_ + 1) % playlist_.size());
}

void PlayerEngine::previous()
{
    if (playlist_.isEmpty()) {
        return;
    }
    const int previousIndex = playlistIndex_ <= 0 ? playlist_.size() - 1 : playlistIndex_ - 1;
    playPlaylistItem(previousIndex);
}

QVariantList PlayerEngine::recentMedia(int limit) const
{
    return library_.recentMedia(limit);
}

QVariantList PlayerEngine::libraryMedia(const QString &filter) const
{
    return library_.libraryMedia(filter);
}

QUrl PlayerEngine::cachedImageSource(const QString &remoteUrl, const QString &kind)
{
    const QString category = kind == QLatin1String("background")
                                 ? QStringLiteral("background")
                                 : kind == QLatin1String("episode")
                                       ? QStringLiteral("episode")
                                       : QStringLiteral("poster");
    QUrl remote(remoteUrl.trimmed());
    if (!remote.isValid() || remote.isEmpty()) return {};
    if (category == QLatin1String("background")
        && remote.host().compare(QStringLiteral("image.tmdb.org"), Qt::CaseInsensitive) == 0) {
        const QRegularExpression sizePath(QStringLiteral("^(/t/p/)(?:w[0-9]+|original)(/.*)$"));
        const auto match = sizePath.match(remote.path());
        if (match.hasMatch())
            remote.setPath(match.captured(1) + QStringLiteral("original") + match.captured(2));
    }
    if (remote.isLocalFile() || (remote.scheme() != QLatin1String("http")
                                && remote.scheme() != QLatin1String("https"))) {
        return remote;
    }

    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString directory = QDir(dataRoot).filePath(QStringLiteral("qPlay/%1").arg(category));
    const QString digest = QString::fromLatin1(
        QCryptographicHash::hash(remote.toString(QUrl::FullyEncoded).toUtf8(),
                                 QCryptographicHash::Sha256).toHex());
    QString extension = QFileInfo(remote.path()).suffix().toLower();
    if (extension != QLatin1String("jpg") && extension != QLatin1String("jpeg")
        && extension != QLatin1String("png") && extension != QLatin1String("webp")
        && extension != QLatin1String("avif")) {
        extension = QStringLiteral("jpg");
    }
    const QString cachePath = QDir(directory).filePath(digest + QLatin1Char('.') + extension);
    if (QFileInfo(cachePath).isFile() && QFileInfo(cachePath).size() > 0)
        return QUrl::fromLocalFile(cachePath);

    if (!pendingImageDownloads_.contains(cachePath)) {
        if (!QDir().mkpath(directory)) return remote;
        pendingImageDownloads_.insert(cachePath);
        QNetworkRequest request(remote);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("qPlay/1.0.0"));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = imageNetwork_->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, cachePath] {
            pendingImageDownloads_.remove(cachePath);
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray mime = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray().toLower();
            const qint64 announcedSize = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
            const QByteArray data = reply->readAll();
            if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300
                && (mime.isEmpty() || mime.startsWith("image/"))
                && (announcedSize <= 0 || announcedSize <= 32 * 1024 * 1024)
                && !data.isEmpty() && data.size() <= 32 * 1024 * 1024) {
                QSaveFile file(cachePath);
                if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit()) {
                    ++imageCacheRevision_;
                    emit imageCacheChanged();
                    emit libraryChanged();
                }
            }
            reply->deleteLater();
        });
    }
    return remote;
}

QString PlayerEngine::webDavUrl() const { return webDavUrl_; }
void PlayerEngine::setWebDavUrl(const QString &url)
{
    QUrl parsed(url.trimmed());
    if (parsed.isValid() && !parsed.path().isEmpty() && !parsed.path().endsWith(QLatin1Char('/')))
        parsed.setPath(parsed.path() + QLatin1Char('/'));
    const QString normalized = parsed.isValid() ? parsed.toString() : url.trimmed();
    if (webDavUrl_ == normalized) return;
    webDavUrl_ = normalized;
    QSettings settings = userConfig();
    settings.setValue(QStringLiteral("webdav/url"), webDavUrl_);
    secureUserConfig(settings);
    emit webDavSettingsChanged();
}

QString PlayerEngine::webDavUsername() const { return webDavUsername_; }
void PlayerEngine::setWebDavUsername(const QString &username)
{
    if (webDavUsername_ == username) return;
    webDavUsername_ = username;
    QSettings settings = userConfig();
    settings.setValue(QStringLiteral("webdav/username"), webDavUsername_);
    secureUserConfig(settings);
    emit webDavSettingsChanged();
}

QString PlayerEngine::webDavPassword() const { return webDavPassword_; }
void PlayerEngine::setWebDavPassword(const QString &password)
{
    if (webDavPassword_ == password) return;
    webDavPassword_ = password;
    QSettings settings = userConfig();
    settings.setValue(QStringLiteral("webdav/password"), webDavPassword_);
    secureUserConfig(settings);
    emit webDavSettingsChanged();
}

bool PlayerEngine::setFavorite(const QUrl &source, bool favorite)
{
    const bool changed = library_.setFavorite(source, favorite);
    if (changed) {
        emit const_cast<PlayerEngine *>(this)->libraryChanged();
    }
    return changed;
}

bool PlayerEngine::markWatched(const QUrl &source)
{
    const bool changed = library_.markWatched(source);
    if (changed) emit libraryChanged();
    return changed;
}

bool PlayerEngine::removeRecent(const QUrl &source)
{
    const bool removed = library_.removeMedia(source);
    if (removed) {
        emit const_cast<PlayerEngine *>(this)->libraryChanged();
    }
    return removed;
}

void PlayerEngine::clearRecent()
{
    if (library_.clearHistory()) {
        emit libraryChanged();
    }
}

qint64 PlayerEngine::resumePosition(const QUrl &source) const
{
    return library_.resumePosition(source);
}

QString PlayerEngine::formatTime(qint64 milliseconds) const
{
    const bool negative = milliseconds < 0;
    qint64 totalSeconds = std::llabs(milliseconds) / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    QString result;
    if (hours > 0) {
        result = QStringLiteral("%1:%2:%3")
                     .arg(hours)
                     .arg(minutes, 2, 10, QLatin1Char('0'))
                     .arg(seconds, 2, 10, QLatin1Char('0'));
    } else {
        result = QStringLiteral("%1:%2")
                     .arg(minutes)
                     .arg(seconds, 2, 10, QLatin1Char('0'));
    }
    return negative ? QStringLiteral("-") + result : result;
}

void PlayerEngine::setPlaybackState(PlaybackState state)
{
    if (playbackState_ == state) {
        return;
    }
    playbackState_ = state;
    emit playbackStateChanged();
}

void PlayerEngine::setStatusMessage(const QString &message)
{
    if (statusMessage_ == message) {
        return;
    }
    statusMessage_ = message;
    emit statusMessageChanged();
}

void PlayerEngine::handleMediaOpened(const MediaInfo &media)
{
    mediaInfo_ = media;
    source_ = media.source;
    title_ = playbackTitleOverride_.isEmpty() ? media.title : playbackTitleOverride_;
    playbackTitleOverride_.clear();
    durationMs_ = qMax<qint64>(0, media.durationMs);
    videoTracks_ = tracksByType(media, TrackType::Video);
    audioTracks_ = tracksByType(media, TrackType::Audio);
    subtitleTracks_ = tracksByType(media, TrackType::Subtitle);
    currentVideoTrack_ = media.videoStream;
    currentAudioTrack_ = media.audioStream;
    currentSubtitleTrack_ = media.subtitleStream;
    buffering_ = false;
    setStatusMessage({});
    setPlaybackState(PlaybackState::Paused);

    const qint64 resumeMs = resumeOnOpen_ ? library_.resumePosition(source_) : 0;
    if (resumeMs > 5000 && (durationMs_ <= 0 || resumeMs < durationMs_ - 10000)) {
        positionMs_ = resumeMs;
        emit positionChanged();
        QMetaObject::invokeMethod(session_, "seek",
                                  Qt::QueuedConnection, Q_ARG(qint64, resumeMs));
    } else {
        positionMs_ = 0;
    }

    library_.rememberMedia(media, resumeMs);
    lastSavedPositionMs_ = positionMs_;
    emit sourceChanged();
    emit titleChanged();
    emit mediaInfoChanged();
    emit durationChanged();
    emit tracksChanged();
    emit bufferingChanged();
    emit libraryChanged();

    if (autoPlayOnOpen_) {
        play();
    }
}

void PlayerEngine::handlePositionChanged(qint64 positionMs)
{
    positionMs = qMax<qint64>(0, positionMs);
    if (positionMs_ == positionMs) {
        return;
    }
    positionMs_ = positionMs;
    emit positionChanged();
}

void PlayerEngine::handlePlaybackFinished()
{
    saveProgress(true);
    setPlaybackState(PlaybackState::Ended);
    if (!playlist_.isEmpty() && playlistIndex_ >= 0) {
        const int nextIndex = playlistIndex_ + 1;
        if (nextIndex < playlist_.size()) {
            QTimer::singleShot(0, this, [this, nextIndex] {
                playPlaylistItem(nextIndex);
            });
        }
    }
}

void PlayerEngine::saveProgress(bool resetAtEnd)
{
    if (!source_.isValid() || durationMs_ <= 0) {
        return;
    }
    const qint64 positionToSave = resetAtEnd ? 0 : positionMs_;
    if (!resetAtEnd && std::llabs(positionToSave - lastSavedPositionMs_) < 1000) {
        return;
    }
    if (library_.updateProgress(source_, positionToSave, durationMs_)) {
        lastSavedPositionMs_ = positionToSave;
        emit libraryChanged();
    }
}

void PlayerEngine::appendPlaylistEntry(const QUrl &source, const QString &title)
{
    for (int index = 0; index < playlist_.size(); ++index) {
        const QVariantMap item = playlist_.at(index).toMap();
        if (sameSource(item.value(QStringLiteral("source")).toUrl(), source)) {
            if (playlistIndex_ < 0) {
                playlistIndex_ = index;
            }
            emit playlistChanged();
            return;
        }
    }

    playlist_.append(QVariantMap{
        {QStringLiteral("source"), source},
        {QStringLiteral("title"), title.isEmpty() ? displayNameFor(source) : title},
    });
    if (playlistIndex_ < 0) {
        playlistIndex_ = playlist_.size() - 1;
    }
    emit playlistChanged();
}

QVariantList PlayerEngine::tracksByType(const MediaInfo &media, TrackType type)
{
    QVariantList result;
    for (const TrackInfo &track : media.tracks) {
        if (track.type == type) {
            result.append(track.toVariantMap());
        }
    }
    return result;
}

} // namespace QuarkTV::App
