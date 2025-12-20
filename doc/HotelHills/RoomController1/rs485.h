/**
 ******************************************************************************
 * File Name          : rs485.h
 * Date               : 28/02/2016 23:16:19
 * Description        : rs485 communication modul header
 ******************************************************************************
 *
 *  RS485 DATA PACKET FORMAT
 *  ================================================================
 *  B0 = SOH                    - start of master to slave packet
 *  B0 = STX                    - start of slave to master packet
 *  B1 = ADDRESS MSB            - addressed unit high byte
 *  B2 = ADDRESS LSB            - addressed unit low byte
 *  B3 = MESSAGE LENGHT         - data lenght
 *  B4 = DATA [0]               - data first byte
 *  Bn = DATA [n]               - data last byte
 *  Bn+1 = CRC MSB              - crc16 high byte
 *  Bn+2 = CRC LSB              - crc16 low byte
 *  Bn+3 = EOT                  - end of transmission
 *
 ******************************************************************************
 */
#ifndef RS485_H
#define RS485_H     100     // version 1.00

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Types    ------------------------------------------------------------------*/
typedef enum 
{
	TRANSFER_IDLE			= 0x00,
	TRANSFER_P2P 			= 0x01,
	TRANSFER_GROUP			= 0x02,
	TRANSFER_BROADCAST		= 0x03	
		
}eTransferModeTypeDef;

typedef enum
{
	COM_INIT				= 0x00,
	COM_PACKET_PENDING		= 0x01,
	COM_PACKET_RECEIVED		= 0x02,
	COM_RECEIVE_SUSPEND		= 0x03,
	COM_ERROR				= 0x04
	
}eComStateTypeDef;

/** ==========================================================================*/
/**                                                                           */
/**    R S 4 8 5   C O N T R O L   P R O T O C O L     C O N S T A N T S      */ 
/**                                                                           */
/** ==========================================================================*/
#define RS485_DEFAULT_INTERFACE_ADDRESS		((uint32_t)0x0400)
#define RS485_DEFFAULT_GROUP_ADDRESS		((uint32_t)0x6776)
#define RS485_DEFFAULT_BROADCAST_ADDRESS	((uint32_t)0x9999)

#define PACKET_TRANSFER_TIMEOUT				((uint32_t)100)		/* 100 ms timeout for packet transfer */
#define BYTE_TRANSFER_TIMEOUT				((uint32_t)2)		/* 2 ms timeout for next byte transfer*/
#define RECEIVE_SUSPEND_TIME				((uint32_t)5)		/* stop receiving  */
#define MAX_ERRORS                  		((uint32_t)5)

#define SOH                         		((uint8_t)0x01) 	/* start of command packet */
#define STX                         		((uint8_t)0x02) 	/* start of 1024-byte data packet */
#define EOT                         		((uint8_t)0x04) 	/* end of transmission */
#define ACK                         		((uint8_t)0x06) 	/* acknowledge */
#define NAK                         		((uint8_t)0x15) 	/* negative acknowledge */

/** ==========================================================================*/
/**                                                                           */
/**             R S  4 8 5   P A C K E T   F O R M A T        				  */ 
/**                                                                           */
/** ==========================================================================*/
/** 	
*		command packet
*/
//		PACKET_START_IDENTIFIER
//		PACKET_RECEIVER_ADDRESS_MSB
//		PACKET_RECEIVER_ADDRESS_LSB	
//		PACKET_SENDER_ADDRESS_MSB
//		PACKET_SENDER_ADDRESS_LSB
//		PACKET_LENGHT						
//		PACKET_DATA		
//		PACKET_CHECKSUM_MSB	
//		PACKET_CHECKSUM_LSB	
//		PACKET_END_IDENTIFIER
/** 	
*		data packet
*/
//		PACKET_START_IDENTIFIER
//		PACKET_RECEIVER_ADDRESS_MSB
//		PACKET_RECEIVER_ADDRESS_LSB	
//		PACKET_SENDER_ADDRESS_MSB
//		PACKET_SENDER_ADDRESS_LSB
//		PACKET_LENGHT						
//		PACKET_NUMBER_MSB
//		PACKET_NUMBER_LSB
//		PACKET_CHECKSUM_MSB	
//		PACKET_CHECKSUM_LSB	
//		PACKET_END_IDENTIFIER

/** ==========================================================================*/
/**                                                                           */
/**    			 R S 4 8 5		C O M M A N D		L I S T          		  */ 
/**                                                                           */
/** ==========================================================================*/
#define DOWNLOAD_DISPLAY_IMAGE_1		((uint8_t)0xce)
#define DOWNLOAD_DISPLAY_IMAGE_2		((uint8_t)0xcd)
#define DOWNLOAD_DISPLAY_IMAGE_3		((uint8_t)0xcc)
#define DOWNLOAD_DISPLAY_IMAGE_4		((uint8_t)0xcb)
#define DOWNLOAD_DISPLAY_IMAGE_5		((uint8_t)0xca)
#define DOWNLOAD_DISPLAY_IMAGE_6		((uint8_t)0xc9)
#define DOWNLOAD_DISPLAY_IMAGE_7		((uint8_t)0xc8)
#define DOWNLOAD_DISPLAY_IMAGE_8		((uint8_t)0xc7)
#define DOWNLOAD_DISPLAY_IMAGE_9		((uint8_t)0xc6)
#define DOWNLOAD_DISPLAY_IMAGE_10		((uint8_t)0xc5)
#define DOWNLOAD_SMALL_FONTS			((uint8_t)0xc4)
#define DOWNLOAD_MIDDLE_FONTS			((uint8_t)0xc3)
#define DOWNLOAD_BIG_FONTS				((uint8_t)0xc2)
#define DOWNLOAD_TEXT_DATE_TIME			((uint8_t)0xc1)
#define DOWNLOAD_TEXT_EVENTS 			((uint8_t)0xc0)
#define DOWNLOAD_FIRMWARE           	((uint8_t)0xbf)
#define FLASH_PROTECTION_ENABLE			((uint8_t)0xbe)
#define FLASH_PROTECTION_DISABLE		((uint8_t)0xbd)
#define START_BOOTLOADER				((uint8_t)0xbc)
#define EXECUTE_APPLICATION				((uint8_t)0xbb)
#define GET_SYS_STATUS					((uint8_t)0xba)
#define GET_SYS_INFO					((uint8_t)0xb9)
#define GET_SYS_CONFIG					((uint8_t)0xb8)
#define SET_SYS_CONFIG					((uint8_t)0xb7)
#define GET_RTC_DATE_TIME				((uint8_t)0xb6)
#define SET_RTC_DATE_TIME				((uint8_t)0xb5)
#define GET_LOG_LIST 					((uint8_t)0xb4)
#define DELETE_LOG_LIST 				((uint8_t)0xb3)
#define GET_CLOCK_ALARM 				((uint8_t)0xb2)
#define SET_CLOCK_ALARM					((uint8_t)0xb1)
#define GET_RESTART_COUNTER				((uint8_t)0xb0)
#define CLEAR_RESTART_COUNTER			((uint8_t)0xaf)
#define GET_RS485_CONFIG				((uint8_t)0xae)
#define SET_RS485_CONFIG				((uint8_t)0xad)
#define GET_MIFARE_CONFIG				((uint8_t)0xac)
#define SET_MIFARE_CONFIG				((uint8_t)0xab)
#define GET_MIFARE_PERMITED_GROUP		((uint8_t)0xaa)
#define SET_MIFARE_PERMITED_GROUP		((uint8_t)0xa9)	
#define GET_MIFARE_PERMITED_CARD		((uint8_t)0xa8)
#define SET_MIFARE_PERMITED_CARD		((uint8_t)0xa7)	
#define GET_MIFARE_FORBIDEN_CARD		((uint8_t)0xa6)
#define SET_MIFARE_FORBIDEN_CARD		((uint8_t)0xa5)	
#define GET_MIFARE_COMMAND				((uint8_t)0xa4)
#define SET_MIFARE_COMMAND				((uint8_t)0xa3)	
#define GET_MIFARE_CARD_DATA			((uint8_t)0xa2)
#define SET_MIFARE_CARD_DATA			((uint8_t)0xa1)
#define GET_DIN_STATE					((uint8_t)0xa0)
#define SET_DOUT_STATE					((uint8_t)0x9f)
#define GET_DOUT_STATE					((uint8_t)0x9e)
#define GET_PCB_TEMPERATURE				((uint8_t)0x9d)
#define GET_TEMP_CARD_BUFFER			((uint8_t)0x9c)

/* Exported variables  -------------------------------------------------------*/
extern volatile uint32_t rs485_timer;
extern volatile uint32_t rs485_flags;
extern volatile uint8_t receiving_errors;

extern eTransferModeTypeDef eTransferMode;
extern eComStateTypeDef eComState;

/* Exported macros     -------------------------------------------------------*/
#define RS485_DirRx()   			(HAL_GPIO_WritePin (RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_SET))
#define RS485_DirTx()   			(HAL_GPIO_WritePin (RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET))
#define RS485_StartTimer(TIME)		((rs485_timer = TIME), (rs485_flags &= 0xfffffffe))
#define RS485_StopTimer()			(rs485_flags |= 0x00000001)
#define IsRS485_TimerExpired()		(rs485_flags & 0x00000001)	

/* Exported functions ------------------------------------------------------- */
extern void RS485_Init(void);
extern void RS485_Service(void);

#endif
/******************************   END OF FILE  **********************************/
