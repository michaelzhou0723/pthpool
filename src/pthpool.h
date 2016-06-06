#ifndef _PTHPOOL_H
#define _PTHPOOL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _future *pthpool_future_t;

typedef struct _threadpool *pthpool_t;

pthpool_t pthpool_create(size_t count);

pthpool_future_t pthpool_apply(pthpool_t pool, void *(*func)(void *), void *arg);

int pthpool_join(pthpool_t pool);

int pthpool_terminate(pthpool_t pool);

void *pthpool_future_get(pthpool_future_t future, unsigned int seconds);

int pthpool_future_destroy(pthpool_future_t future);

int pthpool_future_ready(pthpool_future_t future);

int pthpool_future_timeout(pthpool_future_t future);

#ifdef __cplusplus
}
#endif

#endif
