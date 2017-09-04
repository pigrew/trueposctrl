/*
 * displayTask.h
 *
 *  Created on: Sep 4, 2017
 *      Author: Nathan
 */

#ifndef DISPLAYTASK_H_
#define DISPLAYTASK_H_

enum {
	GPSDO_CONNECTED = 1,
	BAD_ANTENNA = 2,
	STARTUP = 2,
};

typedef struct {
	__IO uint8_t statusFlags;
	__IO uint8_t status;
	__IO uint8_t NumSats;
	__IO float Temp;
	__IO uint32_t Clock;
	__IO uint8_t UTCOffset;
	__IO float Vset_uV;
} dispState_struct;

extern dispState_struct dispState;
extern TaskHandle_t  displayTaskHandle;

void StartDisplayTask(void const * argument);
void displayRequestRefresh();



#endif /* DISPLAYTASK_H_ */
