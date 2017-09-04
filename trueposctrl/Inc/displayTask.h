/*
 * displayTask.h
 *
 *  Created on: Sep 4, 2017
 *      Author: Nathan
 */

#ifndef DISPLAYTASK_H_
#define DISPLAYTASK_H_

typedef struct {
	__IO int status;
	__IO uint8_t NumSats;
	__IO float Temp;
	__IO uint32_t Clock;
	__IO uint8_t UTCOffset;
} dispState_struct;

extern dispState_struct dispState;
extern TaskHandle_t  displayTaskHandle;

void StartDisplayTask(void const * argument);
void displayRequestRefresh();



#endif /* DISPLAYTASK_H_ */
