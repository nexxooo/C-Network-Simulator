/**
 * @file equipement.c
 * @brief Implémentation des fonctions de gestion du réseau local.
 *
 * Ce fichier implémente toutes les opérations sur les structures du réseau :
 *   - Création et destruction du réseau (allocation mémoire dynamique)
 *   - Ajout d'équipements et de câbles
 *   - Chargement d'un réseau depuis un fichier texte
 *   - Algorithmes sur le graphe (connexité, arbre, voisins)
 *   - Commutation Ethernet (apprentissage MAC, inondation, unicast)
 */

#include "../include/equipement.h"  /* Structures et prototypes */
#include "../include/affichage.h"   /* Pour mac_to_str() dans les logs */
#include <string.h>                 /* Pour memset, memcpy, strtok, etc. */

/* =========================================================
   INITIALISATION ET LIBÉRATION DU RÉSEAU
   ========================================================= */

/**
 * @brief Initialise un réseau vide en allouant les tableaux dynamiques.
 *
 * Avant d'utiliser un reseau_local, il faut obligatoirement appeler cette fonction.
 * Elle alloue deux tableaux dynamiques :
 *   - Un tableau pour les équipements (stations et switchs)
 *   - Un tableau pour les câbles
 *
 * La capacité initiale est CAPACITE_INITIALE (12 éléments).
 * Si le réseau devient plus grand, les tableaux seront agrandis automatiquement.
 *
 * @param r  Pointeur vers la structure réseau à initialiser.
 * @return   true si l'initialisation a réussi, false si malloc a échoué.
 */
bool init_reseau(reseau_local *r)
{
    /* Définit la capacité initiale du tableau d'équipements */
    r->equipement_capacite = CAPACITE_INITIALE;

    /* Alloue dynamiquement un tableau de CAPACITE_INITIALE équipements.
     * sizeof(equipement) donne la taille en octets d'un équipement.
     * malloc retourne un pointeur vers la mémoire allouée, ou NULL en cas d'échec. */
    r->equipements = malloc(CAPACITE_INITIALE * sizeof(equipement));

    /* Vérifie si l'allocation a réussi (malloc peut retourner NULL si pas de mémoire) */
    if ( r->equipements == NULL )
        return false;  /* Échec : le réseau ne peut pas être initialisé */

    r->nb_equipements = 0;  /* Au départ, le réseau est vide (0 équipements) */

    /* Même chose pour le tableau des câbles */
    r->nb_cables = 0;
    r->cables_capacite = CAPACITE_INITIALE;
    r->cables = malloc(CAPACITE_INITIALE * sizeof(cable));

    if ( r->cables == NULL )
        return false;  /* Échec : la mémoire pour les câbles n'a pas pu être allouée */

    return true;  /* Succès */
}

/**
 * @brief Libère toute la mémoire allouée pour le réseau.
 *
 * IMPORTANT : Cette fonction doit toujours être appelée quand on n'a plus
 * besoin du réseau, pour éviter les fuites mémoire.
 *
 * Pour les switchs, il faut aussi libérer :
 *   - sw.ports : le tableau des ports
 *   - sw.tab   : la table de commutation
 *
 * @param r  Pointeur vers le réseau à libérer.
 * @return   true (toujours, pour cohérence avec l'API).
 */
bool free_reseau(reseau_local *r)
{
    /* Remet les compteurs de câbles à 0 et libère le tableau */
    r->cables_capacite = 0;
    r->nb_cables = 0;
    free(r->cables);       /* Libère le tableau de câbles */
    r->cables = NULL;      /* Met le pointeur à NULL pour éviter les "dangling pointers" */

    /* Pour chaque équipement, si c'est un switch, libère ses tableaux internes */
    for ( size_t i = 0; i < r->nb_equipements; i++ )
    {
        if ( r->equipements[i].type_equ == SWITCH )
        {
            /* Libère le tableau des ports du switch */
            free(r->equipements[i].sw.ports);

            /* Libère la table de commutation (MAC → port) du switch */
            free(r->equipements[i].sw.tab);
        }
        /* Note : les stations n'ont pas de mémoire allouée dynamiquement */
    }

    /* Remet les compteurs d'équipements à 0 et libère le tableau principal */
    r->equipement_capacite = 0;
    r->nb_equipements = 0;
    free(r->equipements);  /* Libère le tableau d'équipements */
    r->equipements = NULL; /* Met le pointeur à NULL */

    return true;
}

/**
 * @brief Initialise les champs internes d'un switch (ports et table de commutation).
 *
 * Cette fonction est appelée après avoir rempli les champs de base (mac, nb_port, priorite).
 * Elle alloue les tableaux dynamiques internes du switch et initialise chaque port
 * à l'état INCONNU (STP n'a pas encore été exécuté).
 *
 * @param sw      Pointeur vers le switch à initialiser.
 * @param nb_port Nombre de ports physiques de ce switch.
 * @return        true si l'initialisation a réussi, false si malloc a échoué.
 */
bool init_switch(switch_ *sw, size_t nb_port)
{
    sw->nb_port = nb_port;  /* Enregistre le nombre de ports */

    /* Alloue la table de commutation (capacité initiale = TAILLE_REALOC entrées) */
    sw->tab = malloc(sizeof(table_de_commutation) * sw->taille_max);
    if ( sw->tab == NULL )
        return false;  /* Échec malloc */
    sw->taille_tab = 0;  /* La table est vide au départ */

    /* Alloue le tableau des ports du switch */
    sw->ports = malloc(sizeof(port) * sw->nb_port);
    if ( sw->ports == NULL )
    {
        /* Si l'allocation des ports échoue, on libère la table déjà allouée
         * pour ne pas laisser de fuite mémoire partielle */
        free(sw->tab);
        sw->tab = NULL;
        return false;
    }

    /* Initialise chaque port du switch */
    for ( size_t i = 0; i < sw->nb_port; i++ )
    {
        sw->ports[i].numero_port = i;              /* Numéro du port (0, 1, 2...) */
        sw->ports[i].etat = ETAT_PORT_INCONNU;     /* État initial : inconnu (STP pas encore fait) */
        sw->ports[i].a_recu_bpdu = false;          /* Ce port n'a encore reçu aucun BPDU */

        /* Remet à zéro le meilleur BPDU reçu (tous les octets à 0) */
        memset(&sw->ports[i].meilleur_bpdu_recu, 0, sizeof(BPDU));
    }

    return true;
}

/* =========================================================
   AJOUT D'ÉLÉMENTS AU RÉSEAU
   ========================================================= */

/**
 * @brief Ajoute un équipement au réseau.
 *
 * Si le tableau d'équipements est plein, il est agrandi automatiquement
 * avec realloc (augmentation de TAILLE_REALOC cases supplémentaires).
 *
 * @param e  L'équipement à ajouter (copié par valeur dans le tableau).
 * @param r  Pointeur vers le réseau.
 * @return   true si l'ajout a réussi, false si realloc a échoué.
 */
bool ajouter_equipement(equipement e, reseau_local *r)
{
    /* Vérifie si le tableau est plein (nb >= capacite) */
    if ( r->nb_equipements >= r->equipement_capacite )
    {
        /* Tente d'agrandir le tableau avec realloc.
         * realloc étend le bloc mémoire existant (ou le déplace si besoin).
         * IMPORTANT : on stocke le résultat dans une variable temporaire `verif`
         * car si realloc échoue, il retourne NULL MAIS NE LIBÈRE PAS l'ancien bloc.
         * Si on faisait r->equipements = realloc(...) et que ça échoue,
         * on perdrait le pointeur vers l'ancien bloc → fuite mémoire ! */
        equipement *verif =
            realloc(r->equipements,
                    (r->equipement_capacite + TAILLE_REALOC) * sizeof(equipement));

        if ( verif == NULL )
            return false;  /* Échec realloc : le tableau reste intact mais l'ajout échoue */

        r->equipements = verif;                      /* Met à jour le pointeur */
        r->equipement_capacite += TAILLE_REALOC;     /* Met à jour la capacité */
    }

    /* Ajoute l'équipement à la fin du tableau */
    r->equipements[r->nb_equipements] = e;
    r->nb_equipements++;   /* Incrémente le compteur */
    return true;
}

/**
 * @brief Ajoute un câble au réseau.
 *
 * Même logique que ajouter_equipement() : agrandissement automatique si nécessaire.
 *
 * @param c  Le câble à ajouter.
 * @param r  Pointeur vers le réseau.
 * @return   true si l'ajout a réussi.
 */
bool ajouter_cable(cable c, reseau_local *r)
{
    /* Vérifie si le tableau de câbles est plein */
    if ( r->nb_cables >= r->cables_capacite )
    {
        /* Agrandit le tableau de câbles */
        cable *verif = realloc(r->cables, (r->cables_capacite + TAILLE_REALOC) *
                                              sizeof(cable));

        if ( verif == NULL )
            return false;  /* Échec realloc */

        r->cables = verif;
        r->cables_capacite += TAILLE_REALOC;
    }

    /* Ajoute le câble à la fin du tableau */
    r->cables[r->nb_cables] = c;
    r->nb_cables++;

    return true;
}

/* =========================================================
   CHARGEMENT DEPUIS UN FICHIER
   ========================================================= */

/**
 * @brief Charge un réseau depuis un fichier texte de configuration.
 *
 * FORMAT DU FICHIER :
 * ┌─────────────────────────────────────────────────────┐
 * │ Ligne 1 : nb_équipements nb_câbles                  │
 * │ Lignes suivantes (une par équipement) :             │
 * │   Type 1 (station) : 1;MAC;IP                       │
 * │     ex: 1;54:d6:a6:82:c5:01;130.79.80.1             │
 * │   Type 2 (switch)  : 2;MAC;nb_ports;priorité        │
 * │     ex: 2;01:45:23:a6:f7:01;8;1024                  │
 * │ Lignes suivantes (une par câble) :                  │
 * │   sommet1;sommet2;pondération                       │
 * │     ex: 0;1;4                                       │
 * └─────────────────────────────────────────────────────┘
 *
 * @param fichier  Chemin vers le fichier de configuration.
 * @param r        Pointeur vers le réseau à remplir.
 * @return         ERR_OK si succès, ERR_FICHIER_NON_TROUVE si le fichier n'existe pas.
 */
Erreur_fichier charger_reseau(const char *fichier, reseau_local *r)
{
    /* Ouvre le fichier en mode lecture ("r") */
    FILE *f = fopen(fichier, "r");

    /* Si fopen retourne NULL, le fichier n'existe pas ou n'est pas accessible */
    if ( f == NULL )
        return ERR_FICHIER_NON_TROUVE;

    char ligne[1024];    /* Buffer pour lire une ligne à la fois (max 1024 caractères) */
    fgets(ligne, sizeof(ligne), f);  /* Lit la première ligne : "nb_équipements nb_câbles" */

    size_t expected_equipements = 0;  /* Nombre d'équipements annoncé dans le fichier */
    size_t expected_cables = 0;       /* Nombre de câbles annoncé dans le fichier */

    /* Extrait les deux nombres de la première ligne avec sscanf
     * %zu = format pour size_t (entier non signé de taille système) */
    sscanf(ligne, "%zu %zu", &expected_equipements, &expected_cables);

    /* === LECTURE DES ÉQUIPEMENTS === */
    for ( size_t eq = 0; eq < expected_equipements; eq++ )
    {
        /* Lit la prochaine ligne */
        fgets(ligne, sizeof(ligne), f);

        /* Supprime le caractère de fin de ligne '\n' en le remplaçant par '\0'.
         * strcspn retourne la position du premier '\n' dans la chaîne. */
        ligne[strcspn(ligne, "\n")] = '\0';

        if ( ligne[0] == '1' ) /* Le type '1' désigne une station */
        {
            station st;

            /* strtok découpe la ligne sur le délimiteur ';'.
             * Premier appel : retourne le premier token (ici "1", le type).
             * Appels suivants avec NULL : continue depuis où on s'est arrêté. */
            char *token = strtok(ligne, ";");  /* token = "1" (le type, ignoré) */
            token = strtok(NULL, ";");          /* token = "54:d6:a6:82:c5:01" (MAC) */
            st.mac = str_to_mac(token);         /* Convertit la chaîne en structure MAC */
            token = strtok(NULL, ";");           /* token = "130.79.80.1" (IP) */
            st.ipv4 = str_to_ipv4(token);       /* Convertit la chaîne en structure IPV4 */

            /* Crée l'équipement avec le type STATION et les données de la station.
             * La syntaxe .type_equ et .st = st est une initialisation désignée (C99). */
            equipement equ = {.type_equ = STATION, .st = st};
            ajouter_equipement(equ, r);  /* Ajoute l'équipement au réseau */
        }

        else if ( ligne[0] == '2' ) /* Le type '2' désigne un switch */
        {
            switch_ sw = {0};  /* Initialise tous les champs à 0 avec {0} */

            char *token = strtok(ligne, ";");  /* token = "2" (le type, ignoré) */
            token = strtok(NULL, ";");          /* token = "01:45:23:a6:f7:01" (MAC) */

            sw.mac = str_to_mac(token);  /* Convertit et stocke l'adresse MAC du switch */

            token = strtok(NULL, ";");      /* token = "8" (nombre de ports) */
            sw.nb_port = atoi(token);       /* Convertit la chaîne en entier */

            token = strtok(NULL, ";");      /* token = "1024" (priorité STP) */
            sw.priorite = atoi(token);      /* Priorité STP (plus petit = plus prioritaire pour la racine) */

            /* Définit la capacité initiale de la table de commutation avant init_switch */
            sw.taille_max = TAILLE_REALOC;
            init_switch(&sw, sw.nb_port);  /* Alloue les tableaux internes du switch */

            equipement equ = {.type_equ = SWITCH, .sw = sw};
            ajouter_equipement(equ, r);
        }
    }

    /* === LECTURE DES CÂBLES === */
    for ( size_t c = 0; c < expected_cables; c++ )
    {
        fgets(ligne, sizeof(ligne), f);
        ligne[strcspn(ligne, "\n")] = '\0';  /* Supprime le '\n' final */

        /* Lit les trois champs du câble : sommet1;sommet2;pondération */
        char *token = strtok(ligne, ";");
        /* strtoul : convertit une chaîne en entier non signé (base 10)
         * NULL : pas de pointeur vers la fin de la chaîne
         * 10 : base décimale */
        size_t sommet1 = (size_t)strtoul(token, NULL, 10);  /* Index de l'équipement 1 */

        token = strtok(NULL, ";");
        size_t sommet2 = (size_t)strtoul(token, NULL, 10);  /* Index de l'équipement 2 */

        token = strtok(NULL, ";");
        size_t poids = (size_t)strtoul(token, NULL, 10);    /* Pondération du lien */

        /* Crée le câble et l'ajoute au réseau */
        cable cab = {.sommet1 = sommet1, .sommet2 = sommet2, .ponderation = poids};
        ajouter_cable(cab, r);
    }

    fclose(f);      /* Ferme le fichier (libère la ressource système) */
    return ERR_OK;  /* Succès */
}

/* =========================================================
   COMPARAISON D'ADRESSES MAC
   ========================================================= */

/**
 * @brief Détermine si mac1 est "meilleure" (plus petite) que mac2.
 *
 * La comparaison se fait octet par octet, de l'octet le plus significatif
 * (index 0) au moins significatif (index 5).
 * Dès qu'un octet diffère, on retourne le résultat.
 * C'est le même principe que strcmp() mais pour des MACs.
 *
 * Dans STP, une MAC "meilleure" (plus petite) donne la priorité
 * pour être élu racine ou pour avoir le port désigné.
 *
 * @param mac1  Première adresse MAC.
 * @param mac2  Deuxième adresse MAC.
 * @return      true si mac1 < mac2 (mac1 est plus petite).
 */
bool mac_est_meilleure(MAC *mac1, MAC *mac2)
{
    for ( size_t i = 0; i < 6; i++ )
        if ( mac1->bytes[i] < mac2->bytes[i] )
            return true;   /* mac1 est plus petite sur cet octet → elle est meilleure */
        else if ( mac1->bytes[i] > mac2->bytes[i] )
            return false;  /* mac1 est plus grande sur cet octet → elle est moins bonne */
    return false;          /* Les deux MACs sont égales → mac1 n'est pas "meilleure" */
}

/**
 * @brief Vérifie si deux adresses MAC sont identiques.
 *
 * Compare les 6 octets un par un. Dès qu'un octet diffère, retourne false.
 *
 * @param mac1  Première adresse MAC.
 * @param mac2  Deuxième adresse MAC.
 * @return      true si toutes les MACs sont égales octet par octet.
 */
bool mac_est_egale(MAC *mac1, MAC *mac2)
{
    for ( size_t i = 0; i < 6; i++ )
        if ( mac1->bytes[i] != mac2->bytes[i] )
            return false;  /* Dès qu'un octet diffère, les MACs sont différentes */
    return true;  /* Tous les octets sont identiques */
}

/* =========================================================
   ALGORITHMES DE THÉORIE DES GRAPHES
   ========================================================= */

/**
 * @brief Vérifie si le réseau forme un arbre au sens mathématique.
 *
 * Un graphe est un arbre si et seulement si :
 *   1. Il est connexe (tous les sommets sont reliés entre eux)
 *   2. Il a exactement N-1 arêtes pour N sommets (nb_câbles = nb_équipements - 1)
 *
 * La condition 2 seule est insuffisante (une forêt peut la vérifier).
 * La condition 1 seule est insuffisante (un graphe avec cycle peut être connexe).
 * Les DEUX ensemble garantissent l'absence de cycle et la connexité.
 *
 * @param r  Pointeur vers le réseau.
 * @return   true si le réseau est un arbre.
 */
bool est_un_arbre(reseau_local *r)
{
    /* nb_aretes = ordre - 1 : propriété fondamentale des arbres */
    if ( reseau_est_connexe(r) && r->nb_cables == r->nb_equipements - 1 )
        return true;
    return false;
}

/**
 * @brief Trouve tous les voisins (sommets adjacents) d'un équipement dans le réseau.
 *
 * Parcourt tous les câbles et retourne les indices des équipements connectés
 * directement à `sommet`. Un câble relie sommet1 et sommet2 : si l'un d'eux
 * est `sommet`, l'autre est un voisin.
 *
 * @param r          Pointeur vers le réseau.
 * @param sommet     Index de l'équipement dont on cherche les voisins.
 * @param adjacents  Tableau à remplir avec les indices des voisins (doit être assez grand).
 * @return           Nombre de voisins trouvés.
 */
size_t sommets_adjacent(const reseau_local *r, size_t sommet, size_t *adjacents)
{
    size_t n_adj = 0;            /* Compteur de voisins trouvés */
    size_t deg = r->nb_cables;   /* Nombre total de câbles à parcourir */

    for ( size_t i = 0; i < deg; i++ )
        if ( r->cables[i].sommet1 == sommet )
            adjacents[n_adj++] = r->cables[i].sommet2;  /* sommet est à gauche → voisin est à droite */
        else if ( r->cables[i].sommet2 == sommet )
            adjacents[n_adj++] = r->cables[i].sommet1;  /* sommet est à droite → voisin est à gauche */

    return n_adj;  /* Retourne le nombre de voisins trouvés */
}

/**
 * @brief Parcourt en profondeur (DFS) tous les équipements connectés à ind_equip.
 *
 * Algorithme de parcours en profondeur (Depth-First Search) :
 *   1. Marque l'équipement courant comme visité
 *   2. Trouve tous ses voisins
 *   3. Pour chaque voisin non encore visité → appel récursif
 *
 * Après l'appel, le tableau `visite` contient true pour tous les équipements
 * dans la même composante connexe que ind_equip.
 *
 * @param g          Pointeur vers le réseau (graphe).
 * @param ind_equip  Index du point de départ du parcours.
 * @param visite     Tableau booléen (taille = nb_equipements) pour marquer les visités.
 */
void visite_composante_connexe(reseau_local const *g, size_t ind_equip, bool *visite)
{
    visite[ind_equip] = true;  /* Marque l'équipement courant comme visité */

    /* Crée un tableau local (sur la pile) pour stocker les voisins.
     * Taille = nb_equipements au maximum (un équipement ne peut avoir plus de voisins que ça) */
    size_t adjacents[g->nb_equipements];
    size_t n_adj = sommets_adjacent(g, ind_equip, adjacents);  /* Trouve les voisins */

    /* Visite récursivement chaque voisin non encore visité */
    for ( size_t i = 0; i < n_adj; i++ )
        if ( !visite[adjacents[i]] )
            visite_composante_connexe(g, adjacents[i], visite);  /* Appel récursif */
}

/**
 * @brief Vérifie si le réseau est connexe (tous les équipements sont atteignables).
 *
 * Algorithme de comptage des composantes connexes :
 *   1. Initialise tous les équipements comme non visités
 *   2. Parcourt les équipements : à chaque équipement non visité, lance un DFS
 *      et incrémente le compteur de composantes
 *   3. Si le compteur final est 1 → le réseau est connexe
 *
 * @param r  Pointeur vers le réseau.
 * @return   true si le réseau est connexe (une seule composante connexe).
 */
bool reseau_est_connexe(reseau_local *r)
{
    /* Tableau pour marquer quels équipements ont été visités.
     * Déclaré sur la pile (VLA - Variable Length Array, taille connue à l'exécution) */
    bool visite[r->nb_equipements];

    /* Initialise tous à false (non visité) */
    for ( size_t i = 0; i < r->nb_equipements; i++ )
        visite[i] = false;

    uint32_t nbComposantes = 0;  /* Compteur de composantes connexes */

    /* Pour chaque équipement non encore visité, lance un DFS depuis lui.
     * Si le réseau est connexe, le premier DFS visitera TOUT le réseau → nbComposantes = 1. */
    for ( size_t s = 0; s < r->nb_equipements; s++ )
    {
        if ( !visite[s] )  /* Si cet équipement n'a pas encore été visité... */
        {
            visite_composante_connexe(r, s, visite);  /* Lance le DFS depuis s */
            nbComposantes++;  /* Une nouvelle composante connexe a été trouvée */
        }
    }

    /* Si nbComposantes == 1 → un seul "groupe connecté" → réseau connexe */
    return nbComposantes == 1;
}

/* =========================================================
   UTILITAIRES CÂBLES ET PORTS
   ========================================================= */

/**
 * @brief Vérifie si un câble relie bien deux sommets donnés.
 *
 * Comme un câble est non-orienté (bidirectionnel), on vérifie les deux sens :
 *   - sommet1 → sommet2 (câble.sommet1 == sommet1 && câble.sommet2 == sommet2)
 *   - sommet2 → sommet1 (câble.sommet1 == sommet2 && câble.sommet2 == sommet1)
 *
 * @param c       Pointeur vers le câble à vérifier.
 * @param sommet1 Premier sommet.
 * @param sommet2 Deuxième sommet.
 * @return        true si le câble relie ces deux sommets.
 */
bool cable_est_relie(cable *c, size_t sommet1, size_t sommet2)
{
    return (c->sommet1 == sommet1 && c->sommet2 == sommet2) ||  /* Sens direct */
           (c->sommet1 == sommet2 && c->sommet2 == sommet1);    /* Sens inverse */
}

/**
 * @brief Détermine le numéro de port local d'un switch pour un câble donné.
 *
 * Les ports d'un switch sont numérotés dans l'ordre des câbles qui le touchent :
 * le 1er câble le touchant correspond au port 0, le 2ème au port 1, etc.
 *
 * Cette fonction itère sur tous les câbles dans l'ordre, et pour chaque câble
 * qui touche sw_idx, incrémente un compteur de port. Quand on trouve le câble
 * recherché (cable_idx), on retourne le compteur courant.
 *
 * Exemple : si le switch 0 est impliqué dans les câbles 0, 2, 5 du tableau :
 *   - câble 0 → port local 0 du switch 0
 *   - câble 2 → port local 1 du switch 0
 *   - câble 5 → port local 2 du switch 0
 *
 * @param r          Pointeur vers le réseau.
 * @param sw_idx     Index du switch dans le tableau d'équipements.
 * @param cable_idx  Index du câble dans le tableau de câbles.
 * @return           Numéro de port local, ou SIZE_MAX si le câble ne touche pas ce switch.
 */
size_t obtenir_port_local(reseau_local *r, size_t sw_idx, size_t cable_idx)
{
    size_t port_loc = 0;  /* Compteur de port local (commence à 0) */

    for ( size_t i = 0; i < r->nb_cables; i++ )
    {
        /* Ce câble touche-t-il le switch sw_idx ? */
        if ( r->cables[i].sommet1 == sw_idx || r->cables[i].sommet2 == sw_idx )
        {
            /* Si c'est le câble qu'on cherche, retourne le numéro de port actuel */
            if ( i == cable_idx )
                return port_loc;

            /* Sinon, c'est un autre câble du switch → port suivant */
            port_loc++;
        }
    }
    return SIZE_MAX;  /* Le câble ne touche pas ce switch (erreur) */
}

/* =========================================================
   CONSTRUCTION DE L'ARBRE DE RECOUVREMENT
   ========================================================= */

/**
 * @brief Construit un nouveau réseau contenant uniquement les liens actifs après STP.
 *
 * Après l'exécution de STP, certains ports sont bloqués.
 * Cette fonction crée une copie du réseau (dst) en n'incluant que les câbles
 * dont les ports aux deux extrémités ne sont PAS bloqués simultanément.
 *
 * RÈGLES D'INCLUSION D'UN CÂBLE :
 *   - Câble entre une station et un switch : TOUJOURS inclus (les stations ne font pas STP)
 *   - Câble entre deux switchs : inclus seulement si les deux ports sont actifs
 *     (aucun des deux n'est BLOQUÉ)
 *
 * @param src  Réseau source (après STP).
 * @param dst  Réseau destination (arbre résultant, à remplir).
 * @return     true si la construction a réussi.
 */
bool construire_arbre_selon_reseau(reseau_local *src, reseau_local *dst)
{
    /* Initialise le réseau destination (alloue les tableaux) */
    init_reseau(dst);

    /* --- COPIE DE TOUS LES ÉQUIPEMENTS --- */
    for ( size_t i = 0; i < src->nb_equipements; i++ )
    {
        equipement e = src->equipements[i];
        if ( e.type_equ == SWITCH )
        {
            /* Pour un switch, il faut faire une COPIE PROFONDE (deep copy) :
             * copier les données de base + allouer et copier les tableaux internes.
             * Une simple copie de la structure copierait les POINTEURS, pas les données ! */
            switch_ sw_copy = e.sw;  /* Copie les champs de base (mac, nb_port, etc.) */

            /* Alloue un nouveau tableau de ports et copie les données */
            sw_copy.ports = malloc(sizeof(port) * e.sw.nb_port);
            if ( sw_copy.ports )
            {
                memcpy(sw_copy.ports, e.sw.ports, sizeof(port) * e.sw.nb_port);
            }

            /* Alloue une nouvelle table de commutation et copie les données */
            sw_copy.tab = malloc(sizeof(table_de_commutation) * e.sw.taille_max);
            if ( sw_copy.tab )
            {
                memcpy(sw_copy.tab, e.sw.tab, sizeof(table_de_commutation) * e.sw.taille_tab);
            }

            equipement equ = {.type_equ = SWITCH, .sw = sw_copy};
            ajouter_equipement(equ, dst);
        }
        else
        {
            /* Pour une station : copie simple (pas de pointeurs internes) */
            equipement equ = {.type_equ = STATION, .st = e.st};
            ajouter_equipement(equ, dst);
        }
    }

    /* --- FILTRAGE DES CÂBLES SELON L'ÉTAT STP --- */
    /*
     * On n'ajoute un câble que si le lien est actif après STP.
     * Un câble entre deux switches est inclus si le port local
     * correspondant n'est PAS BLOQUÉ des deux côtés simultanément,
     * c'est-à-dire qu'au moins un des deux extrémités voit ce port
     * comme RACINE ou DÉSIGNÉ.
     * Un câble reliant une station est toujours inclus (les stations
     * ne participent pas à STP et sont joignables via leur switch désigné).
     */
    for ( size_t c = 0; c < src->nb_cables; c++ )
    {
        size_t s1 = src->cables[c].sommet1;  /* Index de l'équipement 1 du câble */
        size_t s2 = src->cables[c].sommet2;  /* Index de l'équipement 2 du câble */

        equipement *e1 = &src->equipements[s1];  /* Pointeur vers l'équipement 1 */
        equipement *e2 = &src->equipements[s2];  /* Pointeur vers l'équipement 2 */

        /* Câble station↔switch ou station↔station : toujours actif (les stations ne bloquent pas) */
        if ( e1->type_equ == STATION || e2->type_equ == STATION )
        {
            ajouter_cable(src->cables[c], dst);  /* Toujours inclus dans l'arbre */
            continue;                             /* Passe au câble suivant */
        }

        /* Câble switch↔switch : vérifier l'état des ports de chaque côté */
        switch_ *sw1 = &e1->sw;  /* Accès au switch 1 */
        switch_ *sw2 = &e2->sw;  /* Accès au switch 2 */

        /* Retrouver le numéro de port local pour chaque switch sur ce câble */
        size_t port_loc1 = obtenir_port_local(src, s1, c);  /* Port de sw1 vers sw2 */
        size_t port_loc2 = obtenir_port_local(src, s2, c);  /* Port de sw2 vers sw1 */

        /* Par défaut, on considère les ports comme bloqués (cas le plus sûr) */
        etat_port etat1 = ETAT_PORT_BLOQUE;
        etat_port etat2 = ETAT_PORT_BLOQUE;

        /* Récupère l'état réel du port si le numéro est valide */
        if ( port_loc1 < sw1->nb_port )
            etat1 = sw1->ports[port_loc1].etat;
        if ( port_loc2 < sw2->nb_port )
            etat2 = sw2->ports[port_loc2].etat;

        /* Le lien est actif si les deux côtés ne sont pas bloqués simultanément.
         * Si l'un des deux est BLOQUÉ, le lien est désactivé par STP. */
        if ( etat1 != ETAT_PORT_BLOQUE && etat2 != ETAT_PORT_BLOQUE )
            ajouter_cable(src->cables[c], dst);  /* Lien actif → inclus dans l'arbre */
    }

    return true;
}

/* =========================================================
   TABLE DE COMMUTATION (APPRENTISSAGE ET RECHERCHE MAC)
   ========================================================= */

/**
 * @brief Enregistre une association MAC → port dans la table d'un switch.
 *
 * C'est la fonctionnalité centrale d'un switch :
 * quand il reçoit une trame sur un port, il retient que la machine source
 * est accessible par ce port. La prochaine fois qu'une trame est destinée
 * à cette machine, le switch saura où l'envoyer directement.
 *
 * Si la MAC est déjà dans la table, on met à jour le port (la machine a peut-être
 * été branchée sur un autre port).
 *
 * @param sw           Pointeur vers le switch.
 * @param source_mac   Adresse MAC à apprendre.
 * @param port_entree  Numéro du port par lequel cette MAC est accessible.
 */
void switch_apprendre_mac(switch_ *sw, MAC source_mac, size_t port_entree)
{
    /* 1. Vérifie si l'adresse MAC est déjà dans la table */
    for ( size_t i = 0; i < sw->taille_tab; i++ )
    {
        if ( mac_est_egale(&sw->tab[i].mac, &source_mac) )
        {
            /* La MAC existe déjà : met à jour le port (peut avoir changé) */
            sw->tab[i].interface_port = port_entree;
            return;  /* Fin de fonction : pas besoin d'ajouter une nouvelle entrée */
        }
    }

    /* 2. Agrandit la table si elle est pleine */
    if ( sw->taille_tab >= sw->taille_max )
    {
        size_t nouvelle_taille = sw->taille_max + 24;  /* Augmente de 24 cases */
        table_de_commutation *verif = realloc(sw->tab, sizeof(table_de_commutation) * nouvelle_taille);
        if ( verif == NULL )
            return;  /* Echec realloc : on ne peut pas apprendre cette MAC */
        sw->tab = verif;
        sw->taille_max = nouvelle_taille;
    }

    /* 3. Ajoute la nouvelle association MAC → port en fin de table */
    sw->tab[sw->taille_tab].mac = source_mac;         /* Stocke l'adresse MAC */
    sw->tab[sw->taille_tab].interface_port = port_entree;  /* Stocke le numéro de port */
    sw->taille_tab++;  /* Incrémente le nombre d'entrées dans la table */
}

/**
 * @brief Cherche dans la table d'un switch par quel port atteindre une MAC destination.
 *
 * Parcourt la table de commutation et retourne le numéro de port associé à dest_mac.
 * Si la MAC n'est pas connue, retourne SIZE_MAX (valeur sentinelle = "non trouvé").
 *
 * @param sw        Pointeur vers le switch.
 * @param dest_mac  Adresse MAC de la destination recherchée.
 * @return          Numéro de port pour atteindre dest_mac, ou SIZE_MAX si inconnu.
 */
size_t switch_trouver_port(switch_ *sw, MAC dest_mac)
{
    /* Parcourt toutes les entrées de la table */
    for ( size_t i = 0; i < sw->taille_tab; i++ )
    {
        if ( mac_est_egale(&sw->tab[i].mac, &dest_mac) )
        {
            return sw->tab[i].interface_port;  /* Trouvé ! Retourne le port */
        }
    }
    return SIZE_MAX;  /* Non trouvé : il faudra faire du flooding (diffusion sur tous les ports) */
}

/**
 * @brief Retrouve l'équipement voisin et le câble correspondant à un port d'un switch.
 *
 * Un port d'un switch correspond à un câble particulier. Cette fonction retrouve
 * l'index du câble et l'index de l'équipement à l'autre extrémité.
 *
 * @param r          Pointeur vers le réseau.
 * @param sw_idx     Index du switch dans le tableau d'équipements.
 * @param port_num   Numéro du port local du switch.
 * @param voisin_idx Pointeur où stocker l'index du voisin trouvé (peut être NULL).
 * @param cable_idx  Pointeur où stocker l'index du câble trouvé (peut être NULL).
 * @return           true si le voisin a été trouvé, false sinon.
 */
bool obtenir_voisin_par_port(const reseau_local *r, size_t sw_idx, size_t port_num, size_t *voisin_idx, size_t *cable_idx)
{
    size_t port_loc = 0;  /* Compteur de port local (même logique que obtenir_port_local) */

    for ( size_t i = 0; i < r->nb_cables; i++ )
    {
        /* Ce câble touche-t-il le switch sw_idx ? */
        if ( r->cables[i].sommet1 == sw_idx || r->cables[i].sommet2 == sw_idx )
        {
            /* Ce câble correspond-il au port recherché ? */
            if ( port_loc == port_num )
            {
                /* Stocke l'index du câble si le pointeur est fourni */
                if ( cable_idx ) *cable_idx = i;

                /* Stocke l'index du voisin (l'autre extrémité du câble) */
                if ( voisin_idx )
                {
                    /* Si sw_idx est le sommet1, le voisin est sommet2, et vice-versa */
                    *voisin_idx = (r->cables[i].sommet1 == sw_idx) ? r->cables[i].sommet2 : r->cables[i].sommet1;
                }
                return true;  /* Voisin trouvé */
            }
            port_loc++;  /* Ce câble touche le switch mais ce n'est pas le bon port → port suivant */
        }
    }
    return false;  /* Aucun voisin trouvé pour ce numéro de port */
}

/* =========================================================
   CRÉATION ET TRANSMISSION DE TRAMES ETHERNET
   ========================================================= */

/**
 * @brief Crée une trame Ethernet avec tous ses champs remplis.
 *
 * Structure d'une trame Ethernet (IEEE 802.3) :
 *   - Préambule  : 7 octets à 0xAA (synchronisation)
 *   - SFD        : 1 octet à 0xAB (Start Frame Delimiter, début de la trame)
 *   - Destination: 6 octets (MAC destination)
 *   - Source     : 6 octets (MAC source)
 *   - Type       : 2 octets (EtherType : 0x0800=IPv4, 0x0806=ARP, 0x8809=STP...)
 *   - Données    : 0 à 1500 octets (le payload)
 *   - FCS        : 4 octets (checksum, mis à 0 ici par simplification)
 *
 * @param source       MAC de l'émetteur de la trame.
 * @param destination  MAC du destinataire de la trame.
 * @param type         EtherType (protocole encapsulé dans les données).
 * @param data         Pointeur vers les données à encapsuler.
 * @param data_len     Taille des données en octets (max 1500).
 * @return             La trame Ethernet créée.
 */
trame creer_trame_ethernet(MAC source, MAC destination, uint16_t type, const uint8_t *data, size_t data_len)
{
    trame t = {0};  /* Initialise tous les champs à 0 */

    /* Remplit le préambule : 7 octets à 0xAA (motif de synchronisation pour le récepteur) */
    for ( int i = 0; i < 7; i++ )
        t.preambule[i] = 0xAA;

    /* SFD (Start Frame Delimiter) : 0xAB indique la fin du préambule et le début de la trame */
    t.SFD = 0xAB;

    t.source = source;           /* Adresse MAC de l'émetteur */
    t.destination = destination; /* Adresse MAC du destinataire */
    t.type = type;               /* EtherType (ex: 0x0800 pour IPv4) */

    /* Copie les données dans le champ data de la trame.
     * On limite à 1500 octets maximum (norme Ethernet). */
    if ( data && data_len > 0 )
    {
        size_t copy_len = data_len > 1500 ? 1500 : data_len;  /* Truncature si trop long */
        memcpy(t.data, data, copy_len);  /* Copie les données */
    }

    t.FCS = 0;  /* Frame Check Sequence : mis à 0 par simplification (normalement calculé sur la trame) */
    return t;
}

/* =========================================================
   PROPAGATION INTERNE DES TRAMES (fonction statique)
   ========================================================= */

/**
 * @brief Propage récursivement une trame reçue à travers le réseau.
 *
 * Cette fonction statique (privée au fichier) gère la commutation de la trame
 * à partir d'un équipement qui vient de la recevoir.
 *
 * COMPORTEMENT SELON LE TYPE D'ÉQUIPEMENT :
 *
 * Si c'est une STATION :
 *   - Vérifie si la trame lui est destinée (MAC match)
 *   - Affiche "SUCCESS" si oui, "rejetée" si non, "Broadcast" si c'est un broadcast
 *
 * Si c'est un SWITCH :
 *   - Vérifie que le port d'entrée n'est pas bloqué par STP
 *   - Apprend la MAC source (pour la commutation future)
 *   - Cherche le port de sortie dans la table de commutation :
 *     → Si CONNU : envoie directement sur ce port (Unicast/Commutation)
 *     → Si INCONNU : inonde tous les ports actifs sauf celui d'entrée (Flooding)
 *
 * @param r                Pointeur vers le réseau.
 * @param eq_idx           Index de l'équipement qui reçoit la trame.
 * @param cable_idx_entree Index du câble par lequel la trame est arrivée.
 * @param tr               Pointeur vers la trame reçue.
 * @param visite           Tableau booléen pour éviter les boucles de propagation.
 * @param verbose          Si true, affiche les détails du traitement à chaque étape.
 */
static void propager_trame_recue(reseau_local *r, size_t eq_idx, size_t cable_idx_entree, trame *tr, bool *visite, bool verbose)
{
    /* GARDE-FOU : si l'équipement est hors limites ou déjà visité, on arrête.
     * Le tableau `visite` sert à éviter qu'une trame tourne en boucle
     * si des cycles subsistent dans le graphe (ne devrait pas arriver après STP). */
    if ( eq_idx >= r->nb_equipements || visite[eq_idx] )
        return;
    visite[eq_idx] = true;  /* Marque l'équipement comme déjà traité */

    equipement *eq = &r->equipements[eq_idx];  /* Pointeur vers l'équipement courant */

    /* === CAS 1 : L'équipement est une STATION === */
    if ( eq->type_equ == STATION )
    {
        station *st = &eq->st;

        /* Prépare les chaînes pour l'affichage */
        char mac_src_str[19], mac_dst_str[19], mac_st_str[19];
        mac_to_str(&tr->source, mac_src_str);          /* MAC source de la trame */
        mac_to_str(&tr->destination, mac_dst_str);     /* MAC destination de la trame */
        mac_to_str(&st->mac, mac_st_str);              /* MAC de cette station */

        if ( verbose )
        {
            printf("[Station %s] Reçoit une trame de %s destinée à %s\n",
                   mac_st_str, mac_src_str, mac_dst_str);
        }

        /* Vérifie si la trame est un broadcast (destination = FF:FF:FF:FF:FF:FF).
         * Un broadcast est reçu par TOUS les équipements du réseau. */
        bool est_broadcast = true;
        for ( int i = 0; i < 6; i++ )
        {
            if ( tr->destination.bytes[i] != 0xFF )
            {
                est_broadcast = false;  /* Au moins un octet n'est pas FF → pas un broadcast */
                break;
            }
        }

        /* Vérifie si la trame est destinée à CETTE station (MAC match) */
        if ( mac_est_egale(&st->mac, &tr->destination) )
        {
            /* La station est la vraie destinataire → accepte et affiche le contenu */
            printf("[Station %s] SUCCESS : Trame acceptée ! Contenu : %s\n",
                   mac_st_str, (char *)tr->data);  /* Cast en char* pour afficher comme texte */
        }
        else if ( est_broadcast )
        {
            /* C'est un broadcast : la station le reçoit même si ce n'est pas pour elle */
            printf("[Station %s] Broadcast reçu ! Contenu : %s\n",
                   mac_st_str, (char *)tr->data);
        }
        else
        {
            /* La trame est pour quelqu'un d'autre : la station la rejette */
            if ( verbose )
            {
                printf("[Station %s] Trame rejetée (MAC non correspondante)\n", mac_st_str);
            }
        }
        return;  /* Fin du traitement pour une station : elle ne retransmet pas */
    }

    /* === CAS 2 : L'équipement est un SWITCH === */
    if ( eq->type_equ == SWITCH )
    {
        switch_ *sw = &eq->sw;

        /* Prépare les chaînes pour l'affichage */
        char mac_sw_str[19], mac_src_str[19], mac_dst_str[19];
        mac_to_str(&sw->mac, mac_sw_str);
        mac_to_str(&tr->source, mac_src_str);
        mac_to_str(&tr->destination, mac_dst_str);

        /* Trouve le numéro de port local par lequel la trame est arrivée */
        size_t port_entree = obtenir_port_local(r, eq_idx, cable_idx_entree);
        if ( port_entree >= sw->nb_port )
        {
            /* Le port n'a pas été trouvé : erreur interne */
            if ( verbose )
                printf("[Switch %s] Erreur : Port local introuvable pour le câble %zu\n", mac_sw_str, cable_idx_entree);
            return;
        }

        /* STP : si le port d'entrée est bloqué, le switch refuse la trame.
         * Un port bloqué ne doit pas laisser passer de trames de données. */
        if ( sw->ports[port_entree].etat == ETAT_PORT_BLOQUE )
        {
            if ( verbose )
                printf("[Switch %s] Trame reçue sur le Port %zu qui est BLOQUÉ par STP. Trame rejetée.\n", mac_sw_str, port_entree);
            return;
        }

        if ( verbose )
        {
            printf("[Switch %s] Trame reçue sur le Port %zu (Source: %s, Dest: %s)\n",
                   mac_sw_str, port_entree, mac_src_str, mac_dst_str);
        }

        /* APPRENTISSAGE : le switch mémorise que la MAC source
         * est accessible par le port d'entrée (pour les prochains envois) */
        switch_apprendre_mac(sw, tr->source, port_entree);
        if ( verbose )
        {
            printf("[Switch %s] Apprentissage : MAC %s associée au Port %zu\n",
                   mac_sw_str, mac_src_str, port_entree);
        }

        /* RECHERCHE DU PORT DE SORTIE dans la table de commutation */
        size_t port_sortie = switch_trouver_port(sw, tr->destination);

        if ( port_sortie != SIZE_MAX )
        {
            /* La MAC destination est CONNUE → COMMUTATION UNICAST */

            /* Si le port de sortie est le même que le port d'entrée :
             * la destination est sur le même segment → inutile de retransmettre */
            if ( port_sortie == port_entree )
            {
                if ( verbose )
                    printf("[Switch %s] Filtrage : la destination est sur le même port que l'entrée (%zu). Trame non propagée.\n", mac_sw_str, port_entree);
                return;
            }

            /* Si le port de sortie est bloqué par STP, on ne peut pas transmettre */
            if ( sw->ports[port_sortie].etat == ETAT_PORT_BLOQUE )
            {
                if ( verbose )
                    printf("[Switch %s] Commutation : destination sur Port %zu, mais ce port est BLOQUÉ. Trame jetée.\n", mac_sw_str, port_sortie);
                return;
            }

            /* Trouve l'équipement voisin accessible par ce port de sortie */
            size_t voisin_idx = SIZE_MAX;
            size_t cable_idx_sortie = SIZE_MAX;
            if ( obtenir_voisin_par_port(r, eq_idx, port_sortie, &voisin_idx, &cable_idx_sortie) )
            {
                if ( verbose )
                {
                    printf("[Switch %s] Commutation monocast : envoi direct de la trame via le Port %zu vers l'équipement %zu\n",
                           mac_sw_str, port_sortie, voisin_idx);
                }
                /* Propagation récursive : envoie la trame au voisin */
                propager_trame_recue(r, voisin_idx, cable_idx_sortie, tr, visite, verbose);
            }
        }
        else
        {
            /* La MAC destination est INCONNUE (ou broadcast) → INONDATION (Flooding) :
             * envoie la trame sur TOUS les ports actifs sauf le port d'entrée */
            if ( verbose )
            {
                printf("[Switch %s] Destination %s inconnue ou broadcast. Inondation (Flooding)...\n",
                       mac_sw_str, mac_dst_str);
            }

            /* Itère sur tous les ports du switch pour inonder */
            for ( size_t p = 0; p < sw->nb_port; p++ )
            {
                /* Ne pas retransmettre sur le port par lequel la trame est arrivée */
                if ( p == port_entree )
                    continue;

                /* Ne pas transmettre sur un port bloqué par STP */
                if ( sw->ports[p].etat == ETAT_PORT_BLOQUE )
                {
                    if ( verbose )
                        printf("[Switch %s] Inondation : Port %zu ignoré car BLOQUÉ.\n", mac_sw_str, p);
                    continue;
                }

                /* Trouve le voisin accessible par ce port */
                size_t voisin_idx = SIZE_MAX;
                size_t cable_idx_sortie = SIZE_MAX;
                if ( obtenir_voisin_par_port(r, eq_idx, p, &voisin_idx, &cable_idx_sortie) )
                {
                    if ( verbose )
                    {
                        printf("[Switch %s] Inondation : envoi de la trame sur le Port %zu vers l'équipement %zu\n",
                               mac_sw_str, p, voisin_idx);
                    }

                    /* Pour le flooding, chaque branche a sa propre copie du tableau `visite`
                     * pour éviter les problèmes si plusieurs branches mènent au même équipement.
                     * On fait une copie avec malloc+memcpy pour chaque branche. */
                    bool *visite_branche = malloc(sizeof(bool) * r->nb_equipements);
                    if ( visite_branche )
                    {
                        /* Copie l'état actuel des visites pour cette branche */
                        memcpy(visite_branche, visite, sizeof(bool) * r->nb_equipements);

                        /* Propagation récursive vers le voisin avec la copie des visites */
                        propager_trame_recue(r, voisin_idx, cable_idx_sortie, tr, visite_branche, verbose);

                        free(visite_branche);  /* Libère la copie des visites */
                    }
                }
            }
        }
    }
}

/**
 * @brief Envoie une trame depuis une station source à travers le réseau.
 *
 * Point d'entrée de la simulation de transmission :
 *   1. Vérifie que la source est bien une station (les switchs n'initient pas d'envoi)
 *   2. Trouve le câble qui relie la station à son switch
 *   3. Lance la propagation récursive de la trame à travers le réseau
 *
 * @param r              Pointeur vers le réseau.
 * @param eq_source_idx  Index de la station émettrice.
 * @param tr             Pointeur vers la trame à envoyer.
 * @param verbose        Si true, affiche les détails de la propagation.
 */
void envoyer_trame(reseau_local *r, size_t eq_source_idx, trame *tr, bool verbose)
{
    /* Vérifie que l'index est valide (ne dépasse pas le nombre d'équipements) */
    if ( eq_source_idx >= r->nb_equipements )
    {
        printf("Erreur : Équipement source invalide (%zu)\n", eq_source_idx);
        return;
    }

    equipement *source_eq = &r->equipements[eq_source_idx];

    /* Seules les STATIONS peuvent initier un envoi.
     * Les switchs ne génèrent que des BPDUs, pas des trames de données normales. */
    if ( source_eq->type_equ != STATION )
    {
        printf("Erreur : Seules les stations peuvent initier l'envoi de trames de données.\n");
        return;
    }

    /* Affiche les informations de l'envoi pour le débogage */
    char mac_src_str[19], mac_dst_str[19];
    mac_to_str(&tr->source, mac_src_str);
    mac_to_str(&tr->destination, mac_dst_str);

    printf("\n>>> TRANSMISSION DE TRAME : %s -> %s (Message: \"%s\") <<<\n",
           mac_src_str, mac_dst_str, (char *)tr->data);

    /* Trouve le câble qui relie la station source à son switch.
     * Une station est généralement connectée à un seul câble. */
    size_t cable_idx = SIZE_MAX;  /* SIZE_MAX = valeur sentinelle "non trouvé" */
    for ( size_t i = 0; i < r->nb_cables; i++ )
    {
        if ( r->cables[i].sommet1 == eq_source_idx || r->cables[i].sommet2 == eq_source_idx )
        {
            cable_idx = i;  /* Trouvé ! Mémorise l'index du câble */
            break;          /* Une station n'a qu'un seul câble → on s'arrête au premier */
        }
    }

    /* Si la station n'est connectée à aucun câble, elle ne peut rien envoyer */
    if ( cable_idx == SIZE_MAX )
    {
        printf("Erreur : La station source n'est connectée à aucun câble !\n");
        return;
    }

    /* Trouve l'équipement à l'autre extrémité du câble (normalement le switch directement connecté) */
    size_t voisin_idx = (r->cables[cable_idx].sommet1 == eq_source_idx)
                            ? r->cables[cable_idx].sommet2   /* La station est sommet1 → voisin = sommet2 */
                            : r->cables[cable_idx].sommet1;  /* La station est sommet2 → voisin = sommet1 */

    /* Alloue le tableau de visites pour éviter les boucles de propagation.
     * calloc alloue ET initialise tout à 0 (false pour les booléens).
     * Tous les équipements commencent comme "non visités". */
    bool *visite = calloc(r->nb_equipements, sizeof(bool));
    if ( visite == NULL )
        return;  /* Echec d'allocation → impossible d'envoyer */

    /* Marque la station source comme déjà visitée pour éviter qu'on lui renvoie la trame */
    visite[eq_source_idx] = true;

    /* Lance la propagation de la trame depuis le voisin direct de la station source */
    propager_trame_recue(r, voisin_idx, cable_idx, tr, visite, verbose);

    free(visite);  /* Libère le tableau de visites */
}
