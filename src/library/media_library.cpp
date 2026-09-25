#include "media_library.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace QuarkTV::Library {

MediaLibrary::MediaLibrary(QObject *parent)
    : QObject(parent)
    , connectionName_(connectionName(this))
{
}

MediaLibrary::~MediaLibrary()
{
    if (database_.isValid()) {
        database_.close();
    }
    database_ = {};
    QSqlDatabase::removeDatabase(connectionName_);
}

QString MediaLibrary::connectionName(const MediaLibrary *library)
{
    return QStringLiteral("quarktv_library_%1")
        .arg(reinterpret_cast<quintptr>(library), 0, 16);
}

QString MediaLibrary::sourceKey(const QUrl &source)
{
    if (source.isLocalFile()) {
        return QFileInfo(source.toLocalFile()).absoluteFilePath();
    }
    return source.toString(QUrl::FullyEncoded);
}

bool MediaLibrary::execute(const QString &sql, QString *error) const
{
    const QString statement = sql.trimmed();
    if (statement.isEmpty()) {
        return true;
    }

    QSqlQuery query(database_);
    if (query.exec(statement)) {
        return true;
    }
    if (error) {
        *error = query.lastError().text();
    }
    return false;
}

bool MediaLibrary::initialize(QString *error)
{
    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir directory(dataDirectory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = tr("Unable to create the media library directory: %1").arg(dataDirectory);
        }
        return false;
    }

    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(directory.filePath(QStringLiteral("library.sqlite3")));
    if (!database_.open()) {
        if (error) {
            *error = database_.lastError().text();
        }
        return false;
    }

    const QString schema = QStringLiteral(R"(
        PRAGMA journal_mode = WAL;
        PRAGMA synchronous = NORMAL;
        CREATE TABLE IF NOT EXISTS media (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            source TEXT NOT NULL UNIQUE,
            title TEXT NOT NULL DEFAULT '',
            format_name TEXT NOT NULL DEFAULT '',
            duration_ms INTEGER NOT NULL DEFAULT 0,
            bitrate INTEGER NOT NULL DEFAULT 0,
            width INTEGER NOT NULL DEFAULT 0,
            height INTEGER NOT NULL DEFAULT 0,
            position_ms INTEGER NOT NULL DEFAULT 0,
            play_count INTEGER NOT NULL DEFAULT 0,
            favorite INTEGER NOT NULL DEFAULT 0,
            tmdb_id INTEGER NOT NULL DEFAULT 0,
            poster_url TEXT NOT NULL DEFAULT '',
            overview TEXT NOT NULL DEFAULT '',
            release_date TEXT NOT NULL DEFAULT '',
            last_opened_ms INTEGER NOT NULL DEFAULT 0,
            created_ms INTEGER NOT NULL DEFAULT 0
        );
        CREATE INDEX IF NOT EXISTS media_last_opened_idx
            ON media(last_opened_ms DESC);
        CREATE INDEX IF NOT EXISTS media_favorite_idx
            ON media(favorite, title);
    )");

    for (const QString &statement : schema.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        if (!execute(statement, error)) {
            qCritical().noquote() << "Media library schema failed:" << (error ? *error : QString());
            return false;
        }
    }
    const QList<QPair<QString, QString>> migrations{
        {QStringLiteral("tmdb_id"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")},
        {QStringLiteral("poster_url"), QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("overview"), QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("release_date"), QStringLiteral("TEXT NOT NULL DEFAULT ''")},
    };
    for (const auto &[name, type] : migrations) {
        QSqlQuery columns(database_);
        if (!columns.exec(QStringLiteral("PRAGMA table_info(media)"))) continue;
        bool exists = false;
        while (columns.next()) exists |= columns.value(1).toString() == name;
        if (!exists && !execute(QStringLiteral("ALTER TABLE media ADD COLUMN %1 %2").arg(name, type), error))
            return false;
    }
    return true;
}

bool MediaLibrary::rememberMedia(const MediaInfo &media, qint64 positionMs)
{
    if (!database_.isOpen() || !media.source.isValid()) {
        return false;
    }

    int width = 0;
    int height = 0;
    for (const TrackInfo &track : media.tracks) {
        if (track.type == TrackType::Video) {
            width = track.width;
            height = track.height;
            break;
        }
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(R"(
        INSERT INTO media (
            source, title, format_name, duration_ms, bitrate, width, height,
            position_ms, play_count, last_opened_ms, created_ms
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 1, ?, ?)
        ON CONFLICT(source) DO UPDATE SET
            title = excluded.title,
            format_name = excluded.format_name,
            duration_ms = excluded.duration_ms,
            bitrate = excluded.bitrate,
            width = excluded.width,
            height = excluded.height,
            position_ms = excluded.position_ms,
            play_count = media.play_count + 1,
            last_opened_ms = excluded.last_opened_ms
    )"));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    query.addBindValue(sourceKey(media.source));
    query.addBindValue(media.title);
    query.addBindValue(media.formatName);
    query.addBindValue(qMax<qint64>(0, media.durationMs));
    query.addBindValue(qMax<qint64>(0, media.bitrate));
    query.addBindValue(width);
    query.addBindValue(height);
    query.addBindValue(qMax<qint64>(0, positionMs));
    query.addBindValue(now);
    query.addBindValue(now);
    return query.exec();
}

bool MediaLibrary::indexMedia(const QUrl &source, const QString &title)
{
    if (!database_.isOpen() || !source.isValid()) return false;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(R"(
        INSERT INTO media (source, title, created_ms)
        VALUES (?, ?, ?)
        ON CONFLICT(source) DO UPDATE SET
            title = CASE WHEN media.title = '' THEN excluded.title ELSE media.title END
    )"));
    query.addBindValue(sourceKey(source));
    query.addBindValue(title);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return query.exec();
}

bool MediaLibrary::updateTmdbMetadata(const QUrl &source, int tmdbId,
                                     const QString &posterUrl, const QString &overview,
                                     const QString &releaseDate)
{
    if (!database_.isOpen() || !source.isValid()) return false;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("UPDATE media SET tmdb_id=?, poster_url=?, overview=?, release_date=? WHERE source=?"));
    query.addBindValue(tmdbId);
    query.addBindValue(posterUrl);
    query.addBindValue(overview);
    query.addBindValue(releaseDate);
    query.addBindValue(sourceKey(source));
    return query.exec() && query.numRowsAffected() > 0;
}

QVariantList MediaLibrary::libraryMedia(const QString &filter, int limit) const
{
    QVariantList result;
    if (!database_.isOpen()) return result;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(R"(
        SELECT source, title, format_name, duration_ms, position_ms, play_count,
               favorite, last_opened_ms, width, height, poster_url, overview, release_date, tmdb_id
        FROM media
        WHERE title LIKE ? COLLATE NOCASE
        ORDER BY title COLLATE NOCASE, last_opened_ms DESC
        LIMIT ?
    )"));
    query.addBindValue(QStringLiteral("%") + filter + QLatin1Char('%'));
    query.addBindValue(qBound(1, limit, 2000));
    if (!query.exec()) return result;
    while (query.next()) {
        const QString stored = query.value(0).toString();
        const QUrl source = !stored.contains(QStringLiteral("://"))
                                ? QUrl::fromLocalFile(stored)
                                : QUrl::fromUserInput(stored);
        result.append(QVariantMap{
            {QStringLiteral("source"), source},
            {QStringLiteral("title"), query.value(1).toString()},
            {QStringLiteral("formatName"), query.value(2).toString()},
            {QStringLiteral("durationMs"), query.value(3).toLongLong()},
            {QStringLiteral("positionMs"), query.value(4).toLongLong()},
            {QStringLiteral("playCount"), query.value(5).toInt()},
            {QStringLiteral("favorite"), query.value(6).toBool()},
            {QStringLiteral("lastOpenedMs"), query.value(7).toLongLong()},
            {QStringLiteral("width"), query.value(8).toInt()},
            {QStringLiteral("height"), query.value(9).toInt()},
            {QStringLiteral("posterUrl"), query.value(10).toString()},
            {QStringLiteral("overview"), query.value(11).toString()},
            {QStringLiteral("releaseDate"), query.value(12).toString()},
            {QStringLiteral("tmdbId"), query.value(13).toInt()},
        });
    }
    return result;
}

bool MediaLibrary::updateProgress(const QUrl &source, qint64 positionMs, qint64 durationMs)
{
    if (!database_.isOpen() || !source.isValid()) {
        return false;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(R"(
        INSERT INTO media (source, position_ms, duration_ms, last_opened_ms, created_ms)
        VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(source) DO UPDATE SET
            position_ms = excluded.position_ms,
            duration_ms = CASE
                WHEN excluded.duration_ms > 0 THEN excluded.duration_ms
                ELSE media.duration_ms
            END,
            last_opened_ms = excluded.last_opened_ms
    )"));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    query.addBindValue(sourceKey(source));
    query.addBindValue(qMax<qint64>(0, positionMs));
    query.addBindValue(qMax<qint64>(0, durationMs));
    query.addBindValue(now);
    query.addBindValue(now);
    return query.exec();
}

bool MediaLibrary::setFavorite(const QUrl &source, bool favorite)
{
    if (!database_.isOpen() || !source.isValid()) {
        return false;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("UPDATE media SET favorite = ? WHERE source = ?"));
    query.addBindValue(favorite ? 1 : 0);
    query.addBindValue(sourceKey(source));
    return query.exec() && query.numRowsAffected() > 0;
}

bool MediaLibrary::removeMedia(const QUrl &source)
{
    if (!database_.isOpen() || !source.isValid()) {
        return false;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("DELETE FROM media WHERE source = ?"));
    query.addBindValue(sourceKey(source));
    return query.exec();
}

bool MediaLibrary::clearHistory()
{
    if (!database_.isOpen()) {
        return false;
    }
    return execute(QStringLiteral("UPDATE media SET position_ms = 0, last_opened_ms = 0"));
}

QVariantList MediaLibrary::recentMedia(int limit) const
{
    QVariantList result;
    if (!database_.isOpen()) {
        return result;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(R"(
        SELECT source, title, format_name, duration_ms, position_ms, play_count,
               favorite, last_opened_ms, width, height
        FROM media
        WHERE last_opened_ms > 0
        ORDER BY last_opened_ms DESC
        LIMIT ?
    )"));
    query.addBindValue(qBound(1, limit, 500));
    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        const QString storedSource = query.value(0).toString();
        const QUrl source =
            !storedSource.contains(QStringLiteral("://"))
                && QFileInfo::exists(storedSource)
            ? QUrl::fromLocalFile(storedSource)
            : QUrl::fromUserInput(storedSource);
        result.append(QVariantMap{
            {QStringLiteral("source"), source},
            {QStringLiteral("title"), query.value(1).toString()},
            {QStringLiteral("formatName"), query.value(2).toString()},
            {QStringLiteral("durationMs"), query.value(3).toLongLong()},
            {QStringLiteral("positionMs"), query.value(4).toLongLong()},
            {QStringLiteral("playCount"), query.value(5).toInt()},
            {QStringLiteral("favorite"), query.value(6).toBool()},
            {QStringLiteral("lastOpenedMs"), query.value(7).toLongLong()},
            {QStringLiteral("width"), query.value(8).toInt()},
            {QStringLiteral("height"), query.value(9).toInt()},
        });
    }
    return result;
}

QVariantList MediaLibrary::favoriteMedia(int limit) const
{
    QVariantList result;
    if (!database_.isOpen()) {
        return result;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(R"(
        SELECT source, title, format_name, duration_ms, position_ms, play_count,
               favorite, last_opened_ms, width, height
        FROM media
        WHERE favorite = 1
        ORDER BY title COLLATE NOCASE
        LIMIT ?
    )"));
    query.addBindValue(qBound(1, limit, 500));
    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        result.append(QVariantMap{
            {QStringLiteral("source"), QUrl::fromUserInput(query.value(0).toString())},
            {QStringLiteral("title"), query.value(1).toString()},
            {QStringLiteral("formatName"), query.value(2).toString()},
            {QStringLiteral("durationMs"), query.value(3).toLongLong()},
            {QStringLiteral("positionMs"), query.value(4).toLongLong()},
            {QStringLiteral("playCount"), query.value(5).toInt()},
            {QStringLiteral("favorite"), true},
            {QStringLiteral("lastOpenedMs"), query.value(7).toLongLong()},
            {QStringLiteral("width"), query.value(8).toInt()},
            {QStringLiteral("height"), query.value(9).toInt()},
        });
    }
    return result;
}

qint64 MediaLibrary::resumePosition(const QUrl &source) const
{
    if (!database_.isOpen() || !source.isValid()) {
        return 0;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT position_ms FROM media WHERE source = ?"));
    query.addBindValue(sourceKey(source));
    if (query.exec() && query.next()) {
        return qMax<qint64>(0, query.value(0).toLongLong());
    }
    return 0;
}

} // namespace QuarkTV::Library
