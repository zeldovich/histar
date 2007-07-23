#ifndef JOS_INC_FEATURES_H
#define JOS_INC_FEATURES_H

#if defined(JOS_ARCH_amd64)
enum { __jos_entry_allregs = 1 };
#define JOS_THREAD_ENTRY_GCCATTR regparm(1)
#elif defined(JOS_ARCH_i386)
enum { __jos_entry_allregs = 0 };
#define JOS_THREAD_ENTRY_GCCATTR regparm(1)
#elif defined(JOS_ARCH_sparc)
enum { __jos_entry_allregs = 0 };
#define JOS_THREAD_ENTRY_GCCATTR
#else
#error Unknown CPU architecture
#endif

#endif
