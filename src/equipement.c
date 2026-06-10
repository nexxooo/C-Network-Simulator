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
            free(r->equipements[i].sw.tab);
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

bool est_un_arbre(reseau_local *r)
{
    // nb_aretes = ordre - 1
    if ( reseau_est_connexe(r) && r->nb_cables == r->nb_equipements - 1 )
        return true;
    return false;
}

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

bool construire_arbre_selon_reseau(reseau_local *src, reseau_local *dst)
{
    /* Copie tous les équipements (switches + stations) dans dst */
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
            sw_copy.tab = malloc(sizeof(table_de_commutation) * e.sw.taille_max);
            if ( sw_copy.tab )
            {
                memcpy(sw_copy.tab, e.sw.tab, sizeof(table_de_commutation) * e.sw.taille_tab);
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

    /*
     * On n'ajoute un câble que si le lien est actif après STP.
     * Un câble entre deux switches est inclus si le port local
     * correspondant n'est PAS BLOQUÉ des deux côtés simultanément,
     * c'est-à-dire qu'au moins un des deux extrémités voit ce port
     * comme RACINE ou DÉSIGNÉ.
     * Un câble reliant une station est toujours inclus (les stations
     * ne participent pas à STP et sont joignables via leur switch désigné).
     */
    for ( size_t c = 0; c < src->nb_cables; c++ )
    {
        size_t s1 = src->cables[c].sommet1;
        size_t s2 = src->cables[c].sommet2;

        equipement *e1 = &src->equipements[s1];
        equipement *e2 = &src->equipements[s2];

        /* Câble station↔switch ou station↔station : toujours actif */
        if ( e1->type_equ == STATION || e2->type_equ == STATION )
        {
            ajouter_cable(src->cables[c], dst);
            continue;
        }

        /* Câble switch↔switch : vérifier l'état des ports de chaque côté */
        switch_ *sw1 = &e1->sw;
        switch_ *sw2 = &e2->sw;

        /* Retrouver le numéro de port local pour chaque switch */
        size_t port_loc1 = obtenir_port_local(src, s1, c);
        size_t port_loc2 = obtenir_port_local(src, s2, c);

        etat_port etat1 = ETAT_PORT_BLOQUE;
        etat_port etat2 = ETAT_PORT_BLOQUE;

        if ( port_loc1 < sw1->nb_port )
            etat1 = sw1->ports[port_loc1].etat;
        if ( port_loc2 < sw2->nb_port )
            etat2 = sw2->ports[port_loc2].etat;

    /* Le lien est actif si les deux côtés ne sont pas bloqués */
        if ( etat1 != ETAT_PORT_BLOQUE && etat2 != ETAT_PORT_BLOQUE )
            ajouter_cable(src->cables[c], dst);
    }

    return true;
}

void switch_apprendre_mac(switch_ *sw, MAC source_mac, size_t port_entree)
{
    // 1. Vérifier si l'adresse MAC est déjà présente
    for ( size_t i = 0; i < sw->taille_tab; i++ )
    {
        if ( mac_est_egale(&sw->tab[i].mac, &source_mac) )
        {
            sw->tab[i].interface_port = port_entree;
            return;
        }
    }

    // 2. Agrandir la table si elle est pleine
    if ( sw->taille_tab >= sw->taille_max )
    {
        size_t nouvelle_taille = sw->taille_max + 24;
        table_de_commutation *verif = realloc(sw->tab, sizeof(table_de_commutation) * nouvelle_taille);
        if ( verif == NULL )
            return;
        sw->tab = verif;
        sw->taille_max = nouvelle_taille;
    }

    // 3. Ajouter l'association
    sw->tab[sw->taille_tab].mac = source_mac;
    sw->tab[sw->taille_tab].interface_port = port_entree;
    sw->taille_tab++;
}

size_t switch_trouver_port(switch_ *sw, MAC dest_mac)
{
    for ( size_t i = 0; i < sw->taille_tab; i++ )
    {
        if ( mac_est_egale(&sw->tab[i].mac, &dest_mac) )
        {
            return sw->tab[i].interface_port;
        }
    }
    return SIZE_MAX;
}

bool obtenir_voisin_par_port(const reseau_local *r, size_t sw_idx, size_t port_num, size_t *voisin_idx, size_t *cable_idx)
{
    size_t port_loc = 0;
    for ( size_t i = 0; i < r->nb_cables; i++ )
    {
        if ( r->cables[i].sommet1 == sw_idx || r->cables[i].sommet2 == sw_idx )
        {
            if ( port_loc == port_num )
            {
                if ( cable_idx ) *cable_idx = i;
                if ( voisin_idx )
                {
                    *voisin_idx = (r->cables[i].sommet1 == sw_idx) ? r->cables[i].sommet2 : r->cables[i].sommet1;
                }
                return true;
            }
            port_loc++;
        }
    }
    return false;
}

trame creer_trame_ethernet(MAC source, MAC destination, uint16_t type, const uint8_t *data, size_t data_len)
{
    trame t = {0};
    for ( int i = 0; i < 7; i++ )
        t.preambule[i] = 0xAA;
    t.SFD = 0xAB;
    t.source = source;
    t.destination = destination;
    t.type = type;
    if ( data && data_len > 0 )
    {
        size_t copy_len = data_len > 1500 ? 1500 : data_len;
        memcpy(t.data, data, copy_len);
    }
    t.FCS = 0;
    return t;
}

static void propager_trame_recue(reseau_local *r, size_t eq_idx, size_t cable_idx_entree, trame *tr, bool *visite, bool verbose)
{
    if ( eq_idx >= r->nb_equipements || visite[eq_idx] )
        return;
    visite[eq_idx] = true;

    equipement *eq = &r->equipements[eq_idx];

    if ( eq->type_equ == STATION )
    {
        station *st = &eq->st;
        char mac_src_str[19], mac_dst_str[19], mac_st_str[19];
        mac_to_str(&tr->source, mac_src_str);
        mac_to_str(&tr->destination, mac_dst_str);
        mac_to_str(&st->mac, mac_st_str);

        if ( verbose )
        {
            printf("[Station %s] Reçoit une trame de %s destinée à %s\n",
                   mac_st_str, mac_src_str, mac_dst_str);
        }

        bool est_broadcast = true;
        for ( int i = 0; i < 6; i++ )
        {
            if ( tr->destination.bytes[i] != 0xFF )
            {
                est_broadcast = false;
                break;
            }
        }

        if ( mac_est_egale(&st->mac, &tr->destination) )
        {
            printf("[Station %s] SUCCESS : Trame acceptée ! Contenu : %s\n",
                   mac_st_str, (char *)tr->data);
        }
        else if ( est_broadcast )
        {
            printf("[Station %s] Broadcast reçu ! Contenu : %s\n",
                   mac_st_str, (char *)tr->data);
        }
        else
        {
            if ( verbose )
            {
                printf("[Station %s] Trame rejetée (MAC non correspondante)\n", mac_st_str);
            }
        }
        return;
    }

    if ( eq->type_equ == SWITCH )
    {
        switch_ *sw = &eq->sw;
        char mac_sw_str[19], mac_src_str[19], mac_dst_str[19];
        mac_to_str(&sw->mac, mac_sw_str);
        mac_to_str(&tr->source, mac_src_str);
        mac_to_str(&tr->destination, mac_dst_str);

        size_t port_entree = obtenir_port_local(r, eq_idx, cable_idx_entree);
        if ( port_entree >= sw->nb_port )
        {
            if ( verbose )
                printf("[Switch %s] Erreur : Port local introuvable pour le câble %zu\n", mac_sw_str, cable_idx_entree);
            return;
        }

        if ( sw->ports[port_entree].etat == ETAT_PORT_BLOQUE )
        {
            if ( verbose )
                printf("[Switch %s] Trame reçue sur le Port %zu qui est BLOQUÉ par STP. Trame rejetée.\n", mac_sw_str, port_entree);
            return;
        }

        if ( verbose )
        {
            printf("[Switch %s] Trame reçue sur le Port %zu (Source: %s, Dest: %s)\n",
                   mac_sw_str, port_entree, mac_src_str, mac_dst_str);
        }

        switch_apprendre_mac(sw, tr->source, port_entree);
        if ( verbose )
        {
            printf("[Switch %s] Apprentissage : MAC %s associée au Port %zu\n",
                   mac_sw_str, mac_src_str, port_entree);
        }

        size_t port_sortie = switch_trouver_port(sw, tr->destination);

        if ( port_sortie != SIZE_MAX )
        {
            if ( port_sortie == port_entree )
            {
                if ( verbose )
                    printf("[Switch %s] Filtrage : la destination est sur le même port que l'entrée (%zu). Trame non propagée.\n", mac_sw_str, port_entree);
                return;
            }

            if ( sw->ports[port_sortie].etat == ETAT_PORT_BLOQUE )
            {
                if ( verbose )
                    printf("[Switch %s] Commutation : destination sur Port %zu, mais ce port est BLOQUÉ. Trame jetée.\n", mac_sw_str, port_sortie);
                return;
            }

            size_t voisin_idx = SIZE_MAX;
            size_t cable_idx_sortie = SIZE_MAX;
            if ( obtenir_voisin_par_port(r, eq_idx, port_sortie, &voisin_idx, &cable_idx_sortie) )
            {
                if ( verbose )
                {
                    printf("[Switch %s] Commutation monocast : envoi direct de la trame via le Port %zu vers l'équipement %zu\n",
                           mac_sw_str, port_sortie, voisin_idx);
                }
                propager_trame_recue(r, voisin_idx, cable_idx_sortie, tr, visite, verbose);
            }
        }
        else
        {
            if ( verbose )
            {
                printf("[Switch %s] Destination %s inconnue ou broadcast. Inondation (Flooding)...\n",
                       mac_sw_str, mac_dst_str);
            }

            for ( size_t p = 0; p < sw->nb_port; p++ )
            {
                if ( p == port_entree )
                    continue;

                if ( sw->ports[p].etat == ETAT_PORT_BLOQUE )
                {
                    if ( verbose )
                        printf("[Switch %s] Inondation : Port %zu ignoré car BLOQUÉ.\n", mac_sw_str, p);
                    continue;
                }

                size_t voisin_idx = SIZE_MAX;
                size_t cable_idx_sortie = SIZE_MAX;
                if ( obtenir_voisin_par_port(r, eq_idx, p, &voisin_idx, &cable_idx_sortie) )
                {
                    if ( verbose )
                    {
                        printf("[Switch %s] Inondation : envoi de la trame sur le Port %zu vers l'équipement %zu\n",
                               mac_sw_str, p, voisin_idx);
                    }
                    bool *visite_branche = malloc(sizeof(bool) * r->nb_equipements);
                    if ( visite_branche )
                    {
                        memcpy(visite_branche, visite, sizeof(bool) * r->nb_equipements);
                        propager_trame_recue(r, voisin_idx, cable_idx_sortie, tr, visite_branche, verbose);
                        free(visite_branche);
                    }
                }
            }
        }
    }
}

void envoyer_trame(reseau_local *r, size_t eq_source_idx, MAC destination_mac, trame *tr, bool verbose)
{
    if ( eq_source_idx >= r->nb_equipements )
    {
        printf("Erreur : Équipement source invalide (%zu)\n", eq_source_idx);
        return;
    }

    equipement *source_eq = &r->equipements[eq_source_idx];
    if ( source_eq->type_equ != STATION )
    {
        printf("Erreur : Seules les stations peuvent initier l'envoi de trames de données.\n");
        return;
    }

    char mac_src_str[19], mac_dst_str[19];
    mac_to_str(&tr->source, mac_src_str);
    mac_to_str(&tr->destination, mac_dst_str);

    printf("\n>>> TRANSMISSION DE TRAME : %s -> %s (Message: \"%s\") <<<\n",
           mac_src_str, mac_dst_str, (char *)tr->data);

    size_t cable_idx = SIZE_MAX;
    for ( size_t i = 0; i < r->nb_cables; i++ )
    {
        if ( r->cables[i].sommet1 == eq_source_idx || r->cables[i].sommet2 == eq_source_idx )
        {
            cable_idx = i;
            break;
        }
    }

    if ( cable_idx == SIZE_MAX )
    {
        printf("Erreur : La station source n'est connectée à aucun câble !\n");
        return;
    }

    size_t voisin_idx = (r->cables[cable_idx].sommet1 == eq_source_idx)
                            ? r->cables[cable_idx].sommet2
                            : r->cables[cable_idx].sommet1;

    bool *visite = calloc(r->nb_equipements, sizeof(bool));
    if ( visite == NULL )
        return;

    visite[eq_source_idx] = true;

    propager_trame_recue(r, voisin_idx, cable_idx, tr, visite, verbose);

    free(visite);
}

