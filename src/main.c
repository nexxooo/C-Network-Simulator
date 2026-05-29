#include "../include/affichage.h"
#include "../include/equipement.h"
int main(int argc, char *argv[]) {
  IPV4 ip = {120, 12, 3, 10};
  afficher_IPV4(&ip);
  MAC mac = {225, 128, 12, 46, 0, 1};
  afficher_mac(&mac);
}
