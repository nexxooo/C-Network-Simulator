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

    printf("\n=== Affichage individuel (Switch 0) ===\n");
    if ( r.nb_equipements > 0 && r.equipements[0].type_equ == SWITCH )
    {
        afficher_etat_port_switch(&r.equipements[0].sw);
    }

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

    /* --- Simulation de transmission de trames Ethernet avec commutation --- */
    printf("\n==================================================\n");
    printf("  SIMULATION DE COMMUTATION DE TRAMES ETHERNET    \n");
    printf("==================================================\n");

    if ( r.nb_equipements >= 5 )
    {
        size_t idx_station_src = 3; // Station 1 (MAC: 54:d6:a6:82:c5:01)
        size_t idx_station_dst = 4; // Station 2 (MAC: 54:d6:a6:82:c5:02)

        MAC mac_src = r.equipements[idx_station_src].st.mac;
        MAC mac_dst = r.equipements[idx_station_dst].st.mac;

        // 1. Première trame: Station 1 -> Station 2 (Flooding car destination inconnue)
        const char *msg1 = "Hello Station 2!";
        trame t1 = creer_trame_ethernet(mac_src, mac_dst, 0x0800, (const uint8_t *)msg1, strlen(msg1) + 1);
        envoyer_trame(&r, idx_station_src, mac_dst, &t1, true);

        // 2. Deuxième trame: Station 2 -> Station 1 (Unicast car source maintenant apprise par les switchs)
        const char *msg2 = "Hello back, Station 1!";
        trame t2 = creer_trame_ethernet(mac_dst, mac_src, 0x0800, (const uint8_t *)msg2, strlen(msg2) + 1);
        envoyer_trame(&r, idx_station_dst, mac_src, &t2, true);

        // 3. Afficher les tables de commutation après transmission
        printf("\n=== TABLES DE COMMUTATION APRÈS TRANSMISSIONS ===\n");
        for ( size_t i = 0; i < r.nb_equipements; i++ )
        {
            if ( r.equipements[i].type_equ == SWITCH )
            {
                char mac_sw[19];
                mac_to_str(&r.equipements[i].sw.mac, mac_sw);
                printf("\n--- Table de commutation du Switch %zu (%s) ---\n", i, mac_sw);
                afficher_table(&r.equipements[i].sw);
                printf("\n");
            }
        }
    }

    free_reseau(&r);

    (void)argc;
    (void)argv;
}

