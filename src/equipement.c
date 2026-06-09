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
    if (r->equipements != NULL)
    {
        for (size_t i = 0; i < r->nb_equipements; i++)
        {
            if (r->equipements[i].type_equ == SWITCH)
            {
                free(r->equipements[i].sw.tab);
                free(r->equipements[i].sw.ports);
            }
        }
        free(r->equipements);
        r->equipements = NULL;
    }
    r->cables_capacite = 0;
    r->nb_cables = 0;
    free(r->cables);
    r->cables = NULL;
    r->equipement_capacite = 0;
    r->nb_equipements = 0;

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
            switch_ sw;
            char *token = strtok(ligne, ";");
            token = strtok(NULL, ";");

            sw.mac = str_to_mac(token);

            token = strtok(NULL, ";");
            sw.nb_port = atoi(token);

            token = strtok(NULL, ";");
            sw.priorite = atoi(token);

            sw.taille_max = 24;
            sw.tab = malloc(sizeof(table_de_commutation) * 24);
            sw.taille_tab = 0;
            sw.ports = malloc(sizeof(switch_port) * sw.nb_port);
            for (size_t i = 0; i < sw.nb_port; i++)
            {
                sw.ports[i].port_index = i;
                sw.ports[i].equipement_voisin = (size_t)-1;
                sw.ports[i].cable_index = (size_t)-1;
                sw.ports[i].statut = PORT_UNDEFINED;
            }

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

MAC meilleureMac(MAC *m1, MAC *m2)
{
    int res = memcmp(m1->bytes, m2->bytes, 6);
    if ( res < 0 )
    {
        return *m1;
    }
    else
    {
        return *m2;
    }
}

void mettre_a_jour_table(switch_ *sw, MAC mac, size_t port)
{
    for (size_t i = 0; i < sw->taille_tab; i++)
    {
        if (memcmp(sw->tab[i].mac.bytes, mac.bytes, 6) == 0)
        {
            sw->tab[i].interface_port = port;
            return;
        }
    }
    if (sw->taille_tab < sw->taille_max)
    {
        sw->tab[sw->taille_tab].mac = mac;
        sw->tab[sw->taille_tab].interface_port = port;
        sw->taille_tab++;
    }
}

static bool est_mac_broadcast(MAC mac)
{
    for (int i = 0; i < 6; i++)
    {
        if (mac.bytes[i] != 0xFF)
            return false;
    }
    return true;
}

static int transmissions_compteur = 0;

void transmettre_trame(reseau_local *r, trame *t, size_t actuel_idx, int port_entree_idx, int depth)
{
    if (depth == 0)
    {
        transmissions_compteur = 0;
    }

    if (transmissions_compteur > 30)
    {
        if (transmissions_compteur == 31)
        {
            printf("\n!!! TEMPÊTE DE DIFFUSION DÉTECTÉE (Broadcast Storm) !!! La trame tourne en boucle. Simulation arrêtée.\n\n");
        }
        transmissions_compteur++;
        return;
    }
    transmissions_compteur++;

    equipement *eq = &r->equipements[actuel_idx];
    if (eq->type_equ == STATION)
    {
        // Si la trame est destinée à cette station ou est un broadcast
        if (memcmp(t->destination.bytes, eq->st.mac.bytes, 6) == 0 || est_mac_broadcast(t->destination))
        {
            printf("Station [index %zu, MAC %02X:%02X:%02X:%02X:%02X:%02X] a REÇU la trame.\n",
                   actuel_idx,
                   eq->st.mac.bytes[0], eq->st.mac.bytes[1], eq->st.mac.bytes[2],
                   eq->st.mac.bytes[3], eq->st.mac.bytes[4], eq->st.mac.bytes[5]);
        }
        
        // Si c'est l'émetteur de départ
        if (port_entree_idx == -1)
        {
            for (size_t i = 0; i < r->nb_cables; i++)
            {
                cable cab = r->cables[i];
                if (cab.sommet1 == actuel_idx || cab.sommet2 == actuel_idx)
                {
                    size_t voisin_idx = (cab.sommet1 == actuel_idx) ? cab.sommet2 : cab.sommet1;
                    if (r->equipements[voisin_idx].type_equ == SWITCH)
                    {
                        // Trouver le port du switch voisin qui est relié à cette station
                        int port_on_voisin = -1;
                        for (size_t p = 0; p < r->equipements[voisin_idx].sw.nb_port; p++)
                        {
                            if (r->equipements[voisin_idx].sw.ports[p].equipement_voisin == actuel_idx)
                            {
                                port_on_voisin = (int)p;
                                break;
                            }
                        }
                        transmettre_trame(r, t, voisin_idx, port_on_voisin, depth + 1);
                    }
                    else
                    {
                        transmettre_trame(r, t, voisin_idx, 0, depth + 1);
                    }
                    break;
                }
            }
        }
    }
    else if (eq->type_equ == SWITCH)
    {
        switch_ *sw = &eq->sw;
        if (port_entree_idx != -1)
        {
            // Tâche E : Règle cruciale ! Si le port est Blocked ('B'), on ignore la trame
            if (sw->ports[port_entree_idx].statut == PORT_BLOCKED)
            {
                printf("Switch [index %zu, MAC %02X:%02X:%02X:%02X:%02X:%02X] : Trame IGNORÉE/SUPPRIMÉE sur le port d'entrée %d (Blocked).\n",
                       actuel_idx,
                       sw->mac.bytes[0], sw->mac.bytes[1], sw->mac.bytes[2],
                       sw->mac.bytes[3], sw->mac.bytes[4], sw->mac.bytes[5],
                       port_entree_idx);
                return;
            }
            // Apprentissage
            mettre_a_jour_table(sw, t->source, (size_t)port_entree_idx);
        }

        // Recherche dans la table
        bool found = false;
        size_t out_port = 0;
        if (!est_mac_broadcast(t->destination))
        {
            for (size_t i = 0; i < sw->taille_tab; i++)
            {
                if (memcmp(sw->tab[i].mac.bytes, t->destination.bytes, 6) == 0)
                {
                    out_port = sw->tab[i].interface_port;
                    found = true;
                    break;
                }
            }
        }

        if (found)
        {
            // Vérifier si le port de sortie est bloqué
            if (sw->ports[out_port].statut == PORT_BLOCKED)
            {
                printf("Switch [index %zu] : Trame IGNORÉE/SUPPRIMÉE car le port de sortie %zu est Blocked.\n", actuel_idx, out_port);
                return;
            }
            size_t voisin = sw->ports[out_port].equipement_voisin;
            if (voisin != (size_t)-1)
            {
                int port_on_voisin = -1;
                if (r->equipements[voisin].type_equ == SWITCH)
                {
                    for (size_t vp = 0; vp < r->equipements[voisin].sw.nb_port; vp++)
                    {
                        if (r->equipements[voisin].sw.ports[vp].equipement_voisin == actuel_idx)
                        {
                            port_on_voisin = (int)vp;
                            break;
                        }
                    }
                }
                else
                {
                    port_on_voisin = 0; // Voisin est une station
                }
                transmettre_trame(r, t, voisin, port_on_voisin, depth + 1);
            }
        }
        else
        {
            // Inondation (Flooding) sur tous les autres ports non bloqués
            for (size_t p = 0; p < sw->nb_port; p++)
            {
                if (port_entree_idx != -1 && p == (size_t)port_entree_idx)
                    continue;
                if (sw->ports[p].equipement_voisin == (size_t)-1)
                    continue;
                
                if (sw->ports[p].statut == PORT_BLOCKED)
                {
                    printf("Switch [index %zu] : Inondation évitée sur le port de sortie %zu (Blocked).\n", actuel_idx, p);
                    continue;
                }

                size_t voisin = sw->ports[p].equipement_voisin;
                int port_on_voisin = -1;
                if (r->equipements[voisin].type_equ == SWITCH)
                {
                    for (size_t vp = 0; vp < r->equipements[voisin].sw.nb_port; vp++)
                    {
                        if (r->equipements[voisin].sw.ports[vp].equipement_voisin == actuel_idx)
                        {
                            port_on_voisin = (int)vp;
                            break;
                        }
                    }
                }
                else
                {
                    port_on_voisin = 0; // Voisin est une station
                }
                transmettre_trame(r, t, voisin, port_on_voisin, depth + 1);
            }
        }
    }
}
