/*
 * I2C.c
 *
 * Created: 3/13/2025 13:23:32
 *  Author: Student
 */ 
#include "I2C.h"

void i2cInit(void)
{
	TWSR = 0x00;      // Nastaven� prescaleru na 1
	TWBR = 0x02;      // Nastaven� hodnoty pro 400kHz (p?i fosc = 8 MHz)
	TWCR = (1 << TWEN); // Povolen� I2C sb?rnice
}

// Odesl�n� start bitu
void i2cStart(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN); // Nastaven� start bitu a povolen� TWI
	while ((TWCR & (1 << TWINT)) == 0); // ?ek�n� na dokon?en� start bitu
}

// Odesl�n� stop bitu
void i2cStop(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); // Nastaven� stop bitu a povolen� TWI
}

// Z�pis jednoho byte na sb?rnici
void i2cWrite(uint8_t byte)
{
	TWDR = byte; // Ulo�en� byte do data registru
	TWCR = (1 << TWINT) | (1 << TWEN); // Povolen� z�pisu
	while ((TWCR & (1 << TWINT)) == 0); // ?ek�n� na dokon?en� z�pisu
}

// ?ten� jednoho byte se zasl�n�m ACK (pokra?ov�n� ?ten�)
uint8_t i2cReadACK(void)
{
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA); // Nastaven� ACK bitu a povolen� TWI
	while ((TWCR & (1 << TWINT)) == 0); // ?ek�n� na dokon?en� ?ten�
	return TWDR; // Vr�cen� p?ijat�ho byte
}

// ?ten� jednoho byte bez zasl�n� ACK (konec ?ten�)
uint8_t i2cReadNACK(void)
{
	TWCR = (1 << TWINT) | (1 << TWEN); // Povolen� TWI bez ACK
	while ((TWCR & (1 << TWINT)) == 0); // ?ek�n� na dokon?en� ?ten�
	return TWDR; // Vr�cen� p?ijat�ho byte
}

// ?ten� stavu sb?rnice
uint8_t i2cGetStatus(void)
{
	uint8_t status;
	status = TWSR & 0xF8; // Maskov�n� stavov�ch bit?
	return status;
}

