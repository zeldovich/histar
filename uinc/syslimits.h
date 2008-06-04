/*
 * We have our own syslimits.h file because we place gcc's system include
 * directories after our own include directories (unlike typical Linux,
 * where gcc's headers come first).  This is because gcc's limits.h fails
 * to #include_next our limits.h, so we have to place ours first.  However,
 * syslimits.h does an #include_next on limits.h, so we need to replicate
 * it here, earlier in the include path, so we don't run off the include
 * path list.
 */

#include_next <limits.h>
