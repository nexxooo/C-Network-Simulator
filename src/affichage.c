#include "../include/affichage.h"
#include "../include/equipement.h"
#include <stdint.h>
#include <stdio.h>

void afficherIPV4(IPV4 *ip) {
  printf("%i.%i.%i.%i\n", ip->bytes[0], ip->bytes[1], ip->bytes[2],
         ip->bytes[3]);
}

void afficherMac(MAC *mac) {
  printf("%02X:%02X:%02X:%02X:%02X:%02X\n", mac->bytes[0], mac->bytes[1],
         mac->bytes[2], mac->bytes[3], mac->bytes[4], mac->bytes[5]);
}

void afficherReseau(reseau_local *reseau) {
  printf("RESEAU: \n");
  printf("_____________________________________________\n");
  printf("nombre d'équipement: %lu\n", reseau->ordre);
  printf("nombre de liaison: %lu\n", reseau->nb_cables);
}

void afficherTable(switch_ *sw) {
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
