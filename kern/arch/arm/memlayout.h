#if defined(JOS_ARM_GOLDFISH)
#include "memlayout_goldfish.h"
#elif defined(JOS_ARM_HTCDREAM)
#include "memlayout_htcdream.h"
#else
#error unknown arm target
#endif
