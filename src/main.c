/**
 * @file main.c
 * @brief Point d'entrée du programme — démonstration du protocole STP.
 *
 * Ce programme simule un réseau local (LAN) avec des switchs et des stations,
 * applique le protocole STP pour éliminer les boucles, puis simule
 * la transmission de trames Ethernet entre deux stations.
 *
 * DÉROULEMENT DU PROGRAMME :
 *   1. Chargement du réseau depuis un fichier de configuration
 *   2. Affichage du réseau avant STP
 *   3. Exécution de STP (élection de la racine, blocage des ports redondants)
 *   4. Affichage du réseau après STP
 *   5. Construction de l'arbre de recouvrement (réseau sans boucles)
 *   6. Simulation de transmission de trames Ethernet avec commutation
 */

#include "../include/affichage.h"   /* Fonctions d'affichage (afficher_reseau, afficher_cables, etc.) */
#include "../include/equipement.h"  /* Structures et fonctions du réseau (reseau_local, trame, etc.) */
#include "../include/stp.h"         /* Fonctions du protocole STP (stp_init, etc.) */

int main(int argc, char *argv[])
{
    /* === INITIALISATION ET CHARGEMENT DU RÉSEAU === */

    reseau_local r;          /* Déclaration de la variable réseau (structure vide pour l'instant) */
    init_reseau(&r);         /* Initialise le réseau : alloue les tableaux dynamiques (capacité initiale) */

    /* Charge le réseau depuis le fichier de configuration.
     * Le fichier décrit les équipements (stations, switchs) et les câbles.
     * Format : la première ligne indique le nombre d'équipements et de câbles,
     * puis une ligne par équipement, puis une ligne par câble. */
    if ( charger_reseau("configs/config2.txt", &r) != ERR_OK )
        printf("Erreur dans le chargement\n");  /* Affiche une erreur si le chargement a échoué */

    /* === AFFICHAGE INITIAL DU RÉSEAU === */

    /* Vérifie si le réseau est déjà un arbre (connexe + nb_câbles == nb_équipements - 1).
     * Si oui, STP ne changera rien. Si non, STP va bloquer des ports. */
    printf("Le réseau est il un arbre ? %b\n", est_un_arbre(&r));

    /* Affiche tous les équipements du réseau (MAC, IP, nombre de ports...) */
    afficher_reseau(&r);

    /* Affiche tous les câbles du réseau (qui est connecté à qui) */
    afficher_cables(&r);

    /* === ÉTATS DES PORTS AVANT STP === */

    /* Avant STP, tous les ports sont dans l'état INCONNU (état par défaut à la création) */
    printf("\n=== États des ports AVANT STP ===\n");
    afficher_etat_port_reseau(&r);  /* Affiche l'état de chaque port de chaque switch */

    /* === EXÉCUTION DU PROTOCOLE STP === */

    /* Lance le protocole STP sur tout le réseau :
     *   - Initialise chaque switch (se croit la racine, ports bloqués)
     *   - Les switchs échangent des BPDUs pour élire la vraie racine
     *   - Les ports sont classifiés (RACINE / DÉSIGNÉ / BLOQUÉ) */
    stp_init(&r);

    /* === ÉTATS DES PORTS APRÈS STP === */

    /* Après STP, on voit quels ports sont bloqués (pour casser les boucles)
     * et quels ports sont actifs (racine ou désigné) */
    printf("\n=== États des ports APRÈS STP ===\n");
    afficher_etat_port_reseau(&r);

    /* === AFFICHAGE DÉTAILLÉ D'UN SWITCH INDIVIDUEL === */

    /* Affiche les détails du Switch 0 (s'il existe et que c'est bien un switch).
     * Utile pour vérifier port par port l'état STP et les BPDUs reçus. */
    printf("\n=== Affichage individuel (Switch 0) ===\n");
    if ( r.nb_equipements > 0 && r.equipements[0].type_equ == SWITCH )
    {
        afficher_etat_port_switch(&r.equipements[0].sw);  /* Affiche les ports du premier switch */
    }

    /* Réaffiche le réseau après STP pour voir l'évolution */
    afficher_reseau(&r);

    /* Vérifie que le réseau avec les ports actifs forme maintenant un arbre.
     * Note : est_un_arbre() vérifie les câbles du réseau, pas les états des ports.
     * C'est pourquoi on construit l'arbre explicitement à l'étape suivante. */
    printf("Le réseau (après STP) est il un arbre ? %b\n", est_un_arbre(&r));

    /* === CONSTRUCTION DE L'ARBRE DE RECOUVREMENT === */

    /* Crée un nouveau réseau (arbre) qui ne contient que les liens actifs après STP.
     * Les câbles entre switchs dont au moins un port est bloqué sont exclus.
     * Les câbles vers des stations sont toujours inclus (les stations ne font pas STP). */
    reseau_local arbre;
    if ( construire_arbre_selon_reseau(&r, &arbre) )
    {
        printf("\n=== Arbre construit depuis le réseau STP ===\n");
        afficher_reseau(&arbre);      /* Affiche le réseau épuré (sans les liens bloqués) */
        afficher_cables(&arbre);      /* Affiche les câbles restants dans l'arbre */

        /* Vérifie que l'arbre construit est bien un arbre au sens mathématique
         * (graphe connexe avec N-1 arêtes pour N sommets) */
        printf("L'arbre construit est-il un arbre ? %b\n", est_un_arbre(&arbre));

        free_reseau(&arbre);  /* Libère la mémoire de l'arbre (important pour éviter les fuites) */
    }

    /* === SIMULATION DE TRANSMISSION DE TRAMES ETHERNET === */

    printf("\n==================================================\n");
    printf("  SIMULATION DE COMMUTATION DE TRAMES ETHERNET    \n");
    printf("==================================================\n");

    /* On simule uniquement si le réseau a au moins 5 équipements :
     * Dans config2.txt : équipements 0,1,2 = switchs ; 3,4,5 = stations */
    if ( r.nb_equipements >= 5 )
    {
        /* Indices des stations dans le tableau d'équipements */
        size_t idx_station_src = 3; /* Station 1 (MAC: 54:d6:a6:82:c5:01) */
        size_t idx_station_dst = 4; /* Station 2 (MAC: 54:d6:a6:82:c5:02) */

        /* Récupère les adresses MAC des deux stations pour construire les trames */
        MAC mac_src = r.equipements[idx_station_src].st.mac;  /* MAC de la station source */
        MAC mac_dst = r.equipements[idx_station_dst].st.mac;  /* MAC de la station destination */

        /* --- PREMIER ENVOI : Station 1 → Station 2 --- */
        /* La première fois, les switchs ne connaissent pas encore où est la Station 2.
         * Ils vont donc faire du "Flooding" : envoyer la trame sur TOUS les ports actifs.
         * En même temps, ils apprennent que la Station 1 est accessible via son port d'entrée. */
        const char *msg1 = "Hello Station 2!";

        /* Crée une trame Ethernet :
         * - source : MAC de la Station 1
         * - destination : MAC de la Station 2
         * - type 0x0800 : le payload est de l'IPv4
         * - data : le message texte (converti en octets) */
        trame t1 = creer_trame_ethernet(mac_src, mac_dst, 0x0800, (const uint8_t *)msg1, strlen(msg1) + 1);

        /* Envoie la trame depuis la Station 1 à travers le réseau.
         * `true` = mode verbeux (affiche ce qui se passe à chaque switch) */
        envoyer_trame(&r, idx_station_src, &t1, true);

        /* --- DEUXIÈME ENVOI : Station 2 → Station 1 --- */
        /* Cette fois, les switchs ont appris où est la Station 1 (lors du premier envoi).
         * La trame sera donc envoyée directement (Unicast/Commutation) sans flooding. */
        const char *msg2 = "Hello back, Station 1!";
        trame t2 = creer_trame_ethernet(mac_dst, mac_src, 0x0800, (const uint8_t *)msg2, strlen(msg2) + 1);
        envoyer_trame(&r, idx_station_dst, &t2, true);

        /* --- AFFICHAGE DES TABLES DE COMMUTATION --- */
        /* Après les deux transmissions, chaque switch a appris les adresses MAC
         * des stations qui ont envoyé des trames. On affiche ces tables. */
        printf("\n=== TABLES DE COMMUTATION APRÈS TRANSMISSIONS ===\n");
        for ( size_t i = 0; i < r.nb_equipements; i++ )
        {
            if ( r.equipements[i].type_equ == SWITCH )
            {
                /* Récupère l'adresse MAC du switch sous forme de chaîne pour l'affichage */
                char mac_sw[19];
                mac_to_str(&r.equipements[i].sw.mac, mac_sw);

                printf("\n--- Table de commutation du Switch %zu (%s) ---\n", i, mac_sw);
                afficher_table(&r.equipements[i].sw);  /* Affiche la table MAC→Port de ce switch */
                printf("\n");
            }
        }
    }

    /* === NETTOYAGE DE LA MÉMOIRE === */

    /* Libère toute la mémoire allouée pour le réseau principal.
     * IMPORTANT : ne pas oublier cette étape pour éviter les fuites mémoire ! */
    free_reseau(&r);

    /* Ces deux lignes évitent les warnings du compilateur sur les paramètres inutilisés.
     * argc et argv ne sont pas utilisés dans ce programme. */
    (void)argc;
    (void)argv;
}
