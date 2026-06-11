/**
 * @file affichage.h
 * @brief Déclarations des fonctions d'affichage et de conversion du réseau.
 */

#pragma once

#include "equipement.h"
#include <string.h>

/* --- Fonctions d'affichage --- */
void afficher_mac(MAC *mac);
void afficher_ipv4(IPV4 *ip);
void afficher_reseau(reseau_local *rs);
void afficher_cables(const reseau_local *r);

/* --- Fonctions d'affichage STP --- */
void afficher_bpdu(BPDU *bpdu);
void afficher_etat_port_switch(switch_ *sw);
void afficher_etat_port_reseau(reseau_local *rs);

/* --- Fonctions de conversion --- */
MAC str_to_mac(char *str);
IPV4 str_to_ipv4(char *str);
void mac_to_str(MAC *mac, char *buffer);
void ip_to_str(IPV4 *ip, char *buffer);
