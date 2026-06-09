#include "../include/affichage.h"
#include "../include/equipement.h"
#include <stdint.h>
#include <stdio.h>

void afficher_ipv4(IPV4 *ip)
{
    printf("%i.%i.%i.%i\n", ip->bytes[0], ip->bytes[1], ip->bytes[2],
           ip->bytes[3]);
}

void afficher_mac(MAC *mac)
{
    printf("%02X:%02X:%02X:%02X:%02X:%02X\n", mac->bytes[0], mac->bytes[1],
           mac->bytes[2], mac->bytes[3], mac->bytes[4], mac->bytes[5]);
}

void mac_to_str(MAC *mac, char *buffer)
{
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac->bytes[0], mac->bytes[1],
            mac->bytes[2], mac->bytes[3], mac->bytes[4], mac->bytes[5]);
}
void ip_to_str(IPV4 *ip, char *buffer)
{
    sprintf(buffer, "%i.%i.%i.%i\n", ip->bytes[0], ip->bytes[1], ip->bytes[2],
            ip->bytes[3]);
}

void afficher_reseau(reseau_local *reseau)
{
    printf("RESEAU: \n");
    printf("_____________________________________________\n");
    printf("nombre d'équipement: %zu\n", reseau->nb_equipements);
    printf("nombre de liaison: %zu\n", reseau->nb_cables);

    for ( size_t i = 0; i < reseau->nb_equipements; i++ )
    {
        printf("----------------------------------------------------------------------------\n");
        printf("equipement %zu: ", i);
        if ( reseau->equipements[i].type_equ == SWITCH )
        {
            printf("SWITCH ");
            switch_ *sw = &reseau->equipements[i].sw;
            printf("nombre de ports: %zu ", sw->nb_port);
            char macstr[19];
            mac_to_str(&sw->mac, macstr);
            printf("adresse mac: %s ", macstr);

            // afficher_mac(&sw->mac);
            printf("table de commutation:\n");
            afficher_table(sw);
            printf("\n-----------------------------------------------");
        }
        else
        {

            printf("HOTE ");
            station *st = &reseau->equipements[i].st;
            char macstr[19];
            mac_to_str(&st->mac, macstr);
            printf("adresse mac: %s ", macstr);
            // afficher_mac(&st->mac);
            char ipstr[17];
            ip_to_str(&st->ipv4, ipstr);
            printf("adresse ipv4: %s ", ipstr);
            // afficher_ipv4(&st->ipv4);
            printf("----------------------------------------------------------------------------\n");
        }
    }
}

void afficher_cables(const reseau_local *r)
{
    for ( int i = 0; i < r->nb_cables; i++ )
        printf("%zu --> %zu\n", r->cables[i].sommet1,
               r->cables[i].sommet2);
}

void afficher_table(switch_ *sw)
{
    if ( sw->taille_tab == 0 )
    {
        printf("table vide");
    }
    else
    {
        for ( size_t i = 0; i < sw->nb_port; i++ )
            printf("|          port: %zu           ", i);
        printf("\n");
        MAC *tab[sw->nb_port];
        for ( size_t i = 0; i < sw->nb_port; i++ )
            tab[i] = NULL;
        for ( size_t i = 0; i < sw->taille_tab; i++ )
            tab[sw->tab[i].interface_port] = &sw->tab[i].mac;
        for ( size_t i = 0; i < sw->nb_port; i++ )
            if ( tab[i] == NULL )
                printf("| NULL      ");
            else
                printf("| %02X:%02X:%02X:%02X:%02X:%02X\n", tab[i]->bytes[0],
                       tab[i]->bytes[1], tab[i]->bytes[2], tab[i]->bytes[3],
                       tab[i]->bytes[4], tab[i]->bytes[5]);
    }
}

void afficher_tram_user(trame *tr)
{
    printf("TRAM: \n ________________________________ \n adresse source:");
    afficher_mac(&tr->source);
    printf("destination:\n");
    afficher_mac(&tr->destination);
}

void afficher_tram_brute(trame *tr)
{
    printf("TRAM BRUTE: \n");
    for ( int i = 0; i < 7; i++ )
        printf("%02X ", tr->préambule[i]);
    printf("%02X \n", tr->SFD);
    for ( int i = 0; i < 6; i++ )
        printf("%02X ", tr->destination.bytes[i]);
    for ( int i = 0; i < 6; i++ )
        printf("%02X ", tr->source.bytes[i]);
    printf("\n");
    printf("%02X \n", tr->type);
    for ( size_t i = 0; i < 1500; i++ )
    {
        printf("%02X ", tr->data[i]);
        if ( (i + 1) % 16 == 0 )
            printf("\n");
        printf("-------------------------------------\n");
    }
}

MAC str_to_mac(char *str)
{
    MAC res = {0};
    unsigned int b[6];
    if ( sscanf(str, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6 )
        for ( int i = 0; i < 6; i++ )
            res.bytes[i] = (uint8_t)b[i];
    return res;
}

IPV4 str_to_ipv4(char *str)
{
    IPV4 res = {0};
    unsigned int b[4];
    if ( sscanf(str, "%u.%u.%u.%u", &b[0], &b[1], &b[2], &b[3]) == 4 )
        for ( int i = 0; i < 4; i++ )
            res.bytes[i] = (uint8_t)b[i];
    return res;
}
