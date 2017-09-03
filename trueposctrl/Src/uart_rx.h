#ifndef UART_RX
#define UART_RX

#include "stm32f1xx_hal.h"

#ifndef UART_COUNT
#if   defined(USART8)
#define UART_COUNT (8)
#elif defined(USART7)
#define UART_COUNT (7)
#elif defined(USART6)
#define UART_COUNT (6)
#elif defined(USART5)
#define UART_COUNT (5)
#elif defined(USART4)
#define UART_COUNT (4)
#elif defined(USART3)
#define UART_COUNT (3)
#elif defined(USART2)
#define UART_COUNT (2)
#elif defined(USART1)
#define UART_COUNT (1)
#else
#define UART_COUNT (0)
#endif
#endif /* !defined(UART_COUNT) */

extern StaticQueue_t* uartRxQueues[UART_COUNT];

#define uartRxGetQueue(id) (uartRxQueues[id-1])

void uartRxInit(
		int uartId,
		UART_HandleTypeDef *huart,
		int bufSize);

void uartHandleRx(
		int uartId,
		UART_HandleTypeDef *huart);

#endif
