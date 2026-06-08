#include "../include/stp.h"

bool bpdu_est_meilleure(BPDU* bpdu1, BPDU* bpdu2)
{
    if ( bpdu1->racine_id < bpdu2->racine_id )
        return true;
    else if ( bpdu1->racine_id > bpdu2->racine_id )
        return false;
    else
    {
        if ( bpdu1->cout < bpdu2->cout )
            return true;
        else if ( bpdu1->cout > bpdu2->cout )
            return false;
        else
        {
            return mac_est_meilleure(&bpdu1->transmetteur_id, &bpdu2->transmetteur_id);
        }
    }
    return false;
}

bool initialiser_racine_pour_ttSwitchs(reseau_local* r)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ == SWITCH )
        {
            switch_* sw = &r->equipements[i].sw;
            sw->racine = sw->mac;
            sw->cout_vers_racine = 0;
        }
    }
    return true;
}

bool stp_init(reseau_local *r)
{
    printf("=============INIT STP============\n");

    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ == SWITCH )
        {
            switch_* sw = &r->equipements[i].sw;
            printf("=============INIT SWITCH %zu============\n", i);
            for ( size_t j = 0; j < sw->nb_port; j++ )
            {
                //tous les ports se considerent comme racine
                port* p = &sw->ports[j];
                p->etat = ETAT_PORT_RACINE;
                BPDU bpdu = creer_bpdu_8021d(j, 
                    sw->cout_vers_racine, sw->mac);
                transmettre_bpdu(r, i, &bpdu);
            }
        }
    }
    return true;
}
