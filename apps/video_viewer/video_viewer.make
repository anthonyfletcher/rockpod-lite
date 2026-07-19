#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
# $Id$
#
# Core-linked video viewer (ported from the mpegplayer plugin).
#
# The MP2/MP3 audio path uses libmad. lib/rbcodec/codecs/libmad/libmad.make
# already builds a second variant of libmad, libmad-mpeg.a, from the same
# sources with -DMPEGPLAYER so they call the viewer's codec_malloc/codec_free
# (alloc.c) instead of libmad's own allocator. Link that archive into the core
# binary, exactly as the plugin's .rock did.
#
# The viewer's own sources are built through apps/SOURCES with the standard
# core CFLAGS; only the audio decoder library needs the special variant.

CORE_LIBS += $(CODECDIR)/libmad-mpeg.a
