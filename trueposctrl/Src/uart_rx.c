/*
 * uart_rx.c
 *
 * Designed to pipe interrupt-driven UART data into a FreeRTOS queue.
 *
 * This was written to enable use variable-length serial RX for
 *  Stm32cubemx-generated projects on a STM32F103 board.
 *
 *  Created on: Sep 2, 2017
 *      Author: Nathan
 */


#include "freertos.h"
#include "queue.h"
#include "task.h"

#include "uart_rx.h"


StaticQueue_t* uartRxQueues[UART_COUNT];
static void uartRxenqueueData(int uartId, UART_HandleTypeDef *huart);
/*********
 * Initialize the UART Rx with a statically initialized RTOS queue
 *
 * @param uartId: UART number (1-based). For example, for UART1, it should be 1.
 */
void uartRxInit(
		int uartId,
		UART_HandleTypeDef *huart,
		int bufSize) {
	configASSERT(configSUPPORT_DYNAMIC_ALLOCATION );

	configASSERT(uartId >= 1 && uartId <= UART_COUNT && uartRxQueues[uartId-1] == NULL);

	uartRxQueues[uartId-1] = xQueueCreate(bufSize, 1);

	__HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);

}
/**
 * IRQ Handler
 *
 * Call from the UART ISR.
 */
void uartHandleRx(int uartId, UART_HandleTypeDef *huart) {

	uint32_t isrflags   = READ_REG(huart->Instance->SR);
	uint32_t cr1its     = READ_REG(huart->Instance->CR1);
	//uint32_t cr3its     = READ_REG(huart->Instance->CR3);
	//uint32_t errorflags = 0x00U;

	//errorflags = (isrflags & (uint32_t)(USART_SR_PE | USART_SR_FE | USART_SR_ORE | USART_SR_NE));
	/* UART in mode Receiver -------------------------------------------------*/
	/* RXNE is receive buffer not empty flag */
	/* It is cleared when data is read from the SPI_ */
	if(((isrflags & USART_SR_RXNE) != 0) && ((cr1its & USART_CR1_RXNEIE) != 0)) {
		uartRxenqueueData(uartId, huart);
		return;
	}

	//xQueueIsQueueFullFromISR(uartRxQueues[uartId]);
}
// Called from ISR
static void uartRxenqueueData(int uartId, UART_HandleTypeDef *huart) {
	uint16_t tmp;

	if(huart->Init.WordLength == UART_WORDLENGTH_9B) {
		if(huart->Init.Parity == UART_PARITY_NONE) {
			tmp = (uint16_t)(huart->Instance->DR & (uint16_t)0x01FF);
			xQueueSendToBackFromISR(uartRxQueues[uartId-1], ((uint8_t*)&tmp)+1, NULL);
			xQueueSendToBackFromISR(uartRxQueues[uartId-1], &tmp, NULL);
			huart->pRxBuffPtr += 2U;
		} else {
			tmp = (uint16_t)(huart->Instance->DR & (uint16_t)0x00FF);
			xQueueSendToBackFromISR(uartRxQueues[uartId-1], &tmp, NULL);
			huart->pRxBuffPtr += 1U;
		}
	} else {
		if(huart->Init.Parity == UART_PARITY_NONE) {
			tmp = (uint8_t)(huart->Instance->DR & (uint8_t)0x00FF);
			xQueueSendToBackFromISR(uartRxQueues[uartId-1], &tmp, NULL);
		} else {
			tmp = (uint8_t)(huart->Instance->DR & (uint8_t)0x007F);
			xQueueSendToBackFromISR(uartRxQueues[uartId-1], &tmp, NULL);
		}
	}
}
