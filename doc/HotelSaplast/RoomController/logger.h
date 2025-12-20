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
#define __LOGGER_H__               020818 	// version


/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"


/* Exported defines    -------------------------------------------------------*/
#define LOG_SIZE					16

#define PIN_RESET					((uint8_t)0xd0)
#define POWER_ON_RESET				((uint8_t)0xd1)
#define SOFTWARE_RESET				((uint8_t)0xd2)
#define IWDG_RESET					((uint8_t)0xd3)
#define WWDG_RESET					((uint8_t)0xd4)
#define LOW_POWER_RESET				((uint8_t)0xd5)
#define FIRMWARE_UPDATED			((uint8_t)0xd6)
#define FIRMWARE_UPDATE_FAIL		((uint8_t)0xd7)
#define BOOTLOADER_UPDATED			((uint8_t)0xd8)
#define BOOTLOADER_UPDATE_FAIL		((uint8_t)0xd9)
#define IMAGE_UPDATED				((uint8_t)0xda)
#define IMAGE_UPDATE_FAIL			((uint8_t)0xdb)
#define DISPLAY_FAIL				((uint8_t)0xdc)
#define DRIVER_OR_FUNCTION_FAIL     ((uint8_t)0xdd)

#define NO_EVENT                	((uint8_t)0xe0)
#define GUEST_CARD_VALID        	((uint8_t)0xe1)
#define GUEST_CARD_INVALID      	((uint8_t)0xe2)
#define HANDMAID_CARD_VALID     	((uint8_t)0xe3)
#define ENTRY_DOOR_CLOSED			((uint8_t)0xe4)
#define PRESET_CARD					((uint8_t)0xe5)
#define HANDMAID_SERVICE_END    	((uint8_t)0xe6)
#define MANAGER_CARD            	((uint8_t)0xe7)
#define SERVICE_CARD            	((uint8_t)0xe8)
#define ENTRY_DOOR_OPENED          	((uint8_t)0xe9)
#define MINIBAR_USED            	((uint8_t)0xea)
#define BALCON_DOOR_OPENED			((uint8_t)0xeb)
#define BALCON_DOOR_CLOSED			((uint8_t)0xec)
#define CARD_STACKER_ON				((uint8_t)0xed)		
#define CARD_STACKER_OFF			((uint8_t)0xee)
#define DO_NOT_DISTURB_SWITCH_ON 	((uint8_t)0xef)
#define DO_NOT_DISTURB_SWITCH_OFF	((uint8_t)0xf0)
#define HANDMAID_SWITCH_ON			((uint8_t)0xf1)
#define HANDMAID_SWITCH_OFF			((uint8_t)0xf2)
#define SOS_ALARM_TRIGGER			((uint8_t)0xf3)
#define SOS_ALARM_RESET				((uint8_t)0xf4)
#define FIRE_ALARM_TRIGGER			((uint8_t)0xf5)
#define FIRE_ALARM_RESET          	((uint8_t)0xf6)
#define UNKNOWN_CARD				((uint8_t)0xf7)
#define CARD_EXPIRED				((uint8_t)0xf8)
#define WRONG_ROOM					((uint8_t)0xf9)
#define WRONG_SYSTEM_ID				((uint8_t)0xfa)
#define CONTROLLER_RESET			((uint8_t)0xfb)
#define ENTRY_DOOR_NOT_CLOSED		((uint8_t)0xfc)
#define	DOOR_BELL_ACTIVE			((uint8_t)0xfd)
#define	DOOR_LOCK_USER_OPEN			((uint8_t)0xfe)

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
	LOGGER_OK			= 0x00,
	LOGGER_FULL			= 0x01,
	LOGGER_EMPTY		= 0x02,
	LOGGER_ERROR 		= 0x03,
	LOGGER_WRONG_ID 	= 0x04,
	LOGGER_BUSY			= 0x05
	
}LOGGER_StatusTypeDef;



/* Exported variables  -------------------------------------------------------*/
extern uint32_t logger_flags;
extern uint32_t logger_timer;

extern LOGGER_EventTypeDef LogEvent;
extern LOGGER_StatusTypeDef LOGGER_Status;


/* Exported macros     -------------------------------------------------------*/
#define LOGGER_StartTimer(TIME)     (logger_timer = TIME)
#define LOGGER_StopTimer()      	(logger_timer = 0)
#define IsLOGGER_TimerExpired() 	(logger_timer == 0)


/* Exported functions  -------------------------------------------------------*/
void LOGGER_Init(void);
LOGGER_StatusTypeDef LOGGER_Write(void);
LOGGER_StatusTypeDef LOGGER_Read(uint16_t log_id);
LOGGER_StatusTypeDef LOGGER_Delete(uint16_t log_id);


#endif
/******************************   END OF FILE  **********************************/
