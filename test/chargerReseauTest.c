#include "../include/equipement.h"

#include <assert.h>
#include <stdio.h>

void test_config1()
{
    reseau_local r;
    init_reseau(&r);

    charger_reseau("configs/config1.txt", &r);

    assert(r.nb_equipements == 3);
    assert(r.nb_cables == 2);

    free_reseau(&r);
}

void test_config2()
{
    reseau_local r;
    init_reseau(&r);

    charger_reseau("configs/config2.txt", &r);

    assert(r.nb_equipements == 6);
    assert(r.nb_cables == 6);

    free_reseau(&r);
}

void test_config3()
{
    reseau_local r;
    init_reseau(&r);

    charger_reseau("configs/config3.txt", &r);

    assert(r.nb_equipements == 6);
    assert(r.nb_cables == 6);

    free_reseau(&r);
}


int main()
{
    test_config1();
    test_config2();
    test_config3();

    printf("=========Tests OK=========\n");
    return 0;
}