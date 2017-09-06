/*
 * displayTask.c
 *
 *  Created on: Sep 4, 2017
 *      Author: Nathan
 */
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tm_stm32f4_ssd1306.h"
#include "displayTask.h"

static const char* Splash1 = "TruePosition GPSDO";
static const char* Splash2 = "Controller";
static const char* Splash3 = "KI4BZE";
static const char* NoConnection1 = "TruePosition GPSDO";
static const char* NoConnection2 = "NOT DETECTED";
dispState_struct dispState;
TaskHandle_t  displayTaskHandle;

const char* const statusLabels[] = {
		"Locked",NULL, // 0
		"Recovery",NULL, // 1
		"Startup","5/5", //2 (Initialization?)
		"Holdover","(SET1PPS)", //3 (From $SET1PPS?)
		"Forced","Holdover", // 4 (Forced Holdover?)
		"Soft","Holdover", // 5 (Soft Holdover?)
		"Unknown","Location", // 6
		"OCXO","Training", // 7 (OCXO Training?)
		"Holdover","Recovery", // 8
		"Startup","0/5", // 9
		"Startup","1/5", // 10
		"Startup","2/5", // 11
		"Startup","3/5", // 12
		"Startup","4/5", // 13
		"Wait A","0/4", // 14
		"Wait A","1/4", // 15
		"Wait A","2/4", // 16
		"Wait A","3/4", // 17
		"Wait A","4/4", // 18
		"Wait B","0/3", // 19
		"Wait B","1/3", // 20
		"Wait B","2/3", // 11
		"Wait B","3/3" // 22
};
#define statusPrefix "Status="
#define nsatsPrefix "NSats="
#define tempPrefix "T="
#define DOP_PREFIX "DOP="

static void formatDuration(uint32_t totalSec, char *result, uint8_t maxChars) {
	int i=0;
	uint16_t secs = totalSec % 60;
	totalSec /= 60;
	uint16_t mins = totalSec % 60;
	totalSec /= 60;
	uint16_t hours = totalSec % 24;
	uint16_t days = totalSec / 24;
	int tooLong = 0;
	int prefixZeros = 0;
	if(mins>0 || hours > 0 || days > 0) {
		if(hours > 0 || days > 0) {
			if(days > 0) {
				itoa(days,&(result[i]),10);
				i = strlen(result);
				result[i++] = 'd';
				prefixZeros = 1;
			}
			tooLong = tooLong || (i+3 > maxChars);
			if(!tooLong) {
				if(hours<10 && prefixZeros)
					result[i++]='0';
				itoa(hours,&(result[i]),10);
				i = strlen(result);
				result[i++] = 'h';
				prefixZeros = 1;
			}
		}
		tooLong = tooLong || (i+3 > maxChars);
		if(!tooLong) {
			if(mins<10 && prefixZeros)
				result[i++]='0';
			itoa(mins,&(result[i]),10);
			i = strlen(result);
			result[i++] = 'm';
			prefixZeros= 1;
		}
	}
	tooLong = tooLong || (i+2 > maxChars);
	if(!tooLong) {
		if(secs<10 && prefixZeros)
			result[i++]='0';
		itoa(secs,&(result[i]),10);
		i = strlen(result);
	}
	result[i++] = '\0';
}
static void RefreshDisplay() {
	const char *str, *str2;
	char strbuf[20];
	char strbuf2[10];
	const int dY = 13;
	int l, i;
	TM_FontDef_t *font = &TM_Font_7x10;
	TM_SSD1306_Fill(SSD1306_COLOR_BLACK);
	/* no communication */
	if(!(dispState.statusFlags & SF_GPSDO_CONNECTED)) {
		TM_SSD1306_GotoXY(64-strlen(NoConnection1)*(font->FontWidth)/2,10);
		TM_SSD1306_Puts(NoConnection1, font, SSD1306_COLOR_WHITE);
		TM_SSD1306_GotoXY(64-strlen(NoConnection2)*(font->FontWidth)/2,25);
		TM_SSD1306_Puts(NoConnection2, font, SSD1306_COLOR_WHITE);
		TM_SSD1306_UpdateScreen();
		return;
	}
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
	str2 = NULL;
	if(dispState.statusFlags & SF_STARTUP){
		str = "Boot";
	} else if(dispState.status >= 0 && dispState.status <= 22) {
    	str = statusLabels[2*dispState.status];
    }
	if(str != NULL) {
		str2 = statusLabels[2*dispState.status + 1];
	} else {
    	strcpy(strbuf,statusPrefix);
    	itoa(dispState.status,&(strbuf[sizeof(statusPrefix)-1]),10);
    	str = strbuf;
    }
	TM_SSD1306_GotoXY(128-font->FontWidth*strlen(str)-1,0);
	TM_SSD1306_Puts(str, font, SSD1306_COLOR_WHITE);
	if(str2 != NULL) {
		TM_SSD1306_GotoXY(128-font->FontWidth*strlen(str2)-1,dY);
		TM_SSD1306_Puts(str2, font, SSD1306_COLOR_WHITE);
	}
	/* Time locked*/
	if(dispState.status == 0 && dispState.LockStartClock != 0 &&
			dispState.Clock >= dispState.LockStartClock) {
		uint32_t lockSpan = dispState.Clock-dispState.LockStartClock;
		formatDuration(lockSpan, strbuf,9);
		TM_SSD1306_GotoXY(128-font->FontWidth*strlen(strbuf)-1,dY);
		TM_SSD1306_Puts(strbuf, font, SSD1306_COLOR_WHITE);
	}
	/* NSats */
	strcpy(strbuf,nsatsPrefix);
	itoa(dispState.NumSats,&(strbuf[sizeof(nsatsPrefix)-1]),10);
	TM_SSD1306_GotoXY(0,dY*1);
	TM_SSD1306_Puts(strbuf, font, SSD1306_COLOR_WHITE);

	/* Temperature */
	uint16_t tempI = (uint16_t)(dispState.Temp*100.0);
	strcpy(strbuf,tempPrefix);
	itoa(tempI,&(strbuf[sizeof(tempPrefix)-1]),10);
	i=strlen(strbuf)-2;
	if(tempI > 100) {
		strbuf[i+3] = '\0';
		strbuf[i+2] = strbuf[i+1];
		strbuf[i+1] = strbuf[i];
		strbuf[i] = '.';
	}
	TM_SSD1306_GotoXY(0,dY*2);
	TM_SSD1306_Puts(strbuf, font, SSD1306_COLOR_WHITE);
	/* DOP */
	if (dispState.DOP != 0) {
		tempI = (uint16_t)(dispState.DOP*100.0);
		strcpy(strbuf,DOP_PREFIX);
		itoa(tempI,&(strbuf[sizeof(DOP_PREFIX)-1]),10);
		i=strlen(strbuf)-2;
		if(tempI>100) {
			strbuf[i+3] = '\0';
			strbuf[i+2] = strbuf[i+1];
			strbuf[i+1] = strbuf[i];
			strbuf[i] = '.';
		} else if (tempI>10) {
			strbuf[i+4] = '\0';
			strbuf[i+3] = strbuf[i+1];
			strbuf[i+2] = strbuf[i];
			strbuf[i+1] = '.';
			strbuf[i] = '0';
		} else {
			strbuf[i+5] = '\0';
			strbuf[i+4] = strbuf[i+1];
			strbuf[i+3] = strbuf[i];
			strbuf[i+2] = '0';
			strbuf[i+2] = '.';
			strbuf[i] = '0';
		}
		TM_SSD1306_GotoXY(127-(font->FontWidth * strlen(strbuf)),dY*2);
		TM_SSD1306_Puts(strbuf, font, SSD1306_COLOR_WHITE);
	}
	/* Vset */
	uint32_t vsetI = (uint32_t)(dispState.Vset_uV);
	itoa(vsetI,strbuf,10);
	l = strlen(strbuf);
	for( i=l; i>=l-6 && i>=0; i--) {
		strbuf[i+1] = strbuf[i];
	}
	strbuf[i+1] = '.';
	l++;
	for( i=l; i>=l-3 && i>=0; i--) {
		strbuf[i+1] = strbuf[i];
	}
	strbuf[i+1] = ' ';
	TM_SSD1306_GotoXY(0,dY*3);
	TM_SSD1306_Puts(strbuf, font, SSD1306_COLOR_WHITE);
	TM_SSD1306_Puts(" V", font, SSD1306_COLOR_WHITE);
	/* (Bad Antenna, 10MHz,PPS) or (Survey) */
	TM_SSD1306_GotoXY(0,dY*4);
	uint8_t badThings = 0;
	if(dispState.statusFlags & (SF_BAD_ANTENNA|SF_BAD_10M | SF_BAD_PPS)) {
		TM_SSD1306_Puts("Bad ", font, SSD1306_COLOR_WHITE);
		if (dispState.statusFlags & (SF_BAD_ANTENNA)) {
			TM_SSD1306_Puts("Antenna", font, SSD1306_COLOR_WHITE);
			badThings++;
		}
		if (dispState.statusFlags & (SF_BAD_10M)) {
			if(badThings)
				TM_SSD1306_Putc(',', font, SSD1306_COLOR_WHITE);
			TM_SSD1306_Puts("10M", font, SSD1306_COLOR_WHITE);
			badThings++;
		}
		if (dispState.statusFlags & (SF_BAD_PPS)) {
			if(badThings)
				TM_SSD1306_Putc(',', font, SSD1306_COLOR_WHITE);
			TM_SSD1306_Puts("PPS", font, SSD1306_COLOR_WHITE);
			badThings++;
		}
	} else if(dispState.statusFlags & SF_SURVEY) {
		uint32_t secsRemaining = dispState.SurveyEndClock - dispState.Clock;
		TM_SSD1306_GotoXY(0,dY*4);
		TM_SSD1306_Puts("Survey", font, SSD1306_COLOR_WHITE);
		if(dispState.SurveyEndClock >= dispState.Clock) {
			TM_SSD1306_Putc('=', font, SSD1306_COLOR_WHITE);

			formatDuration(secsRemaining, strbuf,11);

			TM_SSD1306_Puts(strbuf, font, SSD1306_COLOR_WHITE);
		}
	}
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
	TM_SSD1306_GotoXY(64-strlen(Splash3)*(font->FontWidth)/2,63-font->FontHeight);
	TM_SSD1306_Puts(Splash3, font, SSD1306_COLOR_WHITE);
	TM_SSD1306_UpdateScreen();
	/* Leave the splash screen for a moment, just because. */
	osDelay(1500);
	do {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY );
		RefreshDisplay();
	} while (1);
}
