#include "../include/equipement.h"
#include "../include/affichage.h"
#include <string.h>

bool init_reseau(reseau_local *r)
{
    r->equipement_capacite = CAPACITE_INITIALE;
    r->equipements = malloc(CAPACITE_INITIALE * sizeof(equipement));
    if ( r->equipements == NULL )
        return false;

    r->nb_equipements = 0;
    r->nb_cables = 0;
    r->cables_capacite = CAPACITE_INITIALE;
    r->cables = malloc(CAPACITE_INITIALE * sizeof(cable));
    if ( r->cables == NULL )
        return false;

    return true;
}

bool free_reseau(reseau_local *r)
{
    r->cables_capacite = 0;
    r->nb_cables = 0;
    free(r->cables);
    r->cables = NULL;

    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ == SWITCH )
        {
            free(r->equipements[i].sw.ports);
        }
    }

    r->equipement_capacite = 0;
    r->nb_equipements = 0;
    free(r->equipements);
    r->equipements = NULL;

    return true;
}

bool init_switch(switch_ *sw, size_t nb_port)
{
    sw->nb_port = nb_port;
    sw->ports = malloc(sizeof(port) * sw->nb_port);
    if ( sw->ports == NULL )
    {
        return false;
    }

    for ( size_t i = 0; i < sw->nb_port; i++ )
    {
        sw->ports[i].numero_port = i;
        sw->ports[i].etat = ETAT_PORT_INCONNU;
        sw->ports[i].a_recu_bpdu = false;
        memset(&sw->ports[i].meilleur_bpdu_recu, 0, sizeof(BPDU));
    }

    return true;
}

bool ajouter_equipement(equipement e, reseau_local *r)
{
    if ( r->nb_equipements >= r->equipement_capacite )
    {
        equipement *verif =
            realloc(r->equipements,
                    (r->equipement_capacite + TAILLE_REALOC) * sizeof(equipement));

        if ( verif == NULL )
            return false;

        r->equipements = verif;
        r->equipement_capacite += TAILLE_REALOC;
    }

    r->equipements[r->nb_equipements] = e;
    r->nb_equipements++;
    return true;
}

bool ajouter_cable(cable c, reseau_local *r)
{
    if ( r->nb_cables >= r->cables_capacite )
    {
        cable *verif = realloc(r->cables, (r->cables_capacite + TAILLE_REALOC) *
                                              sizeof(cable));

        if ( verif == NULL )
            return false;

        r->cables = verif;
        r->cables_capacite += TAILLE_REALOC;
    }

    r->cables[r->nb_cables] = c;
    r->nb_cables++;

    return true;
}

/* Charge un réseau depuis un fichier de configuration */
Erreur_fichier charger_reseau(const char *fichier, reseau_local *r)
{
    FILE *f = fopen(fichier, "r");
    if ( f == NULL )
        return ERR_FICHIER_NON_TROUVE;

    char ligne[1024];
    fgets(ligne, sizeof(ligne), f);

    size_t expected_equipements = 0;
    size_t expected_cables = 0;
    sscanf(ligne, "%zu %zu", &expected_equipements, &expected_cables);

    // lire chaque equipement
    for ( size_t eq = 0; eq < expected_equipements; eq++ )
    {
        fgets(ligne, sizeof(ligne), f);
        ligne[strcspn(ligne, "\n")] = '\0';

        if ( ligne[0] == '1' ) //station
        {
            station st;
            char *token = strtok(ligne, ";");
            token = strtok(NULL, ";");
            st.mac = str_to_mac(token);
            token = strtok(NULL, ";");
            st.ipv4 = str_to_ipv4(token);

            equipement equ = {.type_equ = STATION, .st = st};
            ajouter_equipement(equ, r);
        }
        else if ( ligne[0] == '2' ) //switch
        {
            switch_ sw = {0};
            char *token = strtok(ligne, ";");
            token = strtok(NULL, ";");
            sw.mac = str_to_mac(token);
            token = strtok(NULL, ";");
            sw.nb_port = atoi(token);
            token = strtok(NULL, ";");
            sw.priorite = atoi(token);

            init_switch(&sw, sw.nb_port);

            equipement equ = {.type_equ = SWITCH, .sw = sw};
            ajouter_equipement(equ, r);
        }
    }

    // cables
    for ( size_t c = 0; c < expected_cables; c++ )
    {
        fgets(ligne, sizeof(ligne), f);
        ligne[strcspn(ligne, "\n")] = '\0';

        char *token = strtok(ligne, ";");
        size_t sommet1 = (size_t)strtoul(token, NULL, 10);

        token = strtok(NULL, ";");
        size_t sommet2 = (size_t)strtoul(token, NULL, 10);

        token = strtok(NULL, ";");
        size_t poids = (size_t)strtoul(token, NULL, 10);

        cable cab = {.sommet1 = sommet1, .sommet2 = sommet2, .ponderation = poids};
        ajouter_cable(cab, r);
    }

    fclose(f);
    return ERR_OK;
}

bool mac_est_meilleure(MAC *mac1, MAC *mac2)
{
    for ( size_t i = 0; i < 6; i++ )
        if ( mac1->bytes[i] < mac2->bytes[i] )
            return true;
        else if ( mac1->bytes[i] > mac2->bytes[i] )
            return false;
    return false;
}

bool mac_est_egale(MAC *mac1, MAC *mac2)
{
    for ( size_t i = 0; i < 6; i++ )
        if ( mac1->bytes[i] != mac2->bytes[i] )
            return false;
    return true;
}

/* Fonctions de M23 */
bool est_un_arbre(reseau_local *r)
{
    if ( reseau_est_connexe(r) && r->nb_cables == r->nb_equipements - 1 )
        return true;
    return false;
}

/* Récupère les voisins d'un équipement */
size_t sommets_adjacent(const reseau_local *r, size_t sommet, size_t *adjacents)
{
    size_t n_adj = 0;
    size_t deg = r->nb_cables;

    for ( size_t i = 0; i < deg; i++ )
        if ( r->cables[i].sommet1 == sommet )
            adjacents[n_adj++] = r->cables[i].sommet2;
        else if ( r->cables[i].sommet2 == sommet )
            adjacents[n_adj++] = r->cables[i].sommet1;

    return n_adj;
}

void visite_composante_connexe(reseau_local const *g, size_t ind_equip, bool *visite)
{
    visite[ind_equip] = true;

    size_t adjacents[g->nb_equipements];
    size_t n_adj = sommets_adjacent(g, ind_equip, adjacents);

    for ( size_t i = 0; i < n_adj; i++ )
        if ( !visite[adjacents[i]] )
            visite_composante_connexe(g, adjacents[i], visite);
}

bool reseau_est_connexe(reseau_local *r)
{
    bool visite[r->nb_equipements];
    for ( size_t i = 0; i < r->nb_equipements; i++ )
        visite[i] = false;

    uint32_t nbComposantes = 0;

    for ( size_t s = 0; s < r->nb_equipements; s++ )
    {
        if ( !visite[s] )
        {
            visite_composante_connexe(r, s, visite);
            nbComposantes++;
        }
    }

    return nbComposantes == 1;
}

bool cable_est_relie(cable *c, size_t sommet1, size_t sommet2)
{
    return (c->sommet1 == sommet1 && c->sommet2 == sommet2) ||
           (c->sommet1 == sommet2 && c->sommet2 == sommet1);
}

size_t obtenir_port_local(reseau_local *r, size_t sw_idx, size_t cable_idx)
{
    size_t port_loc = 0;

    for ( size_t i = 0; i < r->nb_cables; i++ )
    {
        if ( r->cables[i].sommet1 == sw_idx || r->cables[i].sommet2 == sw_idx )
        {
            if ( i == cable_idx )
                return port_loc;
            port_loc++;
        }
    }
    return SIZE_MAX;
}

/* Créer un arbre en enlevant les liens bloqués apres stp*/
bool construire_arbre_selon_reseau(reseau_local *src, reseau_local *dst)
{
    init_reseau(dst);

    for ( size_t i = 0; i < src->nb_equipements; i++ )
    {
        equipement e = src->equipements[i];
        if ( e.type_equ == SWITCH )
        {
            switch_ sw_copy = e.sw;
            sw_copy.ports = malloc(sizeof(port) * e.sw.nb_port);
            if ( sw_copy.ports )
            {
                memcpy(sw_copy.ports, e.sw.ports, sizeof(port) * e.sw.nb_port);
            }
            equipement equ = {.type_equ = SWITCH, .sw = sw_copy};
            ajouter_equipement(equ, dst);
        }
        else
        {
            equipement equ = {.type_equ = STATION, .st = e.st};
            ajouter_equipement(equ, dst);
        }
    }

    for ( size_t c = 0; c < src->nb_cables; c++ )
    {
        size_t s1 = src->cables[c].sommet1;
        size_t s2 = src->cables[c].sommet2;

        equipement *e1 = &src->equipements[s1];
        equipement *e2 = &src->equipements[s2];

        if ( e1->type_equ == STATION || e2->type_equ == STATION )
        {
            ajouter_cable(src->cables[c], dst);
            continue;
        }

        switch_ *sw1 = &e1->sw;
        switch_ *sw2 = &e2->sw;

        size_t port_loc1 = obtenir_port_local(src, s1, c);
        size_t port_loc2 = obtenir_port_local(src, s2, c);

        etat_port etat1 = ETAT_PORT_BLOQUE;
        etat_port etat2 = ETAT_PORT_BLOQUE;

        if ( port_loc1 < sw1->nb_port )
            etat1 = sw1->ports[port_loc1].etat;
        if ( port_loc2 < sw2->nb_port )
            etat2 = sw2->ports[port_loc2].etat;

        if ( etat1 != ETAT_PORT_BLOQUE && etat2 != ETAT_PORT_BLOQUE )
            ajouter_cable(src->cables[c], dst);
    }

    return true;
}
