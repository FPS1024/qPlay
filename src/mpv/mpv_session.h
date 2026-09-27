#pragma once

#include "core/media_types.h"

#include <QObject>
#include <QMap>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <mpv/client.h>

namespace QuarkTV::Mpv {

class MpvSession final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool initialized READ initialized CONSTANT)

public:
    explicit MpvSession(QObject *parent = nullptr,
                        const QMap<QString, QString> &overrides = {});
    ~MpvSession() override;

    [[nodiscard]] mpv_handle *handle() const noexcept { return handle_; }
    [[nodiscard]] bool initialized() const noexcept { return handle_ != nullptr; }
    [[nodiscard]] QString initializationError() const { return initializationError_; }

public slots:
    void openUrl(const QUrl &url, bool autoplay = true,
                 const QString &httpUsername = {}, const QString &httpPassword = {});
    void setRenderContextReady();
    void play();
    void pause();
    void stop();
    void seek(qint64 positionMs);
    void setVolume(double volume);
    void setMuted(bool muted);
    void setPlaybackRate(double rate);
    void selectVideoTrack(int id);
    void selectAudioTrack(int id);
    void selectSubtitleTrack(int id);
    void shutdown();

signals:
    void mediaOpened(const QuarkTV::MediaInfo &media);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void playbackFinished();
    void bufferingChanged(bool buffering);
    void errorOccurred(const QString &message);
    void warningOccurred(const QString &message);
    void videoBackendChanged(const QString &backend);
    void audioTrackChanged(int id);
    void subtitleTrackChanged(int id);
    void tracksChanged(const QuarkTV::MediaInfo &media);

private slots:
    void pollEvents();

private:
    void setProperty(const char *name, mpv_format format, void *value);
    void loadCurrentUrl();
    void refreshPlaybackProperties();
    void refreshTracks();
    void handleFileLoaded();

    mpv_handle *handle_ = nullptr;
    QString initializationError_;
    QTimer pollTimer_;
    QUrl currentUrl_;
    QString currentHttpUsername_;
    QString currentHttpPassword_;
    bool shuttingDown_ = false;
    bool renderContextReady_ = false;
    bool hasPendingOpen_ = false;
    bool buffering_ = false;
    MediaInfo currentMedia_;
};

} // namespace QuarkTV::Mpv
