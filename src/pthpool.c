#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include "pthpool.h"

#ifdef CLOCK_MONOTONIC
#define _PTHPOOL_CLOCKID CLOCK_MONOTONIC
#else
#define _PTHPOOL_CLOCKID CLOCK_REALTIME
#endif

enum _future_flags {
    _FUTURE_READY = 01,
    _FUTURE_TIMEOUT = 02,
    _FUTURE_CANCELLED = 04,
    _FUTURE_DESTROYED = 10,
};

typedef struct _threadtask {
    void *(*func)(void *);
    void *arg;
    struct _future *future;
    struct _threadtask *next;
} threadtask_t;

typedef struct _jobqueue {
    threadtask_t *head;
    threadtask_t *tail;
    pthread_cond_t cond_nonempty;
    pthread_mutex_t mutex_rwlock;
} jobqueue_t;

struct _future {
    int flag;
    void *result;
    pthread_mutex_t mutex_flag;
    pthread_cond_t cond_ready;
};

struct _threadpool {
    size_t count;
    pthread_t *workers;
    jobqueue_t *jobqueue;
};

struct _future *pthpool_future_create(void)
{
    struct _future *future = (struct _future *)malloc(sizeof(struct _future));
    
    if (future) {
        future->flag = 0;
        future->result = NULL;
        pthread_mutex_init(&future->mutex_flag, NULL);
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
        pthread_condattr_setclock(&attr, _PTHPOOL_CLOCKID);
        pthread_cond_init(&future->cond_ready, &attr);
        pthread_condattr_destroy(&attr);
    }
    
    return future;
}

int pthpool_future_destroy(struct _future *future)
{
    if (future) {
        pthread_mutex_lock(&future->mutex_flag);
        if (future->flag & _FUTURE_READY || future->flag & _FUTURE_CANCELLED) {
            pthread_mutex_unlock(&future->mutex_flag);
            pthread_mutex_destroy(&future->mutex_flag);
            pthread_cond_destroy(&future->cond_ready);
            free(future);
        }
        else {
            future->flag |= _FUTURE_DESTROYED;
            pthread_mutex_unlock(&future->mutex_flag);
        }
    }
    
    return 0;
}

void *pthpool_future_get(struct _future *future, unsigned int seconds)
{
    int status;
    
    pthread_mutex_lock(&future->mutex_flag);
    future->flag &= _FUTURE_READY;      // turn off the timeout bit
    while ((future->flag & _FUTURE_READY) == 0) {
        if (seconds) {
            struct timespec expire_time;
            clock_gettime(_PTHPOOL_CLOCKID, &expire_time);
            expire_time.tv_sec += seconds;
            status = pthread_cond_timedwait(&future->cond_ready, &future->mutex_flag, &expire_time);
            if (status == ETIMEDOUT) {
                future->flag |= _FUTURE_TIMEOUT;
                pthread_mutex_unlock(&future->mutex_flag);
                return NULL;
            }
        }
        else {
            pthread_cond_wait(&future->cond_ready, &future->mutex_flag);
        }
    }
    pthread_mutex_unlock(&future->mutex_flag);
    
    return future->result;
}

int pthpool_future_timeout(struct _future *future)
{
    int status;
    
    pthread_mutex_lock(&future->mutex_flag);
    status = future->flag & _FUTURE_TIMEOUT;
    pthread_mutex_unlock(&future->mutex_flag);
    
    return status;
}

static jobqueue_t *jobqueue_create(void)
{
    jobqueue_t *jobqueue = (jobqueue_t *)malloc(sizeof(jobqueue_t));
    
    if (jobqueue) {
        jobqueue->head = jobqueue->tail = NULL;
        pthread_cond_init(&jobqueue->cond_nonempty, NULL);
        pthread_mutex_init(&jobqueue->mutex_rwlock, NULL);
    }
    
    return jobqueue;
}

static void jobqueue_destroy(jobqueue_t *jobqueue)
{
    threadtask_t *tmp = jobqueue->head;
    
    while (tmp) {
        jobqueue->head = jobqueue->head->next;
        if (tmp->future->flag & _FUTURE_DESTROYED) {
            pthread_mutex_destroy(&tmp->future->mutex_flag);
            pthread_cond_destroy(&tmp->future->cond_ready);
            free(tmp->future);            
        }
        else {
            pthread_mutex_lock(&tmp->future->mutex_flag);
            tmp->future->flag |= _FUTURE_CANCELLED;
            pthread_mutex_unlock(&tmp->future->mutex_flag);
        }
        free(tmp);
        tmp = jobqueue->head;
    }
    pthread_mutex_destroy(&jobqueue->mutex_rwlock);
    pthread_cond_destroy(&jobqueue->cond_nonempty);
    free(jobqueue);
}

void _jobqueue_fetch_cleanup(void *arg) {
    pthread_mutex_t *mutex = (pthread_mutex_t *)arg;
    pthread_mutex_unlock(mutex);
}

static void *jobqueue_fetch(void *queue)
{
    jobqueue_t *jobqueue = (jobqueue_t *)queue;
    threadtask_t *tmp, *task;
    void *ret_value;
    int old_state;
    
    pthread_cleanup_push(_jobqueue_fetch_cleanup, (void *)&jobqueue->mutex_rwlock);
    
    while (1) {
        pthread_mutex_lock(&jobqueue->mutex_rwlock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
        pthread_testcancel();
                
        while (jobqueue->tail == NULL) {
            pthread_cond_wait(&jobqueue->cond_nonempty, &jobqueue->mutex_rwlock);
        }
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);
        if (jobqueue->head == jobqueue->tail) {
            task = jobqueue->tail;
            jobqueue->head = jobqueue->tail = NULL;
        }
        else {
            for (tmp = jobqueue->head; tmp->next != jobqueue->tail; tmp = tmp->next) {
                ;
            }
            task = tmp->next;
            tmp->next = NULL;
            jobqueue->tail = tmp;
        }
        pthread_mutex_unlock(&jobqueue->mutex_rwlock);
        
        if (task->func) {
            ret_value = task->func(task->arg);
            pthread_mutex_lock(&task->future->mutex_flag);
            if (task->future->flag & _FUTURE_DESTROYED) {
                pthread_mutex_unlock(&task->future->mutex_flag);
                pthread_mutex_destroy(&task->future->mutex_flag);
                pthread_cond_destroy(&task->future->cond_ready);
                free(task->future);
            }
            else {
                task->future->flag |= _FUTURE_READY;
                task->future->result = ret_value;
                pthread_mutex_unlock(&task->future->mutex_flag);
                pthread_cond_broadcast(&task->future->cond_ready);                    
            }
            free(task);
        }
        else {
            pthread_mutex_destroy(&task->future->mutex_flag);
            pthread_cond_destroy(&task->future->cond_ready);
            free(task->future);
            free(task);
            break;
        }
    }
    
    pthread_cleanup_pop(0);
    pthread_exit(NULL);
}

struct _threadpool *pthpool_create(size_t count)
{
    jobqueue_t *jobqueue = jobqueue_create();
    struct _threadpool *pool = (struct _threadpool *)malloc(sizeof(struct _threadpool));
    
    if (!jobqueue || !pool) {
        return NULL;
    }
    pool->count = count;
    pool->jobqueue = jobqueue;
    if ((pool->workers = (pthread_t *)malloc(count * sizeof(pthread_t)))) {
        int i, j;
        for (i = 0; i < count; i++) {
            if (pthread_create(&pool->workers[i], NULL, jobqueue_fetch, (void *)jobqueue)) {
                for (j = 0; j < i; j++) {
                    pthread_cancel(pool->workers[j]);
                }
                for (j = 0; j < i; j++) {
                    pthread_join(pool->workers[j], NULL);
                }
                free(pool->workers);
                jobqueue_destroy(jobqueue);
                free(pool);
                return NULL;
            }
        }
        return pool;
    }
    jobqueue_destroy(jobqueue);
    free(pool);
    return NULL;
}

struct _future *pthpool_apply(struct _threadpool *pool, void *(*func)(void *), void *arg)
{
    jobqueue_t *jobqueue = pool->jobqueue;
    threadtask_t *new_head = (threadtask_t *)malloc(sizeof(threadtask_t));
    struct _future *future = pthpool_future_create();
    
    if (new_head && future) {
        new_head->func = func;
        new_head->arg = arg;
        new_head->future = future;
        pthread_mutex_lock(&jobqueue->mutex_rwlock);
        if (jobqueue->head) {
            new_head->next = jobqueue->head;
            jobqueue->head = new_head;
        }
        else {
            jobqueue->head = jobqueue->tail = new_head;
            pthread_cond_broadcast(&jobqueue->cond_nonempty);
        }
        pthread_mutex_unlock(&jobqueue->mutex_rwlock);
    }
    return future;
}

int pthpool_join(struct _threadpool *pool)
{
    int i;
    size_t num_threads = pool->count;
    
    for (i = 0; i < num_threads; i++) {
        pthpool_apply(pool, NULL, NULL);
    }
    for (i = 0; i < num_threads; i++) {
        pthread_join(pool->workers[i], NULL);
    }
    free(pool->workers);
    jobqueue_destroy(pool->jobqueue);
    free(pool);
    
    return 0;
}

int pthpool_terminate(struct _threadpool *pool)
{
    size_t num_threads = pool->count;
    int i;
    
    for (i = 0; i < num_threads; i++) {
        pthread_cancel(pool->workers[i]);
    }
    for (i = 0; i < num_threads; i++) {
        pthread_join(pool->workers[i], NULL);
    }
    free(pool->workers);
    jobqueue_destroy(pool->jobqueue);
    free(pool);
    
    return 0;
}
