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

#define MIN_PUISS 4
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
static uint8_t prochaine_puissance(unsigned long size);
static void decoupage(void * adr_zone_libre, uint8_t puiss_courante, uint8_t puiss_cherchee);
static void * get_adr_buddy(void * adr_courante, uint8_t puiss);
static void * get_adr_suivant(void * adr_courante);
static void set_header(void * adr_courante, uint8_t taille, void * adr_suivant);
static void * fusion(void * adr_courante, void * adr_buddy, uint8_t taille);

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
    if(zone_memoire == NULL)
    {
        perror("mémoire non initialisée!\n");
        return 0;
    }
    if(size == 0)
        return 0;

    //il nous faut la puissance de 2 de la taille souhaitée
    uint8_t k = prochaine_puissance(size);
    //il faut néanmoins avoir une taille minimale
    if(k < MIN_PUISS)
        k = MIN_PUISS;
    //on cherche la première liste de zones libres non-nulles
    uint8_t p = k;
    while((tzl[p] == NULL) && (p < 20))
    {
        p++; 
    }
    if(tzl[p] == NULL) 
    {
        return 0;      //il n'y a plus de zones libres
    }

    //récupération de la tête de liste
    void * adr_zone_libre = tzl[p];
    struct header h = *((struct header *)tzl[p]);
    //mise à jour du tableau de listes
    tzl[p] = h.suivant;
    //on découpe récursivement la zone libre jusqu'à ce qu'on tombe
    //sur la bonne taille
    decoupage(adr_zone_libre, p, k);
    return adr_zone_libre;
}

void mem_free_aux(void *adr_courante, uint8_t puiss)
{
    
    //on regarde si le buddy est libre
    void * adr_buddy = get_adr_buddy(adr_courante, puiss);
    //il nous faut deux pointeurs décalés pour enlever un élément de la 
    //liste sans la détruire
    void * adr_parcours_tzl = tzl[puiss];
    void * adr_parcours_suivant_tzl = get_adr_suivant(adr_parcours_tzl);
    while((adr_parcours_tzl != NULL) && (adr_parcours_tzl != adr_buddy ) 
            && (adr_parcours_suivant_tzl != adr_buddy))
    {
        adr_parcours_tzl = get_adr_suivant(adr_parcours_tzl); 
        if(adr_parcours_tzl != NULL)
            adr_parcours_suivant_tzl = get_adr_suivant(adr_parcours_tzl);
    }
    //ici, on a trois cas possibles:
    //1. adr_parcours_tzl est null, dans ce cas, le buddy est non-libre
    //et on ajoute simplement la zone libre à tzl[puiss]
    //2. le buddy se trouve à adr_parcours_tzl, i.e. en tête de la liste
    //tzl[puiss], on l'enlève et on fait la fusion
    //3 le buddy se trouve à adr_parcours_suivant_tzl, on l'enlève de la 
    //liste et on fait la fusion
    if(adr_parcours_tzl == NULL)
    {
        set_header(adr_courante, puiss, tzl[puiss]);
        tzl[puiss] = adr_courante;
    } else if(adr_parcours_tzl == adr_buddy)
    {
        tzl[puiss] = adr_parcours_suivant_tzl; 
        void * adr_fusionnee_avec_buddy = fusion(adr_courante, adr_buddy, 
                puiss+1);
        mem_free_aux(adr_fusionnee_avec_buddy, puiss+1);
    } else if(adr_parcours_suivant_tzl == adr_buddy)
    {
        set_header(adr_parcours_tzl, puiss, get_adr_suivant(adr_buddy));
        void * adr_fusionnee_avec_buddy = fusion(adr_courante, adr_buddy, 
                puiss+1);
        mem_free_aux(adr_fusionnee_avec_buddy, puiss+1);
    } 
}

    int 
mem_free(void *ptr, unsigned long size)
{
    if(((int64_t)ptr < (int64_t)zone_memoire) || (int64_t)ptr + size > (int64_t)zone_memoire + (1<<20))
    {
        return -1;
    }
    //il nous faut la puissance de 2 de la taille souhaitée
    uint8_t k = prochaine_puissance(size);
    if(k < MIN_PUISS)
        k = MIN_PUISS;
    mem_free_aux(ptr, k);
    return 0;
}


    int
mem_destroy()
{
    free(zone_memoire);
    zone_memoire = 0;
    for(int i=0; i<=20; i++)
        tzl[i] = NULL;
    return 0;
}

static void decoupage(void * adr_zone_libre, uint8_t puiss_courante, uint8_t puiss_cherchee)
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

static uint8_t prochaine_puissance(unsigned long size)
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

static void * get_adr_buddy(void * adr_courante, uint8_t puiss)
{
    uint64_t adr_courante_in_int = (uint64_t)adr_courante - (uint64_t)zone_memoire; 
    uint64_t rapport = adr_courante_in_int >> puiss; 
    uint64_t adr_buddy_in_int = adr_courante_in_int;
    if(rapport%2 == 0) //pair
        adr_buddy_in_int += (1<<puiss); 
    else
        adr_buddy_in_int -= (1<<puiss);
    adr_buddy_in_int += (uint64_t)zone_memoire;
    return (void *)adr_buddy_in_int;
}

//Fonction qui extrait l'adresse du suivant du début d'une zone libre
static void * get_adr_suivant(void * adr_courante)
{
    if(adr_courante != NULL)
    {
        struct header h = *((struct header *)adr_courante);
        return h.suivant;
    } else {
        return NULL;
    }
}

//Fonction qui écrit le header à une certaine adresse
static void set_header(void * adr_courante, uint8_t taille, void * adr_suivant)
{
    struct header h;
    h.taille = taille;
    h.suivant = adr_suivant;
    *((struct header*)adr_courante) = h;
}

static void * fusion(void * adr_courante, void * adr_buddy, uint8_t taille)
{
    void * adr_de_base;
    if(adr_courante < adr_buddy)
        adr_de_base = adr_courante;
    else 
        adr_de_base = adr_buddy;
    return adr_de_base;
}
