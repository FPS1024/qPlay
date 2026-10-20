#include "media_library.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

namespace QuarkTV::Library {

namespace {

QString seasonDisplayName(int seasonNumber, const QString &seasonName)
{
    if (!seasonName.trimmed().isEmpty()) return seasonName.trimmed();
    if (seasonNumber == 0) return QStringLiteral("特别篇");
    if (seasonNumber < 0) return QStringLiteral("第%1季").arg(seasonNumber);

    const QStringList digits{QStringLiteral("零"), QStringLiteral("一"), QStringLiteral("二"),
                             QStringLiteral("三"), QStringLiteral("四"), QStringLiteral("五"),
                             QStringLiteral("六"), QStringLiteral("七"), QStringLiteral("八"),
                             QStringLiteral("九"), QStringLiteral("十")};
    QString number;
    if (seasonNumber <= 10) {
        number = digits.at(seasonNumber);
    } else if (seasonNumber < 20) {
        number = QStringLiteral("十") + digits.at(seasonNumber % 10);
    } else if (seasonNumber < 100) {
        number = digits.at(seasonNumber / 10) + QStringLiteral("十")
                 + (seasonNumber % 10 == 0 ? QString() : digits.at(seasonNumber % 10));
    } else {
        number = QString::number(seasonNumber);
    }
    return QStringLiteral("第%1季").arg(number);
}

} // namespace

MediaLibrary::MediaLibrary(QObject *parent)
    : QObject(parent)
    , connectionName_(connectionName(this))
{
}

MediaLibrary::~MediaLibrary()
{
    if (database_.isValid()) database_.close();
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
    const QString dataDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                                      .filePath(QStringLiteral("qPlay"));
    QDir directory(dataDirectory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = tr("Unable to create the media library directory: %1").arg(dataDirectory);
        }
        return false;
    }

    const QString databasePath = directory.filePath(QStringLiteral("library.sqlite3"));
    const QString genericDataDirectory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QStringList legacyDatabasePaths{
        QDir(genericDataDirectory).filePath(QStringLiteral("QuarkTV/qPlay/library.sqlite3")),
        QDir(genericDataDirectory).filePath(QStringLiteral("QuarkTV/Quark TV/library.sqlite3")),
    };
    if (!QFileInfo::exists(databasePath)) {
        for (const QString &legacyDatabasePath : legacyDatabasePaths) {
            if (!QFileInfo::exists(legacyDatabasePath)) continue;

            if (!QFile::copy(legacyDatabasePath, databasePath)) {
                if (error) {
                    *error = tr("Unable to migrate the existing media library from %1 to %2.")
                                 .arg(legacyDatabasePath, databasePath);
                }
                return false;
            }

            const QString legacyWalPath = legacyDatabasePath + QStringLiteral("-wal");
            if (QFileInfo::exists(legacyWalPath)
                && !QFile::copy(legacyWalPath, databasePath + QStringLiteral("-wal"))) {
                QFile::remove(databasePath);
                if (error) {
                    *error = tr("Unable to migrate the pending media library changes from %1.")
                                 .arg(legacyWalPath);
                }
                return false;
            }
            break;
        }
    }

    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(databasePath);
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
            backdrop_url TEXT NOT NULL DEFAULT '',
            rating REAL NOT NULL DEFAULT 0,
            overview TEXT NOT NULL DEFAULT '',
            release_date TEXT NOT NULL DEFAULT '',
            media_type TEXT NOT NULL DEFAULT 'movie',
            series_title TEXT NOT NULL DEFAULT '',
            season_number INTEGER NOT NULL DEFAULT 0,
            season_name TEXT NOT NULL DEFAULT '',
            episode_number INTEGER NOT NULL DEFAULT 0,
            episode_title TEXT NOT NULL DEFAULT '',
            still_url TEXT NOT NULL DEFAULT '',
            last_opened_ms INTEGER NOT NULL DEFAULT 0,
            created_ms INTEGER NOT NULL DEFAULT 0
        );
        CREATE INDEX IF NOT EXISTS media_last_opened_idx
            ON media(last_opened_ms DESC);
        CREATE INDEX IF NOT EXISTS media_favorite_idx
            ON media(favorite, title);
        CREATE TABLE IF NOT EXISTS work_genres (
            work_type TEXT NOT NULL CHECK (work_type IN ('movie', 'tv')),
            tmdb_id INTEGER NOT NULL CHECK (tmdb_id > 0),
            genre_id INTEGER NOT NULL CHECK (genre_id > 0),
            genre_name TEXT NOT NULL,
            PRIMARY KEY (work_type, tmdb_id, genre_id)
        );
        CREATE INDEX IF NOT EXISTS work_genres_genre_idx
            ON work_genres(genre_id, work_type, tmdb_id);
        CREATE TABLE IF NOT EXISTS genre_bindings (
            work_type TEXT NOT NULL CHECK (work_type IN ('movie', 'tv')),
            genre_id INTEGER NOT NULL CHECK (genre_id > 0),
            display_name TEXT NOT NULL DEFAULT '',
            PRIMARY KEY (work_type, genre_id)
        );
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
        {QStringLiteral("backdrop_url"), QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("rating"), QStringLiteral("REAL NOT NULL DEFAULT 0")},
        {QStringLiteral("overview"), QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("release_date"), QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("media_type"), QStringLiteral("TEXT NOT NULL DEFAULT 'movie'")},
        {QStringLiteral("series_title"), QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("season_number"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")},
        {QStringLiteral("season_name"), QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("episode_number"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")},
        {QStringLiteral("episode_title"), QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("still_url"), QStringLiteral("TEXT NOT NULL DEFAULT ''")},
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
            title = CASE WHEN media.title = '' THEN excluded.title ELSE media.title END,
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

QVariantList MediaLibrary::libraryMedia(const QString &filter, int limit) const
{
    QVariantList result;
    if (!database_.isOpen()) return result;
    QHash<QString, int> seriesIndices;
    QHash<QString, int> movieIndices;
    QHash<QString, QVariantList> genresByWork;
    QHash<QString, QSet<QString>> genreNamesByWork;
    QSqlQuery genreQuery(database_);
    if (genreQuery.exec(QStringLiteral(
            "SELECT g.work_type, g.tmdb_id, g.genre_id, g.genre_name, "
            "COALESCE(NULLIF(b.display_name, ''), g.genre_name) "
            "FROM work_genres g LEFT JOIN genre_bindings b "
            "ON b.work_type = g.work_type AND b.genre_id = g.genre_id "
            "ORDER BY COALESCE(NULLIF(b.display_name, ''), g.genre_name) COLLATE NOCASE"))) {
        while (genreQuery.next()) {
            const QString key = genreQuery.value(0).toString() + QLatin1Char(':')
                                + QString::number(genreQuery.value(1).toInt());
            const QString displayName = genreQuery.value(4).toString();
            if (genreNamesByWork[key].contains(displayName)) continue;
            genreNamesByWork[key].insert(displayName);
            genresByWork[key].append(QVariantMap{
                {QStringLiteral("id"), genreQuery.value(2).toInt()},
                {QStringLiteral("name"), displayName},
                {QStringLiteral("sourceName"), genreQuery.value(3).toString()},
            });
        }
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(R"(
        WITH matches AS (
            SELECT source, media_type, tmdb_id, title, release_date
            FROM media
            WHERE title LIKE ? COLLATE NOCASE
               OR series_title LIKE ? COLLATE NOCASE
               OR episode_title LIKE ? COLLATE NOCASE
        )
        SELECT source, title, format_name, duration_ms, position_ms, play_count,
               favorite, last_opened_ms, width, height, poster_url, overview, release_date, tmdb_id,
               backdrop_url, rating, media_type, series_title, season_number, season_name,
               episode_number, episode_title, still_url, bitrate
        FROM media
        WHERE source IN (SELECT source FROM matches)
           OR (media_type = 'movie' AND (
               (tmdb_id > 0 AND tmdb_id IN (
                   SELECT tmdb_id FROM matches WHERE media_type = 'movie' AND tmdb_id > 0
               ))
               OR (tmdb_id <= 0 AND EXISTS (
                   SELECT 1 FROM matches m
                   WHERE m.media_type = 'movie' AND m.tmdb_id <= 0
                     AND m.title = media.title COLLATE NOCASE
                     AND m.release_date = media.release_date
               ))
           ))
        ORDER BY CASE WHEN media_type = 'episode'
                      THEN COALESCE(NULLIF(series_title, ''), title)
                      ELSE title END COLLATE NOCASE,
                 CASE WHEN media_type = 'episode' THEN 0 ELSE 1 END,
                 season_number, episode_number, title COLLATE NOCASE
        LIMIT ?
    )"));
    const QString pattern = QStringLiteral("%") + filter + QLatin1Char('%');
    query.addBindValue(pattern);
    query.addBindValue(pattern);
    query.addBindValue(pattern);
    query.addBindValue(qBound(1, limit, 5000));
    if (!query.exec()) return result;
    while (query.next()) {
        const QString stored = query.value(0).toString();
        const QUrl source = !stored.contains(QStringLiteral("://"))
                                ? QUrl::fromLocalFile(stored)
                                : QUrl::fromUserInput(stored);
        const QString mediaType = query.value(16).toString();
        const int tmdbId = query.value(13).toInt();
        const QString workType = mediaType == QLatin1String("movie")
                                     ? QStringLiteral("movie")
                                     : mediaType == QLatin1String("episode")
                                           ? QStringLiteral("tv") : QString();
        const QString genreKey = workType + QLatin1Char(':') + QString::number(tmdbId);
        QVariantMap item{
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
            {QStringLiteral("genres"), tmdbId > 0 ? genresByWork.value(genreKey) : QVariantList{}},
            {QStringLiteral("backdropUrl"), query.value(14).toString()},
            {QStringLiteral("rating"), query.value(15).toDouble()},
            {QStringLiteral("mediaType"), mediaType},
            {QStringLiteral("seriesTitle"), query.value(17).toString()},
            {QStringLiteral("seasonNumber"), query.value(18).toInt()},
            {QStringLiteral("seasonName"), query.value(19).toString()},
            {QStringLiteral("episodeNumber"), query.value(20).toInt()},
            {QStringLiteral("episodeTitle"), query.value(21).toString()},
            {QStringLiteral("stillUrl"), query.value(22).toString()},
        };

        if (query.value(16).toString() == QLatin1String("movie")) {
            const int tmdbId = query.value(13).toInt();
            const QString movieTitle = query.value(1).toString().trimmed();
            const QString releaseDate = query.value(12).toString().trimmed();
            QString key;
            if (tmdbId > 0) {
                key = QStringLiteral("tmdb:%1").arg(tmdbId);
            } else if (!movieTitle.isEmpty() && releaseDate.size() >= 4) {
                key = QStringLiteral("title:%1:%2")
                          .arg(movieTitle.toCaseFolded(), releaseDate.left(4));
            } else {
                key = QStringLiteral("source:%1").arg(stored);
            }

            QString sourceLabel = source.fileName(QUrl::FullyDecoded);
            if (sourceLabel.isEmpty()) sourceLabel = movieTitle;
            if (query.value(9).toInt() > 0) {
                sourceLabel = QStringLiteral("%1p · %2")
                                  .arg(query.value(9).toInt())
                                  .arg(sourceLabel);
            }
            const QVariantMap movieSource{
                {QStringLiteral("source"), source},
                {QStringLiteral("label"), sourceLabel},
                {QStringLiteral("title"), movieTitle},
                {QStringLiteral("formatName"), query.value(2).toString()},
                {QStringLiteral("durationMs"), query.value(3).toLongLong()},
                {QStringLiteral("positionMs"), query.value(4).toLongLong()},
                {QStringLiteral("lastOpenedMs"), query.value(7).toLongLong()},
                {QStringLiteral("width"), query.value(8).toInt()},
                {QStringLiteral("height"), query.value(9).toInt()},
                {QStringLiteral("bitrate"), query.value(23).toLongLong()},
                {QStringLiteral("favorite"), query.value(6).toBool()},
            };

            const auto existing = movieIndices.constFind(key);
            if (existing == movieIndices.cend()) {
                item[QStringLiteral("sources")] = QVariantList{movieSource};
                item[QStringLiteral("sourceCount")] = 1;
                item[QStringLiteral("favoriteSource")] = source;
                movieIndices.insert(key, result.size());
                result.append(item);
            } else {
                QVariantMap movie = result.at(existing.value()).toMap();
                QVariantList sources = movie.value(QStringLiteral("sources")).toList();
                sources.append(movieSource);
                movie[QStringLiteral("sources")] = sources;
                movie[QStringLiteral("sourceCount")] = sources.size();
                movie[QStringLiteral("playCount")] = movie.value(QStringLiteral("playCount")).toInt()
                                                     + query.value(5).toInt();
                movie[QStringLiteral("lastOpenedMs")] = qMax(
                    movie.value(QStringLiteral("lastOpenedMs")).toLongLong(),
                    query.value(7).toLongLong());
                movie[QStringLiteral("favorite")] = movie.value(QStringLiteral("favorite")).toBool()
                                                     || query.value(6).toBool();
                result[existing.value()] = movie;
            }
            continue;
        }

        if (query.value(16).toString() != QLatin1String("episode")) {
            result.append(item);
            continue;
        }

        const QString seriesTitle = query.value(17).toString().trimmed().isEmpty()
                                        ? query.value(1).toString().trimmed()
                                        : query.value(17).toString().trimmed();
        const QString key = tmdbId > 0
                                ? QStringLiteral("tmdb:%1").arg(tmdbId)
                                : QStringLiteral("title:%1").arg(seriesTitle.toCaseFolded());
        const int season = query.value(18).toInt();
        const QString seasonName = query.value(19).toString().trimmed();
        const int episodeNumber = query.value(20).toInt();
        const QString episodeTitle = query.value(21).toString();
        const QString seasonLabel = seasonDisplayName(season, seasonName);
        const QString displayTitle = QStringLiteral("S%1E%2%3")
                                         .arg(season, 2, 10, QLatin1Char('0'))
                                         .arg(episodeNumber, 2, 10, QLatin1Char('0'))
                                         .arg(episodeTitle.isEmpty()
                                                  ? QString()
                                                  : QStringLiteral(" · ") + episodeTitle);
        const QVariantMap episode{
            {QStringLiteral("source"), source},
            {QStringLiteral("title"), query.value(1).toString()},
            {QStringLiteral("seriesTitle"), seriesTitle},
            {QStringLiteral("mediaType"), QStringLiteral("episode")},
            {QStringLiteral("overview"), query.value(11).toString()},
            {QStringLiteral("episodeTitle"), episodeTitle},
            {QStringLiteral("displayTitle"), displayTitle},
            {QStringLiteral("stillUrl"), query.value(22).toString()},
            {QStringLiteral("seasonNumber"), season},
            {QStringLiteral("seasonName"), seasonName},
            {QStringLiteral("episodeNumber"), episodeNumber},
            {QStringLiteral("positionMs"), query.value(4).toLongLong()},
            {QStringLiteral("durationMs"), query.value(3).toLongLong()},
            {QStringLiteral("playCount"), query.value(5).toInt()},
            {QStringLiteral("lastOpenedMs"), query.value(7).toLongLong()},
            {QStringLiteral("favorite"), query.value(6).toBool()},
        };
        const QVariantMap seasonEntry{
            {QStringLiteral("seasonNumber"), season},
            {QStringLiteral("seasonName"), seasonName},
            {QStringLiteral("displayName"), seasonLabel},
            {QStringLiteral("episodes"), QVariantList{episode}},
        };

        const auto existing = seriesIndices.constFind(key);
        if (existing == seriesIndices.cend()) {
            item[QStringLiteral("title")] = seriesTitle;
            item[QStringLiteral("seriesTitle")] = seriesTitle;
            item[QStringLiteral("mediaType")] = QStringLiteral("series");
            item[QStringLiteral("favoriteSource")] = source;
            item[QStringLiteral("episodes")] = QVariantList{episode};
            item[QStringLiteral("episodeCount")] = 1;
            item[QStringLiteral("seasons")] = QVariantList{seasonEntry};
            item[QStringLiteral("seasonCount")] = 1;
            item[QStringLiteral("resumeEpisode")] = QVariantMap{};
            if (query.value(4).toLongLong() > 0) {
                item[QStringLiteral("resumeEpisode")] = episode;
            }
            seriesIndices.insert(key, result.size());
            result.append(item);
            continue;
        }

        const int index = existing.value();
        QVariantMap series = result.at(index).toMap();
        series[QStringLiteral("favorite")] = series.value(QStringLiteral("favorite")).toBool()
                                             || query.value(6).toBool();
        series[QStringLiteral("playCount")] = series.value(QStringLiteral("playCount")).toInt()
                                              + query.value(5).toInt();
        series[QStringLiteral("lastOpenedMs")] = qMax(
            series.value(QStringLiteral("lastOpenedMs")).toLongLong(),
            query.value(7).toLongLong());
        QVariantList episodes = series.value(QStringLiteral("episodes")).toList();
        episodes.append(episode);
        series[QStringLiteral("episodes")] = episodes;
        series[QStringLiteral("episodeCount")] = episodes.size();
        QVariantList seasons = series.value(QStringLiteral("seasons")).toList();
        int seasonIndex = -1;
        for (int i = 0; i < seasons.size(); ++i) {
            if (seasons.at(i).toMap().value(QStringLiteral("seasonNumber")).toInt() == season) {
                seasonIndex = i;
                break;
            }
        }
        if (seasonIndex < 0) {
            seasons.append(seasonEntry);
        } else {
            QVariantMap currentSeason = seasons.at(seasonIndex).toMap();
            QVariantList seasonEpisodes = currentSeason.value(QStringLiteral("episodes")).toList();
            seasonEpisodes.append(episode);
            currentSeason[QStringLiteral("episodes")] = seasonEpisodes;
            if (currentSeason.value(QStringLiteral("seasonName")).toString().isEmpty()
                && !seasonName.isEmpty()) {
                currentSeason[QStringLiteral("seasonName")] = seasonName;
                currentSeason[QStringLiteral("displayName")] = seasonName;
            }
            seasons[seasonIndex] = currentSeason;
        }
        series[QStringLiteral("seasons")] = seasons;
        series[QStringLiteral("seasonCount")] = seasons.size();
        if (series.value(QStringLiteral("posterUrl")).toString().isEmpty())
            series[QStringLiteral("posterUrl")] = item.value(QStringLiteral("posterUrl"));
        if (series.value(QStringLiteral("backdropUrl")).toString().isEmpty())
            series[QStringLiteral("backdropUrl")] = item.value(QStringLiteral("backdropUrl"));
        const QVariantMap currentResume = series.value(QStringLiteral("resumeEpisode")).toMap();
        if (query.value(4).toLongLong() > 0
            && (currentResume.isEmpty()
                || query.value(7).toLongLong() > currentResume.value(QStringLiteral("lastOpenedMs")).toLongLong())) {
            series[QStringLiteral("resumeEpisode")] = episode;
            series[QStringLiteral("source")] = source;
            series[QStringLiteral("positionMs")] = query.value(4).toLongLong();
            series[QStringLiteral("durationMs")] = query.value(3).toLongLong();
        }
        result[index] = series;
    }
    for (auto it = movieIndices.cbegin(); it != movieIndices.cend(); ++it) {
        const int index = it.value();
        QVariantMap movie = result.at(index).toMap();
        QVariantList sources = movie.value(QStringLiteral("sources")).toList();
        std::stable_sort(sources.begin(), sources.end(), [](const QVariant &left, const QVariant &right) {
            const QVariantMap leftSource = left.toMap();
            const QVariantMap rightSource = right.toMap();
            if (leftSource.value(QStringLiteral("height")).toInt()
                != rightSource.value(QStringLiteral("height")).toInt()) {
                return leftSource.value(QStringLiteral("height")).toInt()
                       > rightSource.value(QStringLiteral("height")).toInt();
            }
            if (leftSource.value(QStringLiteral("bitrate")).toLongLong()
                != rightSource.value(QStringLiteral("bitrate")).toLongLong()) {
                return leftSource.value(QStringLiteral("bitrate")).toLongLong()
                       > rightSource.value(QStringLiteral("bitrate")).toLongLong();
            }
            return leftSource.value(QStringLiteral("label")).toString()
                   < rightSource.value(QStringLiteral("label")).toString();
        });

        QVariantMap preferred = sources.isEmpty() ? QVariantMap{} : sources.first().toMap();
        for (const QVariant &candidateValue : sources) {
            const QVariantMap candidate = candidateValue.toMap();
            if (candidate.value(QStringLiteral("positionMs")).toLongLong() > 0
                && (preferred.value(QStringLiteral("positionMs")).toLongLong() <= 0
                    || candidate.value(QStringLiteral("lastOpenedMs")).toLongLong()
                           > preferred.value(QStringLiteral("lastOpenedMs")).toLongLong())) {
                preferred = candidate;
            }
        }

        movie[QStringLiteral("sources")] = sources;
        movie[QStringLiteral("sourceCount")] = sources.size();
        if (!preferred.isEmpty()) {
            movie[QStringLiteral("source")] = preferred.value(QStringLiteral("source"));
            movie[QStringLiteral("favoriteSource")] = preferred.value(QStringLiteral("source"));
            movie[QStringLiteral("formatName")] = preferred.value(QStringLiteral("formatName"));
            movie[QStringLiteral("durationMs")] = preferred.value(QStringLiteral("durationMs"));
            movie[QStringLiteral("positionMs")] = preferred.value(QStringLiteral("positionMs"));
            movie[QStringLiteral("width")] = preferred.value(QStringLiteral("width"));
            movie[QStringLiteral("height")] = preferred.value(QStringLiteral("height"));
        }
        result[index] = movie;
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
    QSqlQuery lookup(database_);
    lookup.prepare(QStringLiteral("SELECT media_type, tmdb_id FROM media WHERE source = ?"));
    lookup.addBindValue(sourceKey(source));
    if (!lookup.exec() || !lookup.next()) return false;

    QSqlQuery query(database_);
    if (lookup.value(0).toString() == QLatin1String("movie") && lookup.value(1).toInt() > 0) {
        query.prepare(QStringLiteral(
            "UPDATE media SET favorite = ? WHERE media_type = 'movie' AND tmdb_id = ?"));
        query.addBindValue(favorite ? 1 : 0);
        query.addBindValue(lookup.value(1).toInt());
        return query.exec() && query.numRowsAffected() > 0;
    }
    query.prepare(QStringLiteral("UPDATE media SET favorite = ? WHERE source = ?"));
    query.addBindValue(favorite ? 1 : 0);
    query.addBindValue(sourceKey(source));
    return query.exec() && query.numRowsAffected() > 0;
}

bool MediaLibrary::markWatched(const QUrl &source)
{
    if (!database_.isOpen() || !source.isValid()) return false;

    QSqlQuery lookup(database_);
    lookup.prepare(QStringLiteral(
        "SELECT media_type, tmdb_id, series_title, title FROM media WHERE source = ?"));
    lookup.addBindValue(sourceKey(source));
    if (!lookup.exec() || !lookup.next()) return false;

    QSqlQuery update(database_);
    if (lookup.value(0).toString() == QLatin1String("episode")) {
        const int tmdbId = lookup.value(1).toInt();
        if (tmdbId > 0) {
            update.prepare(QStringLiteral(
                "UPDATE media SET position_ms = 0 WHERE media_type = 'episode' AND tmdb_id = ?"));
            update.addBindValue(tmdbId);
        } else {
            const QString seriesTitle = lookup.value(2).toString().trimmed();
            const QString fallbackTitle = seriesTitle.isEmpty()
                                              ? lookup.value(3).toString().trimmed()
                                              : seriesTitle;
            update.prepare(QStringLiteral(
                "UPDATE media SET position_ms = 0 WHERE media_type = 'episode' "
                "AND (series_title = ? COLLATE NOCASE OR "
                "(series_title = '' AND title = ? COLLATE NOCASE))"));
            update.addBindValue(fallbackTitle);
            update.addBindValue(fallbackTitle);
        }
    } else {
        update.prepare(QStringLiteral("UPDATE media SET position_ms = 0 WHERE source = ?"));
        update.addBindValue(sourceKey(source));
    }
    return update.exec() && update.numRowsAffected() > 0;
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

QVariantList MediaLibrary::genreBindings() const
{
    QVariantList result;
    if (!database_.isOpen()) return result;

    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(R"(
        SELECT g.work_type, g.genre_id, MIN(g.genre_name),
               COALESCE(NULLIF(b.display_name, ''), MIN(g.genre_name)),
               COUNT(DISTINCT g.tmdb_id)
        FROM work_genres g
        LEFT JOIN genre_bindings b
          ON b.work_type = g.work_type AND b.genre_id = g.genre_id
        GROUP BY g.work_type, g.genre_id
        ORDER BY g.work_type, MIN(g.genre_name) COLLATE NOCASE
    )"))) {
        return result;
    }

    while (query.next()) {
        result.append(QVariantMap{
            {QStringLiteral("workType"), query.value(0).toString()},
            {QStringLiteral("genreId"), query.value(1).toInt()},
            {QStringLiteral("sourceName"), query.value(2).toString()},
            {QStringLiteral("displayName"), query.value(3).toString()},
            {QStringLiteral("workCount"), query.value(4).toInt()},
        });
    }
    return result;
}

bool MediaLibrary::setGenreBinding(const QString &workType, int genreId, const QString &displayName)
{
    if (!database_.isOpen() || (workType != QLatin1String("movie") && workType != QLatin1String("tv"))
        || genreId <= 0) {
        return false;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(R"(
        INSERT INTO genre_bindings (work_type, genre_id, display_name)
        VALUES (?, ?, ?)
        ON CONFLICT(work_type, genre_id) DO UPDATE SET display_name = excluded.display_name
    )"));
    query.addBindValue(workType);
    query.addBindValue(genreId);
    query.addBindValue(displayName.trimmed());
    return query.exec();
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
