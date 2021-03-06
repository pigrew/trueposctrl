/*
 * truepos.c
 *
 *  Created on: Sep 1, 2017
 *      Author: Nathan
 */

#include <string.h>

#include <stm32f103xb.h>
#include "FreeRTOS.h"
#include "cmsis_os.h"
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
#define RSP_SURVEY "$SURVEY"
#define RSP_SETPOS "$SETPOS"
#define RSP_KALDBG "$KALDBG"
#define RSP_SAT "$SAT"
#define RSP_WSAT "$WSAT"

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
static void HandlePPSDbgMsg();
static void HandleStatusMsg();
static void HandleStatusCode(uint8_t code);
static void HandleExtStatusMsg();
static void HandleClockMsg();
static void HandleSurveyMsg();
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
				(dispState.statusFlags & SF_STARTUP)?
						(8000/portTICK_PERIOD_MS):(1500/portTICK_PERIOD_MS)  /* ms*/)) {
			if(cmdBufLen == 0 && x != '$') {
			} else if (x == '\r' || x == '\n') {
				cmdBuf[cmdBufLen] = '\r';
				cmdBuf[cmdBufLen+1] = '\n';
				cmdBuf[cmdBufLen+2] = '\0';
				usbTx(cmdBuf);
				HandleCommand();
				cmdBufLen = 0;

				dispState.statusFlags |= SF_GPSDO_CONNECTED;
			} else if (cmdBufLen < (CMDBUF_LEN-4)) {
				cmdBuf[cmdBufLen] = x;
				cmdBufLen++;
			} else {
				// Too long, something bad happened
				cmdBufLen = 0;
			}
		} else {
			stop=1;
			dispState.statusFlags &= ~SF_GPSDO_CONNECTED;
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
	// Last transmission failed, so lets skip this one.
	if(CDC_Busy())
		return;
	size_t len = strlen(msg);
	uint8_t result = CDC_Transmit_FS((uint8_t*)msg,len);
	if(result == USBD_OK)
		lastXmit = xTaskGetTickCount();
}

static void HandleCommand() {
	if(!strncmp(cmdBuf, RSP_CLOCK, sizeof(RSP_CLOCK)-1)) {
		dispState.statusFlags &= ~SF_STARTUP;
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
	} else if(!strncmp(cmdBuf, RSP_PPSDBG, sizeof(RSP_PPSDBG)-1)) {
		HandlePPSDbgMsg();
		ppsdbgReceived=1;
		displayRequestRefresh();
	}  else if(!strncmp(cmdBuf, RSP_STATUS, sizeof(RSP_STATUS)-1)) {
		HandleStatusMsg();
		if(!ppsdbgReceived)
			displayRequestRefresh();
	} else if(!strncmp(cmdBuf, RSP_EXTSTATUS, sizeof(RSP_EXTSTATUS)-1)) {
		HandleExtStatusMsg();
	} else if(!strncmp(cmdBuf, RSP_SURVEY, sizeof(RSP_SURVEY)-1)) {
		HandleSurveyMsg();
	}else if(!strncmp(cmdBuf, RSP_SETPOS, sizeof(RSP_SETPOS)-1)) {
		if(dispState.statusFlags & SF_STARTUP) {
			ppsdbgReceived = 0;
			usbTx(INFO_PPSDBG1);
			HAL_UART_Transmit(uart,(uint8_t*)"$PPSDBG 1\r\n",10,50);
		}
	} else if(!strncmp(cmdBuf, RSP_GETVER, sizeof(RSP_GETVER)-1)) {
		if(strstr(cmdBuf, "BOOT") != NULL) {
			usbTx(INFO_PROCEED);
			HAL_UART_Transmit(uart,(uint8_t*)"$PROCEED\r\n",10,50);
			dispState.statusFlags |= SF_STARTUP;
			dispState.Clock = 0;
			dispState.NumSats = 0;
			dispState.SurveyEndClock = 0;
			dispState.Temp = 0;
			dispState.UTCOffset = 0;
			dispState.Vset_uV = 0;
			dispState.status = 0;
			dispState.DOP = 0.0f;
			dispState.statusFlags &= ~(SF_BAD_10M | SF_BAD_ANTENNA | SF_BAD_PPS | SF_SURVEY);
			displayRequestRefresh();
		}
	} else if(!strncmp(cmdBuf, RSP_KALDBG, sizeof(RSP_KALDBG)-1)) {
	} else if(!strncmp(cmdBuf, RSP_SAT, sizeof(RSP_SAT)-1)) {
	} else if(!strncmp(cmdBuf, RSP_WSAT, sizeof(RSP_WSAT)-1)) {
	} else {
		// Need to ALWAYS let a \0 be at the end, due to multithreading
		strncpy(dispState.LastMsg, cmdBuf, LastMsg_LEN-1);
		dispState.LastMsg[LastMsg_LEN-1] = '\0';
		dispState.LastMsg[LastMsg_LEN-2] = '\0';
		int16_t i = strlen(dispState.LastMsg)-1;
		while(i>=0 && (dispState.LastMsg[i] == '\r' || dispState.LastMsg[i] == '\n')) {
			dispState.LastMsg[i] = '\0';
			i--;
		}
	}
}
static void HandlePPSDbgMsg() {
	clockNoPPSDBG = 0;
	char *t;
	char *saveptr;
	float VsetF;
	int i=0;
	//static long cl;
	if(!LOCSent) {
		/*
		static char* setpos = "$SETPOS X Y Z\r\n";
		HAL_UART_Transmit(uart,(uint8_t*)setpos,strlen(setpos),100);
		usbTx(setpos);
		*/
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
static void HandleExtStatusMsg() {
	char *t;
	char *saveptr;
	int i=0;
	t = strtok_r(cmdBuf, " \r\n",&saveptr);
	while(t != NULL) {
		switch(i) {
		case 1: // survey?
			if(t[0] != '0')
				dispState.statusFlags |= SF_SURVEY;
			else
				dispState.statusFlags &= ~SF_SURVEY;
			break;
		case 2: // NSats
			dispState.NumSats = strtoul(t,NULL,10);
			break;
		case 3: // DOP
			dispState.DOP = strtof(t,NULL);
			break;
		case 4: // Temperature
			dispState.Temp = strtof(t,NULL);
			break;
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

static void HandleSurveyMsg() {
	char *t;
	char *saveptr;
	int i=0;
	t = strtok_r(cmdBuf, " \r\n",&saveptr);
	while(t != NULL) {
		switch(i) {
		case 5: // Remaining time
			dispState.SurveyEndClock = dispState.Clock + strtoul(t,NULL,10);
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
		case 1: // bad 10MHZ
			if(t[0] != '0')
				dispState.statusFlags |= SF_BAD_10M;
			else
				dispState.statusFlags &= ~SF_BAD_10M;
			break;
		case 2: // bad antenna
			if(t[0] != '0')
				dispState.statusFlags |= SF_BAD_PPS;
			else
				dispState.statusFlags &= ~SF_BAD_PPS;
			break;
		case 3: // bad antenna
			if(t[0] != '0')
				dispState.statusFlags |= SF_BAD_ANTENNA;
			else
				dispState.statusFlags &= ~SF_BAD_ANTENNA;
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
	if(code==0) {
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
		if(dispState.LockStartClock == 0)
			dispState.LockStartClock = dispState.Clock;
	}else {
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
		dispState.LockStartClock = 0;
	}
}

