/* apps/ -> outside-world boundary. See apps/api/README.
 *
 * firmware/powermgmt.c includes "splash.h" by bare name. It used to resolve
 * because tools/configure put apps/gui on the include path; that is no longer
 * the case, so this stub is now load-bearing rather than merely documentary.
 * Update the path below when the real header moves.
 */
#include "../gui/splash.h"
