#pragma once

#include "equipement.h"

typedef struct BPDU
{
    size_t racine_id;        // index (dans le tableau) de la racine supposée
    size_t cout;             // coût du chemin
    size_t emetteur_id;      // index (dans le tableau) de l'émetteur
    size_t emetteur_port;    // port de l'émetteur
} BPDU;

void init_switch_ports_and_links(reseau_local *r);
bool stp_init(reseau_local *r);
