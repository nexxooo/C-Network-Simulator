#include "../include/stp.h"

bool bpdu_est_meilleure(BPDU *bpdu1, BPDU *bpdu2)
{
    if ( bpdu1->racine_id < bpdu2->racine_id )
        return true;
    else if ( bpdu1->racine_id > bpdu2->racine_id )
        return false;
    else if ( bpdu1->cout < bpdu2->cout )
        return true;
    else if ( bpdu1->cout > bpdu2->cout )
        return false;
    else
        return mac_est_meilleure(&bpdu1->transmetteur_id, &bpdu2->transmetteur_id);
    return false;
}

bool initialiser_racine_pour_ttSwitchs(reseau_local *r)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ == SWITCH )
        {
            switch_ *sw = &r->equipements[i].sw;
            sw->racine = sw->mac;
            sw->cout_vers_racine = 0;
        }
    }
    return true;
}

bool stp_init(reseau_local *r)
{
    printf("=============INIT STP============\n");

    /* Chaque switch se considère initialement comme sa propre racine */
    initialiser_racine_pour_ttSwitchs(r);

    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ == SWITCH )
        {
            switch_ *sw = &r->equipements[i].sw;
            printf("=============INIT SWITCH %zu============\n", i);

            /* Tous les ports démarrent à l'état RACINE avant convergence */
            for ( size_t j = 0; j < sw->nb_port; j++ )
                sw->ports[j].etat = ETAT_PORT_RACINE;

            /* Émettre UN seul BPDU par switch :
               racine_id = i  (index du switch, et non j l'index du port)
               cout       = 0  (coût nul vers soi-même)
               transmetteur = mac du switch lui-même
               transmettre_bpdu() se charge de contacter tous les voisins. */
            BPDU bpdu = creer_bpdu_8021d(i, sw->cout_vers_racine, sw->mac);
            transmettre_bpdu(r, i, &bpdu);
        }
    }
    return true;
}

BPDU creer_bpdu_8021d(size_t racine_id, size_t cout, MAC transmetteur_id)
{
    BPDU bpdu;
    bpdu.racine_id = racine_id;
    bpdu.cout = cout;
    bpdu.transmetteur_id = transmetteur_id;
    return bpdu;
}

trame creer_trame_bpdu(MAC source, MAC destination, BPDU *bpdu)
{
    trame t;

    for ( size_t i = 0; i < 7; i++ )
        t.preambule[i] = 0xAA;
    t.SFD = 0xAB;

    t.source = source;
    t.destination = destination;

    t.type = 0x8809;

    t.bpdu = *bpdu;

    t.FCS = 0;

    return t;
}

/* Renvoie l'index de l'équipement dont l'adresse MAC correspond à mac,
   ou SIZE_MAX si introuvable. */
static size_t get_index_par_mac(reseau_local *r, MAC *mac)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        MAC *m = (r->equipements[i].type_equ == SWITCH)
                     ? &r->equipements[i].sw.mac
                     : &r->equipements[i].st.mac;
        if ( mac_est_egale(m, mac) )
            return i;
    }
    return SIZE_MAX;
}

/* Transmet un BPDU depuis le switch id_switch vers tous ses voisins switches.
   Pour chaque voisin :
     - on calcule le coût réel (bpdu->cout + poids du câble)
     - si ce BPDU est meilleur que l'état actuel du voisin, on met à jour
       sa racine et son coût, puis on propage récursivement depuis lui.
   La récursion s'arrête naturellement dès qu'aucun voisin n'est amélioré. */
bool transmettre_bpdu(reseau_local *r, size_t id_switch, BPDU *bpdu)
{
    size_t adjacents[r->nb_equipements];
    size_t n_adj = sommets_adjacent(r, id_switch, adjacents);

    for ( size_t a = 0; a < n_adj; a++ )
    {
        size_t voisin_id = adjacents[a];

        /* On ne transmet qu'aux switches */
        if ( r->equipements[voisin_id].type_equ != SWITCH )
            continue;

        /* Retrouver le câble reliant id_switch et voisin_id pour la pondération */
        size_t ponderation = 0;
        size_t cable_idx = SIZE_MAX;
        for ( size_t i = 0; i < r->nb_cables; i++ )
        {
            if ( cable_est_relie(&r->cables[i], id_switch, voisin_id) )
            {
                ponderation = r->cables[i].ponderation;
                cable_idx = i;
                break;
            }
        }

        switch_ *voisin = &r->equipements[voisin_id].sw;

        /* BPDU tel que reçu par le voisin : même racine, coût augmenté du poids */
        BPDU bpdu_recu = creer_bpdu_8021d(
            bpdu->racine_id,
            bpdu->cout + ponderation,
            bpdu->transmetteur_id);

        /* BPDU représentant l'état actuel du voisin */
        size_t racine_actuelle_id = get_index_par_mac(r, &voisin->racine);
        BPDU bpdu_actuel = creer_bpdu_8021d(
            racine_actuelle_id,
            voisin->cout_vers_racine,
            voisin->mac);

        /* Mise à jour uniquement si le BPDU reçu est meilleur */
        if ( bpdu_est_meilleure(&bpdu_recu, &bpdu_actuel) )
        {
            /* Mettre à jour la racine connue et le coût du voisin */
            if ( r->equipements[bpdu_recu.racine_id].type_equ == SWITCH )
                voisin->racine = r->equipements[bpdu_recu.racine_id].sw.mac;
            voisin->cout_vers_racine = bpdu_recu.cout;

            /* Marquer le port racine du voisin (index du câble = port d'entrée) */
            voisin->port_racine.numero_port = cable_idx;
            voisin->port_racine.etat = ETAT_PORT_RACINE;

            /* Propager depuis le voisin avec son propre MAC comme transmetteur */
            BPDU bpdu_suivant = creer_bpdu_8021d(
                bpdu_recu.racine_id,
                bpdu_recu.cout,
                voisin->mac);
            transmettre_bpdu(r, voisin_id, &bpdu_suivant);
        }
    }
    return true;
}

// renvoie l'index du switch racine dans le tableau des equipements de r
size_t get_index_racine(reseau_local *r)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ == SWITCH )
        {
            switch_ *sw = &r->equipements[i].sw;
            if ( mac_est_egale(&sw->racine, &sw->mac) )
                return i;
        }
    }
    return SIZE_MAX;
}

size_t distance_vers_racine(reseau_local *r, equipement *equ)
{
    size_t idx_racine = get_index_racine(r);
    if ( idx_racine == SIZE_MAX )
        return SIZE_MAX;

    /* Trouver l'index de l'équipement dans le tableau */
    size_t idx_equ = SIZE_MAX;
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( &r->equipements[i] == equ )
        {
            idx_equ = i;
            break;
        }
    }
    if ( idx_equ == SIZE_MAX )
        return SIZE_MAX;

    /* Si c'est un switch : STP a déjà calculé le coût */
    if ( equ->type_equ == SWITCH )
        return equ->sw.cout_vers_racine;

    /* Sinon : Dijkstra depuis idx_equ jusqu'à idx_racine */
    size_t n = r->nb_equipements;
    size_t dist[n];
    bool visite[n];

    for ( size_t i = 0; i < n; i++ )
    {
        dist[i] = SIZE_MAX;
        visite[i] = false;
    }
    dist[idx_equ] = 0;

    for ( size_t iter = 0; iter < n; iter++ )
    {
        /* Sommet non-visité avec la plus petite distance */
        size_t u = SIZE_MAX;
        for ( size_t i = 0; i < n; i++ )
            if ( !visite[i] && (u == SIZE_MAX || dist[i] < dist[u]) )
                u = i;

        if ( u == SIZE_MAX || dist[u] == SIZE_MAX )
            break;

        visite[u] = true;

        size_t adjacents[n];
        size_t n_adj = sommets_adjacent(r, u, adjacents);

        for ( size_t a = 0; a < n_adj; a++ )
        {
            size_t v = adjacents[a];
            if ( visite[v] )
                continue;

            /* Retrouver la pondération du câble u-v */
            size_t poids = 0;
            for ( size_t c = 0; c < r->nb_cables; c++ )
            {
                if ( cable_est_relie(&r->cables[c], u, v) )
                {
                    poids = r->cables[c].ponderation;
                    break;
                }
            }

            if ( dist[u] + poids < dist[v] )
                dist[v] = dist[u] + poids;
        }
    }

    return dist[idx_racine];
}
