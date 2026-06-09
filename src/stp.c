#include "../include/stp.h"
#include "../include/affichage.h"

/* Adresse multicast "all bridges" pour les trames BPDU (01:80:C2:00:00:00) */
const MAC MAC_ALL_BRIDGES = {{0x01, 0x80, 0xC2, 0x00, 0x00, 0x00}};

void stp_initialiser_ponts(reseau_local *r)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ == SWITCH )
        {
            switch_ *sw = &r->equipements[i].sw;
            /* Au démarrage, un pont se considère comme la racine */
            sw->racine = sw->mac;
            sw->cout_vers_racine = 0;

            /* Initialement tous les ports sont inactifs (bloqués) */
            for ( size_t j = 0; j < sw->nb_port; j++ )
            {
                sw->ports[j].etat = ETAT_PORT_BLOQUE;
                sw->ports[j].a_recu_bpdu = false;
            }
        }
    }
}

BPDU creer_bpdu_802_1d(size_t racine_id, size_t cout, MAC transmetteur_id)
{
    BPDU bpdu;
    bpdu.racine_id = racine_id;
    bpdu.cout = cout;
    bpdu.transmetteur_id = transmetteur_id;
    return bpdu;
}

trame encapsuler_bpdu_dans_trame(MAC source, BPDU *bpdu)
{
    trame t;

    /* Préambule Ethernet : 7 octets à 0xAA */
    for ( size_t i = 0; i < 7; i++ )
        t.preambule[i] = 0xAA;
    /* Start Frame Delimiter */
    t.SFD = 0xAB;

    t.source = source;
    /* Les trames BPDU sont adressées en multicast (all bridges) */
    t.destination = MAC_ALL_BRIDGES;

    /* Type Slow Protocols pour STP */
    t.type = 0x8809;

    /* Encapsuler le BPDU dans le champ data de la trame */
    t.bpdu = *bpdu;

    /* Frame Check Sequence (simplifié à 0) */
    t.FCS = 0;

    return t;
}

BPDU extraire_bpdu_de_trame(trame *t)
{
    return t->bpdu;
}

bool bpdu_est_meilleur(BPDU *bpdu1, BPDU *bpdu2)
{
    if ( bpdu1->racine_id < bpdu2->racine_id )
        return true;
    if ( bpdu1->racine_id > bpdu2->racine_id )
        return false;
    if ( bpdu1->cout < bpdu2->cout )
        return true;
    if ( bpdu1->cout > bpdu2->cout )
        return false;
    return mac_est_meilleure(&bpdu1->transmetteur_id, &bpdu2->transmetteur_id);
}


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

static size_t trouver_cable(reseau_local *r, size_t sommet1, size_t sommet2)
{
    for ( size_t i = 0; i < r->nb_cables; i++ )
        if ( cable_est_relie(&r->cables[i], sommet1, sommet2) )
            return i;
    return SIZE_MAX;
}

bool stp_traiter_trame_recue(reseau_local *r, size_t id_switch_recepteur,
                             size_t cable_idx, trame *trame_recue)
{
    switch_ *sw = &r->equipements[id_switch_recepteur].sw;

    /* 1. Extraire le BPDU de la trame reçue */
    BPDU bpdu_extrait = extraire_bpdu_de_trame(trame_recue);

    /* 2. Calculer le coût réel : coût du BPDU + pondération du câble */
    size_t ponderation = r->cables[cable_idx].ponderation;
    BPDU bpdu_recu = creer_bpdu_802_1d(
        bpdu_extrait.racine_id,
        bpdu_extrait.cout + ponderation,
        bpdu_extrait.transmetteur_id);

    /* 3. Construire le BPDU représentant l'état actuel du récepteur */
    size_t racine_actuelle_id = get_index_par_mac(r, &sw->racine);
    BPDU bpdu_actuel = creer_bpdu_802_1d(
        racine_actuelle_id,
        sw->cout_vers_racine,
        sw->mac);

    /* 4. Sauvegarder le meilleur BPDU reçu sur ce port
          (le pont sauvegarde pour chaque port le meilleur message) */
    size_t port_local = obtenir_port_local(r, id_switch_recepteur, cable_idx);
    if ( port_local < sw->nb_port )
    {
        if ( !sw->ports[port_local].a_recu_bpdu ||
             bpdu_est_meilleur(&bpdu_recu, &sw->ports[port_local].meilleur_bpdu_recu) )
        {
            sw->ports[port_local].meilleur_bpdu_recu = bpdu_recu;
            sw->ports[port_local].a_recu_bpdu = true;
        }
    }

    /* 5. Mise à jour uniquement si le BPDU reçu est meilleur que l'état actuel */
    if ( bpdu_est_meilleur(&bpdu_recu, &bpdu_actuel) )
    {
        /* Mettre à jour la racine connue et le coût vers la racine */
        if ( r->equipements[bpdu_recu.racine_id].type_equ == SWITCH )
            sw->racine = r->equipements[bpdu_recu.racine_id].sw.mac;
        sw->cout_vers_racine = bpdu_recu.cout;

        /* Le port racine est le port qui a reçu le meilleur message 802.1d */
        if ( port_local < sw->nb_port )
        {
            sw->port_racine.numero_port = port_local;
            sw->port_racine.etat = ETAT_PORT_RACINE;
        }

        return true; /* État mis à jour → propagation nécessaire */
    }

    return false; /* Pas de changement */
}

void stp_diffuser_trames(reseau_local *r, size_t id_switch)
{
    switch_ *sw = &r->equipements[id_switch].sw;

    /* Créer le BPDU que ce switch transmet :
       racine actuelle, coût vers la racine, MAC du transmetteur */
    size_t racine_id = get_index_par_mac(r, &sw->racine);
    BPDU bpdu = creer_bpdu_802_1d(racine_id, sw->cout_vers_racine, sw->mac);

    /* Encapsuler le BPDU dans une trame Ethernet (destination multicast) */
    trame trame_a_envoyer = encapsuler_bpdu_dans_trame(sw->mac, &bpdu);

    /* Trouver tous les voisins et envoyer la trame */
    size_t adjacents[r->nb_equipements];
    size_t n_adj = sommets_adjacent(r, id_switch, adjacents);

    for ( size_t a = 0; a < n_adj; a++ )
    {
        size_t voisin_id = adjacents[a];

        /* Seuls les ponts lisent les trames BPDU (adressées en multicast) */
        if ( r->equipements[voisin_id].type_equ != SWITCH )
            continue;

        /* Retrouver le câble reliant les deux switches */
        size_t cable_idx = trouver_cable(r, id_switch, voisin_id);
        if ( cable_idx == SIZE_MAX )
            continue;

        /* Le voisin reçoit la trame et la traite */
        if ( stp_traiter_trame_recue(r, voisin_id, cable_idx, &trame_a_envoyer) )
        {
            /* Si l'état du voisin a changé, il propage à son tour ses trames
               (avec son propre MAC comme transmetteur) */
            stp_diffuser_trames(r, voisin_id);
        }
    }
}
size_t stp_obtenir_index_racine(reseau_local *r)
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

static void stp_determiner_ports_racines(reseau_local *r, size_t racine_idx)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ != SWITCH )
            continue;
        if ( i == racine_idx )
            continue; /* La racine n'a pas de port racine */

        switch_ *sw = &r->equipements[i].sw;
        size_t root_port = sw->port_racine.numero_port;
        if ( root_port < sw->nb_port )
            sw->ports[root_port].etat = ETAT_PORT_RACINE;
    }
}

static void stp_determiner_ports_designes(reseau_local *r, size_t racine_idx)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ != SWITCH )
            continue;

        switch_ *sw = &r->equipements[i].sw;

        /* Si c'est le commutateur racine, tous ses ports sont DÉSIGNÉS */
        if ( i == racine_idx )
        {
            for ( size_t p = 0; p < sw->nb_port; p++ )
                sw->ports[p].etat = ETAT_PORT_DESIGNE;
            continue;
        }

        /* Pour les autres switches, vérifier chaque port non-racine */
        size_t root_port_local = sw->port_racine.numero_port;
        size_t port_loc = 0;

        for ( size_t c = 0; c < r->nb_cables; c++ )
        {
            if ( r->cables[c].sommet1 != i && r->cables[c].sommet2 != i )
                continue;

            /* Le port racine est déjà traité à l'étape 5b */
            if ( port_loc == root_port_local )
            {
                port_loc++;
                continue;
            }

            size_t voisin_idx = (r->cables[c].sommet1 == i)
                                    ? r->cables[c].sommet2
                                    : r->cables[c].sommet1;

            /* Port vers une station → toujours DÉSIGNÉ */
            if ( r->equipements[voisin_idx].type_equ == STATION )
            {
                sw->ports[port_loc].etat = ETAT_PORT_DESIGNE;
            }
            /* Port vers un switch → comparer les coûts vers la racine
               Un port est désigné si le message qu'il transmet est meilleur
               que le meilleur message qu'il reçoit */
            else if ( r->equipements[voisin_idx].type_equ == SWITCH )
            {
                switch_ *v_sw = &r->equipements[voisin_idx].sw;

                if ( sw->cout_vers_racine < v_sw->cout_vers_racine )
                {
                    sw->ports[port_loc].etat = ETAT_PORT_DESIGNE;
                }
                else if ( sw->cout_vers_racine == v_sw->cout_vers_racine )
                {
                    /* Si même coût, le pont avec le meilleur MAC est désigné */
                    if ( mac_est_meilleure(&sw->mac, &v_sw->mac) )
                        sw->ports[port_loc].etat = ETAT_PORT_DESIGNE;
                }
            }
            port_loc++;
        }
    }
}

void stp_resoudre_etats_ports(reseau_local *r)
{
    size_t racine_idx = stp_obtenir_index_racine(r);
    if ( racine_idx == SIZE_MAX )
        return;

    /* D'abord, tous les ports sont BLOQUÉS par défaut (ports inactifs) */
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ != SWITCH )
            continue;
        switch_ *sw = &r->equipements[i].sw;
        for ( size_t p = 0; p < sw->nb_port; p++ )
            sw->ports[p].etat = ETAT_PORT_BLOQUE;
    }

    /* Étape 5b : Déterminer les ports racines (un par pont, sauf la racine) */
    stp_determiner_ports_racines(r, racine_idx);

    /* Étape 5c : Déterminer les ports désignés */
    stp_determiner_ports_designes(r, racine_idx);

    /* Les ports restants sont bloqués (déjà initialisés à BLOQUÉ ci-dessus) */
}

size_t stp_distance_vers_racine(reseau_local *r, equipement *equ)
{
    size_t idx_racine = stp_obtenir_index_racine(r);
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

bool stp_init(reseau_local *r)
{
    printf("============= PROTOCOLE STP 802.1d =============\n");

    /* Étape 1 : Initialisation — chaque pont se considère comme la racine
       et transmet un message 802.1d avec un coût de 0 sur tous ses ports */
    printf("[Étape 1] Initialisation des ponts\n");
    stp_initialiser_ponts(r);

    /* Étapes 2-4 : Chaque switch crée un BPDU, l'encapsule dans une trame,
       et la diffuse vers ses voisins. La convergence se fait par propagation
       récursive jusqu'à ce qu'aucun voisin ne soit amélioré. */
    printf("[Étapes 2-4] Diffusion et propagation des trames BPDU\n");
    for ( size_t i = 0; i < r->nb_equipements; i++ )
        if ( r->equipements[i].type_equ == SWITCH )
            stp_diffuser_trames(r, i);

    /* Étape 5 : Déterminer l'état de tous les ports (racine, désigné, bloqué)
       Les ports racines et désignés deviennent actifs. */
    printf("[Étape 5] Résolution de l'état des ports\n");
    stp_resoudre_etats_ports(r);

    printf("============= STP CONVERGÉ =============\n");
    return true;
}
