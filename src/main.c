#include "../include/affichage.h"
#include "../include/equipement.h"
int main(int argc, char *argv[])
{
    IPV4 ip = {120, 12, 3, 10};
    afficher_ipv4(&ip);
    MAC mac = {225, 128, 12, 46, 0, 1};
    afficher_mac(&mac);

    reseau_local r;
    init_reseau(&r);
    if ( charger_reseau("configs/config3.txt", &r) != ERR_OK )
        printf("Erreur dans le chargement\n");
    afficher_reseau(&r);
    afficher_cables(&r);
    free_reseau(&r);
}
