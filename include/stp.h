#pragma once

#include "equipement.h"

bool stp_init(reseau_local *r);

bool initialiser_racine_pour_ttSwitchs(reseau_local* r);

bool bpdu_est_meilleure(BPDU* bpdu1, BPDU* bpdu2);

BPDU creer_bpdu_8021d(size_t racine_id, size_t cout, MAC transmetteur_id);
bool transmettre_bpdu(reseau_local* r, size_t id_switch, BPDU* bpdu);

//probablement utiliser lalgorithme de djisktra ou une saloperie du genre
size_t distance_vers_racine(reseau_local* r, equipement* equ);
