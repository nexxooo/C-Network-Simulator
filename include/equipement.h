/**
 * @file equipement.h
 * @brief Déclaration des structures de données et des prototypes pour le réseau local (LAN).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPACITE_INITIALE 12
#define TAILLE_REALOC 24

/** Adresse MAC (6 octets). */
typedef struct MAC
{
    uint8_t bytes[6];
} MAC;

/** Adresse IPv4 (4 octets). */
typedef struct IPV4
{
    uint8_t bytes[4];
} IPV4;

/** Représente une station (hôte) sur le réseau. */
typedef struct station
{
    MAC mac;
    IPV4 ipv4;
} station;

/** État d'un port dans le protocole STP. */
typedef enum etat_port
{
    ETAT_PORT_BLOQUE,
    ETAT_PORT_INCONNU,
    ETAT_PORT_DESIGNE,
    ETAT_PORT_RACINE
} etat_port;

/** BPDU (Bridge Protocol Data Unit) échangé par STP pour converger. */
typedef struct BPDU
{
    size_t racine_id;       /* ID du switch considéré comme racine */
    size_t cout;            /* Coût cumulé pour atteindre la racine */
    MAC transmetteur_id;    /* Adresse MAC de l'émetteur du BPDU */
} BPDU;

/** Port physique d'un switch. */
typedef struct port
{
    size_t numero_port;
    etat_port etat;
    BPDU meilleur_bpdu_recu;
    bool a_recu_bpdu;
} port;

/** Switch (pont réseau). */
typedef struct switch_
{
    MAC mac;
    size_t nb_port;
    port *ports;
    size_t priorite;

    port port_racine;
    size_t cout_vers_racine;
    MAC racine;
} switch_;

/** Câble reliant deux équipements avec un coût (pondération). */
typedef struct cable
{
    size_t sommet1;
    size_t sommet2;
    size_t ponderation;
} cable;

typedef enum type_equipement
{
    STATION,
    SWITCH
} type_equipement;

/** Équipement générique (station ou switch). */
typedef struct equipement
{
    type_equipement type_equ;
    union
    {
        station st;
        switch_ sw;
    };
} equipement;

/** Graphe du réseau local (équipements et câbles). */
typedef struct reseau_local
{
    size_t equipement_capacite;
    equipement *equipements;
    size_t nb_equipements;

    size_t cables_capacite;
    cable *cables;
    size_t nb_cables;
} reseau_local;

typedef enum Erreur_fichier
{
    ERR_FICHIER_NON_TROUVE,
    ERR_OUVERTURE_IMPOSSIBLE,
    ERR_LECTURE,
    ERR_OK
} Erreur_fichier;

/** Trame Ethernet IEEE 802.3 transportant des données ou un BPDU STP. */
typedef struct trame
{
    uint8_t preambule[7];
    uint8_t SFD;
    MAC destination;
    MAC source;
    uint16_t type;
    uint32_t FCS;

    union
    {
        uint8_t data[1500];
        BPDU bpdu;
    };
} trame;

/* --- Gestion du réseau --- */
bool init_reseau(reseau_local *r);
bool free_reseau(reseau_local *r);
bool ajouter_equipement(equipement e, reseau_local *r);
bool ajouter_cable(cable c, reseau_local *r);

/* --- Comparaison d'adresses MAC --- */
bool mac_est_meilleure(MAC *mac1, MAC *mac2);
bool mac_est_egale(MAC *mac1, MAC *mac2);

/* --- Chargement depuis un fichier --- */
Erreur_fichier charger_reseau(const char *fichier, reseau_local *r);

/* --- Arbre de recouvrement --- */
bool construire_arbre_selon_reseau(reseau_local *src, reseau_local *dst);

/* --- Théorie des graphes --- */
bool est_un_arbre(reseau_local *r);
size_t sommets_adjacent(const reseau_local *r, size_t sommet, size_t *adjacents);
void visite_composante_connexe(reseau_local const *g, size_t ind_equip, bool *visite);
bool reseau_est_connexe(reseau_local *r);

/* --- Utilitaires câbles et ports --- */
bool cable_est_relie(cable *c, size_t sommet1, size_t sommet2);
size_t obtenir_port_local(reseau_local *r, size_t sw_idx, size_t cable_idx);
