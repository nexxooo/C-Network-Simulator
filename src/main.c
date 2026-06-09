#include "../include/affichage.h"
#include "../include/equipement.h"
#include "../include/stp.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    // Utiliser config2.txt qui possède une boucle physique (Switch 0 <-> 1 <-> 2 <-> 0)
    const char *config_file = "configs/config4.txt";
    if (argc > 1)
    {
        config_file = argv[1];
    }

    printf("=== MODÉLISATION ET SIMULATION DE RÉSEAU LOCAL ===\n");
    printf("Chargement de la configuration : %s\n", config_file);

    reseau_local r;
    if (!init_reseau(&r))
    {
        printf("Erreur d'initialisation du réseau.\n");
        return 1;
    }

    if (charger_reseau(config_file, &r) != ERR_OK)
    {
        printf("Erreur dans le chargement du fichier de configuration.\n");
        free_reseau(&r);
        return 1;
    }

    afficher_reseau(&r);
    afficher_cables(&r);

    // Initialisation des ports physiques des switches
    init_switch_ports_and_links(&r);

    // Création d'une trame Unicast de la Station 3 (index 3) vers la Station 4 (index 4)
    trame t_unicast = {0};
    t_unicast.source = r.equipements[3].st.mac;
    t_unicast.destination = r.equipements[4].st.mac;
    t_unicast.type = 0x0800; // IPv4
    strcpy((char*)t_unicast.data, "Message de test Unicast de la Station 3 vers la Station 4");

    printf("\n===========================================================\n");
    printf("1. SIMULATION DE TRANSMISSION SANS PROTOCOLE STP\n");
    printf("===========================================================\n");
    printf("Envoi d'une trame de la Station 3 vers la Station 4...\n");
    transmettre_trame(&r, &t_unicast, 3, -1, 0);

    printf("\n===========================================================\n");
    printf("2. INITIALISATION ET CONVERGENCE DU PROTOCOLE STP\n");
    printf("===========================================================\n");
    if (!stp_init(&r))
    {
        printf("Erreur lors de l'exécution de STP.\n");
        free_reseau(&r);
        return 1;
    }

    // Réinitialiser les tables de commutation pour la nouvelle simulation
    for (size_t i = 0; i < r.nb_equipements; i++)
    {
        if (r.equipements[i].type_equ == SWITCH)
        {
            r.equipements[i].sw.taille_tab = 0;
        }
    }

    printf("\n===========================================================\n");
    printf("3. SIMULATION DE TRANSMISSION AVEC PROTOCOLE STP (UNICAST)\n");
    printf("===========================================================\n");
    printf("Envoi d'une trame Unicast de la Station 3 vers la Station 4...\n");
    transmettre_trame(&r, &t_unicast, 3, -1, 0);

    printf("\n===========================================================\n");
    printf("4. SIMULATION DE TRANSMISSION AVEC PROTOCOLE STP (BROADCAST)\n");
    printf("===========================================================\n");
    trame t_broadcast = {0};
    t_broadcast.source = r.equipements[3].st.mac;
    memset(t_broadcast.destination.bytes, 0xFF, 6); // Adresse Broadcast
    t_broadcast.type = 0x0806; // ARP
    strcpy((char*)t_broadcast.data, "Requête ARP Broadcast");

    printf("Envoi d'une trame Broadcast depuis la Station 3...\n");
    transmettre_trame(&r, &t_broadcast, 3, -1, 0);

    printf("\nLibération des ressources du réseau...\n");
    free_reseau(&r);
    printf("Simulation terminée avec succès !\n");

    return 0;
}
