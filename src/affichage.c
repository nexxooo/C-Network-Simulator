#include "../include/affichage.h"
#include "../include/equipement.h"
#include <stdint.h>
#include <stdio.h>

void afficher_ipv4(IPV4 *ip) {
  printf("%i.%i.%i.%i\n", ip->bytes[0], ip->bytes[1], ip->bytes[2],
         ip->bytes[3]);
}

void afficher_mac(MAC *mac) {
  printf("%02X:%02X:%02X:%02X:%02X:%02X\n", mac->bytes[0], mac->bytes[1],
         mac->bytes[2], mac->bytes[3], mac->bytes[4], mac->bytes[5]);
}

void afficher_reseau(reseau_local *reseau) {
  printf("RESEAU: \n");
  printf("_____________________________________________\n");
  printf("nombre d'équipement: %lu\n", reseau->nb_equipements);
  printf("nombre de liaison: %lu\n", reseau->nb_cables);
}

void afficher_table(switch_ *sw) {
  if (sw->taille_tab == 0) {
    printf("table vide");
  } else {
    for (int i = 0; i < sw->nb_port; i++) {
      printf("|          port: %i           ", i);
    }
    MAC *tab[sw->taille_tab];
    for (int i = 0; i < sw->taille_tab; i++) {
      tab[sw->tab[i].interface_port] = &sw->tab[i].mac;
    }
    for (int i = 0; i < sw->nb_port; i++) {
      if (tab[i] == NULL) {
        printf("| NULL      ");
      } else {
        printf("| %02X:%02X:%02X:%02X:%02X:%02X\n", tab[i]->bytes[0],
               tab[i]->bytes[1], tab[i]->bytes[2], tab[i]->bytes[3],
               tab[i]->bytes[4], tab[i]->bytes[5]);
      }
    }
  }
}

void afficher_tram_user(trame *tr) {
  printf("TRAM: \n ________________________________ \n adresse source:");
  afficher_mac(&tr->source);
  printf("destination:\n");
  afficher_mac(&tr->destination);
}

void afficher_tram_brute(trame *tr) {
  printf("TRAM BRUTE: \n");
  for (int i = 0; i < 7; i++) {
    printf("%02X ", tr->préambule[i]);
  }
  printf("%02X \n", tr->SFD);
  for (int i = 0; i < 6; i++) {
    printf("%02X ", tr->destination.bytes[i]);
  }
  for (int i = 0; i < 6; i++) {
    printf("%02X ", tr->source.bytes[i]);
  }
  printf("\n");
  printf("%02X \n", tr->type);
  for (size_t i = 0; i < 1500; i++) {
    printf("%02X ", tr->data[i]);
    if ((i + 1) % 16 == 0) {
      printf("\n");
    }
    printf("-------------------------------------\n");
  }
}

MAC str_to_mac(char *str) {
  MAC res;
  int courant = 0;

  char *token = strtok(str, ":");

  while (token != NULL) {
    res.bytes[courant] = (uint8_t)strtol(token, NULL, 16);
    token = strtok(NULL, ":");
    courant++;
  }
  return res;
}

IPV4 str_to_ipv4(char *str) {
  IPV4 res;
  int courant = 0;

  char *token = strtok(str, ".");

  while (token != NULL) {
    res.bytes[courant] = (uint8_t)atoi(token);
    token = strtok(NULL, ".");
    courant++;
  }

  return res;
}
