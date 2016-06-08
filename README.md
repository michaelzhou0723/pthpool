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
5. Destroy the future object once it's no longer been used: `pthpool_future_destroy(future);`
6. Wait for all pending tasks to complete and destroy the thread pool: `pthpool_join(pool);`
