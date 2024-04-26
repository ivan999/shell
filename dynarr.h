#ifndef DYNARR_H_SENTRY
#define DYNARR_H_SENTRY

#include <stdlib.h>

#define INIT_DYNARR(ARR, LEN, SIZE) \
    ARR = NULL; \
    LEN = 0; \
    SIZE = 0

#define RESIZE_DYNARR(ARR, TYPE, SIZE, ADDSIZE) \
    SIZE += ADDSIZE; \
    ARR = realloc(ARR, sizeof(TYPE) * (SIZE))

#define SIZEMOD_DYNARR(ARR, TYPE, LEN, SIZE, ADDSIZE) \
    if(LEN >= SIZE) { \
        RESIZE_DYNARR(ARR, TYPE, SIZE, ADDSIZE); \
    }

#endif
