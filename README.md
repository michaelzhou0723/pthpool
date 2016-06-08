# Pthpool

Pthpool is a lightweight, POSIX compliant thread pool implementation that aims to achieve a balance between useful functionality and simple API.

- Easy integration with existing code using pthread
- Timed wait for asynchronous execution result
- Flexible to cancel pending tasks

## Usage

Below is a typical workflow when using Pthpool. Refer to the API section for more detailed documentation.

1. Include the header file: `#include "pthpool.h"`
2. Create a thread pool with desired number of threads: `pthpool_t pool = pthpool_create(NUM_OF_THREADS);`
3. Submit tasks to the thread pool: `pthpool_future_t future = pthpool_apply(pool, task, (void *)arg);`
4. Wait for arbitrary seconds to get the result of execution: `void *result = pthpool_future_get(future, seconds);`
5. Destroy the future object once it's no longer used: `pthpool_future_destroy(future);`
6. Wait for all pending tasks to complete and destroy the thread pool: `pthpool_join(pool);`
7. Compile the source files with *pthpool.c* and option *-pthread*

## API

**pthpool_t pthpool_create(size_t count)**  

Create a thread pool containing specified number of threads. If successful, the thread pool is returned. Otherwise, `pthpool_create()` returns `NULL`.
</br></br>
**pthpool_future_t pthpool_apply(pthpool_t pool, void *(*func)(void *), void *arg)**  
Schedules the function *func* to be executed as `func(arg)`. If successful, a future object representing the execution of the task is returned. Otherwise, `pthpool_apply()` returns `NULL`.
</br></br>
**int pthpool_join(pthpool_t pool)**  
Wait for all pending tasks to complete execution before destroying the thread pool.
</br></br>
**int pthpool_terminate(pthpool_t pool)**  
Cancel all pending tasks and destroy the thread pool.
</br></br>
**void *pthpool_future_get(pthpool_future_t future, unsigned int seconds)**  
Return the result when it becomes available. If *seconds* is non-zero and the result does not arrive within specified time, `NULL` is returned. Each `pthpool_future_get()` resets the timeout status on *future*.
</br></br>
**int pthpool_future_cancel(pthpool_future_t future)**  
Attempt to cancel the task execution. If the function is either running or finished, cancellation fails and returns `-1`. If successful, `0` is returned.
</br></br>
**int pthpool_future_destroy(pthpool_future_t future)**  
Destroy the future object and free resources once it's no longer used. It's an error to refer to a destroyed future object.
</br></br>
**int pthpool_future_finished(pthpool_future_t future)**  
Return `1` if the task completes execution, `0` otherwise.
</br></br>
**int pthpool_future_timeout(pthpool_future_t future)**  
Return `1` if the last `pthpool_future_get()` on *future* results in a timeout, `0` otherwise.
