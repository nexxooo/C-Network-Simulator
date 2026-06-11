/**
 * @file stp.c
 * @brief Implémentation du protocole STP (Spanning Tree Protocol) 802.1d.
 *
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║               FONCTIONNEMENT DU SPANNING TREE PROTOCOL                 ║
 * ╠══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                          ║
 * ║  PROBLÈME : Dans un réseau avec des boucles (redondance physique),       ║
 * ║  les trames de broadcast tournent indéfiniment → "tempête de broadcast"  ║
 * ║  → saturation totale du réseau.                                          ║
 * ║                                                                          ║
 * ║  SOLUTION : STP calcule un arbre couvrant (spanning tree) qui            ║
 * ║  connecte tous les équipements sans créer de boucle.                     ║
 * ║  Les liens "en trop" sont BLOQUÉS logiquement (mais restent prêts        ║
 * ║  à reprendre si un lien principal tombe).                                ║
 * ║                                                                          ║
 * ║  ALGORITHME EN 5 ÉTAPES :                                                ║
 * ║  1. Initialisation : chaque switch se croit la racine, ports bloqués     ║
 * ║  2. Diffusion : chaque switch envoie son BPDU à ses voisins              ║
 * ║  3. Convergence : les switchs mettent à jour leur racine connue          ║
 * ║  4. Propagation : les mises à jour se propagent récursivement            ║
 * ║  5. Classification des ports : RACINE / DÉSIGNÉ / BLOQUÉ                ║
 * ║                                                                          ║
 * ║  RÉSULTAT : Un seul switch est la racine. Chaque switch non-racine       ║
 * ║  a exactement un port RACINE. Chaque segment a exactement un port        ║
 * ║  DÉSIGNÉ. Tous les autres ports sont BLOQUÉS.                            ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 */

#include "../include/stp.h"        /* Prototypes des fonctions STP */
#include "../include/affichage.h"  /* Pour afficher_bpdu() lors du débogage */

/* =========================================================
   CONSTANTE GLOBALE
   ========================================================= */

/**
 * Adresse MAC multicast "all bridges" : 01:80:C2:00:00:00
 * Cette adresse est réservée dans le standard IEEE 802.1d pour les BPDUs.
 * Tous les switchs reçoivent les trames envoyées à cette adresse,
 * mais les stations finales les ignorent (les switchs les filtrent).
 *
 * `const` : la valeur ne peut jamais être modifiée.
 * La double accolade {{ }} est nécessaire car MAC contient un tableau bytes[6].
 */
const MAC MAC_ALL_BRIDGES = {{0x01, 0x80, 0xC2, 0x00, 0x00, 0x00}};

/* =========================================================
   ÉTAPE 1 : INITIALISATION DES PONTS
   ========================================================= */

/**
 * @brief Initialise chaque switch avant de démarrer STP.
 *
 * Au démarrage de STP (ou après une réinitialisation), chaque switch
 * se considère comme la racine de l'arbre :
 *   - sw->racine = sw->mac   (je suis la racine)
 *   - sw->cout_vers_racine = 0  (coût nul : je suis la racine)
 *   - Tous les ports → BLOQUÉ  (état le plus sûr par défaut)
 *
 * @param r  Pointeur vers le réseau.
 */
void stp_initialiser_ponts(reseau_local *r)
{
    /* Itère sur tous les équipements du réseau */
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        /* Ignore les stations (seuls les switches/ponts participent à STP) */
        if ( r->equipements[i].type_equ == SWITCH )
        {
            switch_ *sw = &r->equipements[i].sw;  /* Pointeur vers le switch */

            /* Au démarrage, un pont se considère comme la racine :
             * sa "racine connue" est sa propre adresse MAC */
            sw->racine = sw->mac;
            sw->cout_vers_racine = 0;  /* Coût = 0 car on croit être la racine */

            /* Initialement tous les ports sont inactifs (bloqués).
             * STP les activera un par un selon la topologie. */
            for ( size_t j = 0; j < sw->nb_port; j++ )
            {
                sw->ports[j].etat = ETAT_PORT_BLOQUE;  /* Bloqué par défaut */
                sw->ports[j].a_recu_bpdu = false;      /* Aucun BPDU reçu */
            }
        }
    }
}

/* =========================================================
   CRÉATION ET MANIPULATION DES BPDUs
   ========================================================= */

/**
 * @brief Crée un BPDU (Bridge Protocol Data Unit) 802.1d.
 *
 * Un BPDU est le message clé de STP. Il contient 3 champs :
 *   - racine_id      : qui est la racine (selon l'émetteur)
 *   - cout           : le coût du chemin de l'émetteur jusqu'à la racine
 *   - transmetteur_id: l'identité (MAC) de l'émetteur
 *
 * Les switchs s'échangent ces BPDUs pour converger vers une même racine.
 *
 * @param racine_id        Index du switch racine dans le tableau d'équipements.
 * @param cout             Coût cumulé du chemin vers la racine.
 * @param transmetteur_id  MAC de l'émetteur du BPDU.
 * @return                 Le BPDU créé.
 */
BPDU creer_bpdu_802_1d(size_t racine_id, size_t cout, MAC transmetteur_id)
{
    BPDU bpdu;
    bpdu.racine_id = racine_id;          /* Index du switch racine */
    bpdu.cout = cout;                    /* Coût cumulé pour atteindre la racine */
    bpdu.transmetteur_id = transmetteur_id;  /* Qui envoie ce BPDU */
    return bpdu;
}

/**
 * @brief Encapsule un BPDU dans une trame Ethernet prête à être "envoyée".
 *
 * Pour être transmis sur le réseau, le BPDU doit être enveloppé dans une trame Ethernet.
 * Spécificités de la trame BPDU :
 *   - Préambule : 7 × 0xAA (synchronisation standard Ethernet)
 *   - SFD       : 0xAB (Start Frame Delimiter)
 *   - Destination : MAC_ALL_BRIDGES (01:80:C2:00:00:00) → multicast vers tous les ponts
 *   - Type      : 0x8809 (Slow Protocols, standard IEEE pour STP)
 *   - Payload   : le BPDU
 *   - FCS       : 0 (simplifié)
 *
 * @param source  MAC du switch émetteur.
 * @param bpdu    Pointeur vers le BPDU à encapsuler.
 * @return        La trame Ethernet contenant le BPDU.
 */
trame encapsuler_bpdu_dans_trame(MAC source, BPDU *bpdu)
{
    trame t = {0};

    t.source = source;
    /* Les trames BPDU sont adressées en multicast (all bridges) :
     * tous les switchs les reçoivent, pas les stations */
    t.destination = MAC_ALL_BRIDGES;

    /* Type 0x8809 = "Slow Protocols" (IEEE 802.3) utilisé pour STP */
    t.type = 0x8809;

    /* Encapsuler le BPDU dans la trame */
    t.bpdu = *bpdu;  /* Copie le BPDU par valeur dans la trame */

    return t;
}

/**
 * @brief Extrait le BPDU contenu dans une trame Ethernet.
 *
 * Opération inverse de encapsuler_bpdu_dans_trame().
 * Accède au champ `bpdu` de l'union data/bpdu de la trame.
 * C'est possible car on sait que cette trame est de type 0x8809 (BPDU STP).
 *
 * @param t  Pointeur vers la trame contenant le BPDU.
 * @return   Le BPDU extrait (copie par valeur).
 */
BPDU extraire_bpdu_de_trame(trame *t)
{
    return t->bpdu;  /* Accède directement au champ bpdu */
}

/**
 * @brief Compare deux BPDUs et détermine lequel représente le "meilleur" chemin vers la racine.
 *
 * RÈGLES DE COMPARAISON (par ordre de priorité décroissant) :
 *
 *   Règle 1 : Le plus petit Racine ID gagne.
 *             (Un switch avec un indice plus petit est préféré comme racine)
 *
 *   Règle 2 : Si égalité sur la racine → le plus petit coût gagne.
 *             (Le chemin le moins coûteux vers la racine est préféré)
 *
 *   Règle 3 : Si égalité sur le coût → la plus petite MAC du transmetteur gagne.
 *             (Tiebreak final pour départager des chemins de coût égal)
 *
 * En pratique dans STP 802.1d, les Racine ID et Transmetteur ID sont des priorités+MAC,
 * mais ici on utilise l'index dans le tableau comme priorité (simplifié).
 *
 * @param bpdu1  Premier BPDU.
 * @param bpdu2  Deuxième BPDU.
 * @return       true si bpdu1 est meilleur que bpdu2, false sinon.
 */
bool bpdu_est_meilleur(BPDU *bpdu1, BPDU *bpdu2)
{
    /* Règle 1 : compare les identifiants de racine */
    if ( bpdu1->racine_id < bpdu2->racine_id )
        return true;   /* bpdu1 annonce une meilleure racine */
    if ( bpdu1->racine_id > bpdu2->racine_id )
        return false;  /* bpdu2 annonce une meilleure racine */

    /* Règle 2 : même racine → compare les coûts */
    if ( bpdu1->cout < bpdu2->cout )
        return true;   /* bpdu1 a un chemin moins coûteux */
    if ( bpdu1->cout > bpdu2->cout )
        return false;  /* bpdu2 a un chemin moins coûteux */

    /* Règle 3 : même racine, même coût → compare les MACs des transmetteurs */
    return mac_est_meilleure(&bpdu1->transmetteur_id, &bpdu2->transmetteur_id);
}

/* =========================================================
   FONCTIONS UTILITAIRES INTERNES (statiques)
   ========================================================= */

/**
 * @brief Recherche l'index d'un équipement par son adresse MAC.
 *
 * Fonction statique (privée à ce fichier, non visible depuis les autres .c).
 * Parcourt tous les équipements et compare les MACs.
 *
 * @param r    Pointeur vers le réseau.
 * @param mac  Pointeur vers la MAC recherchée.
 * @return     Index de l'équipement trouvé, ou SIZE_MAX si non trouvé.
 */
static size_t get_index_par_mac(reseau_local *r, MAC *mac)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        /* Récupère la MAC de l'équipement selon son type
         * (les switchs ont sw.mac, les stations ont st.mac) */
        MAC *m = (r->equipements[i].type_equ == SWITCH)
                     ? &r->equipements[i].sw.mac   /* Pointeur vers la MAC du switch */
                     : &r->equipements[i].st.mac;  /* Pointeur vers la MAC de la station */

        if ( mac_est_egale(m, mac) )
            return i;  /* Trouvé ! Retourne l'index */
    }
    return SIZE_MAX;  /* Non trouvé */
}

/**
 * @brief Recherche l'index d'un câble reliant deux sommets donnés.
 *
 * Fonction statique privée.
 * Utilise cable_est_relie() pour la comparaison bidirectionnelle.
 *
 * @param r        Pointeur vers le réseau.
 * @param sommet1  Premier sommet.
 * @param sommet2  Deuxième sommet.
 * @return         Index du câble trouvé, ou SIZE_MAX si non trouvé.
 */
static size_t trouver_cable(reseau_local *r, size_t sommet1, size_t sommet2)
{
    for ( size_t i = 0; i < r->nb_cables; i++ )
        if ( cable_est_relie(&r->cables[i], sommet1, sommet2) )
            return i;  /* Câble trouvé */
    return SIZE_MAX;   /* Aucun câble ne relie ces deux sommets */
}

/* =========================================================
   ÉTAPES 2-4 : DIFFUSION ET TRAITEMENT DES BPDUs
   ========================================================= */

/**
 * @brief Traite un BPDU reçu par un switch et met à jour son état STP.
 *
 * Quand le switch id_switch_recepteur reçoit une trame BPDU via un câble :
 *
 * 1. EXTRACTION : On extrait le BPDU de la trame.
 *
 * 2. CALCUL DU COÛT RÉEL : Le coût dans le BPDU représente le coût de
 *    l'émetteur jusqu'à la racine. Pour calculer NOTRE coût, on ajoute
 *    la pondération du câble entre l'émetteur et nous.
 *    → bpdu_recu.cout = bpdu_extrait.cout + pondération_câble
 *
 * 3. SAUVEGARDE PAR PORT : On garde en mémoire le meilleur BPDU reçu sur
 *    chaque port (utile pour déterminer le port racine).
 *
 * 4. MISE À JOUR : Si le BPDU reçu est meilleur que notre état actuel :
 *    → On met à jour notre racine connue et notre coût vers la racine
 *    → On identifie ce port comme notre port racine
 *    → On retourne true pour déclencher une propagation (on va diffuser à notre tour)
 *
 * @param r                      Pointeur vers le réseau.
 * @param id_switch_recepteur    Index du switch qui reçoit la trame.
 * @param cable_idx              Index du câble par lequel la trame est arrivée.
 * @param trame_recue            Pointeur vers la trame BPDU reçue.
 * @return                       true si l'état du switch a été amélioré (propagation nécessaire).
 */
bool stp_traiter_trame_recue(reseau_local *r, size_t id_switch_recepteur,
                             size_t cable_idx, trame *trame_recue)
{
    switch_ *sw = &r->equipements[id_switch_recepteur].sw;  /* Accès direct au switch récepteur */

    /* 1. Extraire le BPDU de la trame reçue */
    BPDU bpdu_extrait = extraire_bpdu_de_trame(trame_recue);

    /* 2. Calculer le coût réel : coût du BPDU + pondération du câble entrant
     *    La pondération représente le "coût" de ce lien (ex: 4 pour Fast Ethernet 100 Mbps).
     *    Plus un lien est lent, plus son coût est élevé (pour éviter les liens lents). */
    size_t ponderation = r->cables[cable_idx].ponderation;
    BPDU bpdu_recu = creer_bpdu_802_1d(
        bpdu_extrait.racine_id,              /* Racine annoncée par l'émetteur */
        bpdu_extrait.cout + ponderation,     /* Coût de l'émetteur + coût du câble vers nous */
        bpdu_extrait.transmetteur_id);       /* Qui a envoyé ce BPDU */

    /* 3. Construire le BPDU représentant notre état actuel (pour comparaison) */
    size_t racine_actuelle_id = get_index_par_mac(r, &sw->racine);  /* Index de notre racine connue */
    BPDU bpdu_actuel = creer_bpdu_802_1d(
        racine_actuelle_id,       /* Notre racine actuelle */
        sw->cout_vers_racine,     /* Notre coût actuel vers la racine */
        sw->mac);                 /* Notre propre MAC comme "transmetteur" */

    /* 4. Sauvegarder le meilleur BPDU reçu sur ce port
     *    (le pont sauvegarde pour chaque port le meilleur message reçu,
     *    ce qui permet de déterminer le port racine) */
    size_t port_local = obtenir_port_local(r, id_switch_recepteur, cable_idx);
    if ( port_local < sw->nb_port )  /* Vérifie que le port est valide */
    {


        /* Met à jour le meilleur BPDU reçu sur ce port si :
         *   - C'est le premier BPDU reçu sur ce port (a_recu_bpdu == false)
         *   - OU le nouveau BPDU est meilleur que le précédent */
        if ( !sw->ports[port_local].a_recu_bpdu ||
             bpdu_est_meilleur(&bpdu_recu, &sw->ports[port_local].meilleur_bpdu_recu) )
        {
            sw->ports[port_local].meilleur_bpdu_recu = bpdu_recu;  /* Mémorise le meilleur BPDU */
            sw->ports[port_local].a_recu_bpdu = true;               /* Marque qu'on a reçu un BPDU */
        }
    }

    /* 5. Mise à jour de l'état global du switch uniquement si le BPDU reçu
     *    est MEILLEUR que notre état actuel (on a appris quelque chose de nouveau) */
    if ( bpdu_est_meilleur(&bpdu_recu, &bpdu_actuel) )
    {
        /* Mettre à jour la racine connue : on adopte la racine annoncée dans le BPDU */
        if ( r->equipements[bpdu_recu.racine_id].type_equ == SWITCH )
            sw->racine = r->equipements[bpdu_recu.racine_id].sw.mac;

        /* Met à jour notre coût vers la racine */
        sw->cout_vers_racine = bpdu_recu.cout;

        /* Le port racine est le port par lequel on a reçu le meilleur message :
         * c'est notre chemin vers la racine */
        if ( port_local < sw->nb_port )
        {
            sw->port_racine.numero_port = port_local;     /* Numéro du port racine */
            sw->port_racine.etat = ETAT_PORT_RACINE;      /* État du port racine */
        }

        return true; /* État mis à jour → propagation nécessaire (on doit rediffuser) */
    }

    return false; /* Rien de nouveau → pas de propagation nécessaire */
}

/**
 * @brief Diffuse les BPDUs d'un switch vers tous ses voisins switchs.
 *
 * Cette fonction implémente les étapes 2, 3 et 4 de STP :
 *
 * Le switch id_switch crée un BPDU avec ses informations actuelles :
 *   - Quelle racine il connaît (peut avoir changé depuis l'itération précédente)
 *   - Son coût actuel vers cette racine
 *   - Son propre MAC comme identifiant transmetteur
 *
 * Il envoie ce BPDU à tous ses voisins qui sont des SWITCHES.
 * (Les stations ne participent pas à STP et ignorent les BPDUs)
 *
 * Si un voisin est amélioré par ce BPDU (stp_traiter_trame_recue retourne true),
 * alors ce voisin diffuse à son tour → PROPAGATION RÉCURSIVE.
 * La récursion s'arrête quand plus aucun voisin n'est amélioré.
 * C'est ainsi que STP "converge" vers une solution stable.
 *
 * @param r          Pointeur vers le réseau.
 * @param id_switch  Index du switch qui diffuse ses BPDUs.
 */
void stp_diffuser_trames(reseau_local *r, size_t id_switch)
{
    switch_ *sw = &r->equipements[id_switch].sw;  /* Accès au switch émetteur */

    /* Crée le BPDU que ce switch transmet :
     * racine actuelle, coût vers la racine, MAC du transmetteur */
    size_t racine_id = get_index_par_mac(r, &sw->racine);  /* Index de la racine connue */
    BPDU bpdu = creer_bpdu_802_1d(racine_id, sw->cout_vers_racine, sw->mac);

    /* Encapsule le BPDU dans une trame Ethernet (destination multicast) */
    trame trame_a_envoyer = encapsuler_bpdu_dans_trame(sw->mac, &bpdu);

    /* Trouve tous les voisins directs du switch (tous les équipements connectés) */
    size_t adjacents[r->nb_equipements];  /* Tableau pour stocker les indices des voisins */
    size_t n_adj = sommets_adjacent(r, id_switch, adjacents);  /* Remplit le tableau */

    /* Envoie le BPDU à chaque voisin switch */
    for ( size_t a = 0; a < n_adj; a++ )
    {
        size_t voisin_id = adjacents[a];  /* Index du voisin actuel */

        /* Seuls les ponts (switches) lisent les trames BPDU.
         * Les stations ignorent les BPDUs. */
        if ( r->equipements[voisin_id].type_equ != SWITCH )
            continue;  /* Passe au voisin suivant */

        /* Retrouver le câble reliant ce switch à son voisin */
        size_t cable_idx = trouver_cable(r, id_switch, voisin_id);
        if ( cable_idx == SIZE_MAX )
            continue;  /* Pas de câble trouvé (ne devrait pas arriver) */

        /* Le voisin reçoit la trame et la traite.
         * stp_traiter_trame_recue retourne true si l'état du voisin a changé. */
        if ( stp_traiter_trame_recue(r, voisin_id, cable_idx, &trame_a_envoyer) )
        {
            /* Si l'état du voisin a changé, il propage à son tour ses trames
             * (avec son propre MAC comme transmetteur, et ses nouvelles infos).
             * C'est la PROPAGATION RÉCURSIVE : les bonnes nouvelles se propagent
             * jusqu'à ce que le réseau converge (plus aucun état ne change). */
            stp_diffuser_trames(r, voisin_id);
        }
    }
}

/* =========================================================
   ÉTAPE 5 : RÉSOLUTION DE L'ÉTAT DES PORTS
   ========================================================= */

/**
 * @brief Retourne l'index du switch élu comme racine de l'arbre STP.
 *
 * Après convergence, un seul switch a sw->racine == sw->mac
 * (sa racine connue est lui-même → il est la racine).
 *
 * @param r  Pointeur vers le réseau.
 * @return   Index du switch racine, ou SIZE_MAX si non trouvé.
 */
size_t stp_obtenir_index_racine(reseau_local *r)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ == SWITCH )
        {
            switch_ *sw = &r->equipements[i].sw;

            /* Si la racine connue par ce switch est sa propre MAC → c'est la racine */
            if ( mac_est_egale(&sw->racine, &sw->mac) )
                return i;  /* Ce switch est la racine */
        }
    }
    return SIZE_MAX;  /* Aucune racine trouvée (erreur de convergence) */
}

/**
 * @brief Étape 5b — Attribue l'état RACINE au port racine de chaque switch non-racine.
 *
 * Chaque switch non-racine a identifié (pendant la phase de diffusion) son port
 * racine (sw->port_racine.numero_port). Cette fonction rend cet état officiel.
 *
 * La racine elle-même n'a pas de port racine (elle est la racine → aucun chemin
 * "vers la racine" à choisir).
 *
 * @param r          Pointeur vers le réseau.
 * @param racine_idx Index du switch racine (pas de port racine pour lui).
 */
static void stp_determiner_ports_racines(reseau_local *r, size_t racine_idx)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        /* Ignore les stations et le switch racine */
        if ( r->equipements[i].type_equ != SWITCH )
            continue;
        if ( i == racine_idx )
            continue; /* La racine n'a pas de port racine */

        switch_ *sw = &r->equipements[i].sw;
        size_t root_port = sw->port_racine.numero_port;  /* Numéro du port racine identifié */

        /* Valide le numéro de port et lui attribue l'état RACINE */
        if ( root_port < sw->nb_port )
            sw->ports[root_port].etat = ETAT_PORT_RACINE;
    }
}

/**
 * @brief Étape 5c — Attribue l'état DÉSIGNÉ aux ports qui doivent transmettre.
 *
 * RÈGLES :
 *   1. Le switch racine : TOUS ses ports sont DÉSIGNÉS.
 *      (Il est la racine → tous ses ports ouvrent des chemins vers l'extérieur)
 *
 *   2. Pour les autres switches, pour chaque port NON-RACINE :
 *      - Si le voisin est une station → DÉSIGNÉ (toujours, les stations ne font pas STP)
 *      - Si le voisin est un switch :
 *        → Si notre coût vers la racine < celui du voisin → DÉSIGNÉ
 *          (on est "plus proche" de la racine → c'est nous qui servons ce segment)
 *        → Si égalité des coûts → DÉSIGNÉ si notre MAC est "meilleure" (plus petite)
 *          (tiebreak par MAC : le switch avec la plus petite MAC est désigné)
 *        → Sinon → reste BLOQUÉ (le voisin est mieux placé pour être désigné)
 *
 * @param r          Pointeur vers le réseau.
 * @param racine_idx Index du switch racine.
 */
static void stp_determiner_ports_designes(reseau_local *r, size_t racine_idx)
{
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ != SWITCH )
            continue;

        switch_ *sw = &r->equipements[i].sw;

        /* Règle 1 : Si c'est le commutateur racine, tous ses ports sont DÉSIGNÉS */
        if ( i == racine_idx )
        {
            for ( size_t p = 0; p < sw->nb_port; p++ )
                sw->ports[p].etat = ETAT_PORT_DESIGNE;  /* Tous désignés */
            continue;  /* Passe au switch suivant */
        }

        /* Règle 2 : Pour les autres switches, vérifier chaque port non-racine */
        size_t root_port_local = sw->port_racine.numero_port;  /* Port racine de ce switch */
        size_t port_loc = 0;  /* Compteur de port local (même logique que obtenir_port_local) */

        /* Itère sur tous les câbles pour trouver ceux qui touchent ce switch */
        for ( size_t c = 0; c < r->nb_cables; c++ )
        {
            /* Ce câble touche-t-il le switch i ? */
            if ( r->cables[c].sommet1 != i && r->cables[c].sommet2 != i )
                continue;  /* Non → passe au câble suivant */

            /* Le port racine est déjà traité à l'étape 5b (stp_determiner_ports_racines) */
            if ( port_loc == root_port_local )
            {
                port_loc++;  /* Incrémente le compteur de port */
                continue;    /* Passe au câble suivant */
            }

            /* Trouve l'index du voisin à l'autre extrémité du câble */
            size_t voisin_idx = (r->cables[c].sommet1 == i)
                                    ? r->cables[c].sommet2   /* switch i est sommet1 → voisin = sommet2 */
                                    : r->cables[c].sommet1;  /* switch i est sommet2 → voisin = sommet1 */

            /* Règle 2a : Port vers une station → toujours DÉSIGNÉ */
            if ( r->equipements[voisin_idx].type_equ == STATION )
            {
                sw->ports[port_loc].etat = ETAT_PORT_DESIGNE;
            }
            /* Règle 2b : Port vers un switch → comparer les coûts vers la racine */
            else if ( r->equipements[voisin_idx].type_equ == SWITCH )
            {
                switch_ *v_sw = &r->equipements[voisin_idx].sw;  /* Le switch voisin */

                if ( sw->cout_vers_racine < v_sw->cout_vers_racine )
                {
                    /* Notre coût est plus faible → on est plus proche de la racine
                     * → on est le port DÉSIGNÉ pour ce segment */
                    sw->ports[port_loc].etat = ETAT_PORT_DESIGNE;
                }
                else if ( sw->cout_vers_racine == v_sw->cout_vers_racine )
                {
                    /* Même coût → TIEBREAK par MAC : le pont avec le meilleur (plus petit) MAC est désigné */
                    if ( mac_est_meilleure(&sw->mac, &v_sw->mac) )
                        sw->ports[port_loc].etat = ETAT_PORT_DESIGNE;
                    /* Sinon : le port reste BLOQUÉ (le voisin est désigné à notre place) */
                }
                /* Si notre coût est supérieur : le port reste BLOQUÉ (le voisin est mieux placé) */
            }
            port_loc++;  /* Passe au port suivant */
        }
    }
}

/**
 * @brief Étape 5 complète — Résout l'état final de tous les ports après convergence.
 *
 * ALGORITHME :
 *   1. Remet tous les ports à BLOQUÉ (réinitialisation propre)
 *   2. Détermine les ports RACINES (étape 5b)
 *   3. Détermine les ports DÉSIGNÉS (étape 5c)
 *   4. Les ports restants restent BLOQUÉS
 *
 * RÉSULTAT ATTENDU :
 *   - Switch racine : tous ports DÉSIGNÉS
 *   - Chaque autre switch : 1 port RACINE + éventuellement des ports DÉSIGNÉS + des ports BLOQUÉS
 *   - Le réseau résultant (ports non-bloqués) forme un arbre sans boucles
 *
 * @param r  Pointeur vers le réseau.
 */
void stp_resoudre_etats_ports(reseau_local *r)
{
    /* Trouve l'index du switch racine */
    size_t racine_idx = stp_obtenir_index_racine(r);
    if ( racine_idx == SIZE_MAX )
        return;  /* Impossible si la convergence a bien eu lieu */

    /* Étape 5a : D'abord, tous les ports sont BLOQUÉS par défaut.
     * On repart d'une ardoise propre avant d'attribuer les états. */
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ != SWITCH )
            continue;
        switch_ *sw = &r->equipements[i].sw;
        for ( size_t p = 0; p < sw->nb_port; p++ )
            sw->ports[p].etat = ETAT_PORT_BLOQUE;  /* Tout bloqué par défaut */
    }

    /* Étape 5b : Déterminer les ports racines (un par pont, sauf la racine) */
    stp_determiner_ports_racines(r, racine_idx);

    /* Étape 5c : Déterminer les ports désignés */
    stp_determiner_ports_designes(r, racine_idx);

    /* Les ports restants sont bloqués (déjà initialisés à BLOQUÉ ci-dessus) */
}

/* =========================================================
   POINT D'ENTRÉE PRINCIPAL DU PROTOCOLE STP
   ========================================================= */

/**
 * @brief Orchestre l'exécution complète du protocole STP 802.1d sur le réseau.
 *
 * ÉTAPES EXÉCUTÉES :
 *
 * [Étape 1] stp_initialiser_ponts() :
 *   Chaque switch se croit la racine, tous les ports sont bloqués.
 *
 * [Étapes 2-4] Boucle sur stp_diffuser_trames() pour chaque switch :
 *   Chaque switch envoie son BPDU à ses voisins. Si un voisin est amélioré,
 *   il rediffuse à son tour (propagation récursive jusqu'à convergence).
 *   À la fin, tous les switches s'accordent sur la même racine.
 *
 * [Étape 5] stp_resoudre_etats_ports() :
 *   Classifie chaque port comme RACINE, DÉSIGNÉ ou BLOQUÉ.
 *   Les ports actifs (RACINE + DÉSIGNÉ) forment un arbre couvrant.
 *
 * @param r  Pointeur vers le réseau sur lequel appliquer STP.
 * @return   true si STP s'est exécuté sans erreur.
 */
bool stp_init(reseau_local *r)
{
    printf("============= PROTOCOLE STP 802.1d =============\n");

    /* Étape 1 : Initialisation — chaque pont se considère comme la racine
     * et transmet un message 802.1d avec un coût de 0 sur tous ses ports */
    printf("[Étape 1] Initialisation des ponts\n");
    stp_initialiser_ponts(r);

    /* Étapes 2-4 : Chaque switch crée un BPDU, l'encapsule dans une trame,
     * et la diffuse vers ses voisins. La convergence se fait par propagation
     * récursive jusqu'à ce qu'aucun voisin ne soit amélioré.
     *
     * Pourquoi boucler sur TOUS les switchs même si la propagation est récursive ?
     * → Pour s'assurer qu'on démarre la diffusion depuis chaque switch initial,
     *   au cas où le réseau ne serait pas entièrement connexe ou si certains
     *   switchs ne sont pas atteints par la première propagation. */
    printf("[Étapes 2-4] Diffusion et propagation des trames BPDU\n");
    for ( size_t i = 0; i < r->nb_equipements; i++ )
        if ( r->equipements[i].type_equ == SWITCH )
            stp_diffuser_trames(r, i);  /* Lance la diffusion depuis chaque switch */

    /* Étape 5 : Déterminer l'état de tous les ports (racine, désigné, bloqué)
     * Les ports racines et désignés deviennent actifs, les autres restent bloqués. */
    printf("[Étape 5] Résolution de l'état des ports\n");
    stp_resoudre_etats_ports(r);

    printf("============= STP CONVERGÉ =============\n");
    return true;
}
