/* apps/ -> outside-world boundary. See apps/api/README.
 *
 * firmware/usb.c:48 includes "gui/yesno.h". The real header is now
 * apps/widgets/yesno.h; this keeps the old path resolvable without editing
 * firmware/, which is outside this fork's cleanup scope.
 */
#include "../../widgets/yesno.h"
