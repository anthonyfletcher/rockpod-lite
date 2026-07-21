/* apps/ -> outside-world boundary shim. See apps/api/README.
 *
 * This is the one boundary header that cannot live in apps/api/, because its
 * consumer does not name it by a plain path:
 *
 *     lib/rbcodec/codecs/spc.c:29   #include "../fracmul.h"
 *
 * A "../" include is resolved against each -I directory in turn, so it only
 * ever worked because tools/configure put apps/gui on the include path and
 * apps/gui/../fracmul.h landed here. apps.make now supplies -I$(APPSDIR)/api
 * instead, so apps/api/../fracmul.h lands here just the same -- but only for
 * as long as SOME apps/ subdirectory is on the include path. Keep the api
 * entry in apps.make's INCLUDES or this breaks.
 *
 * The real header is apps/system/fracmul.h. lib/ is outside the scope this
 * fork's cleanup is confined to, so spc.c cannot be edited to say so.
 */
#include "system/fracmul.h"
