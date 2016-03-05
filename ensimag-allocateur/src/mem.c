/*****************************************************
 * Copyright Grégory Mounié 2008-2013                *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include "mem.h"

/** squelette du TP allocateur memoire */

void *zone_memoire = 0;

/* ecrire votre code ici */

//structures de données
struct header {
    uint8_t taille;
    void * suivant;
} header;

void * tzl[21];
//fonctions statiques
uint8_t prochaine_puissance(unsigned long size);
void decoupage(void * adr_zone_libre, uint8_t puiss_courante, uint8_t puiss_cherchee);
void * get_adr_buddy(void * adr_courante, uint8_t puiss);


void afficher(){
  for (int i=0; i<20; i++){
    printf("%i, adresse: %x\n", i, tzl[i]);
    printf("parcours de la liste: \n");
    void * liste_tmp = tzl[i];
    while(liste_tmp != NULL)
    {
        printf("adresse: %x\n", (uint64_t)liste_tmp - (uint64_t)zone_memoire);
        printf("adresse du buddy: %x\n", (uint64_t)get_adr_buddy(liste_tmp, i) - (uint64_t)zone_memoire);
        struct header h = *((struct header *)liste_tmp);
        liste_tmp = h.suivant;
    }
    // printf("coucou");
  }
}

    int 
mem_init()
{
    if (! zone_memoire)
        zone_memoire = (void *) malloc(ALLOC_MEM_SIZE);
    if (zone_memoire == 0)
    {
        perror("mem_init:");
        return -1;
    }

    /* ecrire votre code ici */
    //initialisation de nos structures de données
    for(int i=0; i<20; i++)
        tzl[i] = NULL;
    //au début il y a une zone de taille 2^20
    tzl[20] = zone_memoire;
    //on écrit la taille et le pointeur vers la zone libre suivant au début
    //de la zone libre
    struct header h;
    h.taille = 20;
    h.suivant = NULL;
    *((struct header*)tzl[20]) = h;

    return 0;
}

    void *
mem_alloc(unsigned long size)
{
    /*  ecrire votre code ici */
    //il nous faut la puissance de 2 de la taille souhaitée
    uint8_t k = prochaine_puissance(size);
    //on cherche la première liste de zones libres non-nulle
    uint8_t p = k;
    while(tzl[p] == NULL)
    {
       p++; 
    }
    printf("première liste non-nulle: %i\n", p);
    //récupération de la tête de liste
    void * adr_zone_libre = tzl[p];
    struct header h = *((struct header *)tzl[p]);
    printf("Taille de zone récupérée: %i\n", h.taille);
    //mise à jour du tableau de listes
    tzl[p] = h.suivant;
    //on découpe récursivement la zone libre jusqu'à ce qu'on tombe
    //sur la bonne taille
    printf("adresse de base: %x\n", adr_zone_libre);
    decoupage(adr_zone_libre, p, k);
    return 0;
}

    int 
mem_free(void *ptr, unsigned long size)
{
    /* ecrire votre code ici */
    return 0;
}


    int
mem_destroy()
{
    /* ecrire votre code ici */

    free(zone_memoire);
    zone_memoire = 0;
    return 0;
}

void decoupage(void * adr_zone_libre, uint8_t puiss_courante, uint8_t puiss_cherchee)
{
    if(puiss_courante != puiss_cherchee) 
    {
        //adresse du bloc de taille 2^(p-1)
        uint64_t adr_in_int = (uint64_t)adr_zone_libre + (1<<(puiss_courante-1));
        void * adr_prochain_bloc = (void *)adr_in_int;

        //ajout en tête de tzl[p-1]
        struct header h;
        h.taille = puiss_courante-1;
        h.suivant = tzl[puiss_courante-1];
        *((struct header*)adr_prochain_bloc) = h;
        tzl[puiss_courante-1] = adr_prochain_bloc;

        //récursion sur la partie "gauche"
        decoupage(adr_zone_libre, puiss_courante-1, puiss_cherchee);
    }
}

uint8_t prochaine_puissance(unsigned long size)
{
    uint8_t puissance=0;
    unsigned long n=1;
    while(n < size)
    {
        n = n << 1;
        puissance++;
    }
    return puissance;
}

void * get_adr_buddy(void * adr_courante, uint8_t puiss)
{
    uint64_t adr_courante_in_int = (uint64_t)adr_courante - (uint64_t)zone_memoire; 
    uint64_t rapport = adr_courante_in_int >> puiss; 
    uint64_t adr_buddy_in_int = adr_courante_in_int;
    printf("adr_courante_in_int: %i, puiss: %i, rapport: %i\n", adr_courante_in_int, puiss, rapport);
    if(rapport%2 == 0) //pair
        adr_buddy_in_int += (1<<puiss); 
    else
        adr_buddy_in_int -= (1<<puiss);
    adr_buddy_in_int += (uint64_t)zone_memoire;
    //void * adr_prochain_bloc = (void *)adr_in_int;
    return (void *)adr_buddy_in_int;
}
