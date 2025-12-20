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
#define __RC522_H__					221018	// version


/* Include  ------------------------------------------------------------------*/
#include "stm32f1xx.h"


/* Exported Type  ------------------------------------------------------------*/
/* Exported Define  ----------------------------------------------------------*/
#define RC522_CARD_VALID_EVENT_TIME			3210U   // 3 s reader unlisten time after card read
#define RC522_CARD_INVALID_EVENT_TIME		987U    // ~1 s reader unlisten time after card read
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
#define CARD_USER_FIRST_NAME_ADDRESS		(sector_0.block_1[0])
#define CARD_USER_LAST_NAME_ADDRESS			(sector_0.block_2[0])
#define CARD_USER_GROUP_ADDRESS				(sector_1.block_0[0])
#define CARD_SYSTEM_ID_ADDRESS				(sector_1.block_1[0])
#define CARD_EXPIRY_TIME_ADDRESS			(sector_2.block_0[0])
#define CARD_CTRL_ID_ADDRESS				(sector_2.block_0[6])
#define CARD_USER_INVALIDITY_ADDRESS		(sector_2.block_0[8])
#define CARD_USER_LANGUAGE_ADDRESS			(sector_2.block_0[9])
#define CARD_USER_LOGO_ADDRESS				(sector_2.block_0[10])
#define CARD_USER_GENDER_ADDRESS			(sector_2.block_0[11])


/* Exported types    ---------------------------------------------------------*/
typedef struct
{
	uint8_t card_status;
    uint8_t user_group;
    uint16_t system_id;
    uint16_t controller_id;
	uint8_t expiry_time[6];
	uint8_t card_id[5];
	
}RC522_CardDataTypeDef;

extern RC522_CardDataTypeDef sCard;


/* Exported variables  -------------------------------------------------------*/
extern __IO uint32_t mifare_process_flags;
extern uint8_t system_id[];
extern uint8_t mifare_key_a[];
extern uint8_t mifare_key_b[];
extern uint8_t rc522_rx_buff[];
extern uint8_t rc522_tx_buff[];
extern uint8_t permitted_add[8][2];


/* Exported macros     -------------------------------------------------------*/
#define RC522_HandmaidReentranceEnable()        (mifare_process_flags |= (1U << 0))
#define RC522_HandmaidReentranceDisable()       (mifare_process_flags &= (~ (1U << 0)))
#define IsRC522_HandmaidReentranceActiv()       ((mifare_process_flags & (1U << 0)) != 0U)

#define RC522_ExtendDoorlockTimeSet()           (mifare_process_flags |= (1U << 1))
#define RC522_ExtendDoorlockTimeReset()         (mifare_process_flags &= (~ (1U << 1)))
#define IsRC522_ExtendDoorlockTimeActiv()       ((mifare_process_flags & (1U << 1)) != 0U)   


/* Exported functions  -------------------------------------------------------*/
void RC522_Init(void);
void RC522_Service(void);
void RC522_ClearData(void);

#endif
/******************************   END OF FILE  **********************************/
