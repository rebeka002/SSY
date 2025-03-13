#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "makra.h"
#include "timer.h"
#include "I2C.h"
#include "AT30TSE758.h"

#define LED_PIN PB4  // Use PB4 for PWM (OC2A)
#define F_CPU 8000000UL
#define PRESCALE ((1 << CS12) | (1 << CS10)) // 1024 prescaler

#define TempSensorAddrW 0b10010110
#define TempSensorAddrR 0b10010111
#define Temp_configRegister 0x01  // Example register for config
#define Temp_tempRegister 0x00

// ADC Initialization function
void ADC_Init(uint8_t prescale, uint8_t uref) {
    ADMUX = 0;                 // Clear ADMUX register
    ADCSRA = 0;                // Clear ADCSRA register
    
    // Set prescaler and reference voltage
    ADCSRA |= (prescale << ADPS0);
    ADMUX |= (uref << REFS0);  // Set the reference voltage (e.g., AVCC or internal)
    
    // Enable ADC
    ADCSRA |= (1 << ADEN);  
    while (!(ADCSRA & (1 << ADEN))) {};  // Wait for ADC to be ready
}

uint16_t ADC_get(uint8_t chan) {
    uint16_t tmp = 0;
    
    // Clear the MUX bits (to select the ADC channel)
    ADMUX &= ~(31 << MUX0);
    ADCSRB &= ~(1 << MUX5);
    
    // Set the appropriate ADC channel (PF1 = ADC1)
    ADMUX |= (chan << MUX0);
    
    // Start ADC conversion
    ADCSRA |= (1 << ADSC);  
    
    // Wait for conversion to complete (Polling)
    while (ADCSRA & (1 << ADSC)) {}; 
    
    tmp = ADC;  // Read the ADC value
    
    // Clear the ADC interrupt flag
    ADCSRA |= (1 << ADIF);
    
    return tmp;
}

void print_ADC_value(void) {
    uint16_t light_intensity = ADC_get(3);  // Get ADC value from channel 1 (PF1)
    
    char light_str[10];
    itoa(light_intensity, light_str, 10);  // Convert ADC value to string
    
    UART_SendString("Light Intensity: ");
    UART_SendString(light_str);  // Send the value over UART
    UART_SendString("\n");
}

uint8_t brightness = 50;  // Starting brightness level (0-100)

ISR(TIMER2_OVF_vect) {
    // Timer2 overflow interrupt, not needed here for PWM control
}

void Timer2_fastpwm_start(uint16_t strida) {
    cli();  // Disable interrupts
    TCCR2A = 0;  // Clear control registers
    TCCR2B = 0;
    TIMSK2 = 0;  // Disable interrupts for Timer2
    OCR2A = (255 * strida) / 100;  // Set PWM duty cycle based on 'strida' (brightness)
    
    // Fast PWM mode:
    TCCR2A |= (1 << WGM21) | (1 << WGM20);  // Set to Fast PWM mode
    TCCR2B |= (1 << CS22) | (1 << CS21) | (1 << CS20);  // 1024 prescaler (CS22, CS21, CS20)
    
    // Enable output on OC2A (PB4):
    TCCR2A |= (1 << COM2A1);  // Non-inverted PWM output on OC2A (PB4)
    
    sei();  // Enable global interrupts
}

void Timer2_stop(void) {
    TCCR2B = 0;  // Stop Timer2 by clearing the prescaler bits
    TCCR2A = 0;  // Reset the control registers
    PORTB &= ~(1 << LED_PIN);  // Turn off the LED
}

void print_menu(void) {
    UART_SendString("\r\nMENU:\n");
    UART_SendString("\r0. Exit\n");
    UART_SendString("\r1. Print lowercase alphabet\n");
    UART_SendString("\r2. Print uppercase alphabet\n");
    UART_SendString("\r3. Blink LED 3 times\n");
    UART_SendString("\r4. Start 2Hz Signal/light intensity\n");
    UART_SendString("\r5. Stop Timer\n");
    UART_SendString("\r6. Brightness Control\n");
	UART_SendString("\r7. Get temperature\n");
	//UART_SendString("\r8. Get Light Intensity\n");

    UART_SendString("\rChoose an option: \n");
}

void brightness_menu(void) {
    UART_SendString("\r\nBrightness Menu:\n");
    UART_SendString("\r To increase Brightness press +\n");
    UART_SendString("\r To decrease Brightness press -\n");
    UART_SendString("\r Back to Main Menu\n");
    UART_SendString("\rChoose an option: \n");
}

void UART_init(uint16_t Baudrate) {
    uint16_t ubrr = F_CPU / 16 / Baudrate - 1;
    UBRR1H = (uint8_t)(ubrr >> 8);
    UBRR1L = (uint8_t)ubrr;
    UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
    UCSR1B = (1 << RXEN1) | (1 << TXEN1);
}

void UART_SendChar(uint8_t data) {
    while (!(UCSR1A & (1 << UDRE1)));
    UDR1 = data;
}

void UART_SendString(char *text) {
    while (*text != '\0') {
        UART_SendChar(*text);
        text++;
    }
}

uint8_t UART_GetChar(void) {
    while (!(UCSR1A & (1 << RXC1)));  // Wait for the reception of data
    return UDR1;  // Return received character
}

void LED_blink(void) {
    DDRB |= (1 << LED_PIN);
    for (int i = 0; i < 3; i++) {
        PORTB |= (1 << LED_PIN);
        _delay_ms(500);
        PORTB &= ~(1 << LED_PIN);
        _delay_ms(500);
    }
}

void print_temperature(void) {
	float temperature = at30_readTemp();  // Get the temperature from the sensor

	// Convert the temperature to a string
	char tempStr[20];
	snprintf(tempStr, sizeof(tempStr), "Temperature: %.2f C\n", temperature);

	// Send the temperature string via UART
	UART_SendString(tempStr);
}

int main(void) {
	UART_init(38400);  // Initialize UART with 38400 baud rate
	DDRB |= (1 << LED_PIN);  // Set LED pin (PB4) as output for PWM control
	ADC_Init(7, 1);  // Initialize ADC with a prescaler of 7 and reference AVCC
	print_menu();  // Print the main menu

	while (1) {
		uint8_t userInput = UART_GetChar();  // Get user input

		if (userInput == '0') {
			UART_SendString("Exiting...\n");
			break;  // Exit the loop and end the program
		} else if (userInput == '1') {
			for (char ch = 'a'; ch <= 'z'; ch++) {
				UART_SendChar(ch);  // Print lowercase alphabet
			}
			UART_SendString("\n");
		} else if (userInput == '2') {
			for (char ch = 'A'; ch <= 'Z'; ch++) {
				UART_SendChar(ch);  // Print uppercase alphabet
			}
			UART_SendString("\n");
		} else if (userInput == '3') {
			LED_blink();  // Blink the LED 3 times
		} else if (userInput == '4') {
            UART_SendString("\rChoose an option:\n");
            UART_SendString("\r1. Start 2Hz Signal\n");
            UART_SendString("\r2. Get Light Intensity\n");

            uint8_t subMenuChoice = UART_GetChar();  // Get user choice from sub-menu

            if (subMenuChoice == '1') {
                UART_SendString("Starting 2Hz signal...\n");
                // Timer1 code for 2Hz signal (you can implement Timer1 for 2Hz here)
            } else if (subMenuChoice == '2') {
                UART_SendString("Getting Light Intensity...\n");
                print_ADC_value();  // Get and print the light intensity from the photo-transistor
            } else {
                UART_SendString("Invalid input, returning to main menu...\n");
            }
		} else if (userInput == '5') {
			UART_SendString("Stopping Timer...\n");
			// Stop the timer for 2Hz signal (not relevant for brightness control)
		} else if (userInput == '6') {
			brightness_menu();  // Enter brightness control menu
			while (1) {
				uint8_t brightnessInput = UART_GetChar();  // Get user input for brightness

				if (brightnessInput == '+') {
					if (brightness < 100) {
						brightness += 10;  // Increase brightness
						UART_SendString("Brightness Increased\n");
					}
				} else if (brightnessInput == '-') {
					if (brightness > 0) {
						brightness -= 10;  // Decrease brightness
						UART_SendString("Brightness Decreased\n");
					}
				} else if (brightnessInput == 'q') {
					break;  // Exit the brightness menu
				} else {
					UART_SendString("Invalid input, try again.\n");
				}

				Timer2_fastpwm_start(brightness);  // Update the LED brightness based on current level
				brightness_menu();  // Show brightness menu again
			}
		} else if (userInput == '7') {
			print_temperature();  // Print the current temperature
		} else {
			UART_SendString("Invalid input, try again.\n");  // Invalid option handling
		}

		print_menu();  // Display the main menu again
	}

	return 0;  // Program ends
}