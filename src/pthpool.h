#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

enum _future_flags {
    _FUTURE_READY = 01,
    _FUTURE_TIMEOUT = 02,
};

typedef struct _future *pthpool_future_t;

typedef struct _threadpool *pthpool_t;

pthpool_t pthpool_create(size_t count);

int pthpool_apply(pthpool_t pool, void *(*func)(void *), void *arg, pthpool_future_t future);

int pthpool_join(pthpool_t pool);

int pthpool_terminate(pthpool_t pool);

pthpool_future_t pthpool_future_create(void);

int pthpool_future_destroy(pthpool_future_t future);

void *pthpool_future_wait(struct _future *future, unsigned int seconds);

#define pthpool_future_ready(f) (((f)->flag & _FUTURE_READY) != 0)

#define pthpool_future_timeout(f) (((f)->flag & _FUTURE_TIMEOUT) != 0)

#ifdef __cplusplus
}
#endif

#endif
