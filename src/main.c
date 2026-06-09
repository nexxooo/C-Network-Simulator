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
    if ( charger_reseau("configs/config1.txt", &r) != ERR_OK )
        printf("Erreur dans le chargement\n");

    printf("Le réseau est il un arbre ? %b\n", est_un_arbre(&r));
    afficher_reseau(&r);
    afficher_cables(&r);

    stp_init(&r);

    afficher_reseau(&r);
    printf("Le réseau (après STP) est il un arbre ? %b\n", est_un_arbre(&r));

    /* Construire l'arbre de recouvrement selon les ports actifs après STP */
    reseau_local arbre;
    if ( construire_arbre_selon_reseau(&r, &arbre) )
    {
        printf("\n=== Arbre construit depuis le réseau STP ===\n");
        afficher_reseau(&arbre);
        afficher_cables(&arbre);
        printf("L'arbre construit est-il un arbre ? %b\n", est_un_arbre(&arbre));
        free_reseau(&arbre);
    }

    free_reseau(&r);

    (void)argc;
    (void)argv;
}
