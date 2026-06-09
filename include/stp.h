#pragma once

#include "equipement.h"

/* Adresse multicast "all bridges" pour les trames BPDU (01:80:C2:00:00:00) */
extern const MAC MAC_ALL_BRIDGES;


/* Exécute le protocole STP complet sur le réseau */
bool stp_init(reseau_local *r);


/* Chaque pont se considère comme la racine avec un coût de 0,
   tous les ports sont initialisés à l'état inactif (bloqué) */
void stp_initialiser_ponts(reseau_local *r);

/* Crée un BPDU contenant Root ID, Coût et Transmitter ID */
BPDU creer_bpdu_802_1d(size_t racine_id, size_t cout, MAC transmetteur_id);

/* Encapsule un BPDU dans une trame Ethernet (destination multicast all bridges) */
trame encapsuler_bpdu_dans_trame(MAC source, BPDU *bpdu);

/* Extrait le BPDU contenu dans une trame reçue */
BPDU extraire_bpdu_de_trame(trame *t);

/* M1[R=R1,C=C1,T=T1] est meilleur que M2[R=R2,C=C2,T=T2] si :
   R1<R2, ou R1=R2 et C1<C2, ou R1=R2 et C1=C2 et T1<T2 */
bool bpdu_est_meilleur(BPDU *bpdu1, BPDU *bpdu2);

/* Émet des trames BPDU depuis un switch vers tous ses voisins switches */
void stp_diffuser_trames(reseau_local *r, size_t id_switch);

/* Traite une trame BPDU reçue : extrait le BPDU, compare, met à jour si meilleur */
bool stp_traiter_trame_recue(reseau_local *r, size_t id_switch_recepteur,
                              size_t cable_idx, trame *trame_recue);


/* Résout l'état de tous les ports (racine, désigné, bloqué) après convergence */
void stp_resoudre_etats_ports(reseau_local *r);

/* Renvoie l'index du switch racine élu (celui dont racine == sa propre MAC) */
size_t stp_obtenir_index_racine(reseau_local *r);

/* Calcule la distance d'un équipement vers la racine (Dijkstra si station) */
size_t stp_distance_vers_racine(reseau_local *r, equipement *equ);
