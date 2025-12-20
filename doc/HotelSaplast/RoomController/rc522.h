/**
 ******************************************************************************
 * File Name          : rc522.h
 * Date               : 08/05/2016 23:15:16
 * Description        : mifare RC522 modul header
 ******************************************************************************
 *
 *
 ******************************************************************************
 */
 
 
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __RC522_H__
#define __RC522_H__                         020818 	// version


/* Include  ------------------------------------------------------------------*/
#include "stm32f1xx.h"


/* Exported Type  ------------------------------------------------------------*/
/* Exported Define  ----------------------------------------------------------*/
#define RC522_CARD_VALID_EVENT_TIME			3210	// 3 s reader unlisten time after card read
#define RC522_CARD_INVALID_EVENT_TIME		987		// ~1 s reader unlisten time after card read
/**
*--------------      card user groups predefine     ---------------------
*/
#define CARD_USER_GROUP_GUEST				('G')
#define CARD_USER_GROUP_HANDMAID			('H')
#define CARD_USER_GROUP_MANAGER				('M')
#define CARD_USER_GROUP_SERVICE				('S')
#define CARD_USER_GROUP_PRESET				('P')
/**
*--------------   card data invalid date predefine    -------------------
*/
#define CARD_ID_INVALID						(':')
#define CARD_ID_INVALID_DATA				(';')
#define USER_GROUP_INVALID					('<')	
#define USER_GROUP_DATA_INVALID				('=')
#define EXPIRY_TIME_INVALID					('>')
#define EXPIRY_TIME_DATA_INVALID			('?')
#define CONTROLLER_ID_INVALID				('.')
#define CONTROLLER_ID_DATA_INVALID			('/')
#define SYSTEM_ID_INVALID					('{')
#define SYSTEM_ID_DATA_INVALID				('}')
/**
*---------------     card data predefined addresse    --------------------
*/
#define CARD_USER_FIRST_NAME_ADDRESS		(Sector_0.Block_1[0])
#define CARD_USER_LAST_NAME_ADDRESS			(Sector_0.Block_2[0])
#define CARD_USER_GROUP_ADDRESS				(Sector_1.Block_0[0])
#define CARD_SYSTEM_ID_ADDRESS				(Sector_1.Block_1[0])
#define CARD_EXPIRY_TIME_ADDRESS			(Sector_2.Block_0[0])
#define CARD_CTRL_ID_ADDRESS				(Sector_2.Block_0[6])
#define CARD_USER_INVALIDITY_ADDRESS		(Sector_2.Block_0[8])
#define CARD_USER_LANGUAGE_ADDRESS			(Sector_2.Block_0[9])
#define CARD_USER_LOGO_ADDRESS				(Sector_2.Block_0[10])
#define CARD_USER_GENDER_ADDRESS			(Sector_2.Block_0[11])


/* Exported types    ---------------------------------------------------------*/
typedef struct
{
	uint8_t card_status;
	uint8_t aCardID[5];
	uint8_t user_group;
	uint8_t aExpiryTime[6];
	uint16_t controller_id;
	uint16_t system_id;
	
}RC522_CardDataTypeDef;

extern RC522_CardDataTypeDef sCard;


/* Exported variables  -------------------------------------------------------*/
extern __IO uint32_t mifare_timer;
extern __IO uint32_t handmaid_card_timer;
extern uint8_t rc522_config;
extern uint8_t aSystemID[];
extern uint8_t aMifareKeyA[];
extern uint8_t aMifareKeyB[];
extern uint8_t aRC522_RxBuffer[];
extern uint8_t aRC522_TxBuffer[];
extern uint8_t aPermitedAddresse[8][2];

/* Exported macros     -------------------------------------------------------*/
#define RC522_StartTimer(TIME)					(mifare_timer = TIME)
#define RC522_StopTimer()						(mifare_timer = 0)
#define IsRC522_TimerExpired()					(mifare_timer == 0)
#define RC522_HandmaidCardStartTimer(TIME)		(handmaid_card_timer = TIME)
#define RC522_HandmaidCardStopTimer()			(handmaid_card_timer = 0)
#define IsRC522_HandmaidCardTimerExpired()		(handmaid_card_timer == 0)


/* Exported functions  -------------------------------------------------------*/
extern void RC522_Init(void);
extern void RC522_Service(void);


#endif
/******************************   END OF FILE  **********************************/
