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

#define CMDBUF_LEN 128
char cmdBuf[CMDBUF_LEN];
static int cmdBufLen = 0;
static UART_HandleTypeDef *uart;
static uint16_t uart_id;

static void HandleCommand();

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
				HandleCommand();
				CDC_Transmit_FS((uint8_t*)cmdBuf,cmdBufLen+2);
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

static void HandleCommand() {
	if(!strncmp(cmdBuf, RSP_GETVER, sizeof(RSP_GETVER)-1)) {
		if(strstr(cmdBuf, "BOOT") != NULL) {
			puts("Sending proceed");
			HAL_UART_Transmit(uart,(uint8_t*)"$PROCEED\r\n",10,50);
		}
	} else {
		puts("unknown command received\n");

	}
}
