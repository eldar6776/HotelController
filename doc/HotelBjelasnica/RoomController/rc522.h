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
#ifndef RC522_H
#define RC522_H   							102		// version 1.02

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "common.h"

/* Exported defines    -------------------------------------------------------*/
#define DEFAULT_SYSTEM_ID					0xabcd
#define RC522_SYSTEM_INIT_DISABLE_TIME		8901	// 8 s on power up reader is disbled 
#define RC522_CARD_VALID_EVENT_TIME			3210	// 3 s reader unlisten time after card read
#define RC522_CARD_INVALID_EVENT_TIME		1234	// 1 s reader unlisten time after card read
#define RC522_PROCESS_TIME					234		// 234 ms read rate
#define RC522_POWER_VALID_GROUP_PERMITED	2		// 2 hours room power valid for menager, service, handmaid card
#define RC522_MAX_LEN						16      // FIFO & buffer byte lenght
#define RC522_HANDMAID_CARD_DELAY			2345	// reset handmaid card status		
/**
*-----------------        card data status     --------------------------
*/
#define CARD_PENDING						(NULL)
#define CARD_VALID							(0x06)
#define CARD_INVALID						(0x15)
#define CARD_DATA_FORMATED					(0x7f)
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
#define CARD_USER_GROUP_INVALID				('<')	
#define CARD_USER_GROUP_DATA_INVALID		('=')
#define CARD_EXPIRY_TIME_INVALID			('>')
#define CARD_EXPIRY_TIME_DATA_INVALID		('?')
#define CARD_CONTROLLER_ID_INVALID			('.')
#define CARD_CONTROLLER_ID_DATA_INVALID		('/')
#define CARD_SYSTEM_ID_INVALID				('{')
#define CARD_SYSTEM_ID_DATA_INVALID			('}')

/**
*---------------     card data predefined addresse    --------------------
*/
#define CARD_USER_FIRST_NAME_ADDRESS		(Sector_0.BlockData.B_Dat_1[0])
#define CARD_USER_LAST_NAME_ADDRESS			(Sector_0.BlockData.B_Dat_2[0])
#define CARD_USER_GROUP_ADDRESS				(Sector_1.BlockData.B_Dat_0[0])
#define CARD_SYSTEM_ID_ADDRESS				(Sector_1.BlockData.B_Dat_1[0])
#define CARD_EXPIRY_TIME_ADDRESS			(Sector_2.BlockData.B_Dat_0[0])
#define CARD_CTRL_ID_ADDRESS				(Sector_2.BlockData.B_Dat_0[6])
#define CARD_USER_INVALIDITY_ADDRESS		(Sector_2.BlockData.B_Dat_0[8])
#define CARD_USER_LANGUAGE_ADDRESS			(Sector_2.BlockData.B_Dat_0[9])
#define CARD_USER_LOGO_ADDRESS				(Sector_2.BlockData.B_Dat_0[10])
#define CARD_USER_GENDER_ADDRESS			(Sector_2.BlockData.B_Dat_0[11])

/* Exported types    ---------------------------------------------------------*/
typedef struct
{
	uint8_t card_status;
	uint8_t aUserCardID[5];
	uint8_t card_user_group;
	uint8_t aCardExpiryTime[6];
	uint16_t controller_id;
	uint16_t system_id;
	
}RC522_CardDataTypeDef;

/* Exported variables  -------------------------------------------------------*/
extern uint8_t rc522_config;
extern volatile uint32_t mifare_timer;
extern volatile uint32_t handmaid_card_timer;
extern volatile uint32_t mifare_process_flags;

extern uint8_t aSystemID[2];
extern uint8_t aMifareAuthenticationKeyA[6];
extern uint8_t aMifareAuthenticationKeyB[6];
extern uint8_t aRC522_RxBuffer[RC522_MAX_LEN];
extern uint8_t aRC522_TxBuffer[RC522_MAX_LEN];
extern RC522_CardDataTypeDef sCardData;

/* Exported macros     -------------------------------------------------------*/
#define RC522_StartTimer(TIME)					((mifare_timer = TIME), (mifare_process_flags &= 0xfffffffe))
#define RC522_StopTimer()						(mifare_process_flags |= 0x00000001)
#define IsRC522_TimerExpired()					(mifare_process_flags &  0x00000001)
#define RC522_HandmaidCardStartTimer(TIME)		((handmaid_card_timer = TIME), (mifare_process_flags &= 0xfffffffd))
#define RC522_HandmaidCardStopTimer()			(mifare_process_flags |= 0x00000002)
#define IsRC522_HandmaidCardTimerExpired()		(mifare_process_flags &  0x00000002)
#define RC522_HandmaidReentranceEnable()		(mifare_process_flags |= 0x00000004)	
#define RC522_HandmaidReentranceDisable()		(mifare_process_flags &= 0xfffffffb)
#define IsRC522_HandmaidReentranceActiv()		(mifare_process_flags & 0x00000004)

/* Exported functions  -------------------------------------------------------*/
extern void RC522_Init(void);
extern void RC522_Service(void);

#endif
/******************************   END OF FILE  **********************************/
