/* apps/ -> outside-world boundary. See apps/api/README.
 *
 * apps/misc.c and apps/misc.h were split into six files in stage 6 of the
 * apps/ reorganisation. This header preserves the old name for code outside
 * apps/, which cannot be edited from this fork's cleanup scope.
 *
 * It is an UMBRELLA, not a narrow forward, and deliberately so. Only
 * firmware/powermgmt.c uses symbols from here (read_line, settings_parseline,
 * both now in system/strutil.h) -- but firmware/scroll_engine.c and
 * firmware/usb.c also include "misc.h" while using no symbol from it, which
 * means they may be relying on what it pulled in transitively. Including all
 * six parts keeps their view identical to before the split.
 *
 * If firmware/ ever becomes editable: point powermgmt.c at
 * apps/system/strutil.h, delete the misc.h includes in scroll_engine.c and
 * usb.c, then delete this file.
 */
#include "../system/strutil.h"
#include "../system/format_time.h"
#include "../system/shutdown.h"
#include "../system/volume.h"
#include "../system/activity.h"
#include "../system/app_util.h"
