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
#define RC522_H   						100	// version 1.00

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "common.h"

/* Exported defines    -------------------------------------------------------*/
#define RC522_CARD_EVENT_TIME					5000	// 5 s reader unlisten time after card read
#define RC522_PROCESS_TIME						50		// 50 ms read rate

#define CARD_PENDING							(NULL)
#define CARD_VALID								(0x06)
#define CARD_INVALID							(0x15)
#define CARD_DATA_FORMATED						(0x7f)


#define CARD_USER_GROUP_KINDERGARDEN			('K')
#define CARD_USER_GROUP_HOTEL					('H')
#define CARD_USER_GROUP_POOL					('P')
#define CARD_USER_GROUP_PARKING					('R')
#define CARD_USER_GROUP_1						('1')
#define CARD_USER_GROUP_2						('2')
#define CARD_USER_GROUP_3						('3')
#define CARD_USER_GROUP_4						('4')
#define CARD_USER_GROUP_5						('5')
#define CARD_USER_GROUP_6						('6')
#define CARD_USER_GROUP_7						('7')
#define CARD_USER_GROUP_8						('8')
#define CARD_USER_GROUP_9						('9')
#define CARD_USER_GROUP_10						('A')
#define CARD_USER_GROUP_11						('B')
#define CARD_USER_GROUP_12						('C')
#define CARD_USER_GROUP_13						('D')
#define CARD_USER_GROUP_14						('E')
#define CARD_USER_GROUP_15						('F')
#define CARD_USER_GROUP_16						('G')

#define CARD_TYPE_ONE_TIME						('O')
#define CARD_TYPE_FAMILY						('F')
#define CARD_TYPE_MEMBER						('M')
#define CARD_TYPE_SERVICE						('S')
#define CARD_TYPE_MENAGER						('W')
#define CARD_TYPE_GUEST							('Y')

#define CARD_ID_INVALID							('!')
#define CARD_ID_INVALID_DATA					('"')
#define CARD_USER_GROUP_INVALID					('#')	
#define CARD_USER_GROUP_DATA_INVALID			('$')
#define CARD_EXPIRY_TIME_INVALID				('%')
#define CARD_EXPIRY_TIME_DATA_INVALID			('&')
#define CARD_USAGE_TYPE_INVALID					('/')
#define CARD_USAGE_TYPE_DATA_INVALID			('(')
#define CARD_NUMBER_OF_USERS_INVALID			(')')
#define CARD_NUMBER_OF_USERS_DATA_INVALID		('=')


#define CARD_USER_GROUP_ADDRESS			(Sector_1.BlockData.B_Dat_0[0])
#define CARD_EXPIRY_TIME_ADDRESS		(Sector_2.BlockData.B_Dat_0[0])
#define CARD_USAGE_TYPE_ADDRESS			(Sector_2.BlockData.B_Dat_0[6])
#define CARD_USERS_NUMBER_ADDRESS		(Sector_2.BlockData.B_Dat_0[7])

/* Exported types    ---------------------------------------------------------*/
typedef struct
{
	uint8_t card_status;
	uint8_t aUserCardID[5];
	uint8_t card_user_group;
	uint8_t card_usage_type;
	uint8_t card_number_of_users;
	uint8_t aCardExpiryTime[6];
	
}RC522_CardDataTypeDef;

/* Exported variables  -------------------------------------------------------*/
extern uint8_t rc522_config;
extern uint32_t mifare_timer;
extern uint32_t mifare_process_flags;

extern uint8_t aMifareAuthenticationKeyA[6];
extern uint8_t aMifareAuthenticationKeyB[6];
extern uint8_t aCardUserGroup[16];
extern uint8_t aCardUsageType[16];
extern uint8_t card_max_users_cnt;

extern RC522_CardDataTypeDef sCardData;

/* Exported macros     -------------------------------------------------------*/
#define RC522_StartTimer(TIME)			((mifare_timer = TIME), (mifare_process_flags &= 0xfffffffe))
#define RC522_StopTimer()				(mifare_process_flags |= 0x00000001)
#define IsRC522_TimerExpired()			(mifare_process_flags &  0x00000001)

/* Exported functions  -------------------------------------------------------*/
extern void RC522_Init(void);
extern void RC522_Service(void);

#endif
/******************************   END OF FILE  **********************************/
