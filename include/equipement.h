/**
 * @file equipement.h
 * @brief Déclaration de toutes les structures de données et fonctions
 *        qui modélisent un réseau local (LAN) avec switchs et stations.
 *
 * Ce fichier est le cœur du projet. Il définit :
 *   - Les adresses réseau (MAC, IPv4)
 *   - Les équipements réseau (stations et switchs)
 *   - Les câbles qui relient les équipements
 *   - Le réseau local qui contient tout ça
 *   - Les trames Ethernet qui circulent sur le réseau
 */

#pragma once  /* Empêche l'inclusion multiple de ce fichier (équivalent à #ifndef ... #define ... #endif) */

#include <stdbool.h>  /* Pour le type bool (true/false) */
#include <stdint.h>   /* Pour les types à taille fixe : uint8_t (octet), uint16_t, uint32_t */
#include <stdio.h>    /* Pour printf, FILE, fopen, etc. */
#include <stdlib.h>   /* Pour malloc, realloc, free, etc. */
#include <string.h>   /* Pour memset, memcpy, strcspn, strtok, etc. */

/* =========================================================
   CONSTANTES
   ========================================================= */

/* Taille initiale des tableaux dynamiques d'équipements et câbles.
   Au départ, on réserve de la place pour 12 éléments. */
#define CAPACITE_INITIALE 12

/* Taille du bloc ajouté lors d'un agrandissement dynamique.
   Quand le tableau est plein, on l'agrandit de 24 cases supplémentaires. */
#define TAILLE_REALOC 24


/* =========================================================
   ADRESSES RÉSEAU
   ========================================================= */

/**
 * @brief Adresse MAC (Media Access Control).
 *
 * Une adresse MAC est l'identifiant unique d'une carte réseau.
 * Elle est composée de 6 octets (ex: AA:BB:CC:DD:EE:FF).
 * C'est l'adresse utilisée pour communiquer à l'intérieur d'un réseau local.
 */
typedef struct MAC
{
    uint8_t bytes[6];  /* Les 6 octets de l'adresse MAC, stockés de l'octet le plus significatif au moins significatif */
} MAC;

/**
 * @brief Adresse IPv4 (Internet Protocol version 4).
 *
 * Une adresse IP est l'identifiant logique d'un équipement sur un réseau plus large.
 * Elle est composée de 4 octets (ex: 192.168.1.1).
 */
typedef struct IPV4
{
    uint8_t bytes[4];  /* Les 4 octets de l'adresse IP, ex: bytes[0]=192, bytes[1]=168, bytes[2]=1, bytes[3]=1 */
} IPV4;


/* =========================================================
   ÉQUIPEMENTS RÉSEAU
   ========================================================= */

/**
 * @brief Représente une station (ordinateur, serveur, téléphone, etc.).
 *
 * Une station est un équipement terminal : elle envoie et reçoit des données,
 * mais ne fait pas de routage ni de commutation. Elle a une adresse MAC
 * (pour le réseau local) et une adresse IPv4 (pour le réseau plus large).
 */
typedef struct station
{
    MAC mac;    /* Adresse MAC de la station (identifiant physique unique) */
    IPV4 ipv4;  /* Adresse IPv4 de la station (identifiant logique) */
} station;

/**
 * @brief Entrée dans la table de commutation d'un switch.
 *
 * Un switch apprend dynamiquement quelles adresses MAC sont accessibles
 * par quel port. Chaque entrée dit : "pour atteindre cette adresse MAC,
 * envoie la trame sur ce port numéro X".
 */
typedef struct table_de_commutation
{
    MAC mac;               /* L'adresse MAC de la machine apprise */
    size_t interface_port; /* Le numéro du port par lequel cette machine est accessible */
} table_de_commutation;

/**
 * @brief État possible d'un port de switch dans le protocole STP.
 *
 * Le STP attribue un état à chaque port pour éviter les boucles réseau :
 *   - BLOQUE   : Le port ne transmet pas de trames (pour casser une boucle).
 *   - INCONNU  : État initial avant que STP ait fini de s'exécuter.
 *   - DESIGNE  : Le port est actif et transmet les trames vers le segment qu'il dessert.
 *   - RACINE   : Le port offre le meilleur chemin vers le switch racine.
 */
typedef enum etat_port
{
    ETAT_PORT_BLOQUE,    /* Port bloqué : ne transmet aucune trame de données */
    ETAT_PORT_INCONNU,   /* État inconnu : STP n'a pas encore décidé */
    ETAT_PORT_DESIGNE,   /* Port désigné : actif, dessert ce segment de réseau */
    ETAT_PORT_RACINE     /* Port racine : chemin le plus court vers la racine STP */
} etat_port;

/**
 * @brief BPDU (Bridge Protocol Data Unit) — message échangé par STP.
 *
 * Les BPDUs sont les "messages" que les switchs s'échangent pour élire
 * la racine de l'arbre et calculer les meilleurs chemins.
 * Format simplifié du BPDU 802.1d :
 *   - Qui est la racine (selon moi) ?
 *   - Quel est le coût pour atteindre cette racine depuis moi ?
 *   - Qui suis-je (l'émetteur) ?
 */
typedef struct BPDU
{
    size_t racine_id;       /* Index (dans le tableau d'équipements) du switch que l'émetteur croit être la racine */
    size_t cout;            /* Coût cumulé pour atteindre la racine depuis l'émetteur (somme des poids des câbles) */
    MAC transmetteur_id;    /* Adresse MAC du switch qui envoie ce BPDU */
} BPDU;

/**
 * @brief Représente un port physique d'un switch.
 *
 * Un port est une prise RJ45 sur le switch. Chaque câble branché
 * correspond à un port. STP gère l'état de chaque port individuellement.
 */
typedef struct port
{
    size_t numero_port;          /* Numéro du port (0, 1, 2, ...) */
    etat_port etat;              /* État STP actuel de ce port (bloqué, racine, désigné, inconnu) */
    BPDU meilleur_bpdu_recu;     /* Le meilleur BPDU reçu sur ce port (celui qui a les meilleures infos vers la racine) */
    bool a_recu_bpdu;            /* Indique si ce port a déjà reçu au moins un BPDU (false au départ) */
} port;

/**
 * @brief Représente un switch (commutateur réseau).
 *
 * Un switch est un équipement réseau qui relie plusieurs machines.
 * Il apprend les adresses MAC des machines connectées et envoie
 * les trames directement vers le bon port (contrairement à un hub
 * qui diffuse à tout le monde).
 *
 * Dans STP, les switchs sont appelés "ponts" (bridges).
 */
typedef struct switch_
{
    MAC mac;                    /* Adresse MAC du switch (sert aussi d'identifiant STP) */
    size_t nb_port;             /* Nombre de ports physiques de ce switch */
    port *ports;                /* Tableau dynamique des ports (alloué avec malloc) */
    size_t priorite;            /* Priorité STP du switch (plus petit = prioritaire pour être racine) */
    table_de_commutation *tab;  /* Table de commutation : associe chaque MAC connue à un port */
    size_t taille_tab;          /* Nombre d'entrées actuellement dans la table de commutation */
    size_t taille_max;          /* Capacité maximale actuelle de la table (avant realloc) */

    port port_racine;           /* Le port de ce switch qui mène vers la racine STP */
    size_t cout_vers_racine;    /* Coût total du chemin de ce switch jusqu'à la racine */
    MAC racine;                 /* Adresse MAC du switch que ce switch considère comme racine */
} switch_;

/**
 * @brief Représente un câble (lien physique) entre deux équipements.
 *
 * Un câble relie deux équipements identifiés par leur index dans le tableau
 * d'équipements du réseau. La pondération représente le coût du lien
 * (utilisé par STP pour choisir le meilleur chemin — plus le coût est faible,
 * meilleur est le chemin).
 */
typedef struct cable
{
    size_t sommet1;       /* Index de l'équipement à une extrémité du câble */
    size_t sommet2;       /* Index de l'équipement à l'autre extrémité du câble */
    size_t ponderation;   /* Coût/poids du lien (ex: 4 pour un lien Fast Ethernet à 100 Mbps selon 802.1d) */
} cable;

/**
 * @brief Type d'un équipement réseau.
 *
 * Un équipement est soit une station (terminal), soit un switch (commutateur).
 */
typedef enum type_equipement
{
    STATION,  /* Machine terminale : PC, serveur, téléphone... */
    SWITCH    /* Commutateur réseau : relie plusieurs équipements */
} type_equipement;

/**
 * @brief Représente un équipement générique (station ou switch).
 *
 * On utilise une union pour qu'un équipement puisse être soit une station,
 * soit un switch, sans gaspiller de mémoire. Le champ `type_equ` dit
 * lequel des deux est actif.
 *
 * Exemple d'utilisation :
 *   equipement e;
 *   if (e.type_equ == SWITCH) { ... e.sw.nb_port ... }
 *   if (e.type_equ == STATION) { ... e.st.mac ... }
 */
typedef struct equipement
{
    type_equipement type_equ;  /* Indique si c'est une STATION ou un SWITCH */
    union
    {
        station st;   /* Données de la station (valide si type_equ == STATION) */
        switch_ sw;   /* Données du switch (valide si type_equ == SWITCH) */
    }; /* Union anonyme : on accède directement avec e.st ou e.sw */
} equipement;

/**
 * @brief Représente l'ensemble du réseau local (LAN).
 *
 * Le réseau est modélisé comme un graphe :
 *   - Les sommets sont les équipements (stations et switchs)
 *   - Les arêtes sont les câbles qui les relient
 *
 * Les tableaux sont dynamiques : ils grandissent automatiquement
 * si on ajoute plus d'éléments que prévu.
 */
typedef struct reseau_local
{
    size_t equipement_capacite;  /* Capacité actuelle du tableau d'équipements (nombre max avant realloc) */
    equipement *equipements;     /* Tableau dynamique de tous les équipements du réseau */
    size_t nb_equipements;       /* Nombre d'équipements actuellement dans le tableau */

    size_t cables_capacite;      /* Capacité actuelle du tableau de câbles */
    cable *cables;               /* Tableau dynamique de tous les câbles du réseau */
    size_t nb_cables;            /* Nombre de câbles actuellement dans le tableau */
} reseau_local;

/**
 * @brief Codes d'erreur pour le chargement d'un fichier de configuration.
 */
typedef enum Erreur_fichier
{
    ERR_FICHIER_NON_TROUVE,     /* Le fichier n'existe pas à l'emplacement indiqué */
    ERR_OUVERTURE_IMPOSSIBLE,   /* Impossible d'ouvrir le fichier (problème de permissions) */
    ERR_LECTURE,                /* Erreur pendant la lecture du fichier */
    ERR_OK                      /* Succès : le fichier a été lu sans problème */
} Erreur_fichier;

/**
 * @brief Trame Ethernet — l'unité de données qui circule sur le réseau.
 *
 * Une trame Ethernet est le "paquet" qui transporte les données sur le réseau local.
 * Sa structure suit le standard IEEE 802.3 :
 *
 *  [Préambule 7 octets][SFD 1 octet][Destination MAC 6][Source MAC 6][Type 2][Données 0-1500][FCS 4]
 *
 * Le champ data peut contenir soit des données normales (texte, image...),
 * soit un BPDU (message STP) — c'est pourquoi on utilise une union.
 */
typedef struct trame
{
    uint8_t preambule[7];  /* Préambule : 7 octets à 0xAA. Sert à synchroniser la réception */
    uint8_t SFD;           /* Start Frame Delimiter : valeur 0xAB, indique le début de la trame */
    MAC destination;       /* Adresse MAC de destination (qui doit recevoir cette trame) */
    MAC source;            /* Adresse MAC source (qui envoie cette trame) */
    uint16_t type;         /* EtherType : indique le protocole encapsulé (ex: 0x0800=IPv4, 0x8809=STP) */
    uint32_t FCS;          /* Frame Check Sequence : checksum pour détecter les erreurs (simplifié à 0 ici) */

    union
    {
        uint8_t data[1500]; /* Données normales (max 1500 octets, limite Ethernet standard) */
        BPDU bpdu;          /* Ou un message BPDU STP (pour la phase de configuration STP) */
    };
} trame;


/* =========================================================
   PROTOTYPES DES FONCTIONS
   ========================================================= */

/* --- Gestion du réseau --- */

/** Initialise un réseau vide (alloue les tableaux dynamiques). Retourne false si l'allocation échoue. */
bool init_reseau(reseau_local *r);

/** Libère toute la mémoire allouée pour le réseau (tableaux, ports des switchs, tables de commutation). */
bool free_reseau(reseau_local *r);

/** Ajoute un équipement au réseau (agrandit le tableau si nécessaire). */
bool ajouter_equipement(equipement e, reseau_local *r);

/** Ajoute un câble au réseau (agrandit le tableau si nécessaire). */
bool ajouter_cable(cable c, reseau_local *r);

/* --- Comparaison d'adresses MAC --- */

/** Retourne true si mac1 est "meilleure" (plus petite) que mac2, octet par octet de gauche à droite. */
bool mac_est_meilleure(MAC *mac1, MAC *mac2);

/** Retourne true si les deux adresses MAC sont identiques (octet par octet). */
bool mac_est_egale(MAC *mac1, MAC *mac2);

/* --- Chargement depuis un fichier --- */

/** Lit un fichier de configuration et remplit le réseau avec les équipements et câbles décrits. */
Erreur_fichier charger_reseau(const char *fichier, reseau_local *r);

/* --- Arbre de recouvrement --- */

/**
 * Construit un nouveau réseau (dst) qui ne contient que les liens actifs après STP.
 * Les ports bloqués sont exclus. Les liens vers des stations sont toujours conservés.
 */
bool construire_arbre_selon_reseau(reseau_local *src, reseau_local *dst);

/* --- Théorie des graphes --- */

/** Retourne true si le réseau est un arbre (connexe ET nb_câbles == nb_équipements - 1). */
bool est_un_arbre(reseau_local *r);

/**
 * Remplit le tableau `adjacents` avec les indices des voisins du sommet donné,
 * et retourne le nombre de voisins trouvés.
 */
size_t sommets_adjacent(const reseau_local *r, size_t sommet, size_t *adjacents);

/** Parcours en profondeur (DFS) pour marquer tous les équipements connectés à ind_equip. */
void visite_composante_connexe(reseau_local const *g, size_t ind_equip, bool *visite);

/** Retourne true si tous les équipements du réseau sont connectés entre eux (réseau connexe). */
bool reseau_est_connexe(reseau_local *r);

/* --- Utilitaires câbles et ports --- */

/** Retourne true si le câble c relie bien sommet1 et sommet2 (dans l'un ou l'autre sens). */
bool cable_est_relie(cable *c, size_t sommet1, size_t sommet2);

/**
 * Retourne le numéro de port local (0, 1, 2...) du switch sw_idx qui correspond au câble cable_idx.
 * Les ports sont numérotés dans l'ordre des câbles qui touchent ce switch.
 */
size_t obtenir_port_local(reseau_local *r, size_t sw_idx, size_t cable_idx);

/* --- Commutation et transmission de trames --- */

/** Enregistre dans la table du switch que la MAC source_mac est accessible via port_entree. */
void switch_apprendre_mac(switch_ *sw, MAC source_mac, size_t port_entree);

/** Cherche dans la table du switch par quel port atteindre dest_mac. Retourne SIZE_MAX si inconnu. */
size_t switch_trouver_port(switch_ *sw, MAC dest_mac);

/**
 * Retrouve l'équipement voisin et le câble associés à un port donné d'un switch.
 * Remplit *voisin_idx et *cable_idx, retourne true si trouvé.
 */
bool obtenir_voisin_par_port(const reseau_local *r, size_t sw_idx, size_t port_num, size_t *voisin_idx, size_t *cable_idx);

/** Crée et retourne une trame Ethernet avec les champs remplis correctement. */
trame creer_trame_ethernet(MAC source, MAC destination, uint16_t type, const uint8_t *data, size_t data_len);

/** Envoie une trame depuis une station source à travers le réseau (avec apprentissage et commutation). */
void envoyer_trame(reseau_local *r, size_t eq_source_idx, trame *tr, bool verbose);