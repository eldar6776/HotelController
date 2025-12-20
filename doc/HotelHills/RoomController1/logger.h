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
#ifndef LOGGER_H
#define LOGGER_H    					100	// version 1.00

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Exported defines    -------------------------------------------------------*/
#define LOGGER_BUFFER_SIZE				16
#define LOG_SIZE						16
#define WRITE_NEW_LOG					'N'
#define LOG_EVENT_ENTRY					'I'
#define LOG_EVENT_EXIT					'O'
#define LOG_TYPE_REMOTE_ENABLED			'R'
#define LOGGER_COMMAND_TIMEOUT			100

#define NO_EVENT                	((uint8_t)0xe0)
#define GUEST_CARD_VALID        	((uint8_t)0xe1)
#define GUEST_CARD_INVALID      	((uint8_t)0xe2)
#define HANDMAID_CARD_VALID     	((uint8_t)0xe3)
#define HANDMAID_CARD_INVALID   	((uint8_t)0xe4)
#define HANDMAID_SERVICE_START  	((uint8_t)0xe5)
#define HANDMAID_SERVICE_END    	((uint8_t)0xe6)
#define MANAGER_CARD            	((uint8_t)0xe7)
#define SERVICE_CARD            	((uint8_t)0xe8)
#define DOOR_OPENED             	((uint8_t)0xe9)
#define MINIBAR_USED            	((uint8_t)0xea)
#define WINDOWS_OPENED				((uint8_t)0xeb)
#define WINDOWS_CLOSED				((uint8_t)0xec)
#define INDOOR_READER_ON			((uint8_t)0xed)		
#define INDOOR_READER_OFF			((uint8_t)0xee)
#define DO_NOT_DISTURB_SWITCH_ON 	((uint8_t)0xef)
#define DO_NOT_DISTURB_SWITCH_OFF	((uint8_t)0xf0)
#define HANDMAID_SWITCH_ON			((uint8_t)0xf1)
#define HANDMAID_SWITCH_OFF			((uint8_t)0xf2)
#define SOS_ALARM_TRIGGER           ((uint8_t)0xf3)
#define SOS_ALARM_RESET             ((uint8_t)0xf4)
#define FIRE_ALARM_TRIGGER          ((uint8_t)0xf5)
#define FIRE_ALARM_RESET          	((uint8_t)0xf6)
#define UNKNOWN_CARD				((uint8_t)0xf7)

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
	LOGGER_OK		= 0x00,
	LOGGER_FULL		= 0x01,
	LOGGER_EMPTY	= 0x02,
	LOGGER_ERROR 	= 0x03,
	LOGGER_WRONG_ID = 0x04
	
}LOGGER_StatusTypeDef;

/* Exported variables  -------------------------------------------------------*/
extern uint8_t aLoggerBuffer[LOGGER_BUFFER_SIZE];
extern uint32_t logger_flags;
extern uint32_t logger_timer;

extern LOGGER_EventTypeDef LogEvent;

/* Exported macros     -------------------------------------------------------*/
#define LOGGER_StartTimer(TIME)     ((logger_flags &= 0xfffffffe), (logger_timer = TIME))
#define IsLOGGER_TimerExpired() 	(logger_flags & 0x00000001)
#define LOGGER_StopTimer()      	(logger_flags |= 0x00000001)

/* Exported functions  -------------------------------------------------------*/
extern void LOGGER_Init(void);
extern LOGGER_StatusTypeDef LOGGER_Write(void);
extern LOGGER_StatusTypeDef LOGGER_Read(uint16_t log_id);
extern LOGGER_StatusTypeDef LOGGER_Delete(uint16_t log_id);

#endif
/******************************   END OF FILE  **********************************/
