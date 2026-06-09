#include "../include/affichage.h"
#include "../include/equipement.h"
#include "../include/stp.h"

int main(int argc, char *argv[])
{
    IPV4 ip = {120, 12, 3, 10};
    afficher_ipv4(&ip);
    MAC mac = {225, 128, 12, 46, 0, 1};
    afficher_mac(&mac);

    reseau_local r;
    init_reseau(&r);
    if ( charger_reseau("configs/mylan.txt", &r) != ERR_OK )
        printf("Erreur dans le chargement\n");

    printf("Le réseau est il un arbre ? %b\n", est_un_arbre(&r));
    afficher_reseau(&r);
    afficher_cables(&r);

    stp_init(&r);
    
    afficher_reseau(&r);
    printf("Le réseau est il un arbre ? %b\n", est_un_arbre(&r));



    free_reseau(&r);
}
