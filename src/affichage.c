#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "../include/affichage.h"
#include "../include/equipement.h"


void afficherIPV4(IPV4 *ip){
	printf("%i.%i.%i.%i\n",ip->bytes[0],ip->bytes[1],ip->bytes[2],ip->bytes[3]); 
}

void afficherMac(MAC *mac){
	printf("%02X:%02X:%02X:%02X:%02X:%02X\n",mac->bytes[0],mac->bytes[1],mac->bytes[2],mac->bytes[3],mac->bytes[4],mac->bytes[5]);
}

MAC str_to_mac(char* str)
{
	MAC res;
	int courant = 0;

	char* token = strtok(str, ":");

	while ( token != NULL )
	{
		res.bytes[courant] = (uint8_t) token;
		token = strtok(NULL, ":");
		courant++;

	}
	return res;
}

IPV4 str_to_ipv4(char* str)
{
	IPV4 res;
	int courant;

	char* token = strtok(str, ".");

	while ( token != NULL )
	{
		res.bytes[courant] = stroi(token);
		token = strtok(NULL, ":");
		courant++;
	}

	return res;
}