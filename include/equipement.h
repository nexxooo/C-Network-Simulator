#pragma once

#include <cstdint>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define MAC_TAILLE 48
#define IPV4_TAILLE 32
#define IPV6_TAILLE 128

#define TRAME_PREAMBULE_TAILLE 7
#define TRAME_SFD_TAILLE 1
#define TRAME_DESTINATION_TAILLE 6
#define TRAME_SOURCE_TAILLE 6
#define TRAME_TYPE_TAILLE 2

#define TRAME_BOURRAGE_TAILLE 4

typedef struct MAC {
  uint8_t bytes[6];
} MAC;

typedef struct IPV4 {
  uint8_t bytes[4]; 
} IPV4;

typedef struct station {
  MAC mac;
  IPV4 ipv4;
} station;

typedef struct table_de_commutation {
  MAC mac;
  size_t interface_port;
} table_de_commutation;

typedef struct switch_ {
  MAC mac;
  size_t nb_port;
  size_t priorite;
  table_de_commutation *tab;
  size_t taille_tab;
  size_t taille_max;

} switch_;

typedef struct cable {
  MAC sommet1;
  MAC sommet2;
  size_t ponderation;
} cable;

typedef enum type_equipement {
	STATION,
	SWITCH

} type_equipement;

typedef struct equipement {
	type_equipement type_equ;
	union {	
		station st;
		switch_ sw;
	}; //equipement est soi une station soit un switch
} equipement;

typedef struct reseau_local {
	size_t ordre;
	equipement* equipements;

	size_t nb_equipements_reseau;
	cable* cables;
	size_t nb_cables;

} reseau_local;

typedef struct trame {
	MAC source;
	MAC destination;
	uint8_t SFD;
	uint16_t type;
	uint32_t FCS;
	uint8_t préambule[7];
	uint8_t data[1500];

} trame;

typedef enum Erreur_fichier
{
  ERR_FICHIER_NON_TROUVE,
  ERR_OUVERTURE_IMPOSSIBLE,
  ERR_INCONNU
} Erreur_fichier;

Erreur_fichier charger_reseau(char* fichier, reseau_local *r);
