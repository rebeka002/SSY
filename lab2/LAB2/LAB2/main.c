#include <avr/io.h>
#include <avr/interrupt.h>  // For interrupts
#include <util/delay.h> // For delay functions
#include "makra.h"

// DEFINES -------------------------------------------------------------------
#define LED_PIN PD5       // Pin for LED (Change as needed)
#define F_CPU 8000000UL

// FUNCTIONS -------------------------------------------------------------------
void UART_init(uint16_t Baudrate);
void UART_SendChar(uint8_t data);
void UART_SendString(char *text);
uint8_t UART_GetChar(void);
void LED_blink(void);

//-------------------------------------------------------------------
uint8_t UART_GetChar(void) {
    while (!(UCSR1A & (1 << RXC1)));  // Wait for the reception of data
    return UDR1;  // Return received character
}

void LED_blink(void) {
    DDRB |= (1 << LED_PIN);  // Set PD5 as output (correct the pin)

    for (int i = 0; i < 3; i++) {
        PORTB |= (1 << LED_PIN);  // Turn LED on (set PD5 high)
        _delay_ms(500);           // Wait 500 ms
        PORTB &= ~(1 << LED_PIN); // Turn LED off (set PD5 low)
        _delay_ms(500);           // Wait 500 ms
    }

    // Optional: You can set the pin back to input if you want to disable the output
    // cbi(DDRD, LED_PIN);  // Reset pin to input
}

ISR(USART1_RX_vect) {
    // Interrupt Service Routine for UART Receive
    uint8_t receivedChar = UDR1;  // Read received character
    UART_SendChar(receivedChar);   // Echo received character
}

void UART_init(uint16_t Baudrate) {
    // Výpo?et hodnoty pro nastavení baudrate
    uint16_t ubrr = F_CPU / 16 / Baudrate - 1;

    // Nastavení Baud rate registru (UBRR0H a UBRR0L pro UART0)
    UBRR1H = (uint8_t)(ubrr >> 8);  // Horní 8 bit?
    UBRR1L = (uint8_t)ubrr;         // Dolní 8 bit?

    // Nastavení parametr?: 8 datových bit?, 1 stop bit, bez parity
    UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);  // 8-bitový formát
    UCSR1B = (1 << RXEN1) | (1 << TXEN1);    // Povolení p?íjmu a vysílání
}

void UART_SendChar(uint8_t data) {
    // ?ekáme, dokud není UART p?ipraven k odeslání dat (TXC bit)
    while (!(UCSR1A & (1 << UDRE1)));  // ?ekání na prázdný registr
    UDR1 = data;  // Odeslání dat
}

void UART_SendString(char *text) {
    while (*text != '\0') {  // Procházíme ?et?zec, dokud nenarazíme na '\0'
        UART_SendChar(*text);  // Odeslání aktuálního znaku
        text++;  // Posun na další znak v ?et?zci
    }
}

// Function to clear the screen using ANSI escape sequence
void clear_screen(void) {
    UART_SendString("\x1b[2J");  // ANSI escape sequence for clearing screen
    UART_SendString("\x1b[H");   // Move cursor to top-left corner
}

// Function to set text color to pink (magenta) using ANSI escape sequence
void set_text_color_pink(void) {
    UART_SendString("\x1b[35m");  // Set text color to magenta (pink)
}

// Function to reset text color back to default using ANSI escape sequence
void reset_text_color(void) {
    UART_SendString("\x1b[0m");  // Reset to default text color
}

void print_menu(void) {
    set_text_color_pink();  // Set text color to pink (magenta)
    UART_SendString("\n\r");
    UART_SendString("MENU:\n\r");
    UART_SendString("0. Exit\n\r");
    UART_SendString("1. Print lowercase alphabet\n\r");
    UART_SendString("2. Print uppercase alphabet\n\r");
    UART_SendString("3. Blink LED 3 times\n\r");
    UART_SendString("Choose an option (0-3): \n\r");
    reset_text_color();  // Reset text color to default
}

int main(void) {
    UART_init(38400);  // Initialize UART with baud rate 38400
    clear_screen();  // Clear the screen initially
    print_menu();  // Display the menu

    while (1) {
        uint8_t userInput = UART_GetChar();  // Get user input

        // Respond based on input
        if (userInput == '0') {
            // Exit the program
            set_text_color_pink();  // Set text color to pink
            UART_SendString("Exiting program...\n");
            reset_text_color();  // Reset to default color
            break;
        } else if (userInput == '1') {
            // Print lowercase alphabet
            clear_screen();  // Clear the screen before printing
            set_text_color_pink();  // Set text color to pink
            for (char ch = 'a'; ch <= 'z'; ch++) {
                UART_SendChar(ch);
            }
            UART_SendString("\n");
            reset_text_color();  // Reset text color to default
        } else if (userInput == '2') {
            // Print uppercase alphabet
            clear_screen();  // Clear the screen before printing
            set_text_color_pink();  // Set text color to pink
            for (char ch = 'A'; ch <= 'Z'; ch++) {
                UART_SendChar(ch);
            }
            UART_SendString("\n");
            reset_text_color();  // Reset text color to default
        } else if (userInput == '3') {
            // Blink LED 3 times
            clear_screen();  // Clear the screen before blinking LED
            LED_blink();
        } else {
            // Invalid input
            set_text_color_pink();  // Set text color to pink
            UART_SendString("Invalid choice. Please enter a value between 0-3.\n");
            reset_text_color();  // Reset text color to default
        }

        // After each action, display the menu again and wait for user input
        print_menu();
    }

    return 0;
}
