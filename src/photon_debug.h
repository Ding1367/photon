#ifndef __PHOTON_DEBUG_H__
#define __PHOTON_DEBUG_H__

#if PHOTON_DEBUG
#define PHOTON_DEBUG_OPT(...) __VA_ARGS__

#ifndef PHOTON_NUM_CHANNELS
#define PHOTON_NUM_CHANNELS 128
#endif

void photon_debug_context(int line, const char *file, const char *func);
int photon_debug_begin_rec(int n, const char *out);
int photon_debug_record_raw(int n, const char *fmt, ...);
#define photon_debug_record(...) (photon_debug_context(__LINE__, __FILE__, __func__), photon_debug_record_raw(__VA_ARGS__))
int photon_debug_end_rec(int n);
#else
#define PHOTON_DEBUG_OPT(...)
#endif

#endif//__PHOTON_DEBUG_H__
