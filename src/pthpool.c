#include <stdlib.h>
#include <pthread.h>
#include "pthpool.h"

struct _future {
    int ready;
    void *result;
    pthread_mutex_t mutex_ready;
    pthread_cond_t cond_ready;
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

struct _threadpool {
    size_t count;
    pthread_t *workers;
    jobqueue_t *jobqueue;
};

static jobqueue_t *jobqueue_create(void)
{
    jobqueue_t *jobqueue = malloc(sizeof(jobqueue_t));
    
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
        free(tmp);
        tmp = jobqueue->head;
    }
    pthread_mutex_destroy(&jobqueue->mutex_rwlock);
    pthread_cond_destroy(&jobqueue->cond_nonempty);
    free(jobqueue);
}

static void *jobqueue_fetch(void *queue)
{
    jobqueue_t *jobqueue = (jobqueue_t *)queue;
    threadtask_t *tmp, *task;
    void *ret_value;
    
    while (1) {
        pthread_testcancel();
        pthread_mutex_lock(&jobqueue->mutex_rwlock);
        while (jobqueue->tail == NULL) {
            pthread_cond_wait(&jobqueue->cond_nonempty, &jobqueue->mutex_rwlock);
        }
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
            if (task->future) {
                pthread_mutex_lock(&task->future->mutex_ready);
                task->future->ready = 1;
                task->future->result = ret_value;
                pthread_mutex_unlock(&task->future->mutex_ready);
                pthread_cond_signal(&task->future->cond_ready);
            }
            free(task);
        }
        else {
            free(task);
            pthread_exit(NULL);
        }
    }
}

struct _future *pthpool_future_create(void)
{
    struct _future *future = (struct _future *)malloc(sizeof(struct _future));
    
    if (future) {
        future->ready = 0;
        future->result = NULL;
        pthread_mutex_init(&future->mutex_ready, NULL);
        pthread_cond_init(&future->cond_ready, NULL);
    }
    
    return future;
}

int pthpool_future_destroy(struct _future *future)
{
    if (future) {
        pthread_mutex_destroy(&future->mutex_ready);
        pthread_cond_destroy(&future->cond_ready);
    }
    free(future);
    
    return 0;
}

void *pthpool_future_wait(struct _future *future)
{
    pthread_mutex_lock(&future->mutex_ready);
    while (future->ready == 0) {
        pthread_cond_wait(&future->cond_ready, &future->mutex_ready);
    }
    pthread_mutex_unlock(&future->mutex_ready);
    
    return future->result;
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

int pthpool_apply(struct _threadpool *pool, void *(*func)(void *), void *arg, struct _future *future)
{
    jobqueue_t *jobqueue = pool->jobqueue;
    threadtask_t *new_head = (threadtask_t *)malloc(sizeof(threadtask_t));
    
    if (new_head) {
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
        return 0;
    }
    return -1;
}

int pthpool_join(struct _threadpool *pool)
{
    size_t num_threads = pool->count;
    int i;
    
    for (i = 0; i < num_threads; i++) {
        pthpool_apply(pool, NULL, NULL, NULL);
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
