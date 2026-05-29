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
  printf("nombre d'équipement: %zu\n", reseau->nb_equipements);
  printf("nombre de liaison: %zu\n", reseau->nb_cables);
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

MAC str_to_mac(char* str)
{
	MAC res;
	unsigned int b[6];
	sscanf(str, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
	for(int i = 0; i < 6; i++) {
		res.bytes[i] = (uint8_t)b[i];
	}
	return res;
}

IPV4 str_to_ipv4(char* str)
{
	IPV4 res;
	unsigned int b[4];
	sscanf(str, "%u.%u.%u.%u", &b[0], &b[1], &b[2], &b[3]);
	for(int i = 0; i < 4; i++) {
		res.bytes[i] = (uint8_t)b[i];
	}
	return res;
}
