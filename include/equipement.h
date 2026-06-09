#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t CAPACITE_INITIALE = 12;
static size_t TAILLE_REALOC = 24;

typedef struct MAC
{
    uint8_t bytes[6];
} MAC;

typedef struct IPV4
{
    uint8_t bytes[4];
} IPV4;

typedef struct station
{
    MAC mac;
    IPV4 ipv4;
} station;

typedef struct table_de_commutation
{
    MAC mac;
    size_t interface_port;
} table_de_commutation;

typedef enum etat_port
{
    ETAT_PORT_BLOQUE,
    ETAT_PORT_INCONNU,
    ETAT_PORT_DESIGNE,
    ETAT_PORT_RACINE
} etat_port;

typedef struct port
{
    size_t numero_port;
    etat_port etat;
} port;

typedef struct switch_
{
    MAC mac;
    size_t nb_port;
    port *ports;
    size_t priorite;
    table_de_commutation *tab;
    size_t taille_tab;
    size_t taille_max;

    port port_racine;
    size_t cout_vers_racine;
    MAC racine;

} switch_;

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

typedef struct equipement
{
    type_equipement type_equ;
    union
    {
        station st;
        switch_ sw;
    }; // equipement est soi une station soit un switch

} equipement;

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

/* Protocole STP - doit être déclaré avant trame */
typedef struct BPDU
{
    size_t racine_id;
    size_t cout;
    MAC transmetteur_id;

} BPDU;

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

bool init_reseau(reseau_local *r);
bool free_reseau(reseau_local *r);

bool ajouter_equipement(equipement e, reseau_local *r);
bool ajouter_cable(cable c, reseau_local *r);

bool mac_est_meilleure(MAC *mac1, MAC *mac2);
bool mac_est_egale(MAC *mac1, MAC *mac2);

Erreur_fichier charger_reseau(const char *fichier, reseau_local *r);
