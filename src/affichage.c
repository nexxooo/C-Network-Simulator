/**
 * @file affichage.c
 * @brief Implémentation des fonctions d'affichage et de conversion réseau.
 *
 * Ce fichier implémente toutes les fonctions d'affichage déclarées dans affichage.h.
 * Ces fonctions servent à :
 *   - Afficher les informations du réseau de façon lisible dans le terminal
 *   - Convertir les adresses MAC et IPv4 entre forme binaire et texte
 *   - Afficher les états STP des ports
 */

#include "../include/affichage.h"  /* Nos propres prototypes */
#include "../include/equipement.h" /* Structures de données du réseau */
#include <stdint.h>                /* Pour uint8_t, etc. */
#include <stdio.h>                 /* Pour printf, sprintf */

/* =========================================================
   AFFICHAGE DES ADRESSES RÉSEAU
   ========================================================= */

/**
 * @brief Affiche une adresse IPv4 au format standard (X.X.X.X).
 *
 * Exemple de sortie : "192.168.1.1"
 * %i : format entier signé (ici les octets sont non-signés mais %i fonctionne aussi)
 *
 * @param ip  Pointeur vers l'adresse IPv4 à afficher.
 */
void afficher_ipv4(IPV4 *ip)
{
    /* Affiche les 4 octets séparés par des points */
    printf("%i.%i.%i.%i\n", ip->bytes[0], ip->bytes[1], ip->bytes[2],
           ip->bytes[3]);
}

/**
 * @brief Affiche une adresse MAC au format standard (XX:XX:XX:XX:XX:XX).
 *
 * Exemple de sortie : "01:45:23:A6:F7:01"
 * %02X : affiche en hexadécimal avec au moins 2 chiffres et zéro de remplissage.
 *        'X' = hexadécimal majuscule (A-F), '0' = pad avec des zéros, '2' = largeur min.
 *
 * @param mac  Pointeur vers l'adresse MAC à afficher.
 */
void afficher_mac(MAC *mac)
{
    /* Affiche les 6 octets en hexadécimal séparés par des deux-points */
    printf("%02X:%02X:%02X:%02X:%02X:%02X\n", mac->bytes[0], mac->bytes[1],
           mac->bytes[2], mac->bytes[3], mac->bytes[4], mac->bytes[5]);
}

/* =========================================================
   CONVERSION ADRESSE ↔ CHAÎNE DE CARACTÈRES
   ========================================================= */

/**
 * @brief Convertit une adresse MAC en chaîne de caractères.
 *
 * Écrit dans le buffer la représentation textuelle de la MAC au format "XX:XX:XX:XX:XX:XX".
 * ATTENTION : le buffer doit avoir au moins 18 caractères de large
 * (17 caractères pour la MAC + 1 pour le '\0' de fin de chaîne).
 *
 * Utilise sprintf (comme printf mais écrit dans une chaîne au lieu du terminal).
 *
 * @param mac     Pointeur vers l'adresse MAC à convertir.
 * @param buffer  Buffer de destination (minimum 18 caractères).
 */
void mac_to_str(MAC *mac, char *buffer)
{
    /* Écrit la représentation hexadécimale de la MAC dans le buffer */
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac->bytes[0], mac->bytes[1],
            mac->bytes[2], mac->bytes[3], mac->bytes[4], mac->bytes[5]);
}

/**
 * @brief Convertit une adresse IPv4 en chaîne de caractères.
 *
 * Écrit dans le buffer la représentation textuelle de l'IP au format "X.X.X.X".
 * ATTENTION : le buffer doit avoir au moins 16 caractères de large.
 *
 * @param ip      Pointeur vers l'adresse IPv4 à convertir.
 * @param buffer  Buffer de destination (minimum 16 caractères).
 */
void ip_to_str(IPV4 *ip, char *buffer)
{
    /* Écrit la représentation décimale de l'IP dans le buffer */
    sprintf(buffer, "%i.%i.%i.%i\n", ip->bytes[0], ip->bytes[1], ip->bytes[2],
            ip->bytes[3]);
}

/* =========================================================
   AFFICHAGE DU RÉSEAU
   ========================================================= */

/**
 * @brief Affiche un résumé complet de toute la topologie du réseau.
 *
 * Pour chaque équipement du réseau, affiche :
 *   - Son type (SWITCH ou HOTE/station)
 *   - Son adresse MAC
 *   - Son adresse IP (pour les stations)
 *   - Ses ports et sa table de commutation (pour les switchs)
 *
 * @param reseau  Pointeur vers le réseau à afficher.
 */
void afficher_reseau(reseau_local *reseau)
{
    printf("RESEAU: \n");
    printf("_____________________________________________\n");
    printf("nombre d'équipement: %zu\n", reseau->nb_equipements);  /* %zu = format pour size_t */
    printf("nombre de liaison: %zu\n", reseau->nb_cables);

    /* Itère sur tous les équipements */
    for ( size_t i = 0; i < reseau->nb_equipements; i++ )
    {
        printf("----------------------------------------------------------------------------\n");
        printf("equipement %zu: ", i);  /* Affiche l'index de l'équipement */

        if ( reseau->equipements[i].type_equ == SWITCH )
        {
            printf("SWITCH ");
            switch_ *sw = &reseau->equipements[i].sw;  /* Accès au switch */

            printf("nombre de ports: %zu ", sw->nb_port);

            /* Convertit la MAC en chaîne pour l'affichage */
            char macstr[19];  /* 17 caractères "XX:XX:XX:XX:XX:XX" + '\0' = 18, on prend 19 par sécurité */
            mac_to_str(&sw->mac, macstr);
            printf("adresse mac: %s ", macstr);
        }
        else
        {
            printf("HOTE ");  /* Les stations sont appelées "HOTE" dans l'affichage */
            station *st = &reseau->equipements[i].st;  /* Accès à la station */

            /* Affiche la MAC de la station */
            char macstr[19];
            mac_to_str(&st->mac, macstr);
            printf("adresse mac: %s ", macstr);

            /* Affiche l'adresse IP de la station */
            char ipstr[17];  /* "XXX.XXX.XXX.XXX" = 15 chars + '\n' + '\0' = 17 */
            ip_to_str(&st->ipv4, ipstr);
            printf("adresse ipv4: %s ", ipstr);
            printf("----------------------------------------------------------------------------\n");
        }
    }
}

/**
 * @brief Affiche tous les câbles du réseau au format "sommet1 --> sommet2".
 *
 * Permet de visualiser rapidement la topologie physique du réseau
 * (quels équipements sont directement connectés entre eux).
 *
 * @param r  Pointeur vers le réseau.
 */
void afficher_cables(const reseau_local *r)
{
    for ( size_t i = 0; i < r->nb_cables; i++ )
        printf("%zu --> %zu\n", r->cables[i].sommet1,
               r->cables[i].sommet2);  /* Affiche les deux extrémités du câble */
}

/* =========================================================
   CONVERSION CHAÎNE → ADRESSE RÉSEAU
   ========================================================= */

/**
 * @brief Convertit une chaîne "AA:BB:CC:DD:EE:FF" en structure MAC.
 *
 * Utilise sscanf pour parser la chaîne hexadécimale.
 * Les valeurs sont lues dans des `unsigned int` temporaires (car %x ne peut pas
 * lire directement dans des uint8_t), puis copiées dans la structure.
 *
 * Exemple :
 *   str_to_mac("54:d6:a6:82:c5:01") → MAC {0x54, 0xD6, 0xA6, 0x82, 0xC5, 0x01}
 *
 * @param str  Chaîne de caractères au format "XX:XX:XX:XX:XX:XX".
 * @return     La structure MAC correspondante (remplie à zéro en cas d'erreur).
 */
MAC str_to_mac(char *str)
{
    MAC res = {0};        /* Initialise tous les octets à 0 */
    unsigned int b[6];    /* Tableau temporaire pour la lecture (sscanf besoin d'unsigned int pour %x) */

    /* sscanf : comme scanf mais depuis une chaîne au lieu du clavier.
     * %x : lit un entier hexadécimal (ex: "54" → 0x54 = 84 en décimal)
     * Retourne le nombre de champs lus avec succès. */
    if ( sscanf(str, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6 )
        for ( int i = 0; i < 6; i++ )
            res.bytes[i] = (uint8_t)b[i];  /* Cast en uint8_t : tronque à 8 bits */
    return res;
}

/**
 * @brief Convertit une chaîne "192.168.1.1" en structure IPV4.
 *
 * Même principe que str_to_mac() mais pour les adresses IPv4.
 * %u : lit un entier décimal non signé.
 *
 * @param str  Chaîne de caractères au format "X.X.X.X".
 * @return     La structure IPV4 correspondante.
 */
IPV4 str_to_ipv4(char *str)
{
    IPV4 res = {0};       /* Initialise tous les octets à 0 */
    unsigned int b[4];    /* Tableau temporaire pour la lecture */

    /* Lit 4 entiers décimaux séparés par des points */
    if ( sscanf(str, "%u.%u.%u.%u", &b[0], &b[1], &b[2], &b[3]) == 4 )
        for ( int i = 0; i < 4; i++ )
            res.bytes[i] = (uint8_t)b[i];  /* Cast en uint8_t */
    return res;
}

/* =========================================================
   AFFICHAGE DE L'ÉTAT STP
   ========================================================= */

/**
 * @brief Affiche un BPDU de façon lisible.
 *
 * Format de sortie :
 *   "BPDU [Racine ID: 0 | Coût: 4 | Transmetteur: 01:45:23:A6:F7:01]"
 *
 * @param bpdu  Pointeur vers le BPDU à afficher.
 */
void afficher_bpdu(BPDU *bpdu)
{
    char mac_str[19];
    mac_to_str(&bpdu->transmetteur_id, mac_str);  /* Convertit la MAC du transmetteur en chaîne */
    printf("BPDU [Racine ID: %zu | Coût: %zu | Transmetteur: %s]\n",
           bpdu->racine_id, bpdu->cout, mac_str);
}

/**
 * @brief Convertit un état de port STP en chaîne lisible.
 *
 * Fonction locale (non déclarée dans le .h car utilisée uniquement dans ce fichier).
 * Utilise un switch/case pour mapper chaque valeur d'enum en une chaîne.
 *
 * @param etat  L'état du port à convertir.
 * @return      Chaîne descriptive de l'état (pointeur vers un littéral de chaîne statique).
 */
const char *etat_port_to_str(etat_port etat)
{
    switch ( etat )
    {
        case ETAT_PORT_BLOQUE:   return "BLOQUÉ";    /* Port désactivé par STP */
        case ETAT_PORT_INCONNU:  return "INCONNU";   /* État avant STP */
        case ETAT_PORT_DESIGNE:  return "DÉSIGNÉ";   /* Port actif, dessert un segment */
        case ETAT_PORT_RACINE:   return "RACINE";    /* Port menant vers la racine */
        default:                 return "INCONNU";   /* Cas imprévu */
    }
}

/**
 * @brief Affiche l'état STP de tous les ports d'un switch.
 *
 * Pour chaque port, affiche :
 *   - Le numéro du port
 *   - L'état STP (BLOQUÉ / RACINE / DÉSIGNÉ / INCONNU)
 *   - Le meilleur BPDU reçu sur ce port (s'il en a reçu un)
 *
 * Exemple de sortie :
 *   Switch 01:45:23:A6:F7:01 (Priorité: 1024) - État des ports :
 *     Port 0 : RACINE | Meilleur BPDU reçu -> [Racine ID: 0, Coût: 4, Transmetteur: 01:45:23:A6:F7:01]
 *     Port 1 : DÉSIGNÉ | Aucun BPDU reçu
 *
 * @param sw  Pointeur vers le switch dont afficher les ports.
 */
void afficher_etat_port_switch(switch_ *sw)
{
    char mac_str[19];
    mac_to_str(&sw->mac, mac_str);  /* Convertit la MAC du switch en chaîne */

    printf("Switch %s (Priorité: %zu) - État des ports :\n", mac_str, sw->priorite);

    /* Affiche chaque port un par un */
    for ( size_t i = 0; i < sw->nb_port; i++ )
    {
        port *p = &sw->ports[i];  /* Accès au port i */

        /* Affiche le numéro du port et son état STP */
        printf("  Port %zu : %s", p->numero_port, etat_port_to_str(p->etat));

        if ( p->a_recu_bpdu )
        {
            /* Ce port a reçu au moins un BPDU → affiche le meilleur reçu */
            char trans_mac[19];
            mac_to_str(&p->meilleur_bpdu_recu.transmetteur_id, trans_mac);
            printf(" | Meilleur BPDU reçu -> [Racine ID: %zu, Coût: %zu, Transmetteur: %s]",
                   p->meilleur_bpdu_recu.racine_id,
                   p->meilleur_bpdu_recu.cout,
                   trans_mac);
        }
        else
        {
            /* Ce port n'a reçu aucun BPDU (port vers une station, ou port isolé) */
            printf(" | Aucun BPDU reçu");
        }
        printf("\n");  /* Fin de ligne pour ce port */
    }
}

/**
 * @brief Affiche l'état STP de tous les ports de tous les switchs du réseau.
 *
 * Itère sur tous les équipements, et pour chaque switch,
 * appelle afficher_etat_port_switch() pour afficher ses ports.
 *
 * @param rs  Pointeur vers le réseau local.
 */
void afficher_etat_port_reseau(reseau_local *rs)
{
    printf("=== ÉTAT DES PORTS DU RÉSEAU ===\n");
    for ( size_t i = 0; i < rs->nb_equipements; i++ )
    {
        /* N'affiche que les switchs (les stations n'ont pas de ports STP) */
        if ( rs->equipements[i].type_equ == SWITCH )
        {
            afficher_etat_port_switch(&rs->equipements[i].sw);  /* Affiche les ports de ce switch */
            printf("--------------------------------------------------\n");
        }
    }
}
