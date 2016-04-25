#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <assert.h>
#include <complex.h>
#include <stdbool.h>
#include <unistd.h>

#include "tsp-types.h"
#include "tsp-job.h"
#include "tsp-genmap.h"
#include "tsp-print.h"
#include "tsp-tsp.h"
#include "tsp-lp.h"
#include "tsp-hkbound.h"


/* macro de mesure de temps, retourne une valeur en nanosecondes */
#define TIME_DIFF(t1, t2) \
    ((t2.tv_sec - t1.tv_sec) * 1000000000ll + (long long int) (t2.tv_nsec - t1.tv_nsec))


struct cell{
    pthread_t id;
    struct cell *suiv;
};
typedef struct cell cell_t;
int taille_cell;
int nb_jobs = 0;

void add_cell(cell_t ** head,pthread_t id_a_ajouter){
    cell_t * new_cell;
    new_cell=malloc(sizeof(cell_t));
    new_cell->id=id_a_ajouter;
    new_cell->suiv=*head;
    *head=new_cell;
    taille_cell++;
}

void print_cell(cell_t *head){
    cell_t * tmp=head;
    while(tmp!=NULL){
        printf("%u ", (unsigned int)(tmp->id));
        //printf("%d\n", tmp->pid);
        tmp=tmp->suiv;
    }
    printf("\n");
}

void remove_cell(cell_t **head, pthread_t id){
    cell_t *tmp=*head;
    cell_t *tmp2=*head;
    if (tmp->id==id)
        *head=tmp->suiv;
    else {
        tmp2=tmp->suiv;
        while (tmp2!=NULL && tmp2->id!=id){
            tmp=tmp2;
            tmp2=tmp2->suiv;
        }
    }
    if (tmp2!=NULL){
        tmp->suiv=tmp2->suiv;
        free(tmp2);
    }
}

/* tableau des distances */
tsp_distance_matrix_t tsp_distance ={};

/** Paramètres **/

/* nombre de villes */
int nb_towns=10;
/* graine */
long int myseed= 0;
/* nombre de threads */
int nb_threads=1;

/* affichage SVG */
bool affiche_sol= false;
bool affiche_progress=false;
bool quiet=false;

void * p_thread_function(void * arg);

struct tsp_queue *q_tab;

static void generate_tsp_jobs (struct tsp_queue *q_tab, struct tsp_queue *q, int hops, int len, uint64_t vpres, tsp_path_t path, long long int *cuts, tsp_path_t sol, int *sol_len, int depth)
{
    if (len >= minimum) {
        (*cuts)++ ;//la sous-branche n'est pas intéressante
        return;
    }

    if (hops == depth) {
        /* On enregistre du travail à faire plus tard... */
        nb_jobs++;
        //print_solution(path, len);
        //add_job (q, path, hops, len, vpres); //il s'agit d'une feuille
        add_job (&(q_tab[path[1]%nb_threads]), path, hops, len, vpres); //il s'agit d'une feuille
    } else {
        int me = path [hops - 1];        
        for (int i = 0; i < nb_towns; i++) {
            if (!present (i, hops, path, vpres)) {
                path[hops] = i;
                vpres |= (1<<i);
                int dist = tsp_distance[me][i];
                generate_tsp_jobs (q_tab, q, hops + 1, len + dist, vpres, path, cuts, sol, sol_len, depth);
                vpres &= (~(1<<i));
            }
        }
    }
}

static void usage(const char *name) {
    fprintf (stderr, "Usage: %s [-s] <ncities> <seed> <nthreads>\n", name);
    exit (-1);
}

static void * pthread_main(void * arg)
{
    pthread_arg_t p_arg = (*(pthread_arg_t*)arg);
    tsp_path_t solution; 
    tsp_path_t sol; 
    struct tsp_queue *q = p_arg.q;
    uint64_t local_vpres = *(p_arg.vpres);
    long long int local_cuts = *(p_arg.cuts);
    int local_sol_len = *(p_arg.sol_len);

    int local_minimum; 

    pthread_mutex_lock(&super_mutex);
    memcpy(solution, *p_arg.solution, MAX_TOWNS*sizeof(int));
    memcpy(sol, *p_arg.sol, MAX_TOWNS*sizeof(int));
    local_minimum= minimum;
    pthread_mutex_unlock(&super_mutex);

    int mod_count = 0;
    while (!empty_queue (q)) {
        int hops = 0, len = 0;


        get_job (q, solution, &hops, &len, &local_vpres);
        //print_solution(solution, len);

        // le noeud est moins bon que la solution courante
        if (local_minimum < INT_MAX
                && (nb_towns - hops) > 10
                && ( (lower_bound_using_hk(solution, hops, len, local_vpres)) >= local_minimum
                    || (lower_bound_using_lp(solution, hops, len, local_vpres)) >= local_minimum)
           )

            continue;
    if(mod_count%20 == 0)
    {
        //pthread_mutex_lock(&super_mutex);
        //if(local_minimum < minimum)
        //{
        //    minimum = local_minimum;
        //}
        //pthread_mutex_unlock(&super_mutex);
    }
    mod_count++;

        tsp (hops, len, local_vpres, solution, &local_cuts, sol, &local_sol_len, &local_minimum);
    }

    pthread_mutex_lock(&super_mutex);
    *(p_arg.cuts) += local_cuts;
    *(p_arg.vpres) += local_vpres;
    if(local_minimum <= minimum)
    {
        *(p_arg.sol_len) = local_sol_len;
        minimum = local_minimum;
        memcpy(*p_arg.sol, sol, MAX_TOWNS*sizeof(int));
    }
    pthread_mutex_unlock(&super_mutex);
    return NULL;
}

    int main (int argc, char **argv)
    {
        unsigned long long perf;
        tsp_path_t path;
        uint64_t vpres=0;
        tsp_path_t sol;
        int sol_len;
        long long int cuts = 0;
        struct tsp_queue q;
        struct timespec t1, t2;
        //cell_t *head =NULL;
        //cell_t *tail=NULL;
        taille_cell=0;
        /* lire les arguments */
        int opt;
        while ((opt = getopt(argc, argv, "spq")) != -1) {
            switch (opt) {
                case 's':
                    affiche_sol = true;
                    break;
                case 'p':
                    affiche_progress = true;
                    break;
                case 'q':
                    quiet = true;
                    break;
                default:
                    usage(argv[0]);
                    break;
            }
        }

        if (optind != argc-3)
            usage(argv[0]);

        nb_towns = atoi(argv[optind]);
        myseed = atol(argv[optind+1]);
        nb_threads = atoi(argv[optind+2]);
        assert(nb_towns > 0);
        assert(nb_threads > 0);

        q_tab = malloc(nb_threads*sizeof(struct tsp_queue));
        minimum = INT_MAX;

        /* generer la carte et la matrice de distance */
        if (! quiet)
            fprintf (stderr, "ncities = %3d\n", nb_towns);
        genmap ();

        //printf("nbmax: %i, nb: %i\n", q.nbmax, q.nb);
        init_queue (&q);
        for(int i=0; i<nb_threads; i++)
            init_queue(&(q_tab[i]));
        for(int i=0; i<nb_threads; i++)
        {
            //printf("nbmax: %i, nb: %i\n", q_tab[i].nbmax,q_tab[i].nb);
        }

        clock_gettime (CLOCK_REALTIME, &t1);

        memset (path, -1, MAX_TOWNS * sizeof (int));
        path[0] = 0;
        vpres=1;

        /* mettre les travaux dans la file d'attente */
        generate_tsp_jobs (q_tab, &q, 1, 0, vpres, path, &cuts, sol, & sol_len, 3);
        //printf("nb_jobs: %i\n", nb_jobs);
        no_more_jobs (&q);
        for(int i=0; i<nb_threads; i++)
            no_more_jobs(&(q_tab[i]));
        for(int i=0; i<nb_threads; i++)
        {
            //printf("nbmax: %i, nb: %i\n", q_tab[i].nbmax,q_tab[i].nb);
        }
        //printf("nbmax: %i, nb: %i\n", q.nbmax, q.nb);


        tsp_path_t solution;
        pthread_mutex_init(&super_mutex, NULL);
        /* calculer chacun des travaux */
        memset (solution, -1, MAX_TOWNS * sizeof (int));
        solution[0] = 0;

        pthread_arg_t p_arg;
        p_arg.vpres = &vpres;
        p_arg.cuts = &cuts;
        p_arg.sol_len = &sol_len;
        p_arg.sol = &sol;
        p_arg.solution = &solution;

        pthread_t tid_tab[nb_threads];
        for(int i=0; i<nb_threads; i++)
        {
            p_arg.q = &(q_tab[i]);
            pthread_create(&(tid_tab[i]), NULL, pthread_main, (void*)&p_arg);
        }
        for(int i=0; i<nb_threads; i++)
            pthread_join(tid_tab[i], NULL);

        pthread_mutex_destroy(&super_mutex);
        clock_gettime (CLOCK_REALTIME, &t2);

        if (affiche_sol)
            print_solution_svg (sol, sol_len);

        print_solution(sol, sol_len);
        perf = TIME_DIFF (t1,t2);
        printf("<!-- # = %d seed = %ld len = %d threads = %d time = %lld.%03lld ms ( %lld coupures ) -->\n",
                nb_towns, myseed, sol_len, nb_threads,
                perf/1000000ll, perf%1000000ll, cuts);

        free(q_tab);
        return 0 ;
    }



    void * p_thread_function(void * arg)
    {
        pthread_mutex_lock(&super_mutex);
        //printf("%i\n", *((int*)arg));
        *((int*)arg) = *((int*)arg) + 1;
        pthread_mutex_unlock(&super_mutex);
        return NULL;
    }
