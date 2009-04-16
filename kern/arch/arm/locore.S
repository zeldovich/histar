#if defined(JOS_ARM_GOLDFISH)
#include "locore_goldfish.S"
#elif defined(JOS_ARM_HTCDREAM)
#include "locore_htcdream.S"
#else
#error unknown arm target
#endif
