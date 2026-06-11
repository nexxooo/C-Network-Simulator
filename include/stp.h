#pragma once

#include "equipement.h"

extern const MAC MAC_ALL_BRIDGES;

bool stp_init(reseau_local *r);

void stp_initialiser_ponts(reseau_local *r);

BPDU creer_bpdu_802_1d(size_t racine_id, size_t cout, MAC transmetteur_id);

trame encapsuler_bpdu_dans_trame(MAC source, BPDU *bpdu);

BPDU extraire_bpdu_de_trame(trame *t);

bool bpdu_est_meilleur(BPDU *bpdu1, BPDU *bpdu2);

void stp_diffuser_trames(reseau_local *r, size_t id_switch);

bool stp_traiter_trame_recue(reseau_local *r, size_t id_switch_recepteur,
                              size_t cable_idx, trame *trame_recue);

void stp_resoudre_etats_ports(reseau_local *r);

size_t stp_obtenir_index_racine(reseau_local *r);
