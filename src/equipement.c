#include "equipement.h"

Erreur_fichier charger_reseau(char* fichier, reseau_local *r)
{
    FILE* fichier = fopen(fichier, "r");

    if ( fichier == NULL )
        return ERR_FICHIER_NON_TROUVE;
    
    char ligne[1024];
    fgets(ligne, sizeof(ligne), fichier );

    r->nb_equipements_reseau = ligne[0];
    r->nb_cables = ligne[3];

    for ( size_t eq = 0; eq < r->nb_equipements_reseau; eq++ )
    {
        fgets(ligne, sizeof(ligne), fichier);

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

        }
        
        else if ( ligne[0] == 1 )//switch
        {
            switch_ sw;
            char* token = strtok(ligne, ";");
            sw.mac = str_to_mac(token);
            strtok(NULL, ";");
            sw.nb_port = strol(token);
            strok(NULL, ";");
            sw.priorite = strol(token);
            sw.taille_max = 24;
            sw.tab = malloc(sizeof(table_de_commutation) * 24 );
            sw.taille_tab = 0;
                        
            equipement equ = {
                .type_equ = STATION,
                .st = sw
             };


        }
     }


}