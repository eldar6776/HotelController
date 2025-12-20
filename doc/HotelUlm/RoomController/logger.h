/**
 ******************************************************************************
 * File Name          : logger.h
 * Date               : 08/05/2016 23:15:16
 * Description        : data logger modul header
 ******************************************************************************
 *
 *
 ******************************************************************************
 */
 
 
#ifndef __LOGGER_H__
#define __LOGGER_H__					    FW_BUILD	// version
/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx.h"
/* Exported defines    -------------------------------------------------------*/
/* Exported types  -----------------------------------------------------------*/
typedef struct
{
	uint8_t log_event;
	uint8_t log_type;
	uint8_t log_group;
	uint8_t log_card_id[5];
	uint8_t log_time_stamp[6];
	
}LOGGER_EventTypeDef;


typedef enum
{
	LOGGER_OK       = ((uint8_t)0x0U),
	LOGGER_FULL     = ((uint8_t)0x1U),
	LOGGER_EMPTY    = ((uint8_t)0x2U),
	LOGGER_ERROR    = ((uint8_t)0x3U),
	LOGGER_WRONG_ID = ((uint8_t)0x4U),
	LOGGER_BUSY     = ((uint8_t)0x5U)
	
}LOGGER_StatusTypeDef;

extern LOGGER_EventTypeDef LogEvent;
/* Exported variables  -------------------------------------------------------*/
/* Exported macros     -------------------------------------------------------*/
/* Exported functions  -------------------------------------------------------*/
extern void LOGGER_Init(void);
extern LOGGER_StatusTypeDef LOGGER_Write(void);
extern LOGGER_StatusTypeDef LOGGER_Delete(void);
extern LOGGER_StatusTypeDef LOGGER_Read(uint8_t *buff);
#endif
/************************ (C) COPYRIGHT JUBERA D.O.O Sarajevo ************************/
