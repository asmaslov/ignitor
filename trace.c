#include "trace.h"
#include <stdarg.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

static inline void outcharDefault(char c) {}
static inline void flushDefault(void) {}

/****************************************************************************
 * Public types/enumerations/variables                                      *
 ****************************************************************************/

FILE trace;
OutcharFunc outchar = outcharDefault;
FlushFunc flush = flushDefault;

/****************************************************************************
 * Private functions                                                        *
 ****************************************************************************/

static int trace_putchar(char c, FILE *stream) {
    outchar(c);
    return 0;
}

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

void trace_setup(OutcharFunc o, FlushFunc f)
{
    fdev_setup_stream(&trace, trace_putchar, NULL, _FDEV_SETUP_RW);
    if (o && f) {
        outchar = o;
        flush = f;
    }
}
