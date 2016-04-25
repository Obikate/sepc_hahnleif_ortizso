#ifndef TSP_TSP_H
#define TSP_TSP_H
#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pthread_arg {
   struct tsp_queue *q;
   uint64_t *vpres;
   long long int *cuts;
   int *sol_len;
   tsp_path_t *solution;
   tsp_path_t *sol;
};

typedef struct pthread_arg pthread_arg_t;

/* dernier minimum trouvé */
extern int minimum;
pthread_mutex_t super_mutex;

int present (int city, int hops, tsp_path_t path, uint64_t vpres);
void tsp (int hops, int len, uint64_t vpres, tsp_path_t path, long long int *cuts, 
        tsp_path_t sol, int *sol_len, int * minimum);

#ifdef __cplusplus
}
#endif


#endif
