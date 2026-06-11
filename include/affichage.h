/**
 * @file affichage.h
 * @brief Déclarations des fonctions d'affichage et de conversion du réseau.
 *
 * Ce fichier regroupe toutes les fonctions servant à :
 *   - Afficher les informations du réseau dans le terminal
 *   - Convertir les adresses (MAC, IPv4) entre leur forme binaire et textuelle
 */

#pragma once  /* Protège contre l'inclusion multiple */

#include "equipement.h"  /* Besoin des structures MAC, IPV4, reseau_local, trame, BPDU... */
#include <string.h>      /* Pour les fonctions de manipulation de chaînes de caractères */

/* =========================================================
   FONCTIONS D'AFFICHAGE
   ========================================================= */

/** Affiche une adresse MAC au format XX:XX:XX:XX:XX:XX dans le terminal. */
void afficher_mac(MAC *mac);

/** Affiche une adresse IPv4 au format X.X.X.X dans le terminal. */
void afficher_ipv4(IPV4 *ip);

/**
 * Affiche un résumé complet du réseau :
 * nombre d'équipements, nombre de câbles, et les détails de chaque équipement
 * (type, MAC, IPv4 pour les stations / ports, table de commutation pour les switchs).
 */
void afficher_reseau(reseau_local *rs);

/**
 * Affiche une trame Ethernet de façon lisible (source, destination, données).
 * Version "user-friendly" : on affiche uniquement les infos utiles.
 */
void afficher_tram_user(trame *tr);

/**
 * Affiche une trame Ethernet octet par octet en hexadécimal.
 * Version "brute" : on voit tous les octets de la trame tels qu'ils circulent sur le câble.
 */
void afficher_tram_brute(trame *tr);

/* =========================================================
   FONCTIONS DE CONVERSION
   ========================================================= */

/**
 * Convertit une chaîne de caractères au format "AA:BB:CC:DD:EE:FF"
 * en structure MAC (tableau de 6 octets).
 * Exemple : str_to_mac("01:45:23:a6:f7:01") → MAC {0x01, 0x45, 0x23, 0xA6, 0xF7, 0x01}
 */
MAC str_to_mac(char *str);

/**
 * Convertit une chaîne de caractères au format "192.168.1.1"
 * en structure IPV4 (tableau de 4 octets).
 */
IPV4 str_to_ipv4(char *str);

/**
 * Convertit une structure MAC en chaîne de caractères au format "AA:BB:CC:DD:EE:FF".
 * Le résultat est écrit dans le buffer fourni (doit faire au moins 18 caractères).
 */
void mac_to_str(MAC *mac, char *buffer);

/**
 * Convertit une structure IPV4 en chaîne de caractères au format "X.X.X.X".
 * Le résultat est écrit dans le buffer fourni.
 */
void ip_to_str(IPV4 *ip, char *buffer);

/* =========================================================
   FONCTIONS D'AFFICHAGE STP
   ========================================================= */

/**
 * Affiche la table de commutation d'un switch sous forme de tableau formaté :
 *   | Port | Adresse MAC       |
 *   |    0 | AA:BB:CC:DD:EE:FF |
 *   ...
 */
void afficher_table(switch_ *sw);

/**
 * Affiche tous les câbles du réseau au format :
 *   sommet1 --> sommet2
 */
void afficher_cables(const reseau_local *r);

/**
 * Affiche les informations d'un BPDU (Racine ID, Coût, MAC du transmetteur).
 */
void afficher_bpdu(BPDU *bpdu);

/**
 * Affiche l'état STP (BLOQUÉ / RACINE / DÉSIGNÉ / INCONNU) de chaque port
 * d'un switch donné, ainsi que le meilleur BPDU reçu sur chaque port.
 */
void afficher_etat_port_switch(switch_ *sw);

/**
 * Affiche l'état STP de tous les ports de tous les switchs du réseau.
 * Appelle afficher_etat_port_switch() pour chaque switch.
 */
void afficher_etat_port_reseau(reseau_local *rs);
