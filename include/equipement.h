#pragma once

#include <stdint.h>
#include <stdlib.h>

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

typedef struct reseau_local {

} reseau_local;

typedef struct trame {

} trame;
