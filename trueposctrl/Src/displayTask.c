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
		"Recovery", // 1
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
		"WaitA 0/4", // 14
		"WaitA 1/4", // 15
		"WaitA 2/4", // 16
		"WaitA 3/4", // 17
		"WaitA 4/4", // 18
		"StartupC", // 19
		"WaitB 0/2", // 20
		"WaitB 1/2", // 11
		"WaitB 2/2" // 22
};
#define statusPrefix "Status="
#define nsatsPrefix "NSats="
#define tempPrefix "T="

static void RefreshDisplay() {
	const char *str;
	char strbuf[20];
	char strbuf2[10];
	const int dY = 14;
	TM_FontDef_t *font = &TM_Font_7x10;
	TM_SSD1306_Fill(SSD1306_COLOR_BLACK);
	/* Clock */
	uint32_t c = (dispState.Clock - dispState.UTCOffset) % (86400UL);
	uint8_t sec = c % 60UL;
	c = (c - sec) / (60);
	uint8_t min = c % 60UL;
	c = (c - sec) / (60);
	uint8_t hour = c;
	strbuf[0] = '\0';
	itoa(hour,strbuf2,10);
	if(hour<10)
		strcat(strbuf,"0");
	strcat(strbuf,strbuf2);
	strcat(strbuf, ":");
	if(min<10)
		strcat(strbuf,"0");
	itoa(min,strbuf2,10);
	strcat(strbuf, strbuf2);
	strcat(strbuf, ":");
	if(sec<10)
		strcat(strbuf,"0");
	itoa(sec,strbuf2,10);
	strcat(strbuf, strbuf2);
	TM_SSD1306_GotoXY(0,0);
	TM_SSD1306_Puts(strbuf, font, SSD1306_COLOR_WHITE);

	/* State */
	str = NULL;
    if(dispState.status >= 0 && dispState.status <= 22) {
    	str = statusLabels[dispState.status];
    }
    if(str == NULL){
    	strcpy(strbuf,statusPrefix);
    	itoa(dispState.status,&(strbuf[sizeof(statusPrefix)-1]),10);
    	str = strbuf;
    }
	TM_SSD1306_GotoXY(128-font->FontWidth*strlen(str)-1,0);
	TM_SSD1306_Puts(str, font, SSD1306_COLOR_WHITE);

	/* NSats */
	strcpy(strbuf,nsatsPrefix);
	itoa(dispState.NumSats,&(strbuf[sizeof(nsatsPrefix)-1]),10);
	TM_SSD1306_GotoXY(0,dY*1);
	TM_SSD1306_Puts(strbuf, font, SSD1306_COLOR_WHITE);
	/* Temperature */
	uint16_t tempI = (uint16_t)(dispState.Temp*100.0);
	strcpy(strbuf,tempPrefix);
	itoa(tempI,&(strbuf[sizeof(tempPrefix)-1]),10);
	int i=strlen(strbuf)-2;
	if(tempI > 100) {
		strbuf[i+3] = '\0';
		strbuf[i+2] = strbuf[i+1];
		strbuf[i+1] = strbuf[i];
		strbuf[i] = '.';
	}
	TM_SSD1306_GotoXY(0,dY*2);
	TM_SSD1306_Puts(strbuf, font, SSD1306_COLOR_WHITE);


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
