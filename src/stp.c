#include "../include/stp.h"

/* Adresse MAC multicast "all bridges" pour les trames BPDU */
const MAC MAC_ALL_BRIDGES = {{0x01, 0x80, 0xC2, 0x00, 0x00, 0x00}};

void stp_initialiser_ponts(reseau_local *r)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ == SWITCH )
        {
            switch_ *sw = &r->equipements[i].sw;
            sw->racine = sw->mac;
            sw->cout_vers_racine = 0; // tout le monde se croit racine

            for ( size_t j = 0; j < sw->nb_port; j++ )
            {
                sw->ports[j].etat = ETAT_PORT_BLOQUE; // on bloque tt les ports
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
    trame t = {0};
    t.source = source;
    t.destination = MAC_ALL_BRIDGES;
    t.type = 0x8809;
    t.bpdu = *bpdu;
    return t;
}

BPDU extraire_bpdu_de_trame(trame *t)
{
    return t->bpdu;
}

bool bpdu_est_meilleur(BPDU *bpdu1, BPDU *bpdu2)
{
        // R1 < R2
    if ( bpdu1->racine_id < bpdu2->racine_id )
        return true;
    if ( bpdu1->racine_id > bpdu2->racine_id )
        return false;

        //C1 < C2
    if ( bpdu1->cout < bpdu2->cout )
        return true;
    if ( bpdu1->cout > bpdu2->cout )
        return false;

    // on compare le mac
    return mac_est_meilleure(&bpdu1->transmetteur_id, &bpdu2->transmetteur_id);
}

/* Recherche l'index d'un équipement par son adresse MAC */
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

/* Recherche l'index d'un câble reliant deux sommets */
static size_t trouver_cable(reseau_local *r, size_t sommet1, size_t sommet2)
{
    for ( size_t i = 0; i < r->nb_cables; i++ )
        if ( cable_est_relie(&r->cables[i], sommet1, sommet2) )
            return i;
    return SIZE_MAX;
}

/* Traite une trame BPDU reçue et met à jour l'état STP du switch */
bool stp_traiter_trame_recue(reseau_local *r, size_t id_switch_recepteur,
                             size_t cable_idx, trame *trame_recue)
{
    switch_ *sw = &r->equipements[id_switch_recepteur].sw;

    // cout calculé = cout BPDU recu + ponderation cable
    BPDU bpdu_extrait = extraire_bpdu_de_trame(trame_recue);
    size_t ponderation = r->cables[cable_idx].ponderation;
    BPDU bpdu_recu = creer_bpdu_802_1d(
        bpdu_extrait.racine_id,
        bpdu_extrait.cout + ponderation,
        bpdu_extrait.transmetteur_id);

    size_t racine_actuelle_id = get_index_par_mac(r, &sw->racine);
    BPDU bpdu_actuel = creer_bpdu_802_1d(
        racine_actuelle_id,
        sw->cout_vers_racine,
        sw->mac);

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

    // on met a jour la racine si le bpdu est meilleur
    if ( bpdu_est_meilleur(&bpdu_recu, &bpdu_actuel) )
    {
        if ( r->equipements[bpdu_recu.racine_id].type_equ == SWITCH )
            sw->racine = r->equipements[bpdu_recu.racine_id].sw.mac;

        sw->cout_vers_racine = bpdu_recu.cout;

        if ( port_local < sw->nb_port )
        {
            sw->port_racine.numero_port = port_local;
            sw->port_racine.etat = ETAT_PORT_RACINE;
        }

        return true; // renvoie true pour apres appeler stp_diffuser_trames
    }

    return false;
}

/* Diffuse les BPDUs d'un switch et propage récursivement en cas de mise à jour */
void stp_diffuser_trames(reseau_local *r, size_t id_switch)
{
    switch_ *sw = &r->equipements[id_switch].sw;
    size_t racine_id = get_index_par_mac(r, &sw->racine);
    BPDU bpdu = creer_bpdu_802_1d(racine_id, sw->cout_vers_racine, sw->mac);
    trame trame_a_envoyer = encapsuler_bpdu_dans_trame(sw->mac, &bpdu);

    size_t adjacents[r->nb_equipements];
    size_t n_adj = sommets_adjacent(r, id_switch, adjacents);

    for ( size_t a = 0; a < n_adj; a++ )
    {
        size_t voisin_id = adjacents[a];

        if ( r->equipements[voisin_id].type_equ != SWITCH )
            continue;

        size_t cable_idx = trouver_cable(r, id_switch, voisin_id);
        if ( cable_idx == SIZE_MAX )
            continue;

        if ( stp_traiter_trame_recue(r, voisin_id, cable_idx, &trame_a_envoyer) )
        {
            stp_diffuser_trames(r, voisin_id);
        }
    }
}

/* Retourne l'index du switch racine (celui qui a sw->racine == sw->mac) */
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

/* Attribue l'état RACINE aux ports racines identifiés sur chaque switch non-racine */
static void stp_determiner_ports_racines(reseau_local *r, size_t racine_idx)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ != SWITCH || i == racine_idx )
            continue;

        switch_ *sw = &r->equipements[i].sw;
        size_t root_port = sw->port_racine.numero_port;

        if ( root_port < sw->nb_port )
            sw->ports[root_port].etat = ETAT_PORT_RACINE;
    }
}

/* Détermine les ports DÉSIGNÉS (ports actifs qui transmettent) */
static void stp_determiner_ports_designes(reseau_local *r, size_t racine_idx)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ != SWITCH )
            continue;

        switch_ *sw = &r->equipements[i].sw;

        // tous les ports sont désignés sur la racine
        if ( i == racine_idx )
        {
            for ( size_t p = 0; p < sw->nb_port; p++ )
                sw->ports[p].etat = ETAT_PORT_DESIGNE;
            continue;
        }

        size_t root_port_local = sw->port_racine.numero_port;
        size_t port_loc = 0;

        for ( size_t c = 0; c < r->nb_cables; c++ )
        {
            if ( r->cables[c].sommet1 != i && r->cables[c].sommet2 != i )
                continue;

            if ( port_loc == root_port_local )
            {
                port_loc++;
                continue;
            }

            size_t voisin_idx = (r->cables[c].sommet1 == i)
                                    ? r->cables[c].sommet2
                                    : r->cables[c].sommet1;

            // un port vers une station est designe
            if ( r->equipements[voisin_idx].type_equ == STATION )
            {
                sw->ports[port_loc].etat = ETAT_PORT_DESIGNE;
            }
            // comparer la distance vers la racine
            else if ( r->equipements[voisin_idx].type_equ == SWITCH )
            {
                switch_ *v_sw = &r->equipements[voisin_idx].sw;

                if ( sw->cout_vers_racine < v_sw->cout_vers_racine )
                {
                    sw->ports[port_loc].etat = ETAT_PORT_DESIGNE;
                }
                else if ( sw->cout_vers_racine == v_sw->cout_vers_racine )
                {
                    if ( mac_est_meilleure(&sw->mac, &v_sw->mac) )
                        sw->ports[port_loc].etat = ETAT_PORT_DESIGNE;
                }
            }
            port_loc++;
        }
    }
}

/* Résout l'état final de tous les ports (RACINE, DÉSIGNÉ ou BLOQUÉ) */
void stp_resoudre_etats_ports(reseau_local *r)
{
    size_t racine_idx = stp_obtenir_index_racine(r);
    if ( racine_idx == SIZE_MAX )
        return;

    // mettre tous les ports bloqués par défaut
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ != SWITCH )
            continue;
        switch_ *sw = &r->equipements[i].sw;
        for ( size_t p = 0; p < sw->nb_port; p++ )
            sw->ports[p].etat = ETAT_PORT_BLOQUE;
    }

    stp_determiner_ports_racines(r, racine_idx);
    stp_determiner_ports_designes(r, racine_idx);
}

bool stp_init(reseau_local *r)
{
    printf("============= PROTOCOLE STP 802.1d =============\n");

    printf("[Étape 1] Initialisation des ponts\n");
    stp_initialiser_ponts(r);

    printf("[Étapes 2-4] Diffusion et propagation des trames BPDU\n");
    for ( size_t i = 0; i < r->nb_equipements; i++ )
        if ( r->equipements[i].type_equ == SWITCH )
            stp_diffuser_trames(r, i);

    printf("[Étape 5] Résolution de l'état des ports\n");
    stp_resoudre_etats_ports(r);

    printf("============= STP CONVERGÉ =============\n");
    return true;
}
