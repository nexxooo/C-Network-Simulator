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

typedef struct station {
    int8_t addrMac[MAC_TAILLE];
    int8_t ip[IPV4_TAILLE];
} station;

typedef struct switch_ {
    int8_t addrMac[MAC_TAILLE];
    size_t nb_ports;
} switch_;

typedef struct trame {


} trame;
