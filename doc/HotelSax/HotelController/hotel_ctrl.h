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
#define HOTEL_ROOM_CONTROLLER_H   				100	// version 1.00

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
#define RUBICON_NOT_RESPONDING					((uint8_t)0xbf)

#define FANCOIL_RPM_SENSOR_ERROR                ((uint8_t)0xc0)
#define FANCOIL_NTC_SENSOR_ERROR                ((uint8_t)0xc1)
#define FANCOIL_LO_TEMP_ERROR                   ((uint8_t)0xc2)
#define FANCOIL_HI_TEMP_ERROR                   ((uint8_t)0xc3)
#define FANCOIL_FREEZING_PROTECTION             ((uint8_t)0xc4)
#define THERMOSTAT_NTC_SENSOR_ERROR             ((uint8_t)0xc5)
#define THERMOSTAT_ERROR                        ((uint8_t)0xc6) // + 0-e = event (0xd0-0xde)

#define RUBICON_RS485_BUS_ERROR					((uint8_t)0xca)
//#define RUBICON_BOOTLOADER_UPDATE_FAIL        ((uint8_t)0xcb) // old version
//#define RUBICON_BOOTLOADER_UPDATED            ((uint8_t)0xcc)	// old version
//#define RUBICON_IMAGE_UPDATED					((uint8_t)0xcd)	// old version
//#define RUBICON_FIRMWARE_UPDATE_FAIL			((uint8_t)0xce)	// old version
//#define RUBICON_IMAGE_UPDATE_FAIL				((uint8_t)0xcf)	// old version

#define RUBICON_PIN_RESET						((uint8_t)0xd0)
#define RUBICON_POWER_ON_RESET					((uint8_t)0xd1)
#define RUBICON_SOFTWARE_RESET					((uint8_t)0xd2)
#define RUBICON_IWDG_RESET						((uint8_t)0xd3)
#define RUBICON_WWDG_RESET						((uint8_t)0xd4)
#define RUBICON_LOW_POWER_RESET					((uint8_t)0xd5)
#define RUBICON_FIRMWARE_UPDATE					((uint8_t)0xd6)
#define RUBICON_FIRMWARE_UPDATE_FAIL		    ((uint8_t)0xd7)
#define RUBICON_BOOTLOADER_UPDATED			    ((uint8_t)0xd8)
#define RUBICON_BOOTLOADER_UPDATE_FAIL		    ((uint8_t)0xd9)
#define RUBICON_IMAGE_UPDATED				    ((uint8_t)0xda)
#define RUBICON_IMAGE_UPDATE_FAIL			    ((uint8_t)0xdb)
#define RUBICON_DISPLAY_FAIL				    ((uint8_t)0xdc)
#define RUBICON_DRIVER_OR_FUNCTION_FAIL         ((uint8_t)0xdd)

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
#define RUBICON_CONTROLLER_RESET				((uint8_t)0xfb)
#define RUBICON_ENTRY_DOOR_NOT_CLOSED			((uint8_t)0xfc)
#define	RUBICON_DOOR_BELL_ACTIVE				((uint8_t)0xfd)
#define	RUBICON_DOOR_LOCK_USER_OPEN             ((uint8_t)0xfe)


/** ==========================================================================*/
/**                                                                           */
/**    R U B I C O N    E R R O R       T R A C K I N G         L O G         */
/**                                                                           */
/** ==========================================================================*/
#define RUBICON_SPI_DRIVER_FAIL                 1
#define RUBICON_I2C_DRIVER_FAIL                 2
#define RUBICON_USART_DRIVER_FAIL               3
#define RUBICON_RTC_DRIVER_FAIL                 4
#define RUBICON_TMR_DRIVER_FAIL                 5

#define RUBICON_CAP_FUNC_FAIL                   11
#define RUBICON_RC522_FUNC_FAIL                 12
#define RUBICON_ONEWIRE_FUNC_FAIL               13
#define RUBICON_RS485_FUNC_FAIL                 14
#define RUBICON_MAIN_FUNC_FAIL                  15 
#define RUBICON_DISPLAY_FUNC_FAIL               16
#define RUBICON_LOGGER_FUNC_FAIL                17
#define RUBICON_DIO_FUNC_FAIL                   18
#define RUBICON_EEPROM_FUNC_FAIL                19
#define RUBICON_SIGNAL_FUNC_FAIL                20

/** ==========================================================================*/
/**                                                                           */
/**    R U B I C O N    R S 4 8 5   P R O T O C O L     C O N S T A N T S     */
/**                                                                           */
/** ==========================================================================*/
#define RUBICON_DEFAULT_INTERFACE_ADDRESS		((uint32_t)0x0400)
#define RUBICON_DEFFAULT_GROUP_ADDRESS			((uint32_t)0x6776)
#define RUBICON_DEFFAULT_BROADCAST_ADDRESS		((uint32_t)0x9999)
#define	FIRMWARE_UPDATED						'N'
#define	FIRMWARE_NOT_UPDATED					'O'
#define RS485_BAUDRATE_2400						'0'
#define RS485_BAUDRATE_4800						'1'
#define RS485_BAUDRATE_9600						'2'
#define RS485_BAUDRATE_19200					'3'
#define RS485_BAUDRATE_38400					'4'
#define RS485_BAUDRATE_56700					'5'
#define RS485_BAUDRATE_115200					'6'
#define RS485_BAUDRATE_230400					'7'
#define RS485_BAUDRATE_460800					'8'
#define RS485_BAUDRATE_921600					'9'
#define RS485_BUS_ERROR							1
#define RS485_BUS_CONNECTED						2
#define RS485_BUS_DISCONNECTED					3

#define RUBICON_SOH                 			((uint8_t)0x01) 	/* start of command packet */
#define RUBICON_STX                  			((uint8_t)0x02) 	/* start of 1024-byte data packet */
#define RUBICON_EOT                       		((uint8_t)0x04) 	/* end of transmission */
#define RUBICON_ACK                    			((uint8_t)0x06) 	/* acknowledge */
#define RUBICON_NAK                     		((uint8_t)0x15) 	/* negative acknowledge */

#define RS485_INTERFACE_DEFAULT_ADDRESS			0x0005
#define RUBICON_TIME_UPDATE_PERIOD				6789
#define RUBICON_RESTART_TIME					12345
#define MSG_DISPL_TIME							567
#define RUBICON_BUFFER_SIZE						512
#define RUBICON_PACKET_BUFFER_SIZE				64
#define RUBICON_IMAGE_COPY_TIME					4567
#define RUBICON_BOOTLOADER_START_TIME			3456
#define RUBICON_FW_UPLOAD_TIMEOUT				2345
#define RUBICON_FW_EXE_BOOT_TIME				1567
#define RUBICON_FILE_UPLOAD_TIMEOUT				321
#define RUBICON_LOG_SIZE						16
#define RUBICON_RESPONSE_TIMEOUT				78	
#define RUBICON_BYTE_RX_TIMEOUT					3	
#define RUBICON_RX_TO_TX_DELAY					10
#define RUBICON_MAX_ERRORS          			10
#define RUBICON_HTTP_RESPONSE_TIMEOUT			189
#define RUBICON_CONFIG_FILE_MAX_SIZE			(RUBICON_BUFFER_SIZE - 16)
#define RUBICON_CONFIG_FILE_BUFFER_SIZE			64
#define RUBICON_CONFIG_FILE_TAG_LENGHT			5
#define RUBICON_FILE_UPDATE_LIST_SIZE 			32
#define RUBICON_JOURNAL_SIZE					128
#define RUBICON_ROOM_STATUS_UPDATE_PERIOD		10
#define WEATHER_FORECAST_UPDATE_PERIOD          120000U // 2 min. weather forecast update period
#define WEATHER_FORECAST_DATA_SIZE              21U
/** ==========================================================================*/
/**                                                                           */
/**    	R U B I C O N    R S 4 8 5	  C O M M A N D		L I S T           	  */
/**                                                                           */
/** ==========================================================================*/
#define RUBICON_DOWNLOAD_DIPLAY_IMAGE			((uint8_t)0x63)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_1 		((uint8_t)0x64)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_2 		((uint8_t)0x65)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_3		((uint8_t)0x66)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_4		((uint8_t)0x67)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_5		((uint8_t)0x68)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_6		((uint8_t)0x69)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_7		((uint8_t)0x6a)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_8		((uint8_t)0x6b)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_9		((uint8_t)0x6c)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_10		((uint8_t)0x6d)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_11		((uint8_t)0x6e)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_12		((uint8_t)0x6f)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_13		((uint8_t)0x70)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_14		((uint8_t)0x71)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_15		((uint8_t)0x72)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_16		((uint8_t)0x73)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_17		((uint8_t)0x74)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_18		((uint8_t)0x75)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_19		((uint8_t)0x76)
#define RUBICON_DOWNLOAD_FIRMWARE_IMAGE			((uint8_t)0x77)
#define RUBICON_DOWNLOAD_BOOTLOADER_IMAGE		((uint8_t)0x78)
#define RUBICON_DOWNLOAD_SMALL_FONT				((uint8_t)0x79)
#define RUBICON_DOWNLOAD_MIDDLE_FONT			((uint8_t)0x7a)
#define RUBICON_DOWNLOAD_BIG_FONT				((uint8_t)0x7b)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_25 		((uint8_t)0x7c)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_26 		((uint8_t)0x7d)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_27		((uint8_t)0x7e)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_28		((uint8_t)0x7f)
#define RUBICON_DOWNLOAD_DISPLAY_IMAGE_29 		((uint8_t)0x80)

#define RUBICON_DOWNLOAD_JOURNAL_1				((uint8_t)0x85)
#define RUBICON_DOWNLOAD_WEATHER_FORECAST       ((uint8_t)0x86)

#define RUBICON_GET_SYS_STATUS					((uint8_t)0xa0)
#define RUBICON_GET_ROOM_STATUS					((uint8_t)0xa1)
#define RUBICON_GET_RS485_CONFIG				((uint8_t)0xa2)
#define RUBICON_GET_LOG_LIST 					((uint8_t)0xa3)
#define RUBICON_GET_MIFARE_PERMITED_GROUP		((uint8_t)0xa4)
#define RUBICON_GET_MIFARE_AUTHENTICATION_KEY_A	((uint8_t)0xa5)
#define RUBICON_GET_MIFARE_AUTHENTICATION_KEY_B	((uint8_t)0xa6)


#define RUBICON_EXECUTE_APPLICATION				((uint8_t)0xbb)
#define RUBICON_START_BOOTLOADER				((uint8_t)0xbc)
#define RUBICON_FLASH_PROTECTION_DISABLE		((uint8_t)0xbd)
#define RUBICON_FLASH_PROTECTION_ENABLE			((uint8_t)0xbe)
#define RUBICON_DOWNLOAD_FIRMWARE           	((uint8_t)0xbf)

#define RUBICON_RESTART_CONTROLLER 				((uint8_t)0xc0)
#define RUBICON_UPDATE_FIRMWARE           		((uint8_t)0xc1)
#define RUBICON_UPDATE_BOOTLOADER				((uint8_t)0xc2)

#define RUBICON_SET_ROOM_STATUS					((uint8_t)0xd0)
#define RUBICON_SET_RS485_CONFIG				((uint8_t)0xd1)
#define RUBICON_SET_DOUT_STATE					((uint8_t)0xd2)
#define RUBICON_DELETE_LOG_LIST 				((uint8_t)0xd3)
#define RUBICON_RESET_SOS_ALARM					((uint8_t)0xd4)
#define RUBICON_SET_RTC_DATE_TIME				((uint8_t)0xd5)
#define RUBICON_SET_ROOM_TEMPERATURE			((uint8_t)0xd6)
#define RUBICON_SET_DISPLAY_BRIGHTNESS			((uint8_t)0xd7)
#define RUBICON_SET_SYSTEM_ID					((uint8_t)0xd8)
#define RUBICON_PREVIEW_DISPLAY_IMAGE			((uint8_t)0xd9)
#define RUBICON_SET_BEDDING_REPLACEMENT_PERIOD	((uint8_t)0xda)
#define RUBICON_SET_MIFARE_PERMITED_GROUP 		((uint8_t)0xdb)
#define RUBICON_SET_MIFARE_KEY_A                ((uint8_t)0xdc)
#define RUBICON_SET_MIFARE_KEY_B                ((uint8_t)0xdd)
#define RUBICON_SET_ROOM_THERMOSTAT_IMAGE	    ((uint8_t)0xde)

/** ==========================================================================*/
/**                                                                           */
/**    	C O N T R O L L E R 	C O M M A N D		L I S T            		  */
/**                                                                           */
/** ==========================================================================*/
#define HTTP_STATUS_RESPONSE_TIMEOUT            34
#define HTTP_STATUS_RESPONSE_ERROR              35
#define HTTP_STATUS_RESPONSE_BUSY               36
#define HTTP_STATUS_RESPONSE_OK                 37
#define HTTP_LOG_TRANSFER_IDLE					38
#define HTTP_GET_LOG_LIST						39
#define HTTP_LOG_LIST_READY						40
#define HTTP_DELETE_LOG_LIST					41
#define HTTP_LOG_LIST_DELETED					42
#define HTTP_FORMAT_LOG_LIST					43
#define HTTP_LOG_LIST_FORMATED					44
#define HTTP_LOG_TRANSFER_FAIL					45
#define HTTP_GET_RUBICON_TEMPERATURE			46
#define HTTP_RUBICON_TEMPERATURE_READY			47
#define HTTP_SET_RUBICON_TEMPERATURE			48
#define HTTP_RUBICON_TEMPERATURE_FAIL			49
#define HTTP_GET_RUBICON_ROOM_STATUS			50
#define HTTP_RUBICON_ROOM_STATUS_READY			51
#define HTTP_SET_RUBICON_ROOM_STATUS			52
#define HTTP_RUBICON_ROOM_STATUS_FAIL			53
#define HTTP_GET_RUBICON_SYS_INFO				54
#define HTTP_RUBICON_SYS_INFO_READY				55
#define HTTP_RUBICON_SYS_INFO_FAIL				56

#define FILE_OK									1
#define FILE_SYS_ERROR							2	
#define FILE_DIR_ERROR							3
#define FILE_ERROR								4
#define OUT_OF_MEMORY_ERROR						5

#define FW_UPDATE_IDLE							16
#define FW_UPDATE_INIT 							17
#define FW_UPDATE_BOOTLOADER 					18
#define FW_UPDATE_RUN							19
#define FW_UPDATE_FINISHED						20
#define FW_UPDATE_FAIL							21
#define FW_UPDATE_FROM_CONFIG_FILE				22
#define FW_UPDATE_TO_FLASH						23

#define LOG_TRANSFER_IDLE						30
#define LOG_TRANSFER_QUERY_LIST					31
#define LOG_TRANSFER_DELETE_LOG					32
#define LOG_TRANSFER_FAIL						33

#define FILE_UPDATE_IDLE						40
#define FILE_UPDATE_IMAGE_1 					41
#define FILE_UPDATE_IMAGE_2 					42
#define FILE_UPDATE_IMAGE_3 					43
#define FILE_UPDATE_IMAGE_4 					44
#define FILE_UPDATE_IMAGE_5 					45
#define FILE_UPDATE_IMAGE_6 					46
#define FILE_UPDATE_IMAGE_7 					47
#define FILE_UPDATE_IMAGE_8 					48
#define FILE_UPDATE_IMAGE_9 					49
#define FILE_UPDATE_IMAGE_10 					50
#define FILE_UPDATE_IMAGE_11 					51
#define FILE_UPDATE_IMAGE_12 					52
#define FILE_UPDATE_IMAGE_13 					53
#define FILE_UPDATE_IMAGE_14 					54
#define FILE_UPDATE_IMAGE_15 					55
#define FILE_UPDATE_IMAGE_16 					56
#define FILE_UPDATE_IMAGE_17 					57
#define FILE_UPDATE_IMAGE_18 					58
#define FILE_UPDATE_IMAGE_19 					59
#define FILE_UPDATE_IMAGE_20 					60
#define FILE_UPDATE_IMAGE_21 					61
#define FILE_UPDATE_IMAGE_22 					62
#define FILE_UPDATE_IMAGE_23 					63
#define FILE_UPDATE_IMAGE_24 					64
#define FILE_UPDATE_IMAGE_25 					65
#define FILE_UPDATE_IMAGE_26 					66
#define FILE_UPDATE_IMAGE_27 					67
#define FILE_UPDATE_IMAGE_28 					68
#define FILE_UPDATE_IMAGE_29 					69

#define FILE_UPDATE_RUN							70
#define FILE_UPDATE_FINISHED					71
#define FILE_UPDATE_FAIL						72
#define FILE_UPDATE_FROM_CONFIG_FILE			73

#define RS485_SCANNER_FIND_FIRST				0
#define RS485_SCANNER_FIND_NEXT					1
#define RS485_SCANNER_FIND_NEW					2
#define RS485_SCANNER_FIND_ALL					3
#define RS485_SCANNER_FIND_ADDRESSED			4

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
    RUBICON_UPDATE_SYS_STATUS 		= 0x02,
	RUBICON_UPDATE_ROOM_STATUS		= 0x03,
    RUBICON_UPDATE_BINARY 			= 0x04,
    RUBICON_UPDATE_FILE 			= 0x05,
    RUBICON_UPDATE_LOG 				= 0x06,
    RUBICON_HTTP_REQUEST 			= 0x07,
    RUBICON_NO_UPDATE 				= 0x08

} eRubiconUpdateTypeDef;

typedef enum {
    RUBICON_TIME_UPDATE_INIT 		= 0x00,
    RUBICON_TIME_UPDATE_P2P 		= 0x01,
    RUBICON_TIME_UPDATE_GROUP		= 0x02,
    RUBICON_TIME_UPDATE_BROADCAST	= 0x03,
    RUBICON_NO_TIME_UPDATE 			= 0x04,

} eRubiconTimeUpdateTypeDef;

typedef enum
{
	LOG_LIST_UNDEFINED	= 0x00,
	LOG_LIST_TYPE_1		= 0x01,
	LOG_LIST_TYPE_2		= 0x02,
	LOG_LIST_TYPE_3		= 0x03,
	LOG_LIST_TYPE_4		= 0x04,
	LOG_LIST_TYPE_5		= 0x05,
	LOG_LIST_TYPE_6		= 0x06
	
}LOG_MemoryFragmentTypeDef;

typedef struct 
{
	LOG_MemoryFragmentTypeDef LOG_MemoryFragment;
    uint16_t log_list_cnt;
	uint32_t first_log_address;
    uint32_t last_log_address;
	uint32_t next_log_address;
	
} RUBICON_LogMemoryTypeDef;

typedef struct
{
    uint8_t log_transfer_state;
    uint8_t last_attempt;
    uint8_t send_attempt;
	uint32_t log_transfer_end_address;
	
} RUBICON_LogListTransferTypeDef;

typedef struct
{
	uint8_t update_command;
    uint8_t update_state;
    uint8_t send_attempt;
    uint32_t packet_total;
    uint32_t packet_send;
    uint32_t last_packet_send;
    uint32_t file_data_read;

} RUBICON_UpdatePacketTypeDef;


#define RUBICON_ROOM_IDLE					((uint8_t)0x30)
#define RUBICON_ROOM_READY 					((uint8_t)0x31)
#define RUBICON_ROOM_BUSY					((uint8_t)0x32)
#define RUBICON_ROOM_CLEANING				((uint8_t)0x33)
#define RUBICON_ROOM_BEDDING_REPLACEMENT	((uint8_t)0x34)
#define RUBICON_ROOM_GENERAL_CLEANING		((uint8_t)0x35)
#define RUBICON_ROOM_OUT_OF_ORDER			((uint8_t)0x36)
#define RUBICON_ROOM_LATE_CHECKOUT			((uint8_t)0x37)
#define RUBICON_ROOM_HANDMAID_IN			((uint8_t)0x38)
#define RUBICON_ROOM_FORCING_DND			((uint8_t)0x39)
#define RUBICON_ROOM_FIRE_ALARM				((uint8_t)0x3a)
#define RUBICON_ROOM_FIRE_EXIT				((uint8_t)0x3b)


/* Exported variables  -------------------------------------------------------*/
extern volatile uint32_t rubicon_timer;
extern volatile uint32_t rubicon_flags;
extern volatile uint32_t rubicon_display_timer;
extern volatile uint32_t rubicon_rx_timer;
extern volatile uint32_t rubicon_fw_update_timer;
extern volatile uint32_t rubicon_tftp_file;
extern volatile uint32_t rubicon_response_timer;
extern uint16_t system_id;
extern uint16_t rs485_rubicon_address;
extern uint16_t rs485_interface_address;
extern uint16_t rs485_broadcast_address;
extern uint16_t rs485_group_address;
extern uint8_t rs485_interface_baudrate;
extern uint8_t rs485_bus_status;
extern uint8_t rubicon_ctrl_buffer[RUBICON_BUFFER_SIZE];
extern uint8_t *p_rubicon_buffer;
extern uint8_t rubicon_ctrl_request;
extern uint8_t rubicon_http_cmd_state;
extern uint8_t config_file_image_cnt;
extern uint8_t rubicon_file_update_list[RUBICON_FILE_UPDATE_LIST_SIZE];
extern uint16_t rubicon_file_update_list_cnt;
extern uint16_t rubiconJournal_1[RUBICON_JOURNAL_SIZE];
extern uint8_t rubicon_journal_list_item;
extern uint8_t rubicon_status_request_cnt;

extern uint16_t *rubicon_address_list;
extern uint16_t rubicon_address_list_size;
extern uint16_t rubicon_address_list_cnt;

extern uint16_t *rubicon_firmware_update_address_list;
extern uint16_t rubicon_firmware_update_address_list_size;
extern uint16_t rubicon_firmware_update_list_cnt;

extern uint16_t *rubicon_image_update_address_list;
extern uint16_t rubicon_image_update_address_list_size;
extern uint16_t rubicon_image_update_list_cnt;

extern RUBICON_UpdatePacketTypeDef RUBICON_FileUpdatePacket;
extern RUBICON_UpdatePacketTypeDef RUBICON_FirmwareUpdatePacket;
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
#define RUBICON_StartRxTimeoutTimer(TIME)	((rubicon_rx_timer = TIME),(rubicon_flags &= 0xfffffffb)) 
#define RUBICON_StopRxTimeoutTimer()		(rubicon_flags |= 0x00000004)
#define IsRUBICON_RxTimeoutTimerExpired()	(rubicon_flags & 0x00000004)
#define RUBICON_StartFwUpdateTimer(TIME)	((rubicon_fw_update_timer = TIME),(rubicon_flags &= 0xfffffff7)) 
#define RUBICON_StopFwUpdateTimer()			(rubicon_flags |= 0x00000008)
#define IsRUBICON_FwUpdateTimerExpired()	(rubicon_flags & 0x00000008)
#define RUBICON_StartResponseTimer(TIME)	((rubicon_response_timer = TIME),(rubicon_flags &= 0xffffffef)) 
#define RUBICON_StopResponseTimer()			(rubicon_flags |= 0x00000010)
#define IsRUBICON_ResponseTimerExpired()	(rubicon_flags & 0x00000010)
#define RS485_BusStatusFlagSet()			(rubicon_flags |= 0x00000020)
#define RS485_BusStatusFlagReset()			(rubicon_flags &= 0xffffffdf)
#define IsRS485BusStatusFlagSet()			(rubicon_flags & 0x00000020)
#define RUBICON_RoomStatusUpdateSet()		(rubicon_flags |= 0x00000040)
#define RUBICON_RoomStatusUpdateReset()		(rubicon_flags &= 0xffffffbf)
#define IsRUBICON_RoomStatusUpdateSet()		(rubicon_flags & 0x00000040)
#define RUBICON_RoomStatusRequestSet()		(rubicon_flags |= 0x00000080)
#define RUBICON_RoomStatusRequestReset()	(rubicon_flags &= 0xffffff7f)
#define IsRUBICON_RoomStatusRequestSet()	(rubicon_flags & 0x00000080)
#define RUBICON_UpdateJournalCmdSet()		(rubicon_flags |= 0x00000100)
#define RUBICON_UpdateJournalCmdReset()		(rubicon_flags &= 0xfffffeff)
#define IsRUBICON_UpdateJournalCmdSet()		(rubicon_flags & 0x00000100)

/* Exported functions  -------------------------------------------------------*/
void RUBICON_Init(void);
void RUBICON_ProcessService(void);
void RUBICON_PrepareTimeUpdatePacket(void);
void RUBICON_PrepareSysStatusRequestPacket(void);
void RUBICON_PrepareRoomStatusRequestPacket(void);
void RUBICON_PrepareFirmwareUpdatePacket(void);
void RUBICON_PrepareFileUpdatePacket(void);
void RUBICON_PrepareLogUpdatePacket(void);
void RUBICON_PrepareCommandPacket(uint8_t command, uint8_t *ibuff);
void RUBICON_WriteLogToList(uint8_t source);
void RUBICON_DeleteBlockFromLogList(void);
void RUBICON_FormatLogList(void);
void RUBICON_ReadBlockFromLogList(void);
uint8_t RUBICON_LoadAddressList(void);
uint8_t RUBICON_CreateAddressList(void);
uint8_t RUBICON_CheckNewFirmwareFile(void);
uint8_t RUBICON_CheckNewImageFile(void);
uint16_t RUBICON_GetNextAddress(void);
uint8_t RUBICON_CheckConfigFile(void);
uint8_t RUBICON_CreateUpdateAddresseList(void);
uint8_t RUBICON_CreateJournalList(void);
void CONTROLLER_WriteLogEvent(uint8_t log_event, uint8_t event_detail);
int RUBICON_ScanRS485_Bus(uint16_t start_address, uint16_t end_address, uint8_t option);
void delay(__IO uint32_t nCount);

#endif
/******************************   END OF FILE  **********************************/

