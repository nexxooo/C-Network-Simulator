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
    r->equipement_capacite = 0;
    r->nb_equipements = 0;
    free(r->equipements);
    r->equipements = NULL;

    return true;
}

bool init_switch(switch_ *sw, size_t nb_port)
{
    sw->tab = malloc(sizeof(table_de_commutation) * sw->taille_max);
    if ( sw->tab == NULL )
        return false;
    sw->taille_tab = 0;

    sw->ports = malloc(sizeof(port) * sw->nb_port);
    if ( sw->ports == NULL )
    {
        free(sw->tab);
        sw->tab = NULL;
        return false;
    }

    for ( size_t i = 0; i < sw->nb_port; i++ )
        sw->ports[i].etat = ETAT_PORT_INCONNU;
    
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

    for ( size_t eq = 0; eq < expected_equipements; eq++ )
    {
        fgets(ligne, sizeof(ligne), f);
        ligne[strcspn(ligne, "\n")] = '\0';

        if ( ligne[0] == '1' ) // station
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

        else if ( ligne[0] == '2' ) // switch
        {
            switch_ sw = {0};
            char *token = strtok(ligne, ";");
            token = strtok(NULL, ";");

            sw.mac = str_to_mac(token);

            token = strtok(NULL, ";");
            sw.nb_port = atoi(token);

            token = strtok(NULL, ";");
            sw.priorite = atoi(token);

            sw.taille_max = TAILLE_REALOC;
            init_switch(&sw, sw.nb_port);

            equipement equ = {.type_equ = SWITCH, .sw = sw};

            ajouter_equipement(equ, r);
        }
    }

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

bool mac_est_meilleure(MAC* mac1, MAC* mac2)
{
    for ( size_t i = 0; i < 6; i++ )
    {
        if ( mac1->bytes[i] < mac2->bytes[i] )
            return true;
        else if ( mac1->bytes[i] > mac2->bytes[i] )
            return false;
    }
    return false;
}