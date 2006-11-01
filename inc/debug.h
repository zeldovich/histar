#ifndef JOS_INC_DEBUG_H_
#define JOS_INC_DEBUG_H_

#define debug_print(__exp, __frmt, __args...) \
    do {                    \
    if (__exp)                \
        printf("(debug) %s: " __frmt "\n", __FUNCTION__, ##__args);   \
    } while (0)

#endif 
