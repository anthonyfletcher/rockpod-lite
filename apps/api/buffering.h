/* apps/ -> outside-world boundary. See apps/api/README.
 *
 * firmware/ and/or lib/ include "buffering.h" by bare name. This stub keeps that
 * name resolvable from the include path no matter where the real header
 * lives inside apps/, so apps/ can be reorganised without editing files
 * outside it. Update the path below when the real header moves.
 */
#include "../buffering.h"
