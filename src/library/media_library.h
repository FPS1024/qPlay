#pragma once

#include "core/media_types.h"

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QUrl>
#include <QVariantList>

namespace QuarkTV::Library {

class MediaLibrary final : public QObject
{
    Q_OBJECT

public:
    explicit MediaLibrary(QObject *parent = nullptr);
    ~MediaLibrary() override;

    bool initialize(QString *error = nullptr);

    bool rememberMedia(const MediaInfo &media, qint64 positionMs = 0);
    bool indexMedia(const QUrl &source, const QString &title);
    bool updateTmdbMetadata(const QUrl &source, int tmdbId, const QString &posterUrl,
                            const QString &overview, const QString &releaseDate);
    bool updateProgress(const QUrl &source, qint64 positionMs, qint64 durationMs);
    bool setFavorite(const QUrl &source, bool favorite);
    bool removeMedia(const QUrl &source);
    bool clearHistory();

    [[nodiscard]] QVariantList recentMedia(int limit = 50) const;
    [[nodiscard]] QVariantList libraryMedia(const QString &filter = {}, int limit = 500) const;
    [[nodiscard]] QVariantList favoriteMedia(int limit = 100) const;
    [[nodiscard]] qint64 resumePosition(const QUrl &source) const;

private:
    static QString sourceKey(const QUrl &source);
    static QString connectionName(const MediaLibrary *library);
    bool execute(const QString &sql, QString *error = nullptr) const;

    QString connectionName_;
    QSqlDatabase database_;
};

} // namespace QuarkTV::Library
