#include "../include/affichage.h"
#include "../include/equipement.h"
#include "../include/stp.h"

int main( void )
{
    reseau_local r;
    init_reseau(&r);

    if ( charger_reseau("configs/config2.txt", &r) != ERR_OK )
        printf("Erreur dans le chargement\n");

    printf("Le réseau est il un arbre ? %b\n", est_un_arbre(&r));
    afficher_reseau(&r);
    afficher_cables(&r);

    printf("\n=== États des ports AVANT STP ===\n");
    afficher_etat_port_reseau(&r);

    stp_init(&r);

    printf("\n=== États des ports APRÈS STP ===\n");
    afficher_etat_port_reseau(&r);

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
}
