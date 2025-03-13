/*
 * AT30TSE758.c
 *
 * Created: 3/13/2025 13:36:20
 *  Author: Student
 */ 
#include "AT30TSE758.h"
#include "I2C.h"  // Tento soubor p?edpokládá existenci funkce pro komunikaci s I2C sb?rnicí

// Funkce pro nastavení rozlišení teplotního senzoru
uint8_t at30_setPrecision(uint8_t precision) {
	uint16_t config_register = 0;

	// Nastavení rozlišení v registrech
	config_register |= (uint16_t)(precision << 0);  // Shift precision do správné pozice

	// Zapsání do pointer registru
	i2cStart();
	i2cWrite(TempSensorAddrW);  // Adresa pro zápis
	if (i2cGetStatus() != 0x18) {
		UART_SendString("Error 18\n\r");
		return 0;  // Chyba p?i komunikaci
	}

	i2cWrite(TempConfigRegister);  // Adresa konfigura?ního registru
	if (i2cGetStatus() != 0x28) {
		UART_SendString("Error 28\n\r");
		return 0;  // Chyba p?i zápisu do registru
	}

	i2cWrite((uint8_t)(config_register >> 8));  // Zápis vyšší byte
	if (i2cGetStatus() != 0x28) {
		UART_SendString("Error 28\n\r");
		return 0;  // Chyba p?i zápisu
	}

	i2cWrite((uint8_t)(config_register));  // Zápis nižší byte
	if (i2cGetStatus() != 0x28) {
		UART_SendString("Error 28\n\r");
		return 0;  // Chyba p?i zápisu
	}

	i2cStop();
	return 1;  // Úsp?šné nastavení
}

// Funkce pro ?tení teploty
float at30_readTemp(void) {
	uint8_t buffer[2];
	int16_t temperatureTMP;

	// Nastavení pointer registru na teplotní registr
	i2cStart();
	i2cWrite(TempSensorAddrW);  // Adresa pro zápis
	if (i2cGetStatus() != 0x18) {
		UART_SendString("Error 18\n\r");
	}

	i2cWrite(TempTemperatureRegister);  // Adresa pro teplotu
	if (i2cGetStatus() != 0x28) {
		UART_SendString("Error 28\n\r");
	}

	i2cStop();

	// ?tení teploty
	i2cStart();
	if (i2cGetStatus() != 0x08) {
		UART_SendString("Error 08\n\r");
	}

	i2cWrite(TempSensorAddrR);  // Adresa pro ?tení
	if (i2cGetStatus() != 0x40) {
		UART_SendString("Error 40\n\r");
	}

	buffer[0] = i2cReadACK();  // ?teme první byte
	if (i2cGetStatus() != 0x50) {
		UART_SendString("Error 50\n\r");
	}

	buffer[1] = i2cReadNACK();  // ?teme druhý byte
	if (i2cGetStatus() != 0x58) {
		UART_SendString("Error 58\n\r");
	}

	i2cStop();

	// Spo?ítáme teplotu (2 byty jako 16-bitová hodnota)
	temperatureTMP = (buffer[0] << 8) | buffer[1];

	// P?epo?teme na teplotu ve °C, p?evod je z datasheetu
	return (float)temperatureTMP / 256;
}