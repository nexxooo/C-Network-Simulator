/**
 * @file affichage.c
 * @brief Implémentation des fonctions d'affichage et de conversion réseau.
 */

#include "../include/affichage.h"
#include "../include/equipement.h"
#include <stdint.h>
#include <stdio.h>

/* Affiche une adresse IPv4 au format X.X.X.X */
void afficher_ipv4(IPV4 *ip)
{
    printf("%i.%i.%i.%i\n", ip->bytes[0], ip->bytes[1], ip->bytes[2],
           ip->bytes[3]);
}

/* Affiche une adresse MAC au format XX:XX:XX:XX:XX:XX */
void afficher_mac(MAC *mac)
{
    printf("%02X:%02X:%02X:%02X:%02X:%02X\n", mac->bytes[0], mac->bytes[1],
           mac->bytes[2], mac->bytes[3], mac->bytes[4], mac->bytes[5]);
}

/* Convertit une adresse MAC en chaîne de caractères */
void mac_to_str(MAC *mac, char *buffer)
{
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac->bytes[0], mac->bytes[1],
            mac->bytes[2], mac->bytes[3], mac->bytes[4], mac->bytes[5]);
}

/* Convertit une adresse IPv4 en chaîne de caractères */
void ip_to_str(IPV4 *ip, char *buffer)
{
    sprintf(buffer, "%i.%i.%i.%i", ip->bytes[0], ip->bytes[1], ip->bytes[2],
            ip->bytes[3]);
}

/* Affiche un résumé complet du réseau local */
void afficher_reseau(reseau_local *reseau)
{
    printf("RESEAU: \n");
    printf("_____________________________________________\n");
    printf("nombre d'équipement: %zu\n", reseau->nb_equipements);
    printf("nombre de liaison: %zu\n", reseau->nb_cables);

    for ( size_t i = 0; i < reseau->nb_equipements; i++ )
    {
        printf("----------------------------------------------------------------------------\n");
        printf("equipement %zu: ", i);

        if ( reseau->equipements[i].type_equ == SWITCH )
        {
            printf("SWITCH ");
            switch_ *sw = &reseau->equipements[i].sw;
            printf("nombre de ports: %zu ", sw->nb_port);

            char macstr[19];
            mac_to_str(&sw->mac, macstr);
            printf("adresse mac: %s\n", macstr);
        }
        else
        {
            printf("HOTE ");
            station *st = &reseau->equipements[i].st;

            char macstr[19];
            mac_to_str(&st->mac, macstr);
            printf("adresse mac: %s ", macstr);

            char ipstr[17];
            ip_to_str(&st->ipv4, ipstr);
            printf("adresse ipv4: %s\n", ipstr);
        }
    }
}

/* Affiche les câbles physiques du réseau */
void afficher_cables(const reseau_local *r)
{
    for ( size_t i = 0; i < r->nb_cables; i++ )
        printf("%zu --> %zu\n", r->cables[i].sommet1, r->cables[i].sommet2);
}

/* Convertit une chaîne XX:XX:XX:XX:XX:XX en structure MAC */
MAC str_to_mac(char *str)
{
    MAC res = {0};
    unsigned int b[6];

    if ( sscanf(str, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6 )
        for ( int i = 0; i < 6; i++ )
            res.bytes[i] = (uint8_t)b[i];
    return res;
}

/* Convertit une chaîne X.X.X.X en structure IPV4 */
IPV4 str_to_ipv4(char *str)
{
    IPV4 res = {0};
    unsigned int b[4];

    if ( sscanf(str, "%u.%u.%u.%u", &b[0], &b[1], &b[2], &b[3]) == 4 )
        for ( int i = 0; i < 4; i++ )
            res.bytes[i] = (uint8_t)b[i];
    return res;
}

/* Affiche un BPDU de manière lisible */
void afficher_bpdu(BPDU *bpdu)
{
    char mac_str[19];
    mac_to_str(&bpdu->transmetteur_id, mac_str);
    printf("BPDU [Racine ID: %zu | Coût: %zu | Transmetteur: %s]\n",
           bpdu->racine_id, bpdu->cout, mac_str);
}

/* Convertit un état de port en chaîne de caractères */
const char *etat_port_to_str(etat_port etat)
{
    switch ( etat )
    {
        case ETAT_PORT_BLOQUE:   return "BLOQUÉ";
        case ETAT_PORT_INCONNU:  return "INCONNU";
        case ETAT_PORT_DESIGNE:  return "DÉSIGNÉ";
        case ETAT_PORT_RACINE:   return "RACINE";
        default:                 return "INCONNU";
    }
}

/* Affiche l'état des ports d'un switch donné */
void afficher_etat_port_switch(switch_ *sw)
{
    char mac_str[19];
    mac_to_str(&sw->mac, mac_str);

    printf("Switch %s (Priorité: %zu) - État des ports :\n", mac_str, sw->priorite);

    for ( size_t i = 0; i < sw->nb_port; i++ )
    {
        port *p = &sw->ports[i];
        printf("  Port %zu : %s", p->numero_port, etat_port_to_str(p->etat));

        if ( p->a_recu_bpdu )
        {
            char trans_mac[19];
            mac_to_str(&p->meilleur_bpdu_recu.transmetteur_id, trans_mac);
            printf(" | Meilleur BPDU reçu -> [Racine ID: %zu, Coût: %zu, Transmetteur: %s]",
                   p->meilleur_bpdu_recu.racine_id,
                   p->meilleur_bpdu_recu.cout,
                   trans_mac);
        }
        else
        {
            printf(" | Aucun BPDU reçu");
        }
        printf("\n");
    }
}

/* Affiche l'état des ports pour tous les switchs du réseau */
void afficher_etat_port_reseau(reseau_local *rs)
{
    printf("=== ÉTAT DES PORTS DU RÉSEAU ===\n");
    for ( size_t i = 0; i < rs->nb_equipements; i++ )
    {
        if ( rs->equipements[i].type_equ == SWITCH )
        {
            afficher_etat_port_switch(&rs->equipements[i].sw);
            printf("--------------------------------------------------\n");
        }
    }
}
