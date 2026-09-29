#pragma once

#include "core/media_types.h"
#include "library/media_library.h"

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QSet>

class QNetworkAccessManager;
namespace QuarkTV::Mpv { class MpvSession; }

namespace QuarkTV::App {

class PlayerEngine final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QuarkTV::PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(QUrl source READ source NOTIFY sourceChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString formatName READ formatName NOTIFY mediaInfoChanged)
    Q_PROPERTY(QString videoCodec READ videoCodec NOTIFY playbackStatsChanged)
    Q_PROPERTY(QString videoResolution READ videoResolution NOTIFY playbackStatsChanged)
    Q_PROPERTY(qint64 videoBitrate READ videoBitrate NOTIFY playbackStatsChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(double playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString videoBackend READ videoBackend NOTIFY videoBackendChanged)
    Q_PROPERTY(QObject *renderSession READ renderSession CONSTANT)
    Q_PROPERTY(QString webDavUrl READ webDavUrl WRITE setWebDavUrl NOTIFY webDavSettingsChanged)
    Q_PROPERTY(QString webDavUsername READ webDavUsername WRITE setWebDavUsername NOTIFY webDavSettingsChanged)
    Q_PROPERTY(QString webDavPassword READ webDavPassword WRITE setWebDavPassword NOTIFY webDavSettingsChanged)
    Q_PROPERTY(QVariantList videoTracks READ videoTracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY tracksChanged)
    Q_PROPERTY(int currentVideoTrack READ currentVideoTrack NOTIFY tracksChanged)
    Q_PROPERTY(int currentAudioTrack READ currentAudioTrack NOTIFY tracksChanged)
    Q_PROPERTY(int currentSubtitleTrack READ currentSubtitleTrack NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList playlist READ playlist NOTIFY playlistChanged)
    Q_PROPERTY(int playlistIndex READ playlistIndex NOTIFY playlistChanged)
    Q_PROPERTY(int imageCacheRevision READ imageCacheRevision NOTIFY imageCacheChanged)

public:
    explicit PlayerEngine(QObject *parent = nullptr);
    ~PlayerEngine() override;

    [[nodiscard]] PlaybackState playbackState() const noexcept;
    [[nodiscard]] QUrl source() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QString formatName() const;
    [[nodiscard]] QString videoCodec() const;
    [[nodiscard]] QString videoResolution() const;
    [[nodiscard]] qint64 videoBitrate() const noexcept;
    [[nodiscard]] qint64 duration() const noexcept;
    [[nodiscard]] qint64 position() const noexcept;
    [[nodiscard]] double volume() const noexcept;
    void setVolume(double volume);
    [[nodiscard]] bool muted() const noexcept;
    void setMuted(bool muted);
    [[nodiscard]] double playbackRate() const noexcept;
    void setPlaybackRate(double rate);
    [[nodiscard]] bool buffering() const noexcept;
    [[nodiscard]] QString statusMessage() const;
    [[nodiscard]] QString videoBackend() const;
    [[nodiscard]] QObject *renderSession() const;
    [[nodiscard]] QString webDavUrl() const;
    void setWebDavUrl(const QString &url);
    [[nodiscard]] QString webDavUsername() const;
    void setWebDavUsername(const QString &username);
    [[nodiscard]] QString webDavPassword() const;
    void setWebDavPassword(const QString &password);
    [[nodiscard]] QVariantList videoTracks() const;
    [[nodiscard]] QVariantList audioTracks() const;
    [[nodiscard]] QVariantList subtitleTracks() const;
    [[nodiscard]] int currentVideoTrack() const noexcept;
    [[nodiscard]] int currentAudioTrack() const noexcept;
    [[nodiscard]] int currentSubtitleTrack() const noexcept;
    [[nodiscard]] QVariantList playlist() const;
    [[nodiscard]] int playlistIndex() const noexcept;
    [[nodiscard]] int imageCacheRevision() const noexcept;

    Q_INVOKABLE void open(const QUrl &source);
    Q_INVOKABLE void openWithTitle(const QUrl &source, const QString &displayTitle);
    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(qint64 positionMs);
    Q_INVOKABLE void seekRelative(qint64 offsetMs);
    Q_INVOKABLE void selectVideoTrack(int streamIndex);
    Q_INVOKABLE void selectAudioTrack(int streamIndex);
    Q_INVOKABLE void selectSubtitleTrack(int streamIndex);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void addToPlaylist(const QUrl &source, const QString &title = {});
    Q_INVOKABLE void removePlaylistItem(int index);
    Q_INVOKABLE void clearPlaylist();
    Q_INVOKABLE void playPlaylistItem(int index);
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE QVariantList recentMedia(int limit = 50) const;
    Q_INVOKABLE QVariantList libraryMedia(const QString &filter = {}) const;
    Q_INVOKABLE QUrl cachedImageSource(const QString &remoteUrl, const QString &kind);
    Q_INVOKABLE bool setFavorite(const QUrl &source, bool favorite);
    Q_INVOKABLE bool removeRecent(const QUrl &source);
    Q_INVOKABLE void clearRecent();
    Q_INVOKABLE qint64 resumePosition(const QUrl &source) const;
    Q_INVOKABLE QString formatTime(qint64 milliseconds) const;

signals:
    void playbackStateChanged();
    void sourceChanged();
    void titleChanged();
    void mediaInfoChanged();
    void playbackStatsChanged();
    void durationChanged();
    void positionChanged();
    void volumeChanged();
    void mutedChanged();
    void playbackRateChanged();
    void bufferingChanged();
    void statusMessageChanged();
    void videoBackendChanged();
    void tracksChanged();
    void playlistChanged();
    void libraryChanged();
    void webDavSettingsChanged();
    void imageCacheChanged();

private:
    void setPlaybackState(PlaybackState state);
    void setStatusMessage(const QString &message);
    void handleMediaOpened(const MediaInfo &media);
    void handlePositionChanged(qint64 positionMs);
    void handlePlaybackFinished();
    void saveProgress(bool resetAtEnd = false);
    void appendPlaylistEntry(const QUrl &source, const QString &title);
    void openInternal(const QUrl &source, const QString &displayTitle);
    [[nodiscard]] static QVariantList tracksByType(const MediaInfo &media, TrackType type);

    QuarkTV::Mpv::MpvSession *session_ = nullptr;
    Library::MediaLibrary library_;
    QNetworkAccessManager *imageNetwork_ = nullptr;
    QSet<QString> pendingImageDownloads_;
    int imageCacheRevision_ = 0;
    QString webDavUrl_;
    QString webDavUsername_;
    QString webDavPassword_;
    QTimer progressTimer_;
    MediaInfo mediaInfo_;
    PlaybackState playbackState_ = PlaybackState::Stopped;
    QUrl source_;
    QString title_;
    QString playbackTitleOverride_;
    QString videoCodec_;
    QString videoResolution_;
    qint64 videoBitrate_ = 0;
    qint64 durationMs_ = 0;
    qint64 positionMs_ = 0;
    double volume_ = 1.0;
    bool muted_ = false;
    double playbackRate_ = 1.0;
    bool buffering_ = false;
    QString statusMessage_;
    QString videoBackend_ = QStringLiteral("libmpv");
    QVariantList videoTracks_;
    QVariantList audioTracks_;
    QVariantList subtitleTracks_;
    int currentVideoTrack_ = -1;
    int currentAudioTrack_ = -1;
    int currentSubtitleTrack_ = -1;
    QVariantList playlist_;
    int playlistIndex_ = -1;
    qint64 lastSavedPositionMs_ = -1;
    bool autoPlayOnOpen_ = true;
    bool resumeOnOpen_ = true;
};

} // namespace QuarkTV::App
