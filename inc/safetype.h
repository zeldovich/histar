#ifndef JOS_INC_SAFETYPE_H
#define JOS_INC_SAFETYPE_H

#define SAFE_TYPE(t)		struct { t __v; }
#define SAFE_WRAP(wt, v)	((wt) { v })
#define SAFE_UNWRAP(w)		((w).__v)
#define SAFE_EQUAL(a, b)	({ __typeof__(a) __x = b;  (a).__v == __x.__v; })

#endif
