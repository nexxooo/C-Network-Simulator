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



    /* === NETTOYAGE DE LA MÉMOIRE === */

    /* Libère toute la mémoire allouée pour le réseau principal.
     * IMPORTANT : ne pas oublier cette étape pour éviter les fuites mémoire ! */
    free_reseau(&r);

    /* Ces deux lignes évitent les warnings du compilateur sur les paramètres inutilisés.
     * argc et argv ne sont pas utilisés dans ce programme. */
    (void)argc;
    (void)argv;
}
