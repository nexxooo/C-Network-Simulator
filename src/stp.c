#include "../include/stp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct stp_switch_info
{
    MAC racine_mac;
    size_t racine_prio;
    size_t root_path_cost;
    size_t root_port_idx;
} stp_switch_info;

static stp_switch_info *switch_infos = NULL;

static int compare_bridge_ids(size_t prio1, MAC mac1, size_t prio2, MAC mac2)
{
    if (prio1 < prio2) return -1;
    if (prio1 > prio2) return 1;
    return memcmp(mac1.bytes, mac2.bytes, 6);
}

// Initialise l'association entre les ports des switches et les liaisons physiques
void init_switch_ports_and_links(reseau_local *r)
{
    // 1. S'assurer que chaque switch a assez de ports alloués pour ses connexions réelles
    for (size_t i = 0; i < r->nb_equipements; i++)
    {
        if (r->equipements[i].type_equ == SWITCH)
        {
            switch_ *sw = &r->equipements[i].sw;
            size_t count = 0;
            for (size_t c = 0; c < r->nb_cables; c++)
            {
                if (r->cables[c].sommet1 == i || r->cables[c].sommet2 == i)
                {
                    count++;
                }
            }
            if (count > sw->nb_port)
            {
                printf("Warning: Switch [index %zu] has only %zu ports in config, but %zu cables connected. Adjusting to %zu ports.\n",
                       i, sw->nb_port, count, count);
                sw->ports = realloc(sw->ports, sizeof(switch_port) * count);
                sw->nb_port = count;
            }
            // Réinitialiser les ports
            for (size_t p = 0; p < sw->nb_port; p++)
            {
                sw->ports[p].port_index = p;
                sw->ports[p].equipement_voisin = (size_t)-1;
                sw->ports[p].cable_index = (size_t)-1;
                sw->ports[p].statut = PORT_UNDEFINED;
            }
        }
    }

    // 2. Associer les câbles et les voisins aux ports des switches
    for (size_t c = 0; c < r->nb_cables; c++)
    {
        size_t s1 = r->cables[c].sommet1;
        size_t s2 = r->cables[c].sommet2;

        if (r->equipements[s1].type_equ == SWITCH)
        {
            switch_ *sw = &r->equipements[s1].sw;
            size_t p = 0;
            while (p < sw->nb_port && sw->ports[p].equipement_voisin != (size_t)-1)
            {
                p++;
            }
            if (p < sw->nb_port)
            {
                sw->ports[p].equipement_voisin = s2;
                sw->ports[p].cable_index = c;
            }
        }

        if (r->equipements[s2].type_equ == SWITCH)
        {
            switch_ *sw = &r->equipements[s2].sw;
            size_t p = 0;
            while (p < sw->nb_port && sw->ports[p].equipement_voisin != (size_t)-1)
            {
                p++;
            }
            if (p < sw->nb_port)
            {
                sw->ports[p].equipement_voisin = s1;
                sw->ports[p].cable_index = c;
            }
        }
    }
}

// Étape 1 : Élection du Root Bridge
bool etape1_election_racine(reseau_local *r)
{
    printf("\n--- ÉTAPE 1 : Élection du Root Bridge ---\n");
    // Initialisation : chaque switch se considère comme racine
    for (size_t i = 0; i < r->nb_equipements; i++)
    {
        if (r->equipements[i].type_equ == SWITCH)
        {
            switch_infos[i].racine_mac = r->equipements[i].sw.mac;
            switch_infos[i].racine_prio = r->equipements[i].sw.priorite;
            switch_infos[i].root_path_cost = 0;
            switch_infos[i].root_port_idx = (size_t)-1;
        }
    }

    bool change = true;
    int iterations = 0;
    while (change && iterations < 100)
    {
        change = false;
        // Propagation des BPDUs d'élection
        for (size_t i = 0; i < r->nb_equipements; i++)
        {
            if (r->equipements[i].type_equ != SWITCH)
                continue;

            switch_ *sw = &r->equipements[i].sw;
            for (size_t p = 0; p < sw->nb_port; p++)
            {
                size_t voisin_idx = sw->ports[p].equipement_voisin;
                if (voisin_idx == (size_t)-1)
                    continue;

                if (r->equipements[voisin_idx].type_equ == SWITCH)
                {
                    MAC notre_racine = switch_infos[i].racine_mac;
                    size_t notre_racine_prio = switch_infos[i].racine_prio;

                    MAC voisin_racine = switch_infos[voisin_idx].racine_mac;
                    size_t voisin_racine_prio = switch_infos[voisin_idx].racine_prio;

                    // Le plus petit Bridge ID gagne
                    if (compare_bridge_ids(notre_racine_prio, notre_racine, voisin_racine_prio, voisin_racine) < 0)
                    {
                        switch_infos[voisin_idx].racine_mac = notre_racine;
                        switch_infos[voisin_idx].racine_prio = notre_racine_prio;
                        change = true;
                    }
                }
            }
        }
        iterations++;
    }

    // Affichage des résultats de l'élection
    printf("Résultat de l'élection du Root Bridge :\n");
    for (size_t i = 0; i < r->nb_equipements; i++)
    {
        if (r->equipements[i].type_equ == SWITCH)
        {
            printf("  Switch [index %zu, MAC %02X:%02X:%02X:%02X:%02X:%02X] -> Racine élue : %02X:%02X:%02X:%02X:%02X:%02X (Priorité %zu)\n",
                   i,
                   r->equipements[i].sw.mac.bytes[0], r->equipements[i].sw.mac.bytes[1], r->equipements[i].sw.mac.bytes[2],
                   r->equipements[i].sw.mac.bytes[3], r->equipements[i].sw.mac.bytes[4], r->equipements[i].sw.mac.bytes[5],
                   switch_infos[i].racine_mac.bytes[0], switch_infos[i].racine_mac.bytes[1], switch_infos[i].racine_mac.bytes[2],
                   switch_infos[i].racine_mac.bytes[3], switch_infos[i].racine_mac.bytes[4], switch_infos[i].racine_mac.bytes[5],
                   switch_infos[i].racine_prio);
        }
    }
    return true;
}

// Étape 2 : Calcul des Coûts (Root Port Selection)
bool etape2_calcul_couts(reseau_local *r)
{
    printf("\n--- ÉTAPE 2 : Calcul des Coûts ---\n");

    // Trouver l'index du switch racine
    size_t racine_idx = (size_t)-1;
    MAC racine_mac;
    for (size_t i = 0; i < r->nb_equipements; i++)
    {
        if (r->equipements[i].type_equ == SWITCH)
        {
            racine_mac = switch_infos[i].racine_mac;
            break;
        }
    }

    for (size_t i = 0; i < r->nb_equipements; i++)
    {
        if (r->equipements[i].type_equ == SWITCH)
        {
            if (memcmp(r->equipements[i].sw.mac.bytes, racine_mac.bytes, 6) == 0)
            {
                racine_idx = i;
            }
        }
    }

    if (racine_idx == (size_t)-1)
    {
        printf("Erreur : Impossible de trouver le switch racine.\n");
        return false;
    }

    // Initialisation des coûts à l'infini (sauf la racine qui a un coût de 0)
    for (size_t i = 0; i < r->nb_equipements; i++)
    {
        if (r->equipements[i].type_equ == SWITCH)
        {
            if (i == racine_idx)
            {
                switch_infos[i].root_path_cost = 0;
            }
            else
            {
                switch_infos[i].root_path_cost = 999999;
            }
            switch_infos[i].root_port_idx = (size_t)-1;
        }
    }

    bool change = true;
    int iterations = 0;
    while (change && iterations < 100)
    {
        change = false;
        for (size_t i = 0; i < r->nb_equipements; i++)
        {
            if (r->equipements[i].type_equ != SWITCH || i == racine_idx)
                continue;

            switch_ *sw = &r->equipements[i].sw;
            size_t meilleur_cout = 999999;
            size_t meilleur_port = (size_t)-1;
            size_t meilleur_voisin_idx = (size_t)-1;

            for (size_t p = 0; p < sw->nb_port; p++)
            {
                size_t voisin = sw->ports[p].equipement_voisin;
                if (voisin == (size_t)-1)
                    continue;

                if (r->equipements[voisin].type_equ == SWITCH)
                {
                    size_t cable_idx = sw->ports[p].cable_index;
                    size_t cout_cable = r->cables[cable_idx].ponderation;
                    size_t cout_total = switch_infos[voisin].root_path_cost + cout_cable;

                    if (cout_total < meilleur_cout)
                    {
                        meilleur_cout = cout_total;
                        meilleur_port = p;
                        meilleur_voisin_idx = voisin;
                    }
                    else if (cout_total == meilleur_cout && meilleur_port != (size_t)-1)
                    {
                        // En cas d'égalité de coût, briser l'égalité avec le Bridge ID du voisin
                        switch_ *voisin_sw = &r->equipements[voisin].sw;
                        switch_ *meilleur_voisin_sw = &r->equipements[meilleur_voisin_idx].sw;
                        if (compare_bridge_ids(voisin_sw->priorite, voisin_sw->mac,
                                               meilleur_voisin_sw->priorite, meilleur_voisin_sw->mac) < 0)
                        {
                            meilleur_port = p;
                            meilleur_voisin_idx = voisin;
                        }
                    }
                }
            }

            if (meilleur_cout < switch_infos[i].root_path_cost)
            {
                switch_infos[i].root_path_cost = meilleur_cout;
                switch_infos[i].root_port_idx = meilleur_port;
                change = true;
            }
        }
        iterations++;
    }

    // Assigner le statut Root Port ('R') sur le port sélectionné
    for (size_t i = 0; i < r->nb_equipements; i++)
    {
        if (r->equipements[i].type_equ == SWITCH && i != racine_idx)
        {
            size_t rp = switch_infos[i].root_port_idx;
            if (rp != (size_t)-1)
            {
                r->equipements[i].sw.ports[rp].statut = PORT_ROOT;
            }
        }
    }

    // Affichage des coûts et Root Ports
    printf("Résultat du calcul des coûts et Root Ports :\n");
    for (size_t i = 0; i < r->nb_equipements; i++)
    {
        if (r->equipements[i].type_equ == SWITCH)
        {
            if (i == racine_idx)
            {
                printf("  Switch [index %zu (Racine)] -> Coût = 0, Pas de Root Port\n", i);
            }
            else
            {
                printf("  Switch [index %zu] -> Coût = %zu, Root Port = Port %zu (Voisin : Switch %zu)\n",
                       i, switch_infos[i].root_path_cost, switch_infos[i].root_port_idx,
                       r->equipements[i].sw.ports[switch_infos[i].root_port_idx].equipement_voisin);
            }
        }
    }

    return true;
}

// Étape 3 : Assignation des Rôles (Designated Port Selection)
bool etape3_assignation_roles(reseau_local *r)
{
    printf("\n--- ÉTAPE 3 : Assignation des Rôles ---\n");

    // Parcourir tous les segments (câbles)
    for (size_t c = 0; c < r->nb_cables; c++)
    {
        size_t s1 = r->cables[c].sommet1;
        size_t s2 = r->cables[c].sommet2;

        equipement *eq1 = &r->equipements[s1];
        equipement *eq2 = &r->equipements[s2];

        // 1. Si l'un des côtés est une station, le port du switch est forcément Designated
        if (eq1->type_equ == SWITCH && eq2->type_equ == STATION)
        {
            for (size_t p = 0; p < eq1->sw.nb_port; p++)
            {
                if (eq1->sw.ports[p].cable_index == c)
                {
                    eq1->sw.ports[p].statut = PORT_DESIGNATED;
                    break;
                }
            }
        }
        else if (eq1->type_equ == STATION && eq2->type_equ == SWITCH)
        {
            for (size_t p = 0; p < eq2->sw.nb_port; p++)
            {
                if (eq2->sw.ports[p].cable_index == c)
                {
                    eq2->sw.ports[p].statut = PORT_DESIGNATED;
                    break;
                }
            }
        }
        // 2. Si les deux côtés sont des switches, le switch ayant le plus faible coût de chemin est Designated
        else if (eq1->type_equ == SWITCH && eq2->type_equ == SWITCH)
        {
            size_t cost1 = switch_infos[s1].root_path_cost;
            size_t cost2 = switch_infos[s2].root_path_cost;

            size_t designated_switch = (size_t)-1;

            if (cost1 < cost2)
            {
                designated_switch = s1;
            }
            else if (cost2 < cost1)
            {
                designated_switch = s2;
            }
            else
            {
                // En cas d'égalité de coût, le switch avec le plus petit Bridge ID gagne
                if (compare_bridge_ids(eq1->sw.priorite, eq1->sw.mac,
                                       eq2->sw.priorite, eq2->sw.mac) < 0)
                {
                    designated_switch = s1;
                }
                else
                {
                    designated_switch = s2;
                }
            }

            for (size_t p = 0; p < r->equipements[designated_switch].sw.nb_port; p++)
            {
                if (r->equipements[designated_switch].sw.ports[p].cable_index == c)
                {
                    // Si ce n'est pas déjà un Root Port, il devient Designated
                    if (r->equipements[designated_switch].sw.ports[p].statut != PORT_ROOT)
                    {
                        r->equipements[designated_switch].sw.ports[p].statut = PORT_DESIGNATED;
                    }
                    break;
                }
            }
        }
    }

    // Affichage des statuts temporaires
    printf("Résultat temporaire de l'assignation des rôles :\n");
    for (size_t i = 0; i < r->nb_equipements; i++)
    {
        if (r->equipements[i].type_equ == SWITCH)
        {
            switch_ *sw = &r->equipements[i].sw;
            printf("  Switch [index %zu] :\n", i);
            for (size_t p = 0; p < sw->nb_port; p++)
            {
                if (sw->ports[p].equipement_voisin != (size_t)-1)
                {
                    printf("    Port %zu (Voisin %zu) -> statut = %c\n",
                           p, sw->ports[p].equipement_voisin,
                           (sw->ports[p].statut == PORT_UNDEFINED) ? 'U' : sw->ports[p].statut);
                }
            }
        }
    }

    return true;
}

// Étape 4 : Propagation
bool etape4_propagation(reseau_local *r)
{
    printf("\n--- ÉTAPE 4 : Propagation et Validation ---\n");
    // Simulation d'une propagation et validation de l'arbre
    for (size_t i = 0; i < r->nb_equipements; i++)
    {
        if (r->equipements[i].type_equ == SWITCH)
        {
            switch_ *sw = &r->equipements[i].sw;
            printf("  Switch [index %zu] valide ses décisions :\n", i);
            for (size_t p = 0; p < sw->nb_port; p++)
            {
                if (sw->ports[p].equipement_voisin != (size_t)-1)
                {
                    if (sw->ports[p].statut == PORT_ROOT)
                    {
                        printf("    -> Port %zu validé en tant que Root Port ('R')\n", p);
                    }
                    else if (sw->ports[p].statut == PORT_DESIGNATED)
                    {
                        printf("    -> Port %zu validé en tant que Designated Port ('D')\n", p);
                    }
                }
            }
        }
    }
    return true;
}

// Étape 5 : Blocage (Coupage des boucles)
bool etape5_blocage(reseau_local *r)
{
    printf("\n--- ÉTAPE 5 : Blocage (Coupage des boucles) ---\n");
    for (size_t i = 0; i < r->nb_equipements; i++)
    {
        if (r->equipements[i].type_equ == SWITCH)
        {
            switch_ *sw = &r->equipements[i].sw;
            printf("  Switch [index %zu] ports finaux :\n", i);
            for (size_t p = 0; p < sw->nb_port; p++)
            {
                if (sw->ports[p].equipement_voisin != (size_t)-1)
                {
                    if (sw->ports[p].statut != PORT_ROOT && sw->ports[p].statut != PORT_DESIGNATED)
                    {
                        sw->ports[p].statut = PORT_BLOCKED;
                        printf("    Port %zu (Voisin %zu) -> BLOQUÉ ('B')\n", p, sw->ports[p].equipement_voisin);
                    }
                    else
                    {
                        printf("    Port %zu (Voisin %zu) -> ACTIF ('%c')\n",
                               p, sw->ports[p].equipement_voisin, sw->ports[p].statut);
                    }
                }
            }
        }
    }
    return true;
}

bool stp_init(reseau_local *r)
{
    // Initialiser les associations de ports
    init_switch_ports_and_links(r);

    // Allouer la structure d'informations temporaires
    switch_infos = calloc(r->nb_equipements, sizeof(stp_switch_info));
    if (switch_infos == NULL)
        return false;

    // Étape 1 : Élection du Root Bridge
    if (!etape1_election_racine(r))
    {
        free(switch_infos);
        switch_infos = NULL;
        return false;
    }

    // Étape 2 : Calcul des Coûts
    if (!etape2_calcul_couts(r))
    {
        free(switch_infos);
        switch_infos = NULL;
        return false;
    }

    // Étape 3 : Assignation des Rôles
    if (!etape3_assignation_roles(r))
    {
        free(switch_infos);
        switch_infos = NULL;
        return false;
    }

    // Étape 4 : Propagation
    if (!etape4_propagation(r))
    {
        free(switch_infos);
        switch_infos = NULL;
        return false;
    }

    // Étape 5 : Blocage (Coupage des boucles)
    if (!etape5_blocage(r))
    {
        free(switch_infos);
        switch_infos = NULL;
        return false;
    }

    // Libération de la structure temporaire
    free(switch_infos);
    switch_infos = NULL;

    return true;
}
