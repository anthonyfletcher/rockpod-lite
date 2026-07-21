/***************************************************************************
 * RockPod-Lite
 *
 * Original code from RockBox
 * was: apps/filetree.h
 * Copyright (C) 2005 by Björn Stenberg
 * GNU General Public License (version 2+)
 *
 * Interface to browser_files.c (the ft_* entry points).
 ****************************************************************************/
#ifndef BROWSER_FILES_H
#define BROWSER_FILES_H
#include "browser.h"

int ft_load(struct tree_context* c, const char* tempdir);
int ft_enter(struct tree_context* c);
int ft_exit(struct tree_context* c);
int ft_assemble_path(char *buf, size_t bufsz,
                      const char* currdir, const char* filename);
int ft_build_playlist(struct tree_context* c, int start_index);
bool ft_play_playlist(char* pathname, char* dirname, char* filename);

#endif
