#pragma once

#include "equipement.h"
#include <string.h>

void afficher_mac(MAC *mac);
void afficher_ipv4(IPV4 *ip);
void afficher_reseau(reseau_local *rs);
void afficher_tram_user(trame *tr);
void afficher_tram_brute(trame *tr);
// prends le mac au format str et renvoie un vrai mac
MAC str_to_mac(char *str);
IPV4 str_to_ipv4(char *str);

void mac_to_str(MAC *mac,char *buffer);
void ip_to_str(IPV4 *ip,char *buffer);

void afficher_table(switch_ *sw);
void afficher_cables(const reseau_local *r);
