/**
 ******************************************************************************
 * File Name          : hotel_room_controller.h
 * Date               : 21/08/2016 20:59:16
 * Description        : hotel room Rubicon controller data link module header
 ******************************************************************************
 *
 *  RS485 DATA PACKET FORMAT
 *  ================================================================
 *  B0 = SOH                    - start of master to slave packet
 *  B0 = STX                    - start of slave to master packet
 *  B1 = ADDRESS MSB            - addressed unit high byte
 *  B2 = ADDRESS LSB            - addressed unit low byte
 *  B3 = ADDRESS MSB            - sender unit address high byte
 *  B4 = ADDRESS LSB            - sender unit address low byte
 *  B5 = MESSAGE LENGHT         - data lenght
 *  B6 = DATA [0]               - data first byte
 *  Bn = DATA [B5 + 5]          - data last byte
 *  Bn+1 = CRC MSB              - crc16 high byte
 *  Bn+2 = CRC LSB              - crc16 low byte
 *  Bn+3 = EOT                  - end of transmission
 ******************************************************************************
 */
#ifndef HOTEL_ROOM_CONTROLLER_H
#define HOTEL_ROOM_CONTROLLER_H   				201	// version 2.01

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
#include "rtc.h"
#include "GUI.h"

/* Exported defines    -------------------------------------------------------*/


/** ==========================================================================*/
/**                                                                           */
/**    R U B I C O N    L O G    L I S T     C O N S T A N T S     			  */
/**                                                                           */
/** ==========================================================================*/
#define RUBICON_LOG_NO_EVENT                	((uint8_t)0xe0)
#define RUBICON_LOG_GUEST_CARD_VALID        	((uint8_t)0xe1)
#define RUBICON_LOG_GUEST_CARD_INVALID      	((uint8_t)0xe2)
#define RUBICON_LOG_HANDMAID_CARD_VALID     	((uint8_t)0xe3)
#define RUBICON_LOG_ENTRY_DOOR_CLOSED			((uint8_t)0xe4)
#define RUBICON_LOG_PRESET_CARD					((uint8_t)0xe5)
#define RUBICON_LOG_HANDMAID_SERVICE_END    	((uint8_t)0xe6)
#define RUBICON_LOG_MANAGER_CARD            	((uint8_t)0xe7)
#define RUBICON_LOG_SERVICE_CARD            	((uint8_t)0xe8)
#define RUBICON_LOG_ENTRY_DOOR_OPENED          	((uint8_t)0xe9)
#define RUBICON_LOG_MINIBAR_USED            	((uint8_t)0xea)
#define RUBICON_LOG_BALCON_DOOR_OPENED			((uint8_t)0xeb)
#define RUBICON_LOG_BALCON_DOOR_CLOSED			((uint8_t)0xec)
#define RUBICON_LOG_CARD_STACKER_ON				((uint8_t)0xed)		
#define RUBICON_LOG_CARD_STACKER_OFF			((uint8_t)0xee)
#define RUBICON_LOG_DO_NOT_DISTURB_SWITCH_ON 	((uint8_t)0xef)
#define RUBICON_LOG_DO_NOT_DISTURB_SWITCH_OFF	((uint8_t)0xf0)
#define RUBICON_LOG_HANDMAID_SWITCH_ON			((uint8_t)0xf1)
#define RUBICON_LOG_HANDMAID_SWITCH_OFF			((uint8_t)0xf2)
#define RUBICON_LOG_SOS_ALARM_TRIGGER			((uint8_t)0xf3)
#define RUBICON_LOG_SOS_ALARM_RESET				((uint8_t)0xf4)
#define RUBICON_LOG_FIRE_ALARM_TRIGGER			((uint8_t)0xf5)
#define RUBICON_LOG_FIRE_ALARM_RESET          	((uint8_t)0xf6)
#define RUBICON_LOG_UNKNOWN_CARD				((uint8_t)0xf7)
#define RUBICON_LOG_CARD_EXPIRED				((uint8_t)0xf8)
#define RUBICON_LOG_WRONG_ROOM					((uint8_t)0xf9)
#define RUBICON_LOG_WRONG_SYSTEM_ID				((uint8_t)0xfa)


/** ==========================================================================*/
/**                                                                           */
/**    R U B I C O N    R S 4 8 5   P R O T O C O L     C O N S T A N T S     */
/**                                                                           */
/** ==========================================================================*/
#define RUBICON_DEFAULT_INTERFACE_ADDRESS		((uint32_t)0x0400)
#define RUBICON_DEFFAULT_GROUP_ADDRESS			((uint32_t)0x6776)
#define RUBICON_DEFFAULT_BROADCAST_ADDRESS		((uint32_t)0x9999)

#define RUBICON_SOH                 			((uint8_t)0x01) 	/* start of command packet */
#define RUBICON_STX                  			((uint8_t)0x02) 	/* start of 1024-byte data packet */
#define RUBICON_EOT                       		((uint8_t)0x04) 	/* end of transmission */
#define RUBICON_ACK                    			((uint8_t)0x06) 	/* acknowledge */
#define RUBICON_NAK                     		((uint8_t)0x15) 	/* negative acknowledge */


/** ==========================================================================*/
/**                                                                           */
/**       R U B I C O N    R S  4 8 5   P A C K E T   F O R M A T        	  */
/**                                                                           */
/** ==========================================================================*/
/** 	
 *		command packet
 */
//		RUBICON_PACKET_START_IDENTIFIER
//		RUBICON_PACKET_RECEIVER_ADDRESS_MSB
//		RUBICON_PACKET_RECEIVER_ADDRESS_LSB	
//		RUBICON_PACKET_SENDER_ADDRESS_MSB
//		RUBICON_PACKET_SENDER_ADDRESS_LSB
//		RUBICON_PACKET_LENGHT						
//		RUBICON_PACKET_DATA		
//		RUBICON_PACKET_CHECKSUM_MSB	
//		RUBICON_PACKET_CHECKSUM_LSB	
//		RUBICON_PACKET_END_IDENTIFIER
/** 	
 *		data packet
 */
//		RUBICON_PACKET_START_IDENTIFIER
//		RUBICON_PACKET_RECEIVER_ADDRESS_MSB
//		RUBICON_PACKET_RECEIVER_ADDRESS_LSB	
//		RUBICON_PACKET_SENDER_ADDRESS_MSB
//		RUBICON_PACKET_SENDER_ADDRESS_LSB
//		RUBICON_PACKET_LENGHT						
//		RUBICON_PACKET_NUMBER_MSB
//		RUBICON_PACKET_NUMBER_LSB
//		RUBICON_PACKET_CHECKSUM_MSB	
//		RUBICON_PACKET_CHECKSUM_LSB	
//		RUBICON_PACKET_END_IDENTIFIER


/** ==========================================================================*/
/**                                                                           */
/**    	R U B I C O N    R S 4 8 5	C O M M A N D		L I S T           	  */
/**                                                                           */
/** ==========================================================================*/
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_1 		((uint8_t)0xce) // 206
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_2 		((uint8_t)0xcd)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_3		((uint8_t)0xcc)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_4		((uint8_t)0xcb)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_5		((uint8_t)0xca)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_6		((uint8_t)0xc9)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_7		((uint8_t)0xc8)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_8		((uint8_t)0xc7)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_9		((uint8_t)0xc6)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_10		((uint8_t)0xc5) // 197
#define RUBICON_DOWNLOAD_SMALL_FONTS			((uint8_t)0xc4)
#define RUBICON_DOWNLOAD_MIDDLE_FONTS			((uint8_t)0xc3)
#define RUBICON_DOWNLOAD_BIG_FONTS				((uint8_t)0xc2)
#define RUBICON_DOWNLOAD_TEXT_DATE_TIME			((uint8_t)0xc1)
#define RUBICON_DOWNLOAD_TEXT_EVENTS 			((uint8_t)0xc0)
#define RUBICON_DOWNLOAD_FIRMWARE           	((uint8_t)0xbf)
#define RUBICON_FLASH_PROTECTION_ENABLE			((uint8_t)0xbe)
#define RUBICON_FLASH_PROTECTION_DISABLE		((uint8_t)0xbd)
#define RUBICON_START_BOOTLOADER				((uint8_t)0xbc)
#define RUBICON_EXECUTE_APPLICATION				((uint8_t)0xbb)
#define RUBICON_GET_SYS_STATUS					((uint8_t)0xba)
#define RUBICON_GET_SYS_INFO					((uint8_t)0xb9)
#define RUBICON_GET_DISPLAY_BRIGHTNESS			((uint8_t)0xb8)
#define RUBICON_SET_DISPLAY_BRIGHTNESS			((uint8_t)0xb7)
#define RUBICON_GET_RTC_DATE_TIME				((uint8_t)0xb6)
#define RUBICON_SET_RTC_DATE_TIME				((uint8_t)0xb5)
#define RUBICON_GET_LOG_LIST 					((uint8_t)0xb4)
#define RUBICON_DELETE_LOG_LIST 				((uint8_t)0xb3)
#define RUBICON_GET_RS485_CONFIG				((uint8_t)0xb2)
#define RUBICON_SET_RS485_CONFIG				((uint8_t)0xb1)
#define RUBICON_GET_DIN_STATE					((uint8_t)0xb0)
#define RUBICON_SET_DOUT_STATE					((uint8_t)0xaf)
#define RUBICON_GET_DOUT_STATE					((uint8_t)0xae)
#define RUBICON_GET_PCB_TEMPERATURE				((uint8_t)0xad)
#define RUBICON_GET_TEMP_CARD_BUFFER			((uint8_t)0xac)
#define RUBICON_GET_MIFARE_AUTHENTICATION_KEY_A	((uint8_t)0xab)
#define RUBICON_SET_MIFARE_AUTHENTICATION_KEY_A	((uint8_t)0xaa)
#define RUBICON_GET_MIFARE_AUTHENTICATION_KEY_B	((uint8_t)0xa9)
#define RUBICON_SET_MIFARE_AUTHENTICATION_KEY_B	((uint8_t)0xa8)
#define RUBICON_GET_MIFARE_PERMITED_GROUP		((uint8_t)0xa7)
#define RUBICON_SET_MIFARE_PERMITED_GROUP 		((uint8_t)0xa6)
#define RUBICON_GET_MIFARE_PERMITED_CARD_1		((uint8_t)0xa5)
#define RUBICON_SET_MIFARE_PERMITED_CARD_1		((uint8_t)0xa4)
#define RUBICON_GET_MIFARE_PERMITED_CARD_2		((uint8_t)0xa3)
#define RUBICON_SET_MIFARE_PERMITED_CARD_2		((uint8_t)0xa2)
#define RUBICON_GET_MIFARE_PERMITED_CARD_3		((uint8_t)0xa1)
#define RUBICON_SET_MIFARE_PERMITED_CARD_3		((uint8_t)0xa0)
#define RUBICON_GET_MIFARE_PERMITED_CARD_4		((uint8_t)0x9f)
#define RUBICON_SET_MIFARE_PERMITED_CARD_4		((uint8_t)0x9e)
#define RUBICON_GET_MIFARE_PERMITED_CARD_5		((uint8_t)0x9d)
#define RUBICON_SET_MIFARE_PERMITED_CARD_5		((uint8_t)0x9c)
#define RUBICON_GET_MIFARE_PERMITED_CARD_6		((uint8_t)0x9b)
#define RUBICON_SET_MIFARE_PERMITED_CARD_6		((uint8_t)0x9a)
#define RUBICON_GET_MIFARE_PERMITED_CARD_7		((uint8_t)0x99)
#define RUBICON_SET_MIFARE_PERMITED_CARD_7		((uint8_t)0x98)
#define RUBICON_GET_MIFARE_PERMITED_CARD_8		((uint8_t)0x97)
#define RUBICON_SET_MIFARE_PERMITED_CARD_8		((uint8_t)0x96)
#define RUBICON_GET_ROOM_STATUS					((uint8_t)0x95)
#define RUBICON_SET_ROOM_STATUS					((uint8_t)0x94)
#define RUBICON_RESET_SOS_ALARM					((uint8_t)0x93)
#define RUBICON_GET_ROOM_TEMPERATURE			((uint8_t)0x92)
#define RUBICON_SET_ROOM_TEMPERATURE			((uint8_t)0x91)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_11		((uint8_t)0x90)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_12		((uint8_t)0x8f)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_13		((uint8_t)0x8e)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_14		((uint8_t)0x8d)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_15		((uint8_t)0x8c)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_16		((uint8_t)0x8b)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_17		((uint8_t)0x8a)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_18		((uint8_t)0x89)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_19		((uint8_t)0x88)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_20		((uint8_t)0x87)
#define RUBICON_SET_MIFARE_PERMITED_CARD		((uint8_t)0x86)

/** ==========================================================================*/
/**                                                                           */
/**    	C O N T R O L L E R     P R O G R A M     C O N S T A N T S           */
/**                                                                           */
/** ==========================================================================*/
#define RS485_INTERFACE_DEFAULT_ADDRESS			0x0005
#define RUBICON_TIME_UPDATE_PERIOD				6000
#define RUBICON_RESTART_TIME					7500
#define MSG_DISPL_TIME							500
#define RUBICON_BUFFER_SIZE						512
#define RUBICON_PACKET_BUFFER_SIZE				64
#define RUBICON_BOOTLOADER_START_TIME			2000
#define RUBICON_FW_UPLOAD_TIMEOUT				2000
#define RUBICON_FW_EXE_BOOT_TIME				1500
#define RUBICON_READY_FOR_FILE_TIMEOUT			321
#define RUBICON_PACKET_NAK_TIME					50
#define RUBICON_PACKET_RESEND_TIME				100
#define RUBICON_LOG_SIZE						16
#define RUBICON_LOG_COMMAND_TIMEOUT				100
#define RUBICON_RESPONSE_TIMEOUT				50	
#define RUBICON_BYTE_RX_TIMEOUT					3	
#define RUBICON_RX_TO_TX_DELAY					10
#define RUBICON_MAX_ERRORS          			10
#define RUBICON_HTTP_RESPONSE_TIMEOUT			40
#define RUBICON_CONFIG_FILE_MAX_SIZE			(RUBICON_BUFFER_SIZE - 16)
#define RUBICON_CONFIG_FILE_BUFFER_SIZE			64
#define RUBICON_CONFIG_FILE_TAG_LENGHT			5


/** ==========================================================================*/
/**                                                                           */
/**    	C O N T R O L L E R 	C O M M A N D		L I S T            		  */
/**                                                                           */
/** ==========================================================================*/
#define RS485_SCANNER_FIND_FIRST				0
#define RS485_SCANNER_FIND_NEXT					1
#define RS485_SCANNER_FIND_NEW					2
#define RS485_SCANNER_FIND_ALL					3
#define RS485_SCANNER_FIND_ADDRESSED			4

#define FILE_OK									5
#define FILE_SYS_ERROR							6	
#define FILE_DIR_ERROR							7
#define FILE_ERROR								8
#define MAX_QUERY_ATTEMPTS						9

#define FW_UPDATE_IDLE							16
#define FW_UPDATE_INIT 							17
#define FW_UPDATE_BOOTLOADER 					18
#define FW_UPDATE_RUN							19
#define FW_UPDATE_FINISHED						20
#define FW_UPDATE_FAIL							21
#define FW_UPDATE_TO_SELECTED					22
#define FW_UPDATE_TO_ALL_IN_RANGE				23
#define FW_UPDATE_FROM_CONFIG_FILE				24

#define FILE_UPDATE_IDLE						40
#define FILE_UPDATE_RUN							41
#define FILE_UPDATE_FAIL						42
#define FILE_UPDATE_FINISHED					43
#define FILE_UPDATE_IMAGE_1 					44
#define FILE_UPDATE_IMAGE_2 					45
#define FILE_UPDATE_IMAGE_3 					46
#define FILE_UPDATE_IMAGE_4 					47
#define FILE_UPDATE_IMAGE_5 					48
#define FILE_UPDATE_IMAGE_6 					49
#define FILE_UPDATE_IMAGE_7 					50
#define FILE_UPDATE_IMAGE_8 					51
#define FILE_UPDATE_IMAGE_9 					52
#define FILE_UPDATE_IMAGE_10 					53
#define FILE_UPDATE_IMAGE_11 					54
#define FILE_UPDATE_IMAGE_12 					55
#define FILE_UPDATE_IMAGE_13 					56
#define FILE_UPDATE_IMAGE_14 					57
#define FILE_UPDATE_IMAGE_15 					58
#define FILE_UPDATE_IMAGE_16 					59
#define FILE_UPDATE_IMAGE_17 					60
#define FILE_UPDATE_IMAGE_18 					61
#define FILE_UPDATE_IMAGE_19 					62
#define FILE_UPDATE_ALL_IMAGE_TO_SELECTED		63
#define FILE_UPDATE_ALL_IMAGE_TO_ALL_IN_RANGE	64
#define FILE_UPDATE_FROM_CONFIG_FILE			65

#define LOG_TRANSFER_IDLE						70
#define LOG_TRANSFER_QUERY_LIST					71
#define LOG_TRANSFER_DELETE_LOG					72
#define LOG_TRANSFER_FAIL						73

#define HTTP_LOG_TRANSFER_IDLE					80
#define HTTP_GET_LOG_LIST						81
#define HTTP_LOG_LIST_READY						82
#define HTTP_DELETE_LOG_LIST					83
#define HTTP_LOG_LIST_DELETED					84
#define HTTP_LOG_TRANSFER_FAIL					85
#define HTTP_GET_RUBICON_TEMPERATURE			86
#define HTTP_RUBICON_TEMPERATURE_READY			87
#define HTTP_SET_RUBICON_TEMPERATURE			88
#define HTTP_RUBICON_TEMPERATURE_FAIL			89
#define HTTP_GET_RUBICON_ROOM_STATUS			90
#define HTTP_RUBICON_ROOM_STATUS_READY			91
#define HTTP_SET_RUBICON_ROOM_STATUS			92
#define HTTP_RUBICON_ROOM_STATUS_FAIL			93

#define CMD_IDLE								128
#define CMD_REQUEST								129
#define CMD_SEND								130
#define CMD_RECEIVE								131
#define CMD_COMPLETED							132
#define CMD_TERMINATED							133
#define CMD_OK									134
#define CMD_P2P 								135
#define CMD_GROUP								136	
#define CMD_BROADCAST							137
#define CMD_INVALID_ERROR						138
#define CMD_MAX_ATTEMPT_ERROR					139
#define CMD_TIMEOUT_ERROR						140
#define CMD_PARAMETAR_INVALID_ERROR				141

/* Exported types    ---------------------------------------------------------*/
typedef enum {
    RUBICON_INIT 					= 0x00,
    RUBICON_PACKET_ENUMERATOR 		= 0x01,
    RUBICON_PACKET_SEND 			= 0x02,
    RUBICON_PACKET_PENDING 			= 0x03,
	RUBICON_PACKET_RECEIVING		= 0x04,
    RUBICON_PACKET_RECEIVED 		= 0x05,
    RUBICON_PACKET_ERROR 			= 0x06

} eRubiconStateTypeDef;

typedef enum {
    RUBICON_UPDATE_INIT 			= 0x00,
    RUBICON_UPDATE_TIME 			= 0x01,
    RUBICON_UPDATE_STATUS 			= 0x02,
    RUBICON_UPDATE_FIRMWARE 		= 0x03,
    RUBICON_UPDATE_FILE 			= 0x04,
    RUBICON_UPDATE_LOG 				= 0x05,
    RUBICON_HTTP_REQUEST 			= 0x06,
    RUBICON_NO_UPDATE 				= 0x07

} eRubiconUpdateTypeDef;

typedef enum {
    RUBICON_TIME_UPDATE_INIT 		= 0x00,
    RUBICON_TIME_UPDATE_P2P 		= 0x01,
    RUBICON_TIME_UPDATE_GROUP		= 0x02,
    RUBICON_TIME_UPDATE_BROADCAST	= 0x03,
    RUBICON_NO_TIME_UPDATE 			= 0x04,

} eRubiconTimeUpdateTypeDef;

typedef enum {
	LOG_LIST_UNDEFINED	= 0x00,
	LOG_LIST_TYPE_1		= 0x01,
	LOG_LIST_TYPE_2		= 0x02,
	LOG_LIST_TYPE_3		= 0x03,
	LOG_LIST_TYPE_4		= 0x04,
	LOG_LIST_TYPE_5		= 0x05,
	LOG_LIST_TYPE_6		= 0x06
	
}LOG_MemoryFragmentTypeDef;

typedef struct {
	LOG_MemoryFragmentTypeDef LOG_MemoryFragment;
    uint16_t log_list_cnt;
	uint32_t first_log_address;
    uint32_t last_log_address;
	uint32_t next_log_address;
	
} RUBICON_LogMemoryTypeDef;

typedef struct {
    uint8_t log_transfer_state;
    uint8_t last_attempt;
    uint8_t send_attempt;
	uint32_t log_transfer_end_address;
	
} RUBICON_LogListTransferTypeDef;

typedef struct {
    uint8_t update_state;
    uint8_t send_attempt;
    uint32_t packet_total;
    uint32_t packet_send;
    uint32_t last_packet_send;
    uint16_t file_data_read;

} RUBICON_UpdatePacketTypeDef;


struct s_rs485_pcb;
typedef struct s_rs485_pcb t_rs485_pcb;


/* Exported variables  -------------------------------------------------------*/
extern volatile uint32_t rubicon_timer;
extern volatile uint32_t rubicon_flags;
extern volatile uint32_t rubicon_display_timer;
extern volatile uint32_t rubicon_rx_tx_timer;
extern volatile uint32_t rubicon_fw_update_timer;
extern volatile uint32_t rubicon_tftp_file;
extern volatile uint32_t rubicon_response_timer;
extern uint16_t rs485_rubicon_address;
extern uint16_t rs485_interface_address;
extern uint16_t rs485_broadcast_address;
extern uint16_t rs485_group_address;
extern uint8_t rubicon_ctrl_buffer[RUBICON_BUFFER_SIZE];
extern uint8_t *p_rubicon_buffer;
extern uint8_t rubicon_cmd_request;
extern uint8_t rubicon_http_cmd_state;

extern eRubiconStateTypeDef eRubiconTransferState;
extern eRubiconUpdateTypeDef eRubiconUpdate;
extern RUBICON_LogMemoryTypeDef RUBICON_LogMemory;
extern RUBICON_LogListTransferTypeDef HTTP_LogListTransfer;


/* Exported macros     -------------------------------------------------------*/
#define RUBICON_StartTimer(TIME)			((rubicon_timer = TIME), (rubicon_flags &= 0xfffffffe))
#define RUBICON_StopTimer()					(rubicon_flags |= 0x00000001)
#define IsRUBICON_TimerExpired()			(rubicon_flags & 0x00000001)	
#define RUBICON_StartDisplayTimer()			((rubicon_display_timer = MSG_DISPL_TIME),(rubicon_flags &= 0xfffffffd)) 
#define RUBICON_StopDisplayTimer()			(rubicon_flags |= 0x00000002)
#define IsRUBICON_DisplayTimerExpired()		(rubicon_flags & 0x00000002)	
#define RUBICON_StartRxTxTimeoutTimer(TIME)	((rubicon_rx_tx_timer = TIME),(rubicon_flags &= 0xfffffffb)) 
#define RUBICON_StopRxTxTimeoutTimer()		(rubicon_flags |= 0x00000004)
#define IsRUBICON_RxTxTimeoutTimerExpired()	(rubicon_flags & 0x00000004)
#define RUBICON_StartFwUpdateTimer(TIME)	((rubicon_fw_update_timer = TIME),(rubicon_flags &= 0xfffffff7)) 
#define RUBICON_StopFwUpdateTimer()			(rubicon_flags |= 0x00000008)
#define IsRUBICON_FwUpdateTimerExpired()	(rubicon_flags & 0x00000008)
#define RUBICON_StartResponseTimer(TIME)	((rubicon_response_timer = TIME),(rubicon_flags &= 0xffffffef)) 
#define RUBICON_StopResponseTimer()			(rubicon_flags |= 0x00000010)
#define IsRUBICON_ResponseTimerExpired()	(rubicon_flags & 0x00000010)

/* Exported functions  -------------------------------------------------------*/
void RUBICON_Init(void);
//void RUBICON_ProcessService(void);
void RUBICON_SystemService(void);
void RUBICON_PrepareLogUpdatePacket(void);
void RUBICON_PrepareCommandPacket(uint8_t command, uint8_t *ibuff);
void RUBICON_WriteLogToList(void);
void RUBICON_DeleteBlockFromLogList(void);
void RUBICON_ReadBlockFromLogList(void);
void RUBICON_CmdParse(void);
//uint8_t RUBICON_CheckNewFirmwareFile(void);
//uint8_t RUBICON_CheckNewImageFile(void);
uint16_t RUBICON_GetNextAddress(void);
uint16_t RUBICON_GetGroupAddress(uint16_t group);
uint16_t RUBICON_GetBroadcastAddress(void);
uint8_t RUBICON_CheckConfigFile(void);
uint8_t RUBICON_CreateUpdateAddresseList(void);
int RUBICON_ScanRS485_Bus(uint16_t start_address, uint16_t end_address, uint8_t option);

#endif
/******************************   END OF FILE  **********************************/

