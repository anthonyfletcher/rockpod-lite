#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
# $Id$
#

INCLUDES += -I$(APPSDIR) $(patsubst %,-I$(APPSDIR)/%,$(subst :, ,$(APPEXTRA)))
SRC += $(call preprocess, $(APPSDIR)/SOURCES)

# apps/features.txt is a file that (is preprocessed and) lists named features
# based on defines in the config-*.h files. The named features will be passed
# to genlang and thus (translated) phrases can be used based on those names.
# button.h is included for the HAS_BUTTON_HOLD define.
#
# Kludge: depends on config.o which only depends on config-*.h to have config.h
# changes trigger a genlang re-run
#

ifneq (,$(USE_LTO))
$(BUILDDIR)/apps/features: PPCFLAGS += -DUSE_LTO
endif

$(BUILDDIR)/apps/features: $(APPSDIR)/features.txt  $(BUILDDIR)/firmware/common/config.o
	$(SILENT)mkdir -p $(BUILDDIR)/apps
	$(SILENT)mkdir -p $(BUILDDIR)/lang
	$(call PRINTS,PP $(<F))
	$(SILENT)$(CC) $(PPCFLAGS) \
                 -E -P -imacros "config.h" -imacros "button.h" -x c $< | \
		grep -v "^#" | grep -v "^ *$$" > $(BUILDDIR)/apps/features; \

$(BUILDDIR)/apps/genlang-features:  $(BUILDDIR)/apps/features
	$(call PRINTS,GEN $(subst $(BUILDDIR)/,,$@))tr \\n : < $< > $@

# The core credits screen (apps/credits.c) #includes this generated name list.
# The rule used to live in apps/plugins/plugins.make, which meant credits.raw
# was only generated when ENABLEDPLUGINS=yes -- a tie to the dead plugin build
# that would have silently stopped generating it if that flag ever flipped.
# Nothing about it is plugin-specific, so it lives here now.
$(BUILDDIR)/credits.raw credits.raw: $(DOCSDIR)/CREDITS
	$(call PRINTS,Create credits.raw)perl $(APPSDIR)/plugins/credits.pl < $< > $(BUILDDIR)/$(@F)

$(BUILDDIR)/apps/credits.o: $(BUILDDIR)/credits.raw

ASMDEFS_SRC += $(APPSDIR)/core_asmdefs.c
