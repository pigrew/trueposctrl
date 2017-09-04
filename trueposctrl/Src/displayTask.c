/*
 * displayTask.c
 *
 *  Created on: Sep 4, 2017
 *      Author: Nathan
 */
#include "FreeRTOS.h"
#include "task.h"
#include "tm_stm32f4_ssd1306.h"
#include "displayTask.h"

static const char* Splash1 = "TruePosition GPSDO";
static const char* Splash2 = "Controller";
dispState_struct dispState;
TaskHandle_t  displayTaskHandle;

const char* const statusLabels[] = {
		"Locked", // 0
		"Recovering", // 1
		"StartupB", //2
		"Holdover", //3
		NULL, // 4
		NULL, // 5
		NULL, // 6
		NULL, // 7
		"Holdover", // 8
		NULL, // 9
		"StartupA", // 10
		NULL, // 11
		NULL, // 12
		NULL, // 13
		"PreLock 0/4", // 14
		"PreLock 1/4", // 15
		"PreLock 2/4", // 16
		"PreLock 3/4", // 17
		"PreLock 4/4", // 18
		"StartupC", // 19
		"PreRecovery 0/2", // 20
		"PreRecovery 1/2", // 11
		"PreRecovery 2/2" // 22
};
const char* statusPrefix = "Status=";

static void RefreshDisplay() {
	const char *str;
	char strbuf[20];
	TM_FontDef_t *font = &TM_Font_7x10;
	TM_SSD1306_Fill(SSD1306_COLOR_BLACK);
	/* State */
	str = NULL;
    if(dispState.status >= 0 && dispState.status <= 22) {
    	str = statusLabels[dispState.status];
    }
    if(str == NULL){
    	strcpy(strbuf,statusPrefix);
    	itoa(dispState.status,&(strbuf[sizeof(statusPrefix)]),10);
    	str = strbuf;
    }
	TM_SSD1306_GotoXY(0,0);
	TM_SSD1306_Puts(str, font, SSD1306_COLOR_WHITE);
	TM_SSD1306_UpdateScreen();
}

void displayRequestRefresh() {
	 xTaskNotify( displayTaskHandle, ( 1UL), eSetBits );
}

void StartDisplayTask(void const * argument) {
	TM_FontDef_t *font = &TM_Font_7x10;
	TM_SSD1306_Init();
	//TM_SSD1306_Fill(SSD1306_COLOR_BLACK);
	TM_SSD1306_GotoXY(64-strlen(Splash1)*(font->FontWidth)/2,10);
	TM_SSD1306_Puts(Splash1, font, SSD1306_COLOR_WHITE);
	TM_SSD1306_GotoXY(64-strlen(Splash2)*(font->FontWidth)/2,25);
	TM_SSD1306_Puts(Splash2, font, SSD1306_COLOR_WHITE);
	TM_SSD1306_UpdateScreen();
	do {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY );
		RefreshDisplay();
	} while (1);
}
