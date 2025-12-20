/**
 ******************************************************************************
 * File Name          : rs485.h
 * Date               : 28/02/2016 23:16:19
 * Description        : rs485 communication modul header
 ******************************************************************************
 *
 *	RS485 DEFAULT INTERFACE ADDRESS         0xffff
 *	RS485_DEFFAULT GROUP ADDRESS            0x6776
 * 	RS485_DEFFAULT BROADCAST ADDRESS        0x9999
 *	RS485 DEFAULT BAUDRATE                  115200
 *
 *
 ******************************************************************************
 */
 
 
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __RS485_H__
#define __RS485_H__					RC300318	// version


/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"


/* Exported Type  ------------------------------------------------------------*/
typedef enum 
{
	TRANSFER_IDLE = 0U,
	TRANSFER_P2P,
	TRANSFER_GROUP,
	TRANSFER_BROADCAST
		
}eTransferModeTypeDef;

typedef enum
{
	COM_INIT = 0U,
	COM_PACKET_PENDING,
	COM_PACKET_RECEIVED,
	COM_RECEIVE_SUSPEND,
	COM_ERROR
	
}eComStateTypeDef;

extern eTransferModeTypeDef eTransferMode;
extern eComStateTypeDef eComState;


/* Exported Define  ----------------------------------------------------------*/
/** ============================================================================*/
/**			R S  4 8 5			P A C K E T			F O R M A T					*/
/** ============================================================================*/
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
/**    R S 4 8 5   C O N T R O L   P R O T O C O L     C O N S T A N T S      */ 
/** ==========================================================================*/
#define UPDATE_TIMEOUT                      ((uint32_t)6000U)	/* 6 sec. for update transfer */
#define BYTE_TRANSFER_TIMEOUT				((uint32_t)5U)		/* 2 ms timeout for next byte transfer */
#define RECEIVER_REINIT_TIMEOUT				((uint32_t)5678U)	/* if receiver stop receiving for 5 s reinit usart */
#define PACKET_TRANSFER_TIMEOUT				((uint32_t)123U)
#define MAX_ERRORS                  		((uint32_t)5U)
#define RS485_BUFF_SIZE						128U                /* 128 bytes buffer lenght */
#define JOURNAL_BUFF_SIZE                   128U
#define WEATHER_FORECAST_BUFF_SIZE          24U
#define WEATHER_FORECAST_DATA_SIZE          21U  
#define RS485_BAUDRATE_2400					'0'
#define RS485_BAUDRATE_4800					'1'
#define RS485_BAUDRATE_9600					'2'
#define RS485_BAUDRATE_19200				'3'
#define RS485_BAUDRATE_38400				'4'
#define RS485_BAUDRATE_56700				'5'
#define RS485_BAUDRATE_115200				'6'
#define RS485_BAUDRATE_230400				'7'
#define RS485_BAUDRATE_460800				'8'
#define RS485_BAUDRATE_921600				'9'
#define SOH                         		((uint8_t)0x01U) 	/* start of command packet */
#define STX                         		((uint8_t)0x02U) 	/* start of 1024-byte data packet */
#define EOT                         		((uint8_t)0x04U) 	/* end of transmission */
#define ACK                         		((uint8_t)0x06U) 	/* acknowledge */
#define NAK                         		((uint8_t)0x15U) 	/* negative acknowledge */
/** ============================================================================*/
/**			R S 4 8 5			C O M M A N D				L I S T				*/
/** ============================================================================*/
#define COPY_NEW_DISPLAY_IMAGE			    ((uint8_t)0x63U)
#define DOWNLOAD_DISPLAY_IMAGE_1 		    ((uint8_t)0x64U)
#define DOWNLOAD_DISPLAY_IMAGE_2 		    ((uint8_t)0x65U)
#define DOWNLOAD_DISPLAY_IMAGE_3		    ((uint8_t)0x66U)
#define DOWNLOAD_DISPLAY_IMAGE_4		    ((uint8_t)0x67U)
#define DOWNLOAD_DISPLAY_IMAGE_5		    ((uint8_t)0x68U)
#define DOWNLOAD_DISPLAY_IMAGE_6		    ((uint8_t)0x69U)
#define DOWNLOAD_DISPLAY_IMAGE_7		    ((uint8_t)0x6aU)
#define DOWNLOAD_DISPLAY_IMAGE_8		    ((uint8_t)0x6bU)
#define DOWNLOAD_DISPLAY_IMAGE_9		    ((uint8_t)0x6cU)
#define DOWNLOAD_DISPLAY_IMAGE_10		    ((uint8_t)0x6dU)
#define DOWNLOAD_DISPLAY_IMAGE_11		    ((uint8_t)0x6eU)
#define DOWNLOAD_DISPLAY_IMAGE_12		    ((uint8_t)0x6fU)
#define DOWNLOAD_DISPLAY_IMAGE_13		    ((uint8_t)0x70U)
#define DOWNLOAD_DISPLAY_IMAGE_14		    ((uint8_t)0x71U)
#define DOWNLOAD_DISPLAY_IMAGE_15		    ((uint8_t)0x72U)
#define DOWNLOAD_DISPLAY_IMAGE_16		    ((uint8_t)0x73U)
#define DOWNLOAD_DISPLAY_IMAGE_17		    ((uint8_t)0x74U)
#define DOWNLOAD_DISPLAY_IMAGE_18		    ((uint8_t)0x75U)
#define DOWNLOAD_DISPLAY_IMAGE_19		    ((uint8_t)0x76U)
#define DOWNLOAD_FIRMWARE_IMAGE			    ((uint8_t)0x77U)
#define DOWNLOAD_BOOTLOADER_IMAGE		    ((uint8_t)0x78U)
#define DOWNLOAD_SMALL_FONT				    ((uint8_t)0x79U)
#define DOWNLOAD_MIDDLE_FONT			    ((uint8_t)0x7aU)
#define DOWNLOAD_BIG_FONT				    ((uint8_t)0x7bU)
#define DOWNLOAD_DISPLAY_IMAGE_25 		    ((uint8_t)0x7cU)
#define DOWNLOAD_DISPLAY_IMAGE_26 		    ((uint8_t)0x7dU)
#define DOWNLOAD_DISPLAY_IMAGE_27		    ((uint8_t)0x7eU)
#define DOWNLOAD_DISPLAY_IMAGE_28		    ((uint8_t)0x7fU)
#define DOWNLOAD_DISPLAY_IMAGE_29 		    ((uint8_t)0x80U)

#define DOWNLOAD_JOURNAL_1				    ((uint8_t)0x85U)
#define DOWNLOAD_WEATHER_FORECAST           ((uint8_t)0x86U)

#define GET_SYS_STATUS					    ((uint8_t)0xa0U)
#define GET_ROOM_STATUS					    ((uint8_t)0xa1U)
#define GET_RS485_CONFIG				    ((uint8_t)0xa2U)
#define GET_LOG_LIST 					    ((uint8_t)0xa3U)
#define GET_MIFARE_PERMITED_GROUP		    ((uint8_t)0xa4U)
#define GET_MIFAR_KEY_A                     ((uint8_t)0xa5U)
#define GET_MIFAR_KEY_B                     ((uint8_t)0xa6U)

#define EXECUTE_APPLICATION				    ((uint8_t)0xbbU)
#define START_BOOTLOADER				    ((uint8_t)0xbcU)
#define FLASH_PROTECTION_DISABLE		    ((uint8_t)0xbdU)
#define FLASH_PROTECTION_ENABLE			    ((uint8_t)0xbeU)

#define RESTART_CONTROLLER 				    ((uint8_t)0xc0U)
#define UPDATE_FIRMWARE           		    ((uint8_t)0xc1U)
#define UPDATE_BOOTLOADER				    ((uint8_t)0xc2U)

#define SET_ROOM_STATUS					    ((uint8_t)0xd0U)
#define SET_RS485_CONFIG				    ((uint8_t)0xd1U)
#define SET_DOUT_STATE					    ((uint8_t)0xd2U)
#define DELETE_LOG_LIST 				    ((uint8_t)0xd3U)
#define RESET_SOS_ALARM					    ((uint8_t)0xd4U)
#define SET_RTC_DATE_TIME				    ((uint8_t)0xd5U)
#define SET_ROOM_TEMPERATURE			    ((uint8_t)0xd6U)
#define SET_DISPLAY_BRIGHTNESS			    ((uint8_t)0xd7U)
#define SET_SYSTEM_ID					    ((uint8_t)0xd8U)
#define PREVIEW_DISPLAY_IMAGE			    ((uint8_t)0xd9U)
#define SET_BEDDING_REPLACEMENT_PERIOD	    ((uint8_t)0xdaU)
#define SET_MIFARE_PERMITED_GROUP 		    ((uint8_t)0xdbU)
#define SET_MIFARE_KEY_A	                ((uint8_t)0xdcU)
#define SET_MIFARE_KEY_B	                ((uint8_t)0xddU)
#define SET_ROOM_THERMOSTAT_IMAGE           ((uint8_t)0xdeU)
/* Exported variables  -------------------------------------------------------*/
extern __IO uint32_t rec_bcnt;
extern __IO uint32_t packet_type;
extern __IO uint32_t receive_pcnt;
extern __IO uint32_t rs485_timer;
extern __IO uint32_t rs485_flags;
extern __IO uint32_t rs485_sender_address;
extern __IO uint32_t rs485_packet_data_lenght;
extern __IO uint32_t rs485_packet_checksum;

extern uint8_t sys_status;
extern uint8_t activ_command;
extern uint8_t fw_update_status;
extern uint8_t rs485_buff[];
extern uint8_t aJournal_1[];
extern uint8_t weather_forecast_buff[];
extern uint8_t rs485_interface_address[2];
extern uint8_t rs485_group_address[2];
extern uint8_t rs485_broadcast_address[2];
extern uint8_t rs485_interface_baudrate;


/* Exported macros     -------------------------------------------------------*/
#define SYS_NewLogSet()						(sys_status |= (1U << 0U))
#define SYS_NewLogReset()					(sys_status &= (~ (1U << 0U)))
#define IsSYS_NewLogSet()					((sys_status & (1U << 0U)) != 0U)

#define SYS_LogListFullSet()				(sys_status |= (1U << 1U))
#define SYS_LogListFullReset()				(sys_status &= (~ (1U << 1U)))
#define IsSYS_LogListFullSet()				((sys_status & (1U << 1U)) != 0U)

#define SYS_FileTransferSuccessSet()		(sys_status |= (1U << 2U))
#define SYS_FileTransferSuccessReset()		(sys_status &= (~ (1U << 2U)))
#define IsSYS_FileTransferSuccessSet()		((sys_status & (1U << 2U)) != 0U)

#define SYS_FileTransferFailSet()			(sys_status |= (1U << 3U))
#define SYS_FileTransferFailReset()			(sys_status &= (~ (1U << 3U)))
#define IsSYS_FileTransferFailSet()			((sys_status & (1U << 3U)) != 0U)

#define SYS_UpdateSuccessSet()				(sys_status |= (1U << 4U))
#define SYS_UpdateSuccessReset()			(sys_status &= (~ (1U << 4U)))
#define IsSYS_UpdateSuccessSet()			((sys_status & (1U << 4U)) != 0U)

#define SYS_UpdateFailSet()					(sys_status |= (1U << 5U))
#define SYS_UpdateFailReset()				(sys_status &= (~ (1U << 5U)))
#define IsSYS_UpdateFailSet()				((sys_status & (1U << 5U)) != 0U)

#define SYS_ImageUpdateRequestSet()			(sys_status |= (1U << 6U))
#define SYS_ImageUpdateRequestReset()		(sys_status &= (~ (1U << 6U)))
#define IsSYS_ImageUpdateRequestSet()		((sys_status & (1U << 6U)) != 0U)

#define SYS_FirmwareUpdateRequestSet()		(sys_status |= (1U << 7U))
#define SYS_FirmwareUpdateRequestReset()	(sys_status &= (~ (1U << 7U)))
#define IsSYS_FirmwareUpdateRequestSet()	((sys_status & (1U << 7U)) != 0U)

#define RS485_StartTimer(TIME)				(rs485_timer = TIME)
#define RS485_StopTimer()					(rs485_timer = 0U)
#define IsRS485_TimerExpired()				(rs485_timer == 0U)

#define RS485_StartUpdate()					(rs485_flags |= (1U << 0U))
#define RS485_StopUpdate()					(rs485_flags &= (~ (1U << 0U)))
#define IsRS485_UpdateActiv()				((rs485_flags & (1U << 0U)) != 0U)

#define RS485_ResponsePacketReady()			(rs485_flags |= (1U << 1U))
#define RS485_NoResponse()					(rs485_flags &= (~ (1U << 1U)))
#define IsRS485_ResponsePacketPending()		((rs485_flags & (1U << 1U)) != 0U)


/* Exported functions ------------------------------------------------------- */
void RS485_Init(void);
void RS485_Service(void);


#endif
/******************************   END OF FILE  **********************************/
