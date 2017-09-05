/*
 * truepos.c
 *
 *  Created on: Sep 1, 2017
 *      Author: Nathan
 */

#include <string.h>

#include <stm32f103xb.h>
#include "freertos.h"
#include "queue.h"
#include "task.h"
#include "usbd_cdc_if.h"
#include "usb_device.h"

#include "uart_rx.h"
#include "main.h"
#include "displayTask.h"


#define RSP_GETPOS "$GETPOS"
#define RSP_GETVER "$GETVER"
#define RSP_CLOCK "$CLOCK"
#define RSP_PPSDBG "$PPSDBG"
#define RSP_STATUS "$STATUS"
#define RSP_EXTSTATUS "$EXTSTATUS"

#define INFO_PROCEED "$TX PROCEED\r\n"
#define INFO_PPSDBG1 "$TX PPSDBG 1\r\n"

#define CMDBUF_LEN 128
char cmdBuf[CMDBUF_LEN];
static uint8_t ppsdbgReceived = 0;
static int cmdBufLen = 0;
static UART_HandleTypeDef *uart;
static uint16_t uart_id;
static uint16_t clockNoPPSDBG = 0;
static int LOCSent = 0;
static void HandleCommand();
static void HandlePPSDBG();
static void HandleStatusMsg();
static void HandleStatusCode(uint8_t code);
static void HandleExtStatus();
static void HandleClockMsg();
static void usbTx(char *msg);

void TruePosInit(UART_HandleTypeDef *uartPtr, uint16_t id) {
	uart = uartPtr;
	uart_id = id;
}

void TruePosReadBuffer() {
	int stop=0;
	char x;
	do {
		// During boot, messages may be delayed by about 8 seconds.
		if(xQueueReceive(uartRxGetQueue(uart_id), (uint8_t*)&x,
				(dispState.statusFlags & STARTUP)?
						(8000/portTICK_PERIOD_MS):(1500/portTICK_PERIOD_MS)  /* ms*/)) {
			if(cmdBufLen == 0 && x != '$') {
			} else if (x == '\r' || x == '\n') {
				cmdBuf[cmdBufLen] = '\r';
				cmdBuf[cmdBufLen+1] = '\n';
				cmdBuf[cmdBufLen+2] = '\0';
				usbTx(cmdBuf);
				HandleCommand();
				cmdBufLen = 0;

				dispState.statusFlags |= GPSDO_CONNECTED;
			} else if (cmdBufLen < (CMDBUF_LEN-4)) {
				cmdBuf[cmdBufLen] = x;
				cmdBufLen++;
			} else {
				// Too long, something bad happened
				cmdBufLen = 0;
			}
		} else {
			stop=1;
			dispState.statusFlags &= ~GPSDO_CONNECTED;
			ppsdbgReceived=0;
			displayRequestRefresh();
		}
	} while(!stop);

}

static void usbTx(char *msg) {
	static TickType_t lastXmit = 0;
	/* If still transmitting, and it has been more than 200 ms, give up */
	while(CDC_Busy() && (xTaskGetTickCount()-lastXmit < (200/portTICK_PERIOD_MS ))) {
		osDelay(1);
	}

	size_t len = strlen(msg);
	uint8_t result = CDC_Transmit_FS((uint8_t*)msg,len);
	if(result == USBD_OK)
		lastXmit = xTaskGetTickCount();
}

static void HandleCommand() {
	if(!strncmp(cmdBuf, RSP_GETVER, sizeof(RSP_GETVER)-1)) {
		if(strstr(cmdBuf, "BOOT") != NULL) {
			usbTx(INFO_PROCEED);
			HAL_UART_Transmit(uart,(uint8_t*)"$PROCEED\r\n",10,50);
			dispState.statusFlags |= STARTUP;
			displayRequestRefresh();
		}
	} else if(!strncmp(cmdBuf, RSP_CLOCK, sizeof(RSP_CLOCK)-1)) {
		dispState.statusFlags &= ~STARTUP;
		clockNoPPSDBG++;
		if(clockNoPPSDBG >= 3) {
			clockNoPPSDBG = 0;
			ppsdbgReceived = 0;
			usbTx(INFO_PPSDBG1);
			HAL_UART_Transmit(uart,(uint8_t*)"$PPSDBG 1\r\n",10,50);
		}
		HandleClockMsg();
		if(!ppsdbgReceived)
			displayRequestRefresh();
	} else if(!strncmp(cmdBuf, RSP_STATUS, sizeof(RSP_STATUS)-1)) {
		HandleStatusMsg();
		if(!ppsdbgReceived)
			displayRequestRefresh();
	} else if(!strncmp(cmdBuf, RSP_PPSDBG, sizeof(RSP_PPSDBG)-1)) {
		HandlePPSDBG();
		ppsdbgReceived=1;
		displayRequestRefresh();
	} else if(!strncmp(cmdBuf, RSP_EXTSTATUS, sizeof(RSP_EXTSTATUS)-1)) {
		HandleExtStatus();
	}
}
static void HandlePPSDBG() {
	clockNoPPSDBG = 0;
	char *t;
	char *saveptr;
	float VsetF;
	int i=0;
	//static long cl;
	if(!LOCSent) {
		static char* setpos = "$SETPOS 40448445 -86915313 230\r\n";
		HAL_UART_Transmit(uart,(uint8_t*)setpos,strlen(setpos),50);
		usbTx(setpos);
		LOCSent = 1;
	}
	t = strtok_r(cmdBuf, " \r\n",&saveptr);
	while(t != NULL) {
		switch(i) {
		case 1: // clock
			//cl = strtol(t,NULL,10);
			break;
		case 2: // status code
			HandleStatusCode(atoi(t));
			break;
		case 3: // Vset
			VsetF = strtof(t,NULL);
			dispState.Vset_uV = VsetF * 6.25e1f; // uV
			break;
		}
		i++;
		t = strtok_r(NULL, " \r\n",&saveptr);
	}
}
static void HandleExtStatus() {
	char *t;
	char *saveptr;
	int i=0;
	t = strtok_r(cmdBuf, " \r\n",&saveptr);
	while(t != NULL) {
		switch(i) {
		case 2: // NSats
			dispState.NumSats = strtoul(t,NULL,10);
			break;
		case 4:
			dispState.Temp = strtof(t,NULL);
		}
		i++;
		t = strtok_r(NULL, " \r\n",&saveptr);
	}
}
static void HandleClockMsg() {
	char *t;
	char *saveptr;
	int i=0;
	t = strtok_r(cmdBuf, " \r\n",&saveptr);
	while(t != NULL) {
		switch(i) {
		case 1: // NSats
			dispState.Clock = strtoul(t,NULL,10);
			break;
		case 2:
			dispState.UTCOffset = strtoul(t,NULL,10);
			break;
		}
		i++;
		t = strtok_r(NULL, " \r\n",&saveptr);
	}
}

static void HandleStatusMsg() {
	clockNoPPSDBG = 0;
	char *t;
	char *saveptr;
	uint8_t x;
	int i=0;
	t = strtok_r(cmdBuf, " \r\n",&saveptr);
	while(t != NULL) {
		switch(i) {
		case 3: // bad antenna
			x = atoi(t);
			if(x)
				dispState.statusFlags |= BAD_ANTENNA;
			else
				dispState.statusFlags &= ~BAD_ANTENNA;
			break;
		case 6:
			x = atoi(t);
			HandleStatusCode(x);
			break;
		}
		i++;
		t = strtok_r(NULL, " \r\n",&saveptr);
	}
}
static void HandleStatusCode(uint8_t code) {
	dispState.status = code;
	if(code==0)
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
	else
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
}

