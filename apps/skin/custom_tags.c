/***************************************************************************
 * RockPod-Lite
 *
 * Original code from RockBox
 * was: apps/gui/skin_engine/custom_tags.c
 * This build's extra skin tags, kept out of lib/skin_parser/tag_table.c so
 * that file can track rockpod unchanged. find_tag() falls back to
 * find_custom_tag() (weak-linked there) when a tag isn't in the upstream
 * table.
 * GNU General Public License (version 2+)
 *
 * Registry of this fork's non-upstream skin tags, kept separate so the
 * additions are visible in one place.
 ****************************************************************************/

#include <string.h>
#include "tag_table.h"

/* Same row layout as legal_tags[] in tag_table.c: param_pos is the name's
 * length including the '\0', and name holds "name\0params". */
#define TAG(type,name,param,flag) {(type),sizeof(name),(name "\0" param),(flag)}
static const struct tag_info custom_tags[] =
{
    TAG(SKIN_TOKEN_VLED_BUILDING,      "lb", "",     SKIN_REFRESH_DYNAMIC),
    TAG(SKIN_TOKEN_VLED_WORKING,       "lw", "",     SKIN_REFRESH_DYNAMIC),
    TAG(SKIN_TOKEN_LOADING_ANIM,       "la", "",     SKIN_REFRESH_DYNAMIC),
    TAG(SKIN_TOKEN_SPECTRUM_BARS,      "Sb", "i|S",  SKIN_REFRESH_SPECTRUM),
    TAG(SKIN_TOKEN_LIST_ITEM_ALBUMART, "La", "|IS",  SKIN_REFRESH_DYNAMIC),
    TAG(SKIN_TOKEN_UNKNOWN,            "",   "",      0)   /* terminator */
};
#undef TAG

/* Mirrors search_tag() in tag_table.c: n is the candidate length (name length
 * plus the '\0'); the name is truncated to n-1 characters for the compare. */
static const struct tag_info* search_custom(const char* name, int n)
{
    const struct tag_info* current = custom_tags;
    while (current->param_pos > 1
        && (current->param_pos != n || strncmp(current->name, name, n - 1) != 0))
    {
        current++;
    }
    return (current->param_pos <= 1) ? NULL : current;
}

const struct tag_info* find_custom_tag(const char *name)
{
    const struct tag_info *tag = NULL;
    int i = MAX_TAG_LENGTH;
    while (!tag && i > 1)
    {
        tag = search_custom(name, i);
        i--;
    }
    return tag;
}
