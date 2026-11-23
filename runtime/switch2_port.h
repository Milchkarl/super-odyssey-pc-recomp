// What the Switch 2 controller code (switch2.c, switch2_win.c; ported from dkb-pc-recomp, itself from
// adriadam10/mkw-recomp, MIT) needs from the runtime around it: the log, the setting, a mutex and a thread. Kept in
// one place so those files stay close to their source.
#pragma once
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "settings.h"

// "input: ..." lines from the ported code as this runtime's "[input] ..." log lines
static inline void rt_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static inline void rt_log(const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    const char *m = strncmp(msg, "input: ", 7) ? msg : msg + 7;
    fprintf(stderr, "[input] %s\n", m);
}

// Switch 2 controllers over Bluetooth: settings.ini `switch2` (0 off, 1 on, 2 on and logs every Bluetooth LE device
// that advertises, once). ODYSSEY_SWITCH2 overrides it.
static inline int s2_setting(void) {
    const char *e = getenv("ODYSSEY_SWITCH2");
    return e && *e ? atoi(e) : g_settings.switch2;
}

typedef pthread_mutex_t PlatMutex;
#define PLAT_MUTEX_INIT PTHREAD_MUTEX_INITIALIZER
#define plat_mutex_lock pthread_mutex_lock
#define plat_mutex_unlock pthread_mutex_unlock

typedef struct { void (*fn)(void *); void *arg; } S2ThreadStart;
static inline void *s2_thread_trampoline(void *p) {
    S2ThreadStart t = *(S2ThreadStart *)p;
    free(p);
    t.fn(t.arg);
    return NULL;
}
// A detached thread running fn(arg); 1 on success
static inline int plat_thread_spawn(void (*fn)(void *), void *arg, const char *name, int prio) {
    (void)name, (void)prio;
    S2ThreadStart *t = (S2ThreadStart *)malloc(sizeof *t);
    if (!t) return 0;
    t->fn = fn, t->arg = arg;
    pthread_t th;
    if (pthread_create(&th, NULL, s2_thread_trampoline, t)) { free(t); return 0; }
    pthread_detach(th);
    return 1;
}
