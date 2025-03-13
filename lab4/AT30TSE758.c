/*
 * AT30TSE758.c
 *
 * Created: 3/13/2025 13:36:20
 *  Author: Student
 */ 
#include "AT30TSE758.h"
#include "I2C.h"  // Tento soubor p?edpokl�d� existenci funkce pro komunikaci s I2C sb?rnic�

// Funkce pro nastaven� rozli�en� teplotn�ho senzoru
uint8_t at30_setPrecision(uint8_t precision) {
	uint16_t config_register = 0;

	// Nastaven� rozli�en� v registrech
	config_register |= (uint16_t)(precision << 0);  // Shift precision do spr�vn� pozice

	// Zaps�n� do pointer registru
	i2cStart();
	i2cWrite(TempSensorAddrW);  // Adresa pro z�pis
	if (i2cGetStatus() != 0x18) {
		UART_SendString("Error 18\n\r");
		return 0;  // Chyba p?i komunikaci
	}

	i2cWrite(TempConfigRegister);  // Adresa konfigura?n�ho registru
	if (i2cGetStatus() != 0x28) {
		UART_SendString("Error 28\n\r");
		return 0;  // Chyba p?i z�pisu do registru
	}

	i2cWrite((uint8_t)(config_register >> 8));  // Z�pis vy��� byte
	if (i2cGetStatus() != 0x28) {
		UART_SendString("Error 28\n\r");
		return 0;  // Chyba p?i z�pisu
	}

	i2cWrite((uint8_t)(config_register));  // Z�pis ni��� byte
	if (i2cGetStatus() != 0x28) {
		UART_SendString("Error 28\n\r");
		return 0;  // Chyba p?i z�pisu
	}

	i2cStop();
	return 1;  // �sp?�n� nastaven�
}

// Funkce pro ?ten� teploty
float at30_readTemp(void) {
	uint8_t buffer[2];
	int16_t temperatureTMP;

	// Nastaven� pointer registru na teplotn� registr
	i2cStart();
	i2cWrite(TempSensorAddrW);  // Adresa pro z�pis
	if (i2cGetStatus() != 0x18) {
		UART_SendString("Error 18\n\r");
	}

	i2cWrite(TempTemperatureRegister);  // Adresa pro teplotu
	if (i2cGetStatus() != 0x28) {
		UART_SendString("Error 28\n\r");
	}

	i2cStop();

	// ?ten� teploty
	i2cStart();
	if (i2cGetStatus() != 0x08) {
		UART_SendString("Error 08\n\r");
	}

	i2cWrite(TempSensorAddrR);  // Adresa pro ?ten�
	if (i2cGetStatus() != 0x40) {
		UART_SendString("Error 40\n\r");
	}

	buffer[0] = i2cReadACK();  // ?teme prvn� byte
	if (i2cGetStatus() != 0x50) {
		UART_SendString("Error 50\n\r");
	}

	buffer[1] = i2cReadNACK();  // ?teme druh� byte
	if (i2cGetStatus() != 0x58) {
		UART_SendString("Error 58\n\r");
	}

	i2cStop();

	// Spo?�t�me teplotu (2 byty jako 16-bitov� hodnota)
	temperatureTMP = (buffer[0] << 8) | buffer[1];

	// P?epo?teme na teplotu ve �C, p?evod je z datasheetu
	return (float)temperatureTMP / 256;
}