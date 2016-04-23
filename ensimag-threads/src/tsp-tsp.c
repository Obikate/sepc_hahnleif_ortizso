#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "tsp-types.h"
#include "tsp-genmap.h"
#include "tsp-print.h"
#include "tsp-tsp.h"
#include "tsp-lp.h"
#include "tsp-hkbound.h"

/* dernier minimum trouvé */
int minimum;

/* résolution du problème du voyageur de commerce */
int present (int city, int hops, tsp_path_t path, uint64_t vpres)
{
  (void) hops;
  (void) path;
  return (vpres & (1<<city)) != 0;
}



void tsp (int hops, int len, uint64_t vpres, tsp_path_t path, long long int *cuts, tsp_path_t sol, int *sol_len)
{
    if (len + cutprefix[(nb_towns-hops)] >= minimum) {
      (*cuts)++ ;
      return;
    }

    /* calcul de l'arbre couvrant comme borne inférieure */
    if ((nb_towns - hops) > 6 &&
	lower_bound_using_hk(path, hops, len, vpres) >= minimum) {
      (*cuts)++;
      return;
    }


    /* un rayon de coupure à 15, pour ne pas lancer la programmation
       linéaire pour les petits arbres, plus rapide à calculer sans */
    if ((nb_towns - hops) > 22
	&& lower_bound_using_lp(path, hops, len, vpres) >= minimum) {
      (*cuts)++;
      return;
    }

  
    if (hops == nb_towns) {
	    int me = path [hops - 1];
	    int dist = tsp_distance[me][0]; // retourner en 0
            if ( len + dist < minimum ) {
		    minimum = len + dist;
		    *sol_len = len + dist;
		    memcpy(sol, path, nb_towns*sizeof(int));
		    if (!quiet)
		      print_solution (path, len+dist);
	    }
    } else {
        int me = path [hops - 1];        
        for (int i = 0; i < nb_towns; i++) {
	  if (!present (i, hops, path, vpres)) {
                path[hops] = i;
		vpres |= (1<<i);
                int dist = tsp_distance[me][i];
                tsp (hops + 1, len + dist, vpres, path, cuts, sol, sol_len);
		vpres &= (~(1<<i));
            }
        }
    }
}

void * pthread_tsp_eng(void * arg)
{
    pthread_mutex_lock(&super_mutex);
	pthread_tsp(arg);
    pthread_mutex_unlock(&super_mutex);
return NULL;
}

void * pthread_tsp (void * arg)
{
	pthread_arg_t p_arg = *(pthread_arg_t *)arg;
	int hops = p_arg.hops;
	int len = p_arg.len;
	uint64_t vpres = p_arg.vpres;
	long long int *cuts = p_arg.cuts;
	int *sol_len = p_arg.sol_len;
    if (len + cutprefix[(nb_towns-hops)] >= minimum) {
      (*cuts)++ ;
      return NULL;
    }

    /* calcul de l'arbre couvrant comme borne inférieure */
    if ((nb_towns - hops) > 6 &&
	lower_bound_using_hk(*p_arg.path, hops, len, vpres) >= minimum) {
      (*cuts)++;
      return NULL;
    }


    /* un rayon de coupure à 15, pour ne pas lancer la programmation
       linéaire pour les petits arbres, plus rapide à calculer sans */
    if ((nb_towns - hops) > 22
	&& lower_bound_using_lp(*p_arg.path, hops, len, vpres) >= minimum) {
      (*cuts)++;
      return NULL;
    }

  
    if (hops == nb_towns) {
	    int me = (*p_arg.path)[hops - 1];
	    int dist = tsp_distance[me][0]; // retourner en 0
            if ( len + dist < minimum ) {
		    minimum = len + dist;
		    *sol_len = len + dist;
		    memcpy(*p_arg.sol, *p_arg.path, nb_towns*sizeof(int));
		    if (!quiet)
		      print_solution (*p_arg.path, len+dist);
	    }
    } else {
        int me = (*p_arg.path)[hops - 1];        
        for (int i = 0; i < nb_towns; i++) {
	  if (!present (i, hops, *p_arg.path, vpres)) {
                (*p_arg.path)[hops] = i;
		vpres |= (1<<i);
                int dist = tsp_distance[me][i];
		pthread_arg_t p_arg_aux;
		p_arg_aux.hops = hops + 1;
		p_arg_aux.len = len + dist;
		p_arg_aux.vpres = vpres;	
		p_arg_aux.path = p_arg.path;	
		p_arg_aux.cuts = cuts;	
		p_arg_aux.sol = p_arg.sol;	
		p_arg_aux.sol_len = p_arg.sol_len;	
		pthread_tsp((void*)(&p_arg_aux));
                //pthread_tsp (hops + 1, len + dist, vpres, path, cuts, sol, sol_len);
		vpres &= (~(1<<i));
            }
        }
    }
	return NULL;
}

