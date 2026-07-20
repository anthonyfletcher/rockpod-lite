#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
# $Id$
#
# The plugin system has been removed from this fork. This file builds no
# plugins; it survives only because tools/root.make still includes it. The one
# rule kept here generates credits.raw from docs/CREDITS -- the core credits
# screen (apps/credits.c) #includes it, so it is not plugin-specific.

ROCKS :=

$(BUILDDIR)/credits.raw credits.raw: $(DOCSDIR)/CREDITS
	$(call PRINTS,Create credits.raw)perl $(APPSDIR)/plugins/credits.pl < $< > $(BUILDDIR)/$(@F)
