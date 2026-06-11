/**
 * @file stp.h
 * @brief Déclarations des fonctions du protocole STP (Spanning Tree Protocol) 802.1d.
 *
 * Le protocole STP élimine les boucles dans un réseau local commuté en bloquant
 * certains ports pour former un arbre de recouvrement (spanning tree).
 *
 * Étapes principales de STP :
 *   1. Élection de la racine (plus petit ID/MAC)
 *   2. Élection des ports racines (chemin le plus court vers la racine)
 *   3. Élection des ports désignés (un port actif transmetteur par segment)
 *   4. Blocage des ports restants
 */

#pragma once

#include "equipement.h"

/* Adresse MAC multicast "all bridges" (01:80:C2:00:00:00) pour les BPDUs */
extern const MAC MAC_ALL_BRIDGES;

/* Initialise le protocole STP sur le réseau */
bool stp_init(reseau_local *r);

/* Initialise chaque switch comme racine par défaut, ports bloqués */
void stp_initialiser_ponts(reseau_local *r);

/* Crée un BPDU 802.1d */
BPDU creer_bpdu_802_1d(size_t racine_id, size_t cout, MAC transmetteur_id);

/* Encapsule un BPDU dans une trame Ethernet multicast */
trame encapsuler_bpdu_dans_trame(MAC source, BPDU *bpdu);

/* Extrait le BPDU contenu dans une trame */
BPDU extraire_bpdu_de_trame(trame *t);

/* Détermine si bpdu1 offre un meilleur chemin que bpdu2 (Racine ID -> Coût -> MAC) */
bool bpdu_est_meilleur(BPDU *bpdu1, BPDU *bpdu2);

/* Diffuse les BPDUs d'un switch vers tous ses voisins switchs (récursif si changement d'état) */
void stp_diffuser_trames(reseau_local *r, size_t id_switch);

/* Traite un BPDU reçu sur un switch et met à jour son état racine/port racine */
bool stp_traiter_trame_recue(reseau_local *r, size_t id_switch_recepteur,
                              size_t cable_idx, trame *trame_recue);

/* Résout l'état final de tous les ports (RACINE, DÉSIGNÉ, BLOQUÉ) après convergence */
void stp_resoudre_etats_ports(reseau_local *r);

/* Récupère l'index du switch racine */
size_t stp_obtenir_index_racine(reseau_local *r);
