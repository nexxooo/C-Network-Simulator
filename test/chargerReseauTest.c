#include "../include/equipement.h"

#include <assert.h>
#include <stdio.h>

void test_config1()
{
        reseau_local r;

    charger_reseau("configs/config1", &r);

    assert(r.nb_equipements == 3);
    assert(r.nb_cables == 2);

}

void test_config2()
{
        reseau_local r;

    charger_reseau("configs/config2", &r);

    assert(r.nb_equipements == 6);
    assert(r.nb_cables == 6);

}

void test_config3()
{
        reseau_local r;

    charger_reseau("configs/config3", &r);

    assert(r.nb_equipements == 6);
    assert(r.nb_cables == 6);

}


int main()
{
    test_config1();
    test_config2();
    test_config3();

    printf("=========Tests OK=========");

}