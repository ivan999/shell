#ifndef SEPARATORS_H_SENTRY
#define SEPARATORS_H_SENTRY

#include <stddef.h>

static const char *const separators[] = {
    ">", ">>", "<", "|", "&", NULL
};

enum septype {
    rdr_write = 0,
    rdr_append = 1,
    rdr_read = 2,
    conveyor = 3,
    backgrnd = 4
};

#endif
