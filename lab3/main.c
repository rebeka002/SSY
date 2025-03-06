#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "makra.h"
#include "timer.h"

#define LED_PIN PB4  // Use PB4 for PWM (OC2A)
#define F_CPU 8000000UL
#define PRESCALE ((1 << CS12) | (1 << CS10)) // 1024 prescaler

void UART_init(uint16_t Baudrate);
void UART_SendChar(uint8_t data);
void UART_SendString(char *text);
uint8_t UART_GetChar(void);
void LED_blink(void);
void Timer2_fastpwm_start(uint16_t strida);
void Timer2_stop(void);
void print_menu(void);
void brightness_menu(void);

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
    UART_SendString("\r4. Start 2Hz Signal\n");
    UART_SendString("\r5. Stop Timer\n");
    UART_SendString("\r6. Brightness Control\n");
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

int main(void) {
    UART_init(38400);
    DDRB |= (1 << LED_PIN);  // Set LED pin as output (PB4 for PWM)
    print_menu();

    while (1) {
        uint8_t userInput = UART_GetChar();

        if (userInput == '0') {
            UART_SendString("Exiting...\n");
            break;
        } else if (userInput == '1') {
            for (char ch = 'a'; ch <= 'z'; ch++) {
                UART_SendChar(ch);
            }
            UART_SendString("\n");
        } else if (userInput == '2') {
            for (char ch = 'A'; ch <= 'Z'; ch++) {
                UART_SendChar(ch);
            }
            UART_SendString("\n");
        } else if (userInput == '3') {
            LED_blink();
        } else if (userInput == '4') {
            UART_SendString("Starting 2Hz signal...\n");
            // Timer1 code for 2Hz signal (not relevant for brightness)
        } else if (userInput == '5') {
            UART_SendString("Stopping Timer...\n");
            // Stop the timer for 2Hz signal (not relevant for brightness)
        } else if (userInput == '6') {
            brightness_menu();
            while (1) {
	            uint8_t brightnessInput = UART_GetChar();
	            
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

	            Timer2_fastpwm_start(brightness);  // Update the LED brightness
	            brightness_menu();  // Display the brightness menu again
            }
            
        } else {
            UART_SendString("Invalid input, try again.\n");
        }
        print_menu();
    }
    return 0;
}
