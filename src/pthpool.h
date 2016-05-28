#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _threadpool *pthpool_t;

pthpool_t pthpool_create(size_t count);

int pthpool_apply(pthpool_t pool, void (*func)(void *), void *arg);

int pthpool_join(pthpool_t pool);

int pthpool_terminate(pthpool_t pool);

#ifdef __cplusplus
}
#endif

#endif