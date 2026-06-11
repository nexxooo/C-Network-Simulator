#include "../include/equipement.h"
#include "../include/stp.h"

#include <assert.h>
#include <stdio.h>

void test_stp_loop_resolution()
{
    reseau_local r;
    init_reseau(&r);

    assert(charger_reseau("configs/config2.txt", &r) == ERR_OK);

    // Initialiser le STP
    stp_init(&r);

    // Vérifier les états des ports du Switch 0 (Index 0)
    // Switch 0 est la racine, donc tous ses ports doivent être DÉSIGNÉS
    switch_ *sw0 = &r.equipements[0].sw;
    assert(sw0->ports[0].etat == ETAT_PORT_DESIGNE); // Port 0 vers Switch 1
    assert(sw0->ports[1].etat == ETAT_PORT_DESIGNE); // Port 1 vers Switch 2
    assert(sw0->ports[2].etat == ETAT_PORT_DESIGNE); // Port 2 vers Station 3

    // Vérifier les états des ports du Switch 1 (Index 1)
    switch_ *sw1 = &r.equipements[1].sw;
    assert(sw1->ports[0].etat == ETAT_PORT_RACINE);   // Port 0 vers Switch 0 (meilleur coût)
    assert(sw1->ports[1].etat == ETAT_PORT_DESIGNE);  // Port 1 vers Switch 2 (Switch 1 gagne par MAC plus faible)
    assert(sw1->ports[2].etat == ETAT_PORT_DESIGNE);  // Port 2 vers Station 4

    // Vérifier les états des ports du Switch 2 (Index 2)
    switch_ *sw2 = &r.equipements[2].sw;
    assert(sw2->ports[0].etat == ETAT_PORT_RACINE);   // Port 0 vers Switch 0 (meilleur coût)
    assert(sw2->ports[1].etat == ETAT_PORT_BLOQUE);   // Port 1 vers Switch 1 (bloqué pour couper le cycle 0-1-2)
    assert(sw2->ports[2].etat == ETAT_PORT_DESIGNE);  // Port 2 vers Station 5

    free_reseau(&r);
}

int main()
{
    test_stp_loop_resolution();
    printf("========= STP Port Resolution Tests OK =========\n");
    return 0;
}
