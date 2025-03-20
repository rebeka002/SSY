/*
 * LWM_MSSY.c
 *
 * Created: 6.4.2017 15:42:46
 * Author : Krajsa
 */ 

#include <avr/io.h>
/*- Includes ---------------------------------------------------------------*/
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

/*- Definitions ------------------------------------------------------------*/
#ifdef NWK_ENABLE_SECURITY
#define APP_BUFFER_SIZE     (NWK_MAX_PAYLOAD_SIZE - NWK_SECURITY_MIC_SIZE)
#else
#define APP_BUFFER_SIZE     NWK_MAX_PAYLOAD_SIZE
#endif

/*- Types ------------------------------------------------------------------*/
typedef enum AppState_t
{
	APP_STATE_INITIAL,
	APP_STATE_IDLE,
} AppState_t;

/*- Prototypes -------------------------------------------------------------*/
static void appSendData(void);

/*- Variables --------------------------------------------------------------*/
static AppState_t appState = APP_STATE_INITIAL;
static SYS_Timer_t appTimer;
static NWK_DataReq_t appDataReq;
static bool appDataReqBusy = false;
static uint8_t appDataReqBuffer[APP_BUFFER_SIZE];
static uint8_t appUartBuffer[APP_BUFFER_SIZE];
static uint8_t appUartBufferPtr = 0;
static SYS_Timer_t appTimer2;
static void appTimerHandler2(SYS_Timer_t *timer);

/*- Implementations --------------------------------------------------------*/

/*************************************************************************//**
*****************************************************************************/
static void appDataConf(NWK_DataReq_t *req)
{
appDataReqBusy = false;
(void)req;
}

/*************************************************************************//**
*****************************************************************************/
static void appSendData(void)
{
	// Prednastavená hodnota teploty 28 (v desiatkovej sústave)
	uint8_t temperature = 28;
	
	// Odošleme teplotu ako text (napr. "Temperature: 28")
	snprintf((char *)appDataReqBuffer, APP_BUFFER_SIZE, "Temperature: %d", temperature);

	if (appDataReqBusy || 0 == strlen((char *)appDataReqBuffer))
	return;

	appDataReq.dstAddr = 1 - APP_ADDR;
	appDataReq.dstEndpoint = APP_ENDPOINT;
	appDataReq.srcEndpoint = APP_ENDPOINT;
	appDataReq.options = NWK_OPT_ENABLE_SECURITY;
	appDataReq.data = appDataReqBuffer;
	appDataReq.size = strlen((char *)appDataReqBuffer);
	appDataReq.confirm = appDataConf;

	NWK_DataReq(&appDataReq);

	appDataReqBusy = true;
}

/*************************************************************************//**
*****************************************************************************/
void HAL_UartBytesReceived(uint16_t bytes)
{
for (uint16_t i = 0; i < bytes; i++)
{
uint8_t byte = HAL_UartReadByte();

if (appUartBufferPtr == sizeof(appUartBuffer))
appSendData();

if (appUartBufferPtr < sizeof(appUartBuffer))
appUartBuffer[appUartBufferPtr++] = byte;
}

SYS_TimerStop(&appTimer);
SYS_TimerStart(&appTimer);
}

/*************************************************************************//**
*****************************************************************************/
static void appTimerHandler(SYS_Timer_t *timer)
{
appSendData();
(void)timer;
}

/*************************************************************************//**
*****************************************************************************/

static void appSendACK(uint8_t *data, uint8_t size)
{
	if (appDataReqBusy) return;
	appDataReq.dstAddr = 1-APP_ADDR;
	appDataReq.dstEndpoint = APP_ENDPOINT;
	appDataReq.srcEndpoint = APP_ENDPOINT;
	appDataReq.options = NWK_OPT_ENABLE_SECURITY;
	appDataReq.data = data;
	appDataReq.size = size;
	appDataReq.confirm = appDataConf;
	NWK_DataReq(&appDataReq);

	appDataReqBusy = true;
}

static void appTimerHandler2(SYS_Timer_t *timer)
{
static uint8_t dataToSend[50];

float temp = 33.564;
sprintf(dataToSend, "t = %.2f", temp);
uint8_t mydata = 0x65;
appSendACK(&dataToSend, strlen(dataToSend));
(void)timer;
}



static bool appDataInd(NWK_DataInd_t *ind)
{
uint8_t ack = 0x06;
if (ind->size == 1 && ind->data[0]==0x06)
{
 HAL_UartWriteByte('O');
 HAL_UartWriteByte('K');
 return true;

 }
else
for (uint8_t i = 0; i < ind->size; i++) {
HAL_UartWriteByte(ind->data[i]);
}
//appUartBuffer[appUartBufferPtr++]= 0x06;
appSendACK(&ack, 1);

return true;
}
/*************************************************************************//**
*****************************************************************************/
static void appInit(void)
{
appTimer2.interval = 2000;
appTimer2.mode = SYS_TIMER_PERIODIC_MODE;
appTimer2.handler = appTimerHandler2;	
	
NWK_SetAddr(APP_ADDR);
NWK_SetPanId(APP_PANID);
PHY_SetChannel(APP_CHANNEL);
#ifdef PHY_AT86RF212
PHY_SetBand(APP_BAND);
PHY_SetModulation(APP_MODULATION);
#endif
PHY_SetRxState(true);

NWK_OpenEndpoint(APP_ENDPOINT, appDataInd);

HAL_BoardInit();

appTimer.interval = APP_FLUSH_TIMER_INTERVAL;
appTimer.mode = SYS_TIMER_INTERVAL_MODE;
appTimer.handler = appTimerHandler;
}

/*************************************************************************//**
*****************************************************************************/
static void APP_TaskHandler(void)
{
switch (appState)
{
case APP_STATE_INITIAL:
{
appInit();
appState = APP_STATE_IDLE;
} break;

case APP_STATE_IDLE:
break;

default:
break;
}
}

/*************************************************************************//**
*****************************************************************************/
int main(void)
{
SYS_Init();
HAL_UartInit(38400);
HAL_UartWriteByte('a');
SYS_TimerStart(&appTimer2);

while (1)
{
SYS_TaskHandler();
HAL_UartTaskHandler();
APP_TaskHandler();
}
}
