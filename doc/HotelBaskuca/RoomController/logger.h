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
#define __LOGGER_H__					221018	// version


/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"


/* Exported defines    -------------------------------------------------------*/
#define LOG_SIZE					            16U

#define FANCOIL_RPM_SENSOR_ERROR    ((uint8_t)0xc0U)
#define FANCOIL_NTC_SENSOR_ERROR    ((uint8_t)0xc1U)
#define FANCOIL_LO_TEMP_ERROR       ((uint8_t)0xc2U)
#define FANCOIL_HI_TEMP_ERROR       ((uint8_t)0xc3U)
#define FANCOIL_FREEZING_PROTECTION ((uint8_t)0xc4U)
#define AMBIENT_NTC_SENSOR_ERROR    ((uint8_t)0xc5U)
#define THERMOSTAT_ERROR            ((uint8_t)0xc6U)

#define PIN_RESET					((uint8_t)0xd0U)
#define POWER_ON_RESET				((uint8_t)0xd1U)
#define SOFTWARE_RESET				((uint8_t)0xd2U)
#define IWDG_RESET					((uint8_t)0xd3U)
#define WWDG_RESET					((uint8_t)0xd4U)
#define LOW_POWER_RESET				((uint8_t)0xd5U)
#define FIRMWARE_UPDATED			((uint8_t)0xd6U)
#define FIRMWARE_UPDATE_FAIL		((uint8_t)0xd7U)
#define BOOTLOADER_UPDATED			((uint8_t)0xd8U)
#define BOOTLOADER_UPDATE_FAIL		((uint8_t)0xd9U)
#define IMAGE_UPDATED				((uint8_t)0xdaU)
#define IMAGE_UPDATE_FAIL			((uint8_t)0xdbU)
#define DISPLAY_FAIL				((uint8_t)0xdcU)
#define DRIVER_OR_FUNCTION_FAIL     ((uint8_t)0xddU)
#define ONEWIRE_BUS_EXCESSIVE_ERROR ((uint8_t)0xdeU)

#define NO_EVENT                	((uint8_t)0xe0U)
#define GUEST_CARD_VALID        	((uint8_t)0xe1U)
#define GUEST_CARD_INVALID      	((uint8_t)0xe2U)
#define HANDMAID_CARD_VALID     	((uint8_t)0xe3U)
#define ENTRY_DOOR_CLOSED			((uint8_t)0xe4U)
#define PRESET_CARD					((uint8_t)0xe5U)
#define HANDMAID_SERVICE_END    	((uint8_t)0xe6U)
#define MANAGER_CARD            	((uint8_t)0xe7U)
#define SERVICE_CARD            	((uint8_t)0xe8U)
#define ENTRY_DOOR_OPENED          	((uint8_t)0xe9U)
#define MINIBAR_USED            	((uint8_t)0xeaU)
#define BALCON_DOOR_OPENED			((uint8_t)0xebU)
#define BALCON_DOOR_CLOSED			((uint8_t)0xecU)
#define CARD_STACKER_ON				((uint8_t)0xedU)		
#define CARD_STACKER_OFF			((uint8_t)0xeeU)
#define DO_NOT_DISTURB_SWITCH_ON 	((uint8_t)0xefU)
#define DO_NOT_DISTURB_SWITCH_OFF	((uint8_t)0xf0U)
#define HANDMAID_SWITCH_ON			((uint8_t)0xf1U)
#define HANDMAID_SWITCH_OFF			((uint8_t)0xf2U)
#define SOS_ALARM_TRIGGER			((uint8_t)0xf3U)
#define SOS_ALARM_RESET				((uint8_t)0xf4U)
#define FIRE_ALARM_TRIGGER			((uint8_t)0xf5U)
#define FIRE_ALARM_RESET          	((uint8_t)0xf6U)
#define UNKNOWN_CARD				((uint8_t)0xf7U)
#define CARD_EXPIRED				((uint8_t)0xf8U)
#define WRONG_ROOM					((uint8_t)0xf9U)
#define WRONG_SYSTEM_ID				((uint8_t)0xfaU)
#define CONTROLLER_RESET			((uint8_t)0xfbU)
#define ENTRY_DOOR_NOT_CLOSED		((uint8_t)0xfcU)
#define	DOOR_BELL_ACTIVE			((uint8_t)0xfdU)
#define	DOOR_LOCK_USER_OPEN			((uint8_t)0xfeU)

#define WATER_FLOOD_SENOR_ACTIV     ((uint8_t)0xB0U)
#define WATER_FLOOD_SENOR_INACTIV   ((uint8_t)0xB1U)

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
	LOGGER_OK       = ((uint8_t)0x00U),
	LOGGER_FULL     = ((uint8_t)0x01U),
	LOGGER_EMPTY    = ((uint8_t)0x02U),
	LOGGER_ERROR    = ((uint8_t)0x03U),
	LOGGER_WRONG_ID = ((uint8_t)0x04U),
	LOGGER_BUSY     = ((uint8_t)0x05U)
	
}LOGGER_StatusTypeDef;



/* Exported variables  -------------------------------------------------------*/
extern uint32_t logger_flags;
extern uint32_t logger_timer;

extern LOGGER_EventTypeDef LogEvent;
extern LOGGER_StatusTypeDef LOGGER_Status;


/* Exported macros     -------------------------------------------------------*/
#define LOGGER_StartTimer(TIME)                     (logger_timer = TIME)
#define LOGGER_StopTimer()      	                (logger_timer = 0U)
#define IsLOGGER_TimerExpired() 	                (logger_timer == 0U)


/* Exported functions  -------------------------------------------------------*/
void LOGGER_Init(void);
LOGGER_StatusTypeDef LOGGER_Write(void);
LOGGER_StatusTypeDef LOGGER_Read(uint16_t log_id, uint8_t *buff);
LOGGER_StatusTypeDef LOGGER_Delete(uint16_t log_id);


#endif
/******************************   END OF FILE  **********************************/
