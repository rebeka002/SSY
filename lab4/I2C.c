/*
 * I2C.c
 *
 * Created: 3/13/2025 13:23:32
 *  Author: Student
 */ 
#include "I2C.h"

void i2cInit(void)
{
	TWSR = 0x00;      // Nastavení prescaleru na 1
	TWBR = 0x02;      // Nastavení hodnoty pro 400kHz (p?i fosc = 8 MHz)
	TWCR = (1 << TWEN); // Povolení I2C sb?rnice
}

// Odeslání start bitu
void i2cStart(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN); // Nastavení start bitu a povolení TWI
	while ((TWCR & (1 << TWINT)) == 0); // ?ekání na dokon?ení start bitu
}

// Odeslání stop bitu
void i2cStop(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); // Nastavení stop bitu a povolení TWI
}

// Zápis jednoho byte na sb?rnici
void i2cWrite(uint8_t byte)
{
	TWDR = byte; // Uložení byte do data registru
	TWCR = (1 << TWINT) | (1 << TWEN); // Povolení zápisu
	while ((TWCR & (1 << TWINT)) == 0); // ?ekání na dokon?ení zápisu
}

// ?tení jednoho byte se zasláním ACK (pokra?ování ?tení)
uint8_t i2cReadACK(void)
{
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA); // Nastavení ACK bitu a povolení TWI
	while ((TWCR & (1 << TWINT)) == 0); // ?ekání na dokon?ení ?tení
	return TWDR; // Vrácení p?ijatého byte
}

// ?tení jednoho byte bez zaslání ACK (konec ?tení)
uint8_t i2cReadNACK(void)
{
	TWCR = (1 << TWINT) | (1 << TWEN); // Povolení TWI bez ACK
	while ((TWCR & (1 << TWINT)) == 0); // ?ekání na dokon?ení ?tení
	return TWDR; // Vrácení p?ijatého byte
}

// ?tení stavu sb?rnice
uint8_t i2cGetStatus(void)
{
	uint8_t status;
	status = TWSR & 0xF8; // Maskování stavových bit?
	return status;
}

