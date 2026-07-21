/***************************************************************************
 * RockPod-Lite
 *
 * Original code from RockBox
 * was: apps/playlist_menu.h
 * Copyright (C) 2002 Björn Stenberg
 * GNU General Public License (version 2+)
 *
 * Declares save_playlist_screen(), the "save current playlist" screen.
 * Named for what it declares: it is a screen, not a menu, and is unrelated
 * to the playlist settings screen that used to share its name.
 ****************************************************************************/
#ifndef _SAVE_SCREEN_H
#define _SAVE_SCREEN_H

#include "playlist.h"

int save_playlist_screen(struct playlist_info* playlist);

#endif
