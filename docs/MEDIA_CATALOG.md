# Media catalog contract

qPlay and the separate catalog scraper share
`~/.local/share/qPlay/library.sqlite3`. qPlay creates and migrates the `media`
table when it starts. The scraper should upsert rows using the canonical,
credential-free WebDAV playback URL as the unique `source` key.

## Scraper-owned metadata

| Column | Meaning |
| --- | --- |
| `title` | Display title used by the poster wall; for a series episode, use the series title. |
| `tmdb_id` | Matched TMDB movie or TV series identifier. |
| `poster_url` | TMDB poster image URL. |
| `backdrop_url` | TMDB backdrop image URL. |
| `rating` | TMDB vote average as a number. |
| `overview` | Localized movie or episode overview. |
| `release_date` | Movie or series release/first-air date in `YYYY-MM-DD` form when known. |
| `media_type` | `movie` or `episode`. |
| `series_title` | Series title for episode rows; empty for movies. |
| `season_number` | Season number, zero for specials or movies. |
| `season_name` | TMDB season name when available; qPlay falls back to a localized ordinal label. |
| `episode_number` | Episode number, zero for movies. |
| `episode_title` | Episode title; empty for movies. |
| `still_url` | TMDB episode still image URL; empty when no still is available. |

Genres belong to a logical work rather than to an individual media file. They
are stored in the `work_genres` table, keyed by `work_type` (`movie` or `tv`),
TMDB work ID, and TMDB genre ID. `genre_name` stores the localized display name.
All copies of a movie share the movie's genre assignments. Episode rows use the
series TMDB ID and map to `work_type='tv'`, so the entire show shares one set of
genres. A work can have multiple genre rows.

`genre_bindings` stores optional qPlay display terms for each TMDB genre, keyed
by work type and genre ID. The source name in `work_genres` remains unchanged;
an empty display term falls back to that source name. qPlay exposes both the
bound `name` and original `sourceName` to QML, so one display term can group
multiple source genres without rewriting scraped metadata.

The table also contains qPlay-owned playback state and technical fields:
`position_ms`, `play_count`, `favorite`, `last_opened_ms`, `duration_ms`,
`format_name`, `bitrate`, `width`, and `height`. The scraper must preserve
these values when updating an existing `source` row. qPlay preserves existing
catalog titles when it records playback progress.

The scraper reads `[webdav] url`, `username`, and `password`, plus `[tmdb]
apiKey` and `apiToken` from `~/.config/qPlay/user-config.ini`. It prefers the
TMDB API Read Access Token as a Bearer token and falls back to the API key.
Do not include credentials in
`source` URLs; qPlay supplies WebDAV Basic authentication at playback time.

SQLite upserts should run in a transaction. Store WebDAV URLs for the actual
video files (including each episode), not folder URLs. qPlay loads catalog
changes on startup, groups episode rows by TMDB series ID, and exposes seasons
and paged episode cards separately on the details page. Each card can show its
TMDB still image, episode number, title, and synopsis. Restart qPlay after a
scraper run to refresh the poster wall.

The scraper import format accepts either the legacy media-record array or an
object containing `media` and `works` arrays. `media` still has one record per
video file. `works` has one record per matched movie or TV series, with
`work_type`, `tmdb_id`, and a `genres` array of `{id, name}` objects. Genre
assignments and media metadata are committed in the same transaction. Omitting
`works` leaves existing genre assignments untouched; including a work with an
empty `genres` array clears that work's TMDB genre assignments.
