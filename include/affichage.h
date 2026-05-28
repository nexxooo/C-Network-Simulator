#pragma once

#include "equipement.h"
#include <string.h>

void afficher_mac(MAC *mac);
void afficher_IPV4(IPV4 *ip);

//prends le mac au format str et renvoie un vrai mac
MAC str_to_mac(char* str);
IPV4 str_to_ipv4(char* str);
