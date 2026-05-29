#include "../include/equipement.h"
#include "../include/affichage.h"
bool init_reseau(reseau_local* r)
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

bool free_reseau(reseau_local* r)
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

bool ajouter_equipement(equipement e, reseau_local* r)
{
    if ( r->nb_equipements >= r->equipement_capacite )
    {
        equipement* verif = realloc(r->equipements,
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

Erreur_fichier charger_reseau(char* fichier, reseau_local *r)
{
    FILE* f = fopen(fichier, "r");

    if ( f == NULL )
        return ERR_FICHIER_NON_TROUVE;
    
    char ligne[1024];
    fgets(ligne, sizeof(ligne), f );

    r->nb_equipements = ligne[0];
    r->nb_cables = ligne[3];

    for ( size_t eq = 0; eq < r->nb_equipements; eq++ )
    {
        fgets(ligne, sizeof(ligne), f);

        if ( ligne[0] == 1 )//station
        {
            station st;
            char* token = strtok(ligne, ";"); //recuper mac
            st.mac = str_to_mac(token);
            strtok(NULL, ";");
            st.ipv4 = str_to_ipv4(token);
            equipement equ = {
                .type_equ = STATION,
                .st = st
             };
             ajouter_equipement(equ, r);

        }
        
        else if ( ligne[0] == 1 )//switch
        {
            switch_ sw;
            char* token = strtok(ligne, ";");
            sw.mac = str_to_mac(token);
            strtok(NULL, ";");
            sw.nb_port = atoi(token);
            strtok(NULL, ";");
            sw.priorite = atoi(token);
            sw.taille_max = 24;
            sw.tab = malloc(sizeof(table_de_commutation) * 24 );
            sw.taille_tab = 0;
                        
            equipement equ = {
                .type_equ = STATION,
                .sw = sw
             };
            
             ajouter_equipement(equ, r);
        }
     }

     return ERR_OK;


}
