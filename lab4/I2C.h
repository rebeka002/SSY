/*
 * I2C.h
 *
 * Created: 3/13/2025 13:21:26
 *  Author: Student
 */ 


#ifndef I2C_H_
#define I2C_H_

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>



void i2cInit(void);

void i2cStart(void);

void i2cStop(void);

void i2cWrite(uint8_t byte);

uint8_t i2cReadACK(void);

uint8_t i2cReadNACK(void);

uint8_t i2cGetStatus(void);

#endif /* I2C_H_ */