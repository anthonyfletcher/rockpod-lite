/* apps/ -> outside-world boundary. See apps/api/README.
 *
 * firmware/usb.c:51 includes "gui/skin_engine/skin_engine.h". The real header
 * is now apps/skin/skin_engine.h; this keeps the old path resolvable without
 * editing firmware/, which is outside this fork's cleanup scope.
 */
#include "../../../skin/skin_engine.h"
