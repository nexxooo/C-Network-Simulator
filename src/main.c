#include "../include/affichage.h"
#include "../include/equipement.h"
#include "../include/stp.h"

int main(int argc, char *argv[])
{
    /* Initialisation et chargement du réseau */
    reseau_local r;
    init_reseau(&r);

    if ( charger_reseau("configs/config2.txt", &r) != ERR_OK )
        printf("Erreur dans le chargement\n");

    /* Affichage initial du réseau */
    printf("Le réseau est il un arbre ? %b\n", est_un_arbre(&r));
    afficher_reseau(&r);
    afficher_cables(&r);

    /* États des ports avant STP */
    printf("\n=== États des ports AVANT STP ===\n");
    afficher_etat_port_reseau(&r);

    /* Exécution du protocole STP */
    stp_init(&r);

    /* États des ports après STP */
    printf("\n=== États des ports APRÈS STP ===\n");
    afficher_etat_port_reseau(&r);

    /* Affichage individuel (Switch 0) */
    printf("\n=== Affichage individuel (Switch 0) ===\n");
    if ( r.nb_equipements > 0 && r.equipements[0].type_equ == SWITCH )
    {
        afficher_etat_port_switch(&r.equipements[0].sw);
    }

    afficher_reseau(&r);
    printf("Le réseau (après STP) est il un arbre ? %b\n", est_un_arbre(&r));

    /* Construction de l'arbre de recouvrement */
    reseau_local arbre;
    if ( construire_arbre_selon_reseau(&r, &arbre) )
    {
        printf("\n=== Arbre construit depuis le réseau STP ===\n");
        afficher_reseau(&arbre);
        afficher_cables(&arbre);
        printf("L'arbre construit est-il un arbre ? %b\n", est_un_arbre(&arbre));
        free_reseau(&arbre);
    }

    /* Nettoyage de la mémoire */
    free_reseau(&r);

    (void)argc;
    (void)argv;
}
