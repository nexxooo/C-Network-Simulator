/**
 * @file stp.h
 * @brief Déclarations des fonctions du protocole STP (Spanning Tree Protocol) 802.1d.
 *
 * Le Spanning Tree Protocol (protocole de l'arbre couvrant) est un protocole réseau
 * dont le but est d'ÉLIMINER LES BOUCLES dans un réseau local commuté.
 *
 * POURQUOI LES BOUCLES SONT-ELLES DANGEREUSES ?
 * Dans un réseau avec des boucles, une trame de broadcast (diffusion) va tourner
 * en boucle indéfiniment → "tempête de broadcast" → le réseau est saturé et tombe.
 *
 * COMMENT STP RÉSOUT CE PROBLÈME ?
 * STP organise les switchs en arbre (structure sans boucle) en bloquant
 * certains ports. Les liens redondants sont désactivés "logiquement"
 * mais restent prêts à reprendre du service si un lien principal tombe.
 *
 * LES GRANDES ÉTAPES DE STP :
 *   1. Élection de la racine : le switch avec le meilleur (plus petit) identifiant devient la racine.
 *   2. Élection des ports racines : chaque switch (sauf la racine) identifie son port
 *      qui offre le chemin le moins coûteux vers la racine → ce port devient RACINE.
 *   3. Élection des ports désignés : sur chaque segment de réseau, un seul switch
 *      a un port DÉSIGNÉ (celui qui transmet vers ce segment).
 *   4. Blocage : tous les ports qui ne sont ni RACINE ni DÉSIGNÉ sont BLOQUÉS.
 */

#pragma once  /* Protège contre l'inclusion multiple */

#include "equipement.h"  /* On a besoin des structures MAC, reseau_local, BPDU, trame, etc. */

/**
 * @brief Adresse MAC multicast "all bridges" utilisée pour les trames BPDU.
 *
 * Dans le standard 802.1d, les BPDUs sont envoyés à l'adresse multicast
 * 01:80:C2:00:00:00 (appelée "all bridges"). Cela signifie que tous les
 * switchs du réseau reçoivent ces trames, mais pas les stations finales.
 *
 * Le mot-clé `extern` indique que cette variable est définie dans stp.c
 * et simplement déclarée ici (pour pouvoir l'utiliser dans d'autres fichiers).
 */
extern const MAC MAC_ALL_BRIDGES;


/* =========================================================
   FONCTIONS PRINCIPALES DE STP
   ========================================================= */

/**
 * @brief Point d'entrée principal du protocole STP.
 *
 * Cette fonction orchestre l'exécution complète du protocole STP en 5 étapes :
 *   Étape 1 : Initialisation de tous les ponts (chacun se croit la racine)
 *   Étapes 2-4 : Diffusion des BPDUs et convergence vers une même racine
 *   Étape 5 : Résolution de l'état de chaque port (racine / désigné / bloqué)
 *
 * @param r  Pointeur vers le réseau local sur lequel appliquer STP.
 * @return   true si STP s'est exécuté sans erreur.
 */
bool stp_init(reseau_local *r);


/**
 * @brief Étape 1 — Initialisation de tous les ponts du réseau.
 *
 * Au démarrage de STP, chaque switch se considère comme la racine de l'arbre.
 * Il initialise son coût vers la racine à 0, et tous ses ports sont mis
 * à l'état BLOQUÉ (état par défaut, le plus sûr).
 *
 * @param r  Pointeur vers le réseau local.
 */
void stp_initialiser_ponts(reseau_local *r);

/**
 * @brief Crée un BPDU (Bridge Protocol Data Unit) 802.1d.
 *
 * Un BPDU est le message que les switchs s'échangent pour se mettre d'accord
 * sur la topologie de l'arbre. Il contient 3 informations clés :
 *   - Qui est la racine selon l'émetteur
 *   - Quel est le coût du chemin entre l'émetteur et la racine
 *   - Qui est l'émetteur (son MAC sert d'identifiant de tiebreak)
 *
 * @param racine_id       Index dans le tableau d'équipements du switch racine.
 * @param cout            Coût cumulé pour aller jusqu'à la racine.
 * @param transmetteur_id MAC du switch qui envoie ce BPDU.
 * @return                Le BPDU créé.
 */
BPDU creer_bpdu_802_1d(size_t racine_id, size_t cout, MAC transmetteur_id);

/**
 * @brief Encapsule un BPDU dans une trame Ethernet.
 *
 * Pour être transmis sur le réseau, un BPDU doit être "emballé" dans une trame Ethernet.
 * La destination est l'adresse multicast MAC_ALL_BRIDGES (01:80:C2:00:00:00),
 * ce qui signifie que tous les switchs (et seulement eux) reçoivent cette trame.
 *
 * @param source  MAC du switch émetteur.
 * @param bpdu    Pointeur vers le BPDU à encapsuler.
 * @return        La trame Ethernet prête à être envoyée.
 */
trame encapsuler_bpdu_dans_trame(MAC source, BPDU *bpdu);

/**
 * @brief Extrait le BPDU contenu dans une trame reçue.
 *
 * Opération inverse de encapsuler_bpdu_dans_trame().
 * On lit le champ `bpdu` de la trame (via l'union data/bpdu).
 *
 * @param t  Pointeur vers la trame reçue.
 * @return   Le BPDU extrait.
 */
BPDU extraire_bpdu_de_trame(trame *t);

/**
 * @brief Compare deux BPDUs et détermine lequel est le "meilleur".
 *
 * Un BPDU est meilleur s'il indique un meilleur chemin vers la racine.
 * La comparaison se fait par priorité décroissante :
 *   1. Le plus petit Racine ID (l'indice du switch racine)
 *   2. En cas d'égalité : le coût le plus faible
 *   3. En cas d'égalité : la MAC du transmetteur la plus petite (tiebreak final)
 *
 * @param bpdu1  Premier BPDU.
 * @param bpdu2  Deuxième BPDU.
 * @return       true si bpdu1 est meilleur que bpdu2.
 */
bool bpdu_est_meilleur(BPDU *bpdu1, BPDU *bpdu2);

/**
 * @brief Étapes 2-4 — Diffuse les BPDUs d'un switch vers tous ses voisins switchs.
 *
 * Le switch id_switch crée un BPDU avec ses infos actuelles (racine qu'il connaît,
 * coût vers cette racine, son propre MAC), l'encapsule dans une trame,
 * et l'envoie à tous ses voisins switchs.
 *
 * Si un voisin améliore son état grâce à ce BPDU, il rediffuse à son tour
 * (propagation récursive jusqu'à convergence — quand plus rien ne change).
 *
 * @param r          Pointeur vers le réseau.
 * @param id_switch  Index du switch émetteur dans le tableau d'équipements.
 */
void stp_diffuser_trames(reseau_local *r, size_t id_switch);

/**
 * @brief Traite une trame BPDU reçue par un switch.
 *
 * Lorsqu'un switch reçoit une trame BPDU, il :
 *   1. Extrait le BPDU et calcule le coût réel (coût BPDU + poids du câble)
 *   2. Compare le BPDU reçu avec son état actuel
 *   3. Si le BPDU reçu est meilleur : met à jour sa racine connue et son port racine
 *   4. Sauvegarde le meilleur BPDU reçu sur ce port
 *
 * @param r                     Pointeur vers le réseau.
 * @param id_switch_recepteur   Index du switch qui reçoit la trame.
 * @param cable_idx             Index du câble par lequel la trame est arrivée.
 * @param trame_recue           Pointeur vers la trame reçue.
 * @return                      true si l'état du switch a changé (propagation nécessaire).
 */
bool stp_traiter_trame_recue(reseau_local *r, size_t id_switch_recepteur,
                              size_t cable_idx, trame *trame_recue);


/**
 * @brief Étape 5 — Résout l'état final de tous les ports après convergence STP.
 *
 * Une fois que tous les switchs se sont mis d'accord sur la racine,
 * cette fonction attribue l'état définitif à chaque port :
 *   - RACINE   : le port du chemin le plus court vers la racine (1 par switch, sauf la racine)
 *   - DÉSIGNÉ  : le port actif qui dessert un segment (au moins 1 par segment)
 *   - BLOQUÉ   : tous les autres ports (pour casser les boucles)
 *
 * @param r  Pointeur vers le réseau.
 */
void stp_resoudre_etats_ports(reseau_local *r);

/**
 * @brief Renvoie l'index du switch élu comme racine de l'arbre STP.
 *
 * Le switch racine est celui pour lequel sa propre MAC est égale à la MAC
 * qu'il croit être la racine (sw->racine == sw->mac).
 * Après convergence, un seul switch satisfait cette condition.
 *
 * @param r  Pointeur vers le réseau.
 * @return   Index du switch racine, ou SIZE_MAX si non trouvé.
 */
size_t stp_obtenir_index_racine(reseau_local *r);

