#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
# $Id$
#

BITMAPDIR = $(ROOTDIR)/apps/bitmaps
BMPINCDIR = $(BUILDDIR)/bitmaps

INCLUDES += -I$(BMPINCDIR)

ifneq ($(strip $(BMP2RB_MONO)),)
BMP = $(call preprocess, $(BITMAPDIR)/mono/SOURCES)
endif
ifneq ($(strip $(BMP2RB_NATIVE)),)
BMP += $(call preprocess, $(BITMAPDIR)/native/SOURCES)
endif
# No remote_mono / remote_native stanzas: these targets have no remote LCD, so
# tools/configure leaves BMP2RB_REMOTEMONO and BMP2RB_REMOTENATIVE empty and
# those SOURCES were never read. Both directories have been deleted.

BMPOBJ = $(call full_path_subst,$(ROOTDIR)/%.bmp,$(BUILDDIR)/%.o,$(BMP))

# The generated headers the core build includes. usblogo.h, remote_usblogo.h,
# remote_default_icons.h, rockboxicon.h and toolsicon.h were listed here
# upstream but can never be produced now -- their bitmaps are gone.
BMPHFILES = $(BMPINCDIR)/default_icons.h \
	$(BMPINCDIR)/rockpodlogo.h $(BMPINCDIR)/rockpodcredits.h \
	$(BMPINCDIR)/rockpodusb.h $(BMPINCDIR)/rockpodtext.h \
	$(BMPINCDIR)/rockpodpicture.h $(BMPINCDIR)/no_album_cover.h \
	$(BMPINCDIR)/rockpodnoartistcover.h

$(BMPHFILES): $(BMPOBJ)

# pattern rules to create .c files from .bmp, one for each subdir:
$(BUILDDIR)/apps/bitmaps/mono/%.c: $(ROOTDIR)/apps/bitmaps/mono/%.bmp $(TOOLSDIR)/bmp2rb
	$(SILENT)mkdir -p $(dir $@) $(BMPINCDIR)
	$(call PRINTS,BMP2RB $(<F))$(BMP2RB_MONO) -b -h $(BMPINCDIR) $< > $@

$(BUILDDIR)/apps/bitmaps/native/%.c: $(ROOTDIR)/apps/bitmaps/native/%.bmp $(TOOLSDIR)/bmp2rb
	$(SILENT)mkdir -p $(dir $@) $(BMPINCDIR)
	$(call PRINTS,BMP2RB $(<F))$(BMP2RB_NATIVE) -b -h $(BMPINCDIR) $< > $@

$(BUILDDIR)/apps/bitmaps/remote_mono/%.c: $(ROOTDIR)/apps/bitmaps/remote_mono/%.bmp $(TOOLSDIR)/bmp2rb
	$(SILENT)mkdir -p $(dir $@) $(BMPINCDIR)
	$(call PRINTS,BMP2RB $(<F))$(BMP2RB_REMOTEMONO) -b -h $(BMPINCDIR) $< > $@

$(BUILDDIR)/apps/bitmaps/remote_native/%.c: $(ROOTDIR)/apps/bitmaps/remote_native/%.bmp $(TOOLSDIR)/bmp2rb
	$(SILENT)mkdir -p $(dir $@) $(BMPINCDIR)
	$(call PRINTS,BMP2RB $(<F))$(BMP2RB_REMOTENATIVE) -b -h $(BMPINCDIR) $< > $@
