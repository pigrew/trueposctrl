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

#include "uart_rx.h"
#include "main.h"


#define RSP_GETPOS "$GETPOS"
#define RSP_GETVER "$GETVER"
#define RSP_CLOCK "$CLOCK"
#define RSP_PPSDBG "$PPSDBG"
#define RSP_STATUS "$STATUS"

#define INFO_PROCEED "$TX PROCEED\r\n"
#define INFO_PPSDBG1 "$TX PPSDBG 1\r\n"

#define CMDBUF_LEN 128
char cmdBuf[CMDBUF_LEN];
static int cmdBufLen = 0;
static UART_HandleTypeDef *uart;
static uint16_t uart_id;
static uint16_t clockNoPPSDBG = 0;

static void HandleCommand();
static void HandlePPSDBG();
static void HandleStatus();
static void HandleStatusCode(int code);

void TruePosInit(UART_HandleTypeDef *uartPtr, uint16_t id) {
	uart = uartPtr;
	uart_id = id;
}

void TruePosReadBuffer() {
	int stop=0;
	char x;
	do {
		if(xQueueReceive(uartRxGetQueue(uart_id), (uint8_t*)&x, 0)) {
			if(cmdBufLen == 0 && x != '$') {
			} else if (x == '\r' || x == '\n') {
				cmdBuf[cmdBufLen] = '\r';
				cmdBuf[cmdBufLen+1] = '\n';
				cmdBuf[cmdBufLen+2] = '\0';
				CDC_Transmit_FS((uint8_t*)cmdBuf,cmdBufLen+2);
				while(CDC_Busy())
					;
				HandleCommand();
				cmdBufLen = 0;
			} else if (cmdBufLen < (CMDBUF_LEN-4)) {
				cmdBuf[cmdBufLen] = x;
				cmdBufLen++;
			} else {
				// Too long, something bad happened
				cmdBufLen = 0;
			}
		} else {
			stop=1;
		}
	} while(!stop);

}

static void usbTx(char *msg) {
	size_t len = strlen(msg);
	CDC_Transmit_FS((uint8_t*)msg,len);
	while(CDC_Busy())
		;
}

static void HandleCommand() {
	if(!strncmp(cmdBuf, RSP_GETVER, sizeof(RSP_GETVER)-1)) {
		if(strstr(cmdBuf, "BOOT") != NULL) {
			usbTx(INFO_PROCEED);
			HAL_UART_Transmit(uart,(uint8_t*)"$PROCEED\r\n",10,50);
		}
	} else if(!strncmp(cmdBuf, RSP_CLOCK, sizeof(RSP_CLOCK)-1)) {
		clockNoPPSDBG++;
		if(clockNoPPSDBG == 5) {
			clockNoPPSDBG = 0;
			usbTx(INFO_PPSDBG1);
			HAL_UART_Transmit(uart,(uint8_t*)"$PPSDBG 1\r\n",10,50);

		}

	} else if(!strncmp(cmdBuf, RSP_STATUS, sizeof(RSP_STATUS)-1)) {
		HandleStatus();
	} else if(!strncmp(cmdBuf, RSP_PPSDBG, sizeof(RSP_PPSDBG)-1)) {
			HandlePPSDBG();
	}
}
static void HandlePPSDBG() {
	clockNoPPSDBG = 0;
	char *t;
	char *saveptr;
	float VsetF;
	int32_t Vset;
	int i=0;
	int statusCode;
	t = strtok_r(cmdBuf, " \r\n",&saveptr);
	while(t != NULL) {
		switch(i) {
		case 2:
			statusCode = atoi(t);
			HandleStatusCode(statusCode);
			break;
		case 3:
			VsetF = strtof(t,NULL);
			VsetF = VsetF * 6.25e1f; // uV
			Vset = (int32_t)VsetF;
			char buf2[35];
			itoa(Vset, buf2, 10);
			usbTx(buf2);
			usbTx("\r\n");
			break;
		}
		i++;
		t = strtok_r(NULL, " \r\n",&saveptr);
	}
}

static void HandleStatus() {
	clockNoPPSDBG = 0;
	char *t;
	char *saveptr;
	int statusCode;
	int i=0;
	t = strtok_r(cmdBuf, " \r\n",&saveptr);
	while(t != NULL) {
		switch(i) {
		case 6:
			statusCode = atoi(t);
			HandleStatusCode(statusCode);
			break;
		}
		i++;
		t = strtok_r(NULL, " \r\n",&saveptr);
	}
}
static void HandleStatusCode(int code) {
	if(code==0)
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
	else
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
}

