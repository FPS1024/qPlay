#include "player_engine.h"

#include "mpv/mpv_session.h"

#include <QDebug>
#include <QDirIterator>
#include <QFileDialog>
#include <QStandardPaths>
#include <QMetaObject>
#include <QMimeDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>

namespace QuarkTV::App {

namespace {

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
    network_ = new QNetworkAccessManager(this);
    QSettings settings;
    tmdbApiKey_ = settings.value(QStringLiteral("tmdb/apiKey")).toString();
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
        qWarning().noquote() << "libmpv:" << message;
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
    const double bounded = std::clamp(volume, 0.0, 1.0);
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

QString PlayerEngine::tmdbApiKey() const
{
    return tmdbApiKey_;
}

void PlayerEngine::setTmdbApiKey(const QString &key)
{
    const QString cleaned = key.trimmed();
    if (tmdbApiKey_ == cleaned) return;
    tmdbApiKey_ = cleaned;
    QSettings settings;
    settings.setValue(QStringLiteral("tmdb/apiKey"), tmdbApiKey_);
    tmdbQueue_.clear();
    tmdbQueuedSources_.clear();
    emit tmdbApiKeyChanged();
    emit libraryChanged();
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

void PlayerEngine::open(const QUrl &source)
{
    if (!source.isValid()) {
        setStatusMessage(tr("The selected media URL is invalid."));
        return;
    }

    saveProgress();
    source_ = source;
    title_ = displayNameFor(source);
    durationMs_ = 0;
    positionMs_ = 0;
    mediaInfo_ = {};
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

    emit sourceChanged();
    emit titleChanged();
    emit mediaInfoChanged();
    emit durationChanged();
    emit positionChanged();
    emit tracksChanged();
    emit bufferingChanged();
    QMetaObject::invokeMethod(session_, "openUrl",
                              Qt::QueuedConnection,
                              Q_ARG(QUrl, source),
                              Q_ARG(bool, false));
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

int PlayerEngine::scanFolder(const QUrl &folderUrl)
{
    if (!folderUrl.isLocalFile()) return 0;
    QFileInfo selected(folderUrl.toLocalFile());
    const QString folderPath = selected.isDir() ? selected.absoluteFilePath() : selected.absolutePath();
    static const QSet<QString> extensions{
        QStringLiteral("mkv"), QStringLiteral("mp4"), QStringLiteral("m4v"),
        QStringLiteral("mov"), QStringLiteral("avi"), QStringLiteral("webm"),
        QStringLiteral("ts"), QStringLiteral("m2ts"), QStringLiteral("mpg"),
        QStringLiteral("mpeg"), QStringLiteral("wmv"), QStringLiteral("flv")
    };
    int added = 0;
    QDirIterator iterator(folderPath, QDir::Files,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QFileInfo info(iterator.next());
        if (!extensions.contains(info.suffix().toLower())) continue;
        const QUrl source = QUrl::fromLocalFile(info.absoluteFilePath());
        if (library_.indexMedia(source, info.completeBaseName())) ++added;
    }
    if (added > 0) emit libraryChanged();
    return added;
}

int PlayerEngine::chooseAndScanFolder()
{
    const QString selected = QFileDialog::getExistingDirectory(
        nullptr, tr("Add a movie folder"), QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
    return selected.isEmpty() ? 0 : scanFolder(QUrl::fromLocalFile(selected));
}

void PlayerEngine::enrichWithTmdb(const QUrl &source, const QString &filenameTitle)
{
    if (tmdbApiKey_.isEmpty() || !source.isValid()) return;
    const QString key = source.toString(QUrl::FullyEncoded);
    if (tmdbQueuedSources_.contains(key)) return;
    tmdbQueuedSources_.insert(key);
    tmdbQueue_.enqueue({source, filenameTitle});
    fetchNextTmdbMatch();
}

void PlayerEngine::fetchNextTmdbMatch()
{
    if (tmdbRequestActive_ || tmdbQueue_.isEmpty() || tmdbApiKey_.isEmpty()) return;
    const auto [source, filename] = tmdbQueue_.dequeue();

    QString queryTitle = QFileInfo(filename).completeBaseName();
    queryTitle.replace(QRegularExpression(QStringLiteral("[._]+")), QStringLiteral(" "));
    queryTitle.remove(QRegularExpression(
        QStringLiteral("\\b(2160p|1080p|720p|480p|4k|8k|bluray|blu[ .-]?ray|web[ .-]?dl|webrip|hdtv|x26[45]|h26[45]|hevc|avc|aac|dts|proper|remux|hdr10?)\\b"),
        QRegularExpression::CaseInsensitiveOption));
    queryTitle = queryTitle.simplified();
    if (queryTitle.isEmpty()) {
        QTimer::singleShot(250, this, &PlayerEngine::fetchNextTmdbMatch);
        return;
    }

    QUrl url(QStringLiteral("https://api.themoviedb.org/3/search/multi"));
    QUrlQuery parameters;
    parameters.addQueryItem(QStringLiteral("api_key"), tmdbApiKey_);
    parameters.addQueryItem(QStringLiteral("query"), queryTitle);
    parameters.addQueryItem(QStringLiteral("include_adult"), QStringLiteral("false"));
    parameters.addQueryItem(QStringLiteral("language"), QStringLiteral("zh-CN"));
    url.setQuery(parameters);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("qPlay/0.2"));
    tmdbRequestActive_ = true;
    QNetworkReply *reply = network_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, source, queryTitle] {
        tmdbRequestActive_ = false;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            const QJsonArray results = document.object().value(QStringLiteral("results")).toArray();
            auto normalize = [](QString value) {
                value = value.toLower();
                value.remove(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]")));
                return value;
            };
            const QString wanted = normalize(queryTitle);
            double bestScore = 0.0;
            QJsonObject best;
            for (const QJsonValue &value : results) {
                const QJsonObject candidate = value.toObject();
                const QString mediaType = candidate.value(QStringLiteral("media_type")).toString();
                if (mediaType != QLatin1String("movie") && mediaType != QLatin1String("tv")) continue;
                const QString title = candidate.value(QStringLiteral("title")).toString(
                    candidate.value(QStringLiteral("name")).toString());
                const QString originalTitle = candidate.value(QStringLiteral("original_title")).toString(
                    candidate.value(QStringLiteral("original_name")).toString());
                const QString localized = normalize(title);
                const QString original = normalize(originalTitle);
                double score = localized == wanted || original == wanted ? 1.0 : 0.0;
                if (score == 0.0 && !wanted.isEmpty()) {
                    if (localized.contains(wanted) || wanted.contains(localized)
                        || original.contains(wanted) || wanted.contains(original)) {
                        score = 0.78;
                    } else {
                        QSet<QString> wantedWords;
                        QSet<QString> candidateWords;
                        for (const QString &word : queryTitle.toLower().split(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), Qt::SkipEmptyParts))
                            wantedWords.insert(word);
                        const QString candidateTitle = originalTitle;
                        for (const QString &word : candidateTitle.toLower().split(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), Qt::SkipEmptyParts))
                            candidateWords.insert(word);
                        int overlap = 0;
                        for (const QString &word : wantedWords) overlap += candidateWords.contains(word);
                        if (!wantedWords.isEmpty()) score = double(overlap) / wantedWords.size();
                    }
                }
                if (score > bestScore) { bestScore = score; best = candidate; }
            }
            if (bestScore >= 0.55) {
                const int id = best.value(QStringLiteral("id")).toInt();
                const QString posterPath = best.value(QStringLiteral("poster_path")).toString();
                const QString posterUrl = posterPath.isEmpty()
                    ? QString()
                    : QStringLiteral("https://image.tmdb.org/t/p/w342") + posterPath;
                const QString releaseDate = best.value(QStringLiteral("release_date")).toString();
                const QString overview = best.value(QStringLiteral("overview")).toString();
                if (library_.updateTmdbMetadata(source, id, posterUrl, overview, releaseDate))
                    emit libraryChanged();
            }
        }
        reply->deleteLater();
        QTimer::singleShot(300, this, &PlayerEngine::fetchNextTmdbMatch);
    });
}

bool PlayerEngine::setFavorite(const QUrl &source, bool favorite)
{
    const bool changed = library_.setFavorite(source, favorite);
    if (changed) {
        emit const_cast<PlayerEngine *>(this)->libraryChanged();
    }
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
    title_ = media.title;
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
