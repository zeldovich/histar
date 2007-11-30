#define GCC_WRAPS_COMMON \
    GCC_WRAP(_Unwind_Backtrace) \
    GCC_WRAP(_Unwind_Resume) \
    GCC_WRAP(_Unwind_GetIP)

#if defined(JOS_ARCH_i386)
#define GCC_WRAPS_ARCH \
    GCC_WRAP(__moddi3) \
    GCC_WRAP(__umoddi3) \
    GCC_WRAP(__divdi3) \
    GCC_WRAP(__udivdi3)
#else
#define GCC_WRAPS_ARCH
#endif
