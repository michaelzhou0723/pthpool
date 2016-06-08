/* 
 * Use BPP formula to approximate PI 
 * Refer to https://en.wikipedia.org/wiki/Bailey%E2%80%93Borwein%E2%80%93Plouffe_formula for details of this formula
 * For brevity, error checking is omitted in this example
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pthpool.h"

#define PRECISION 100   // upper bound in BPP sum

void *bpp(void *arg)
{
    int k = *(int *)arg;
    double sum = (4.0 / (8*k+1)) - (2.0 / (8*k+4)) - (1.0 / (8*k+5)) - (1.0 / (8*k+6));
    double *product = (double *)malloc(sizeof(double));
    
    if (product) {
        *product = 1 / pow(16, k) * sum;
    }
    return (void *)product;
}

int main()
{   
    int i, bpp_args[PRECISION + 1];
    double bpp_sum = 0, *result;
    pthpool_t pool = pthpool_create(4);
    pthpool_future_t futures[PRECISION + 1];
    
    for (i = 0; i <= PRECISION; i++) {
        bpp_args[i] = i;
        futures[i] = pthpool_apply(pool, bpp, (void *)&bpp_args[i]);
    }
    
    for (i = 0; i <= PRECISION; i++) {
        result = (double *)pthpool_future_get(futures[i], 0);    // use 0 for blocking wait
        bpp_sum += *result;
        pthpool_future_destroy(futures[i]);
        free(result);    // remember to free any memory allocated in the worker function
    }
    
    pthpool_join(pool);
    printf("PI calculated with %d terms: %.15f\n", PRECISION+1, bpp_sum);
    return 0;
}
