#ifndef AT30TSE758_H
#define AT30TSE758_H

#include <stdint.h>

// Definice adresy teplotn�ho senzoru (z�pis a ?ten�)
#define TempSensorAddrW 0b10010110  // Write
#define TempSensorAddrR 0b10010111  // Read

// Definice registr?
#define TempConfigRegister 0x01
#define TempTemperatureRegister 0x00

// Funkce pro nastaven� rozli�en� teplotn�ho senzoru
uint8_t at30_setPrecision(uint8_t precision);

// Funkce pro ?ten� teploty
float at30_readTemp(void);

#endif // AT30TSE758_H