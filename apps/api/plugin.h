/* apps/ -> outside-world boundary. See apps/api/README.
 *
 * Vestigial. lib/rbcodec/metadata/hes.c has a leftover `#include "plugin.h"`
 * from when this fork still had a plugin system. The include needs nothing
 * from this header -- the build succeeds with it empty -- but lib/ is outside
 * the scope this fork's cleanup is confined to, so the include stays and this
 * file keeps it satisfied.
 *
 * Unlike the other stubs here, this one forwards to nothing: apps/plugin.h was
 * deleted, since no file inside apps/ included it.
 *
 * To finish the job: delete lib/rbcodec/metadata/hes.c:12, then delete this.
 */

#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#endif /* _PLUGIN_H_ */
