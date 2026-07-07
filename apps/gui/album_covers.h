#ifndef _ALBUM_COVERS_H_
#define _ALBUM_COVERS_H_

/* Values for global_settings.album_covers_show_album_name */
enum show_album_name_values {
    ALBUM_NAME_HIDE = 0,
    ALBUM_NAME_BOTTOM,
    ALBUM_NAME_TOP,
    ALBUM_AND_ARTIST_TOP,
    ALBUM_AND_ARTIST_BOTTOM
};

/* Values for global_settings.album_covers_sort_albums_by */
enum sort_albums_by_values {
    SORT_BY_ARTIST_AND_NAME = 0,
    SORT_BY_ARTIST_AND_YEAR,
    SORT_BY_YEAR,
    SORT_BY_NAME,

    SORT_VALUES_SIZE
};

/* Values for global_settings.album_covers_year_sort_order */
enum year_sort_order_values {
    ASCENDING = 0,
    DESCENDING
};

/* selected_file: jump to this file's album on open (e.g. onplay.c's "Album
 * covers" context-menu item on a specific track); NULL for the normal
 * entry paths (main menu, WPS shortcuts) -- falls back to the currently
 * playing track's album, or wherever was last viewed. */
int album_covers(const char *selected_file);

/* Forces the on-disk album art cache to be rebuilt (or filled in, in the
 * "update" case) the next time Album covers opens. Used both by the
 * in-screen main menu's own Rebuild/Update Cache actions and by the
 * Settings > Album covers menu (apps/menus/album_covers_menu.c). */
void album_covers_rebuild_cache(void);
void album_covers_update_cache(void);

/* Name/artist of the cover currently centered in the coverflow -- backs the
 * %Cn/%Ca theme tokens (apps/gui/skin_engine/skin_tokens.c). Safe to call
 * even if Album covers has never been opened this session (returns ""). */
const char *album_covers_current_name(void);
const char *album_covers_current_artist(void);

#endif
