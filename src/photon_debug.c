#if PHOTON_DEBUG
#include "photon_debug.h"
#include <stdio.h>
#include <stdarg.h>

static FILE *channels[PHOTON_NUM_CHANNELS];

struct {
    const char *file;
    int line;
    const char *func;
} ctx;

void photon_debug_context(int line, const char *file, const char *func){
    ctx.line = line;
    ctx.file = file + PHOTON_DB_CODE_ROOT_LEN + 1;
    ctx.func = func;
}

int photon_debug_begin_rec(int n, const char *out){
    if (channels[n]) return 0;
    FILE *f = fopen(out, "w");
    channels[n] = f;
    return f ? 1 : -1;
}
int photon_debug_record_raw(int n, const char *fmt, ...){
    if (!channels[n]) return 0;
    while (*fmt == '\n'){
        fputc(*fmt++, channels[n]);
        break;
    }
    if (fprintf(channels[n], "[func %s, %s:%d]\t", ctx.func, ctx.file, ctx.line) < 0) return EOF;
    va_list va;
    va_start(va, fmt);
    int res = vfprintf(channels[n], fmt, va);
    va_end(va);
    return res < 0 ? EOF : res;
}
int photon_debug_end_rec(int n){
    if (!channels[n]) return 0;
    int r = fclose(channels[n]);
    channels[n] = NULL;
    return r ? -1 : 1;
}
#endif
