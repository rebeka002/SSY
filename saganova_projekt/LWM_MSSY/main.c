
#include <avr/io.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "config.h"
#include "hal.h"
#include "phy.h"
#include "sys.h"
#include "nwk.h"
#include "sysTimer.h"
#include "halBoard.h"
#include "halUart.h"
#include "main.h"
#include <avr/io.h>
#include <avr/eeprom.h>

/*- Definitions ------------------------------------------------------------*/
#ifdef NWK_ENABLE_SECURITY
#define APP_BUFFER_SIZE     (NWK_MAX_PAYLOAD_SIZE - NWK_SECURITY_MIC_SIZE)
#else
#define APP_BUFFER_SIZE     NWK_MAX_PAYLOAD_SIZE
uint8_t APP_ADDR = 0;
#endif

// PSK verification defines
#define PSK_MAGIC_ADDRESS   0x40    // Address in EEPROM for magic bytes
#define PSK_MAGIC_VALUE     0xA5    // Expected magic byte value
#define PSK_ADDRESS         0x00    // Start address for PSK in EEPROM
#define PSK_LENGTH          32      // 256-bit key length
#define PSK_DEBUG_MODE      1       // Set to 1 to enable PSK debugging

// Operating mode defines
#define MODE_UNDEFINED      0       // Initial state
#define MODE_SENDER         1       // Device will primarily send messages
#define MODE_LISTENER       2       // Device will primarily listen for messages

// Nonce header size - for synchronization
#define NONCE_HEADER_SIZE   8       // Size of nonce header in transmitted messages

static const uint8_t FIXED_ENCRYPTION_KEY[PSK_LENGTH] = {
    0xA7, 0xF1, 0xD9, 0x2A, 0x82, 0xC8, 0xD8, 0xFE,
    0x43, 0x4D, 0x98, 0x55, 0x8C, 0xE2, 0xB3, 0x47,
    0x17, 0x11, 0x98, 0x54, 0x2F, 0x11, 0x2D, 0x05,
    0x58, 0xF5, 0x6B, 0xD6, 0x88, 0x07, 0x99, 0x92
};

/*- Types ------------------------------------------------------------------*/
typedef enum AppState_t
{
	APP_STATE_INITIAL,
	APP_STATE_IDLE,
	APP_STATE_MODE_SELECTION,
	APP_STATE_OPERATING
} AppState_t;

typedef enum PskState_t
{
	PSK_STATE_INVALID,      // PSK is invalid or uninitialized
	PSK_STATE_VALID,        // PSK is valid and loaded
	PSK_STATE_ERROR         // Error during PSK loading
} PskState_t;

struct salsa20_ctx {
	uint32_t input[16];
};

/*- Prototypes -------------------------------------------------------------*/
static void appSendData(void);
static void increment_nonce(uint8_t *nonce);
static PskState_t load_psk_from_eeprom(uint8_t *key);
static PskState_t verify_psk(uint8_t *key);
static void print_psk(uint8_t *key);
static void print_debug(const char *message);
static void print_debug_hex(const char *prefix, uint8_t *data, uint8_t len);
static void print_char_array(const char *str);
static void prompt_mode_selection(void);
static void handle_mode_selection(uint8_t byte);
static void display_mode_status(void);

/*- Variables --------------------------------------------------------------*/
static AppState_t appState = APP_STATE_INITIAL;
static SYS_Timer_t appTimer;
static NWK_DataReq_t appDataReq;
static bool appDataReqBusy = false;
static uint8_t appDataReqBuffer[APP_BUFFER_SIZE];
static uint8_t appUartBuffer[APP_BUFFER_SIZE - NONCE_HEADER_SIZE]; // Reduced size to accommodate nonce in transmission
static uint8_t appUartBufferPtr = 0;
static uint8_t appOperatingMode = MODE_UNDEFINED;

// Global variables for encryption
static uint8_t app_encryption_key[PSK_LENGTH];  // 256-bit key (loaded from EEPROM)
static uint8_t current_nonce[8] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};  // 64-bit nonce (should be unique for each message)
static PskState_t pskState = PSK_STATE_INVALID;

/*- Implementations --------------------------------------------------------*/

static void print_char_array(const char *str) {
    const char *p = str;
    while (*p != '\0') {
        HAL_UartWriteByte(*p++);
        
        // Ensure buffer is flushed more frequently
        if (*(p-1) == '\n' || *(p-1) == '\r' || ((p - str) % 20 == 0)) {
            HAL_UartTaskHandler();
            // Small delay to ensure transmission completes
            for (volatile uint8_t i = 0; i < 5; i++);
        }
    }
    
    // Final flush to ensure all bytes are transmitted
    HAL_UartTaskHandler();
    // Small delay after transmission
    for (volatile uint16_t i = 0; i < 100; i++);
}

static void print_debug(const char *message) {
	if (PSK_DEBUG_MODE) {
		print_char_array(message);
	}
}

static void print_debug_hex(const char *prefix, uint8_t *data, uint8_t len) {
	if (PSK_DEBUG_MODE) {
		print_char_array(prefix);
		
		char hex_byte[3];
		for (uint8_t i = 0; i < len; i++) {
			snprintf(hex_byte, sizeof(hex_byte), "%02X", data[i]);
			HAL_UartWriteByte(hex_byte[0]);
			HAL_UartWriteByte(hex_byte[1]);
			
			// Add space every 4 bytes for readability
			if ((i + 1) % 4 == 0 && i < len - 1) {
				HAL_UartWriteByte(' ');
			}
		}
		
		print_char_array("\r\n");
	}
}
static PskState_t load_psk_from_eeprom(uint8_t *key)
{
    uint8_t magic_byte = eeprom_read_byte((uint8_t *)(PSK_MAGIC_ADDRESS));
    print_debug_hex("[PSK] Magic byte (read): ", &magic_byte, 1);

    if (magic_byte != PSK_MAGIC_VALUE) {
        print_debug("[PSK] Magic byte verification failed!\r\n");
        return PSK_STATE_INVALID;
    }

    uint8_t checksum = 0;
    for (uint8_t i = 0; i < PSK_LENGTH; i++) {
        key[i] = eeprom_read_byte((uint8_t *)(PSK_ADDRESS + i));
        checksum ^= key[i];
    }
    print_debug_hex("[PSK] Loaded key: ", key, PSK_LENGTH); // Print loaded key

    uint8_t stored_checksum = eeprom_read_byte((uint8_t *)(PSK_ADDRESS + PSK_LENGTH));
    print_debug_hex("[PSK] Calculated checksum: ", &checksum, 1);
    print_debug_hex("[PSK] Stored checksum: ", &stored_checksum, 1);

    if (checksum != stored_checksum) {
        print_debug("[PSK] ERROR: Checksum verification failed!\r\n");
        return PSK_STATE_ERROR;
    }

    print_debug("[PSK] PSK loaded successfully!\r\n");
    return PSK_STATE_VALID;
}

static PskState_t verify_psk(uint8_t *key)
{
	
	bool all_zeros = true;
	bool all_ff = true;
	
	for (uint8_t i = 0; i < PSK_LENGTH; i++) {
		if (key[i] != 0x00) all_zeros = false;
		if (key[i] != 0xFF) all_ff = false;
	}
	
	if (all_zeros) {
		print_debug("[PSK] ERROR: Key appears to be all zeros!\r\n");
		return PSK_STATE_INVALID;
	}
	
	if (all_ff) {
		print_debug("[PSK] ERROR: Key appears to be all 0xFF (uninitialized EEPROM)!\r\n");
		return PSK_STATE_INVALID;
	}
	
	uint8_t checksum = 0;
	for (uint8_t i = 0; i < PSK_LENGTH; i++) {
		checksum ^= key[i];
	}
	
	uint8_t stored_checksum = eeprom_read_byte((uint8_t *)(PSK_ADDRESS + PSK_LENGTH));
	
	if (checksum != stored_checksum) {
		print_debug("[PSK] ERROR: Checksum verification failed!\r\n");
		return PSK_STATE_ERROR;
	}
	
	print_debug("[PSK] Verification successful!\r\n");
	return PSK_STATE_VALID;
}


static void store_psk_to_eeprom(uint8_t *key)
{
//    print_debug_hex("[PSK] Key to store: ", key, PSK_LENGTH); // Print key before storing

    for (uint8_t i = 0; i < PSK_LENGTH; i++) {
        eeprom_write_byte((uint8_t *)(PSK_ADDRESS + i), key[i]);
    }

    uint8_t checksum = 0;
    for (uint8_t i = 0; i < PSK_LENGTH; i++) {
        checksum ^= key[i];
    }

    eeprom_write_byte((uint8_t *)(PSK_ADDRESS + PSK_LENGTH), checksum);

    eeprom_write_byte((uint8_t *)(PSK_MAGIC_ADDRESS), PSK_MAGIC_VALUE);
 

    print_debug("[PSK] Fixed PSK stored successfully to EEPROM!\r\n");
}

static void initialize_psk(void)
{
    memcpy(app_encryption_key, FIXED_ENCRYPTION_KEY, PSK_LENGTH);
    print_debug_hex("[PSK] Initialized key: ", app_encryption_key, PSK_LENGTH); // Print initialized key

    store_psk_to_eeprom(app_encryption_key);

    pskState = PSK_STATE_VALID;
    print_debug("[PSK] PSK initialization successful!\r\n");
}
static void print_psk(uint8_t *key)
{
	print_debug_hex("[PSK] Key: ", key, PSK_LENGTH);
}

static void display_mode_status(void)
{
	print_char_array("\r\n----- Current Operating Mode -----\r\n");
	
	if (appOperatingMode == MODE_SENDER) {
		print_char_array("Mode: SENDER - This device will send messages\r\n");
		print_char_array("Enter message to encrypt: ");
		} else if (appOperatingMode == MODE_LISTENER) {
		print_char_array("Mode: LISTENER - This device is waiting for messages\r\n");
		print_char_array("(Press 'M' to change mode)\r\n");
		} else {
		print_char_array("Mode: UNDEFINED\r\n");
	}
}

static void prompt_mode_selection(void)
{
	print_char_array("\nSelect operating mode:");
	print_char_array("\n\r 1) Sender \n");
	print_char_array("\n\r 2) Listener \n");
	HAL_UartTaskHandler();
}

static void handle_mode_selection(uint8_t byte)
{if (byte == '1') {
	appOperatingMode = MODE_SENDER;
	APP_ADDR = 1; // Sender is address 1
	print_char_array("\r\nMode set to SENDER with address 1\r\n");
	appState = APP_STATE_OPERATING;
	
	// Need to update network address when mode is changed
	NWK_SetAddr(APP_ADDR);
	
	display_mode_status();
	} else if (byte == '2') {
	appOperatingMode = MODE_LISTENER;
	APP_ADDR = 0; // Listener is address 0
	print_char_array("\r\nMode set to LISTENER with address 0\r\n");
	appState = APP_STATE_OPERATING;
	
	// Need to update network address when mode is changed
	NWK_SetAddr(APP_ADDR);
	
	display_mode_status();
	} else if (byte == '\r' || byte == '\n') {
	// Ignore CR/LF
	} else {
	// Invalid input
	print_char_array("\r\nInvalid choice. Please enter 1 or 2: ");
}

}

static void appDataConf(NWK_DataReq_t *req)
{
	appDataReqBusy = false;
	(void)req;
	
	if (appOperatingMode == MODE_SENDER) {
		print_char_array("\r\nEnter message to encrypt (or 'M' to change mode): ");
	}
}
static void appSendData(void)
{
    if (appOperatingMode == MODE_LISTENER || appDataReqBusy || 0 == appUartBufferPtr)
        return;
    
    if (pskState != PSK_STATE_VALID) {
        print_debug("[ERROR] Cannot send data - PSK not valid!\r\n");
        appUartBufferPtr = 0;
        return;
    }

    // Copy the current nonce into the message header
    memcpy(appDataReqBuffer, current_nonce, NONCE_HEADER_SIZE);
    
    if (PSK_DEBUG_MODE) {
        print_debug_hex("[ENCRYPT] Current nonce: ", current_nonce, 8);
    }
    
    struct salsa20_ctx encrypt_ctx;
    salsa20_keysetup(&encrypt_ctx, app_encryption_key, 256);
    salsa20_ivsetup(&encrypt_ctx, current_nonce);
    
    // Encrypt the message data
    salsa20_encrypt_bytes(&encrypt_ctx, appUartBuffer, appDataReqBuffer + NONCE_HEADER_SIZE, appUartBufferPtr);
    
    if (PSK_DEBUG_MODE) {
        print_debug_hex("[ENCRYPT] Ciphertext: ", appDataReqBuffer + NONCE_HEADER_SIZE, appUartBufferPtr);
    }
    
    // Increment the nonce for next message
    increment_nonce(current_nonce);
    
    // Set the destination address based on our address
    appDataReq.dstAddr = (APP_ADDR == 1) ? 0 : 1;
    
    appDataReq.dstEndpoint = APP_ENDPOINT;
    appDataReq.srcEndpoint = APP_ENDPOINT;
    appDataReq.options = NWK_OPT_ENABLE_SECURITY;
    appDataReq.data = appDataReqBuffer;
    appDataReq.size = appUartBufferPtr + NONCE_HEADER_SIZE; // Add nonce size to total size
    appDataReq.confirm = appDataConf;
    NWK_DataReq(&appDataReq);

    appUartBufferPtr = 0;
    appDataReqBusy = true;
}

void HAL_UartBytesReceived(uint16_t bytes)
{
    for (uint16_t i = 0; i < bytes; i++)
    {
        uint8_t byte = HAL_UartReadByte();

        // Echo back what was typed, but only if it's a displayable character
        if (byte >= 32 && byte <= 126) {
            HAL_UartWriteByte(byte);
            HAL_UartTaskHandler(); // Ensure character is transmitted
        } else if (byte == '\r' || byte == '\n') {
            // For newlines, display both CR and LF for proper line handling
            HAL_UartWriteByte('\r');
            HAL_UartWriteByte('\n');
            HAL_UartTaskHandler();
        }
        
        if (appState == APP_STATE_MODE_SELECTION) {
            handle_mode_selection(byte);
            continue;
        }
        else if (appState == APP_STATE_OPERATING) {
            if ((byte == 'M' || byte == 'm') && appUartBufferPtr == 0) {
                print_char_array("\r\nChanging operating mode...\r\n");
                appState = APP_STATE_MODE_SELECTION;
                prompt_mode_selection();
                continue;
            }
            
            if (appOperatingMode == MODE_LISTENER) {
                if (byte == '\r' || byte == '\n') {
                    display_mode_status();
                }
                continue;
            }
            else if (appOperatingMode == MODE_SENDER) {
                if (byte == '\r' || byte == '\n') {
                    // Add a null terminator to ensure proper string handling
                    if (appUartBufferPtr < sizeof(appUartBuffer)) {
                        appUartBuffer[appUartBufferPtr] = '\0';
                    }
                    appSendData();
                    continue;
                }

                // Only accept printable characters for transmission
                if (byte >= 32 && byte <= 126 && appUartBufferPtr < sizeof(appUartBuffer)) {
                    appUartBuffer[appUartBufferPtr++] = byte;
                }
                
                if (appUartBufferPtr == sizeof(appUartBuffer)) {
                    // Add a null terminator to ensure proper string handling
                    if (appUartBufferPtr < sizeof(appUartBuffer)) {
                        appUartBuffer[appUartBufferPtr] = '\0';
                    }
                    appSendData();
                }
            }
        }
    }
}

static void appTimerHandler(SYS_Timer_t *timer)
{
	appSendData();
	(void)timer;
}
static bool appDataInd(NWK_DataInd_t *ind)
{
    if (pskState != PSK_STATE_VALID) {
        print_debug("[ERROR] Cannot decrypt data - PSK not valid!\r\n");
        return false;
    }
    
    if (ind->size <= NONCE_HEADER_SIZE) {
        print_debug("[ERROR] Received data too short - missing nonce!\r\n");
        return false;
    }
    
    uint8_t message_nonce[8];
    memcpy(message_nonce, ind->data, NONCE_HEADER_SIZE);
    
    uint8_t decrypted_data[APP_BUFFER_SIZE];
    memset(decrypted_data, 0, APP_BUFFER_SIZE); // Clear buffer before decryption
    
    if (PSK_DEBUG_MODE) {
        print_debug_hex("[DECRYPT] Received message with nonce: ", message_nonce, 8);
        print_debug_hex("[DECRYPT] Ciphertext: ", ind->data + NONCE_HEADER_SIZE, ind->size - NONCE_HEADER_SIZE);
    }
    
    struct salsa20_ctx decrypt_ctx;
    salsa20_keysetup(&decrypt_ctx, app_encryption_key, 256);
    salsa20_ivsetup(&decrypt_ctx, message_nonce);
    
    salsa20_encrypt_bytes(&decrypt_ctx,
                          ind->data + NONCE_HEADER_SIZE,
                          decrypted_data,
                          ind->size - NONCE_HEADER_SIZE);
    
    if (PSK_DEBUG_MODE) {
      //  print_debug_hex("[DECRYPT] Decrypted plaintext: ", decrypted_data, ind->size - NONCE_HEADER_SIZE);
    }

    // Ensure the decrypted data is null-terminated
    decrypted_data[ind->size - NONCE_HEADER_SIZE] = '\0';

    print_char_array("\r\n[MESSAGE RECEIVED] ");
    
    // Output each byte as a printable character
    for (uint8_t i = 0; i < ind->size - NONCE_HEADER_SIZE; i++) {
        // Only print displayable ASCII characters
        if (decrypted_data[i] >= 32 && decrypted_data[i] <= 126) {
            HAL_UartWriteByte(decrypted_data[i]);
        } else {
            // For non-displayable characters, print a placeholder
            HAL_UartWriteByte('.');
        }
        // Ensure each character is transmitted
        HAL_UartTaskHandler();
    }
    
    print_char_array("\r\n");

    // Update nonce if needed
    bool update_local_nonce = false;
    for (uint8_t i = 0; i < 8; i++) {
        if (message_nonce[i] > current_nonce[i]) {
            update_local_nonce = true;
            break;
        }
        else if (message_nonce[i] < current_nonce[i]) {
            break;
        }
    }
    
    if (update_local_nonce) {
        if (PSK_DEBUG_MODE) {
            print_debug("[NONCE] Updating local nonce to match received nonce\r\n");
        }
        memcpy(current_nonce, message_nonce, 8);
        increment_nonce(current_nonce); // Increment for our next send
    }

    if (appOperatingMode == MODE_SENDER) {
        print_char_array("\r\nEnter message to encrypt (or 'M' to change mode): ");
    } else if (appOperatingMode == MODE_LISTENER) {
        print_char_array("\r\nListening for messages... (Press 'M' to change mode)\r\n");
    }
    
    return true;
}
static void appInit(void)
{

    pskState = load_psk_from_eeprom(app_encryption_key);
    
    if (pskState == PSK_STATE_INVALID) {
        initialize_psk(); // Ak PSK nie je platný, inicializujeme ho
    }
    
    // Network initialization
    NWK_SetAddr(APP_ADDR);
    NWK_SetPanId(APP_PANID);
    PHY_SetChannel(APP_CHANNEL);
    #ifdef PHY_AT86RF212
    PHY_SetBand(APP_BAND);
    PHY_SetModulation(APP_MODULATION);
    #endif
    PHY_SetRxState(true);

    NWK_OpenEndpoint(APP_ENDPOINT, appDataInd);
    
    // Set up timer
    appTimer.interval = 5000;
    appTimer.mode = SYS_TIMER_INTERVAL_MODE;
    appTimer.handler = appTimerHandler;
    
    // Restore debug mode setting
    #undef PSK_DEBUG_MODE
}
static void APP_TaskHandler(void)
{
    switch (appState)
    {
        case APP_STATE_INITIAL:
            print_char_array("\r\n[INIT] Starting application...\r\n");

            // Stav PSK bol už zistený v appInit()
            appState = APP_STATE_MODE_SELECTION;
            prompt_mode_selection();

            char addr_buf[50];
            //snprintf(addr_buf, sizeof(addr_buf), "[DEBUG] Network initialized with address: %d, PAN ID: %d\r\n",
                   // APP_ADDR, APP_PANID);
            print_char_array(addr_buf);
            break;

        // Ostatné stavy zostávajú bez zmeny
        case APP_STATE_MODE_SELECTION:
            break;

        case APP_STATE_OPERATING:
            if (appOperatingMode == MODE_SENDER) {
                // Sender mode handling
            } else if (appOperatingMode == MODE_LISTENER) {
                // Listener mode handling
            } else {
                print_char_array("\r\n[ERROR] Operating mode undefined!\r\n");
                appState = APP_STATE_MODE_SELECTION;
                prompt_mode_selection();
            }
            break;

        default:
            print_char_array("\r\n[ERROR] Invalid appState!\r\n");
            break;
    }
}
static void increment_nonce(uint8_t *nonce) {
	for (uint8_t i = 0; i < 8; i++) {
		nonce[i]++;
		if (nonce[i] != 0) break;
	}
}

static uint32_t rotl32(uint32_t x, int k)
{
	return ((x << k) | (x >> (32 - k)));
}

static uint32_t load_littleendian(const uint8_t *x)
{
	return ((uint32_t)x[0]) |
	((uint32_t)x[1] << 8) |
	((uint32_t)x[2] << 16) |
	((uint32_t)x[3] << 24);
}

static void store_littleendian(uint8_t *x, uint32_t u)
{
	x[0] = u;
	x[1] = u >> 8;
	x[2] = u >> 16;
	x[3] = u >> 24;
}

void salsa20_keysetup(struct salsa20_ctx *ctx, const uint8_t *k, uint32_t keybits)
{
	const uint8_t *sigma = (uint8_t *)"expand 32-byte k";
	const uint8_t *tau = (uint8_t *)"expand 16-byte k";
	const uint8_t *constants = (keybits == 256) ? sigma : tau;

	ctx->input[0] = load_littleendian(constants + 0);
	ctx->input[5] = load_littleendian(constants + 4);
	ctx->input[10] = load_littleendian(constants + 8);
	ctx->input[15] = load_littleendian(constants + 12);

	ctx->input[1] = load_littleendian(k + 0);
	ctx->input[2] = load_littleendian(k + 4);
	ctx->input[3] = load_littleendian(k + 8);
	ctx->input[4] = load_littleendian(k + 12);

	if (keybits == 256) {
		k += 16;
		ctx->input[11] = load_littleendian(k + 0);
		ctx->input[12] = load_littleendian(k + 4);
		ctx->input[13] = load_littleendian(k + 8);
		ctx->input[14] = load_littleendian(k + 12);
		} else {
		ctx->input[11] = ctx->input[1];
		ctx->input[12] = ctx->input[2];
		ctx->input[13] = ctx->input[3];
		ctx->input[14] = ctx->input[4];
	}

	ctx->input[8] = 0;
	ctx->input[9] = 0;
}

void salsa20_ivsetup(struct salsa20_ctx *ctx, const uint8_t *iv)
{
	ctx->input[6] = load_littleendian(iv + 0);
	ctx->input[7] = load_littleendian(iv + 4);
	ctx->input[8] = 0;
	ctx->input[9] = 0;
}
static void salsa20_block(const struct salsa20_ctx *ctx, uint8_t *out)
{
    uint32_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
    uint32_t j0, j1, j2, j3, j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14, j15;

    // Load input state
    j0 = x0 = ctx->input[0];
    j1 = x1 = ctx->input[1];
    j2 = x2 = ctx->input[2];
    j3 = x3 = ctx->input[3];
    j4 = x4 = ctx->input[4];
    j5 = x5 = ctx->input[5];
    j6 = x6 = ctx->input[6];
    j7 = x7 = ctx->input[7];
    j8 = x8 = ctx->input[8];
    j9 = x9 = ctx->input[9];
    j10 = x10 = ctx->input[10];
    j11 = x11 = ctx->input[11];
    j12 = x12 = ctx->input[12];
    j13 = x13 = ctx->input[13];
    j14 = x14 = ctx->input[14];
    j15 = x15 = ctx->input[15];

    // 10 double rounds = 20 rounds
    for (int i = 0; i < 10; i++) {
        // Column round
        x4 ^= rotl32(x0 + x12, 7);
        x8 ^= rotl32(x4 + x0, 9);
        x12 ^= rotl32(x8 + x4, 13);
        x0 ^= rotl32(x12 + x8, 18);

        x9 ^= rotl32(x5 + x1, 7);
        x13 ^= rotl32(x9 + x5, 9);
        x1 ^= rotl32(x13 + x9, 13);
        x5 ^= rotl32(x1 + x13, 18);

        x14 ^= rotl32(x10 + x6, 7);
        x2 ^= rotl32(x14 + x10, 9);
        x6 ^= rotl32(x2 + x14, 13);
        x10 ^= rotl32(x6 + x2, 18);

        x3 ^= rotl32(x15 + x11, 7);
        x7 ^= rotl32(x3 + x15, 9);
        x11 ^= rotl32(x7 + x3, 13);
        x15 ^= rotl32(x11 + x7, 18);

        // Row round
        x1 ^= rotl32(x0 + x3, 7);
        x2 ^= rotl32(x1 + x0, 9);
        x3 ^= rotl32(x2 + x1, 13);
        x0 ^= rotl32(x3 + x2, 18);

        x6 ^= rotl32(x5 + x4, 7);
        x7 ^= rotl32(x6 + x5, 9);
        x4 ^= rotl32(x7 + x6, 13);
        x5 ^= rotl32(x4 + x7, 18);

        x11 ^= rotl32(x10 + x9, 7);
        x8 ^= rotl32(x11 + x10, 9);
        x9 ^= rotl32(x8 + x11, 13);
        x10 ^= rotl32(x9 + x8, 18);

        x12 ^= rotl32(x15 + x14, 7);
        x13 ^= rotl32(x12 + x15, 9);
        x14 ^= rotl32(x13 + x12, 13);
        x15 ^= rotl32(x14 + x13, 18);
    }

    // Add input state to output
    x0 += j0;
    x1 += j1;
    x2 += j2;
    x3 += j3;
    x4 += j4;
    x5 += j5;
    x6 += j6;
    x7 += j7;
    x8 += j8;
    x9 += j9;
    x10 += j10;
    x11 += j11;
    x12 += j12;
    x13 += j13;
    x14 += j14;
    x15 += j15;

    // Store output
    store_littleendian(out + 0, x0);
    store_littleendian(out + 4, x1);
    store_littleendian(out + 8, x2);
    store_littleendian(out + 12, x3);
    store_littleendian(out + 16, x4);
    store_littleendian(out + 20, x5);
    store_littleendian(out + 24, x6);
    store_littleendian(out + 28, x7);
    store_littleendian(out + 32, x8);
    store_littleendian(out + 36, x9);
    store_littleendian(out + 40, x10);
    store_littleendian(out + 44, x11);
    store_littleendian(out + 48, x12);
    store_littleendian(out + 52, x13);
    store_littleendian(out + 56, x14);
    store_littleendian(out + 60, x15);
}

void salsa20_encrypt_bytes(struct salsa20_ctx *ctx, const uint8_t *in, uint8_t *out, size_t len)
{
	size_t i;
	uint8_t block[64];

	while (len > 0) {
		salsa20_block(ctx, block);

		size_t chunk = (len < 64) ? len : 64;
		for (i = 0; i < chunk; i++)
		out[i] = in[i] ^ block[i];

		in += chunk;
		out += chunk;
		len -= chunk;
	}
}


int main(void)
{
    SYS_Init();
    
    HAL_UartInit(38400);
    for (volatile uint16_t i = 0; i < 1000; i++) {
        HAL_UartTaskHandler();
    }
    
    print_char_array("\r\n----- LWM_MSSY with Salsa20 E2E Encryption -----\r\n");
    print_char_array("Debug mode: ");
    print_char_array("\r\n");
    print_char_array("Initializing...\r\n");
    
    HAL_UartTaskHandler();
    
    appInit();
    
    while (1)
    {
        SYS_TaskHandler();
        HAL_UartTaskHandler();
        APP_TaskHandler();
    }
}