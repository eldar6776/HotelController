/**
 ******************************************************************************
 * File Name          : hotel_room_controller.c
 * Date               : 21/08/2016 20:59:16
 * Description        : hotel room Rubicon controller data link modul
 ******************************************************************************
 *
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
//#include "GUI.h"
#include "DIALOG.h"
#include "hotel_ctrl.h"
#include "stm32f429i_lcd.h"
#include "common.h"
#include "uart.h"

#include "ff.h"
#include "i2c_eeprom.h"
#include "buzzer.h"
#include "w25q16.h"
#include "display.h"
#include "lwip/mem.h"
#include "main.h"

/* Private typedef -----------------------------------------------------------*/
eRubiconStateTypeDef eRubiconTransferState = RUBICON_INIT;
eRubiconTimeUpdateTypeDef eRubiconTimeUpdate = RUBICON_TIME_UPDATE_INIT;
eRubiconUpdateTypeDef eRubiconUpdate = RUBICON_UPDATE_INIT;

uint16_t system_id;
uint16_t rs485_rubicon_address;
uint16_t rs485_interface_address;
uint16_t rs485_broadcast_address;
uint16_t rs485_group_address;
uint8_t rs485_interface_baudrate;
uint8_t rs485_bus_status;

/* Private define ------------------------------------------------------------*/
RUBICON_UpdatePacketTypeDef RUBICON_FileUpdatePacket;
RUBICON_UpdatePacketTypeDef RUBICON_FirmwareUpdatePacket;
RUBICON_LogListTransferTypeDef RUBICON_LogListTransfer;
RUBICON_LogListTransferTypeDef HTTP_LogListTransfer;
RUBICON_LogMemoryTypeDef RUBICON_LogMemory;


/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
volatile uint32_t rubicon_display_timer;
volatile uint32_t rubicon_timer;
volatile uint32_t rubicon_flags;
volatile uint32_t rubicon_rx_timer;
volatile uint32_t rubicon_fw_update_timer;
volatile uint32_t rubicon_tftp_file;
volatile uint32_t rubicon_response_timer;

uint32_t config_file_byte_cnt;
uint32_t crc_32_file;

uint16_t image_update_index;
uint16_t rs485_packet_checksum;
uint16_t rs485_packet_lenght;


uint8_t rubicon_ctrl_buffer[RUBICON_BUFFER_SIZE];
uint8_t *p_rubicon_buffer;
uint8_t config_file_buffer[RUBICON_CONFIG_FILE_BUFFER_SIZE];
uint8_t config_file_image_cnt;
uint8_t rubicon_ctrl_request;
uint8_t rubicon_http_cmd_state;
uint8_t rubicon_file_update_list[RUBICON_FILE_UPDATE_LIST_SIZE];
uint16_t rubicon_file_update_list_cnt;
uint16_t rubiconJournal_1[RUBICON_JOURNAL_SIZE];
uint8_t rubicon_journal_list_item;
uint8_t rubicon_status_request_cnt;
int rubicon_file_progress;
uint16_t *rubicon_address_list;
uint16_t rubicon_address_list_size;
uint16_t rubicon_address_list_cnt;

uint16_t *rubicon_firmware_update_address_list;
uint16_t rubicon_firmware_update_address_list_size;
uint16_t rubicon_firmware_update_list_cnt;

uint16_t *rubicon_image_update_address_list;
uint16_t rubicon_image_update_address_list_size;
uint16_t rubicon_image_update_list_cnt;

extern FATFS filesystem;
extern FIL file_SD, file_CR;
extern DIR dir_1, dir_2;
extern GUI_HMEM hPROGBAR_FileTransfer;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
void delay(__IO uint32_t nCount)
{
    __IO uint32_t index = 0;

    for (index = 80000 * nCount; index != 0; index--)
    {
    }
}

void RUBICON_Init(void)
{
	uint32_t temp_log_list_scan;
	
//	rs485_interface_address = RS485_INTERFACE_DEFAULT_ADDRESS;
//	rs485_broadcast_address = RUBICON_DEFFAULT_BROADCAST_ADDRESS;
//	rs485_group_address = RUBICON_DEFFAULT_GROUP_ADDRESS;
	
	RS485ModeGpio_Init();
	RS485_MODE(RS485_RX);
	
	rs485_bus_status = RS485_BUS_ERROR;	
	rs485_bus_status = RUBICON_CreateAddressList();
	if(rs485_bus_status != FILE_OK) rs485_bus_status = RS485_BUS_ERROR;
	else rs485_bus_status = RS485_BUS_DISCONNECTED;
	
	memset(rx_buffer,           0, DATA_BUF_SIZE);
	memset(tx_buffer,           0, DATA_BUF_SIZE);
	memset(i2c_ee_buffer,       0, I2C_EE_BUFFER_SIZE);
	memset(rubicon_ctrl_buffer, 0, RUBICON_BUFFER_SIZE);
	rubicon_address_list_cnt =  0;
//	rs485_interface_address = RS485_INTERFACE_DEFAULT_ADDRESS;
	
	eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
	eRubiconUpdate = RUBICON_NO_UPDATE;
	eRubiconTimeUpdate = RUBICON_TIME_UPDATE_BROADCAST;
	
	RUBICON_FirmwareUpdatePacket.update_state = FILE_UPDATE_IDLE;
	RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IDLE;
	
	RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
	RUBICON_FirmwareUpdatePacket.file_data_read = 0;
	RUBICON_FirmwareUpdatePacket.last_packet_send = 0;
	RUBICON_FirmwareUpdatePacket.packet_send = 0;
	RUBICON_FirmwareUpdatePacket.packet_total = 0;
	RUBICON_FirmwareUpdatePacket.send_attempt = 0;
	
	RUBICON_LogListTransfer.log_transfer_state = LOG_TRANSFER_IDLE;
	RUBICON_LogListTransfer.last_attempt = 0;
	RUBICON_LogListTransfer.send_attempt = 0;
	RUBICON_LogListTransfer.log_transfer_end_address = 0;
	
	RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_UNDEFINED;
	RUBICON_LogMemory.log_list_cnt = 0;
	RUBICON_LogMemory.first_log_address = 0;	
	RUBICON_LogMemory.last_log_address = 0;
	RUBICON_LogMemory.next_log_address = 0;
	
	/**
	*	LOG_LIST_TYPE_1 -> log list is empty and next log address is first address
	*	0000000000000000000000000000000000000000000000000000000000000000000000000
	*
	*	LOG_LIST_TYPE_2 -> log list start at some addres, it's full till last address, next log address is first address and is free for write 
	*	000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
	*
	* 	LOG_LIST_TYPE_3 -> log list start at some addres, end at upper address, next log address is upper address and is free for write
	*	000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxx000000000000000000000000000
	*
	*	LOG_LIST_TYPE_4 -> log list start at first address, end at last address, it's full, next log address is first memory address, write is forbiden
	*	xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
	*	
	*	LOG_LIST_TYPE_5 -> log list start at first address, end at upper address, and next upper log address is free for write
	*	xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00000000000000000000000000000000000000000
	*
	*	LOG_LIST_TYPE_6 -> log list start at upper address, end at lower address and next upper from end address is free for write
	*	xxxxxxxxxxxx0000000000000000000000000000000000000000000000xxxxxxxxxxxxxxx
	*/	
	
	temp_log_list_scan = I2C_EE_LOG_LIST_START_ADDRESS;
	I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, temp_log_list_scan, RUBICON_LOG_SIZE);
	
	/* CHECK FOR LOG_LIST_TYPE_1 */
	if((i2c_ee_buffer[0] == NULL) && (i2c_ee_buffer[1] == NULL)) 
	{
		RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_1;
		RUBICON_LogMemory.first_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
		RUBICON_LogMemory.last_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
		RUBICON_LogMemory.next_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
		
		temp_log_list_scan += RUBICON_LOG_SIZE;
		I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, temp_log_list_scan, RUBICON_LOG_SIZE);
		
		/* CHECK FOR LOG_LIST_TYPE_2 */
		while(temp_log_list_scan <= (I2C_EE_LOG_LIST_END_ADDRESS - RUBICON_LOG_SIZE))
		{
			if((i2c_ee_buffer[0] != NULL) || (i2c_ee_buffer[1] != NULL))
			{
				RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_2;
				RUBICON_LogMemory.first_log_address = temp_log_list_scan;
				RUBICON_LogMemory.last_log_address = (I2C_EE_LOG_LIST_END_ADDRESS - RUBICON_LOG_SIZE);
				++RUBICON_LogMemory.log_list_cnt;
				break;
			}
			else
			{
				temp_log_list_scan += RUBICON_LOG_SIZE;
				
				if(temp_log_list_scan < I2C_EE_PAGE_SIZE)
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, temp_log_list_scan, RUBICON_LOG_SIZE);
				}
				else
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_1, temp_log_list_scan, RUBICON_LOG_SIZE);
				}
			}
		}
		/* CHECK FOR LOG_LIST_TYPE_3 */
		if(RUBICON_LogMemory.LOG_MemoryFragment == LOG_LIST_TYPE_2)
		{
			temp_log_list_scan += RUBICON_LOG_SIZE;
			
			if(temp_log_list_scan <= (I2C_EE_LOG_LIST_END_ADDRESS - RUBICON_LOG_SIZE))
			{
				if(temp_log_list_scan < I2C_EE_PAGE_SIZE)
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, temp_log_list_scan, RUBICON_LOG_SIZE);
				}
				else
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_1, temp_log_list_scan, RUBICON_LOG_SIZE);
				}
				
				while(temp_log_list_scan <= (I2C_EE_LOG_LIST_END_ADDRESS - RUBICON_LOG_SIZE))
				{	
					if((i2c_ee_buffer[0] == NULL) && (i2c_ee_buffer[1] == NULL))
					{
						RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_3;
						RUBICON_LogMemory.last_log_address = temp_log_list_scan - RUBICON_LOG_SIZE;
						RUBICON_LogMemory.next_log_address = temp_log_list_scan;
						break;
					}
					else
					{	
						temp_log_list_scan += RUBICON_LOG_SIZE;
						++RUBICON_LogMemory.log_list_cnt;
						
						if(temp_log_list_scan < I2C_EE_PAGE_SIZE)
						{
							I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, temp_log_list_scan, RUBICON_LOG_SIZE);
						}
						else
						{
							I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_1, temp_log_list_scan, RUBICON_LOG_SIZE);
						}
					}					
				}
			}
		}
	}
	/* CHECK FOR LOG_LIST_TYPE_4 */
	else if((i2c_ee_buffer[0] != NULL) || (i2c_ee_buffer[1] != NULL))
	{
		RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_4;
		RUBICON_LogMemory.first_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
		RUBICON_LogMemory.last_log_address = I2C_EE_LOG_LIST_END_ADDRESS - RUBICON_LOG_SIZE;
		RUBICON_LogMemory.next_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
		++RUBICON_LogMemory.log_list_cnt;
		
		temp_log_list_scan += RUBICON_LOG_SIZE;
		I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, temp_log_list_scan, RUBICON_LOG_SIZE);
		
		/* CHECK FOR LOG_LIST_TYPE_5 */
		while(temp_log_list_scan <= (I2C_EE_LOG_LIST_END_ADDRESS - RUBICON_LOG_SIZE))
		{	
			if((i2c_ee_buffer[0] == NULL) && (i2c_ee_buffer[1] == NULL))
			{
				RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_5;
				RUBICON_LogMemory.last_log_address = temp_log_list_scan - RUBICON_LOG_SIZE;
				RUBICON_LogMemory.next_log_address = temp_log_list_scan;
				break;
			}
			else
			{
				temp_log_list_scan += RUBICON_LOG_SIZE;
				++RUBICON_LogMemory.log_list_cnt;

				if(temp_log_list_scan < I2C_EE_PAGE_SIZE)
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, temp_log_list_scan, RUBICON_LOG_SIZE);
				}
				else
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_1, temp_log_list_scan, RUBICON_LOG_SIZE);
				}
			}
		}
		/* CHECK FOR LOG_LIST_TYPE_6 */
		if(RUBICON_LogMemory.LOG_MemoryFragment == LOG_LIST_TYPE_5)
		{
			temp_log_list_scan += RUBICON_LOG_SIZE;
			
			if(temp_log_list_scan <= (I2C_EE_LOG_LIST_END_ADDRESS - RUBICON_LOG_SIZE))
			{
				if(temp_log_list_scan < I2C_EE_PAGE_SIZE)
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, temp_log_list_scan, RUBICON_LOG_SIZE);
				}
				else
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_1, temp_log_list_scan, RUBICON_LOG_SIZE);
				}
				
				while(temp_log_list_scan <= (I2C_EE_LOG_LIST_END_ADDRESS - RUBICON_LOG_SIZE))
				{	
					if((i2c_ee_buffer[0] != NULL) || (i2c_ee_buffer[1] != NULL))
					{
						RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_6;
						RUBICON_LogMemory.first_log_address = temp_log_list_scan;
						RUBICON_LogMemory.log_list_cnt += ((I2C_EE_LOG_LIST_END_ADDRESS - temp_log_list_scan) / RUBICON_LOG_SIZE);
						break;
					}
					else
					{	
						temp_log_list_scan += RUBICON_LOG_SIZE;
						
						if(temp_log_list_scan < I2C_EE_PAGE_SIZE)
						{
							I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, temp_log_list_scan, RUBICON_LOG_SIZE);
						}
						else
						{
							I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_1, temp_log_list_scan, RUBICON_LOG_SIZE);
						}
					}					
				}
			}
		}
	}
}

void RUBICON_ProcessService(void)
{
	static uint8_t old_rx_cnt;
    static uint32_t weather_forecast_update_timer = 0U;
	uint8_t j, t, fw_status, fl_status;
    
    switch (eRubiconTransferState)
    {
		case RUBICON_INIT:
		{
			/* should newer get here*/
			eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
			break;
		}

		
		case RUBICON_PACKET_ENUMERATOR:
		{
			if ((RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_BOOTLOADER) || \
				   (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN))
			{
				if((RUBICON_FirmwareUpdatePacket.send_attempt <= 1)	||	\
					(RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_BOOTLOADER))
				{
					RUBICON_PrepareFirmwareUpdatePacket();
					eRubiconTransferState = RUBICON_PACKET_SEND;
					eRubiconUpdate = RUBICON_UPDATE_BINARY;
				}
				else if(RUBICON_FirmwareUpdatePacket.send_attempt >= RUBICON_MAX_ERRORS)
				{
					if(RUBICON_FirmwareUpdatePacket.last_packet_send > 2) CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE_FAIL, RUBICON_RS485_BUS_ERROR);
					else CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE_FAIL, RUBICON_NOT_RESPONDING);
					
					RUBICON_PrepareFirmwareUpdatePacket();
					eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
					eRubiconUpdate = RUBICON_NO_UPDATE;
					DISPLAY_FileProgressBarDeleteSet();
				}
				else
				{
					eRubiconTransferState = RUBICON_PACKET_SEND;
					eRubiconUpdate = RUBICON_UPDATE_BINARY;
				}
			}
			else if ((RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FAIL) || \
					   (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINISHED))
			{	
				if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FAIL)
				{
					if(RUBICON_FirmwareUpdatePacket.last_packet_send > 2) CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE_FAIL, RUBICON_RS485_BUS_ERROR);
					else CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE_FAIL, RUBICON_NOT_RESPONDING);
					RUBICON_PrepareFirmwareUpdatePacket();
					RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
					eRubiconUpdate = RUBICON_NO_UPDATE;
				}
				else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINISHED)
				{	
					CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE, RUBICON_UPDATE_FIRMWARE);
					RUBICON_PrepareCommandPacket(RUBICON_EXECUTE_APPLICATION, rubicon_ctrl_buffer);
					eRubiconTransferState = RUBICON_PACKET_SEND;
					eRubiconUpdate = RUBICON_UPDATE_BINARY;
				}
				
				DISPLAY_FileProgressBarDeleteSet();
				break;
			}
			else if ((RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_QUERY_LIST) || \
					   (RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_DELETE_LOG))
			{
				if (RUBICON_LogListTransfer.send_attempt == 0)
				{
					RUBICON_LogListTransfer.send_attempt = 1;
					RUBICON_LogListTransfer.last_attempt = 1;
				}
				else if ((RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_QUERY_LIST) && \
					   (RUBICON_LogListTransfer.send_attempt == RUBICON_LogListTransfer.last_attempt))
				{
					RUBICON_LogListTransfer.log_transfer_state = LOG_TRANSFER_DELETE_LOG;
					RUBICON_LogListTransfer.send_attempt = 1;
					RUBICON_LogListTransfer.last_attempt = 1;
				}
				else if ((RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_DELETE_LOG) && \
					   (RUBICON_LogListTransfer.send_attempt == RUBICON_LogListTransfer.last_attempt))
				{
					RUBICON_LogListTransfer.log_transfer_state = LOG_TRANSFER_QUERY_LIST;
					RUBICON_LogListTransfer.send_attempt = 1;
					RUBICON_LogListTransfer.last_attempt = 1;
				}
				else
				{
					RUBICON_LogListTransfer.last_attempt = RUBICON_LogListTransfer.send_attempt;
				}

				RUBICON_PrepareLogUpdatePacket();
				eRubiconUpdate = RUBICON_UPDATE_LOG;
				eRubiconTransferState = RUBICON_PACKET_SEND;
			}
			else if ((RUBICON_FileUpdatePacket.update_state >= FILE_UPDATE_IMAGE_1) && \
					(RUBICON_FileUpdatePacket.update_state <= FILE_UPDATE_IMAGE_29))
			{
				if(RUBICON_FileUpdatePacket.send_attempt <= 1) 
				{
					RUBICON_PrepareFileUpdatePacket();
					eRubiconTransferState = RUBICON_PACKET_SEND;
					eRubiconUpdate = RUBICON_UPDATE_FILE;
				}
				else if(RUBICON_FileUpdatePacket.send_attempt >= RUBICON_MAX_ERRORS) 
				{
					if(RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_FIRMWARE) CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE_FAIL, FILE_SYS_ERROR);
					else if(RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_BOOTLOADER) CONTROLLER_WriteLogEvent(RUBICON_BOOTLOADER_UPDATE_FAIL, FILE_SYS_ERROR);
					else if(RUBICON_FileUpdatePacket.update_command == RUBICON_DOWNLOAD_DIPLAY_IMAGE) CONTROLLER_WriteLogEvent(RUBICON_IMAGE_UPDATE_FAIL, FILE_SYS_ERROR);
					RUBICON_PrepareFileUpdatePacket();
					eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
					eRubiconUpdate = RUBICON_NO_UPDATE;
					DISPLAY_FileProgressBarDeleteSet();
					
					if((RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_FIRMWARE) || (RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_BOOTLOADER))
					{
						
						rubicon_firmware_update_list_cnt++;
						
						if(rubicon_firmware_update_address_list[rubicon_firmware_update_list_cnt] == NULL) 
						{
							RUBICON_FirmwareUpdatePacket.update_command = NULL;
						}
						else
						{
							rubicon_ctrl_request = RUBICON_DOWNLOAD_DIPLAY_IMAGE;
							return;
						}						
					}
					else if(RUBICON_FileUpdatePacket.update_command == RUBICON_DOWNLOAD_DIPLAY_IMAGE)
					{
						rubicon_file_update_list_cnt++;
								
						if(rubicon_file_update_list[rubicon_file_update_list_cnt] == NULL)
						{
							rubicon_file_update_list_cnt = 0;
							rubicon_image_update_list_cnt++;
				
							if(rubicon_image_update_address_list[rubicon_image_update_list_cnt] == NULL) 
							{
								RUBICON_FirmwareUpdatePacket.update_command = NULL;
								DISPLAY_FileProgressBarDeleteSet();
							}
							else
							{
								rubicon_ctrl_request = RUBICON_DOWNLOAD_DIPLAY_IMAGE;
								return;
							}
						}
						else 
						{
							rubicon_ctrl_request = RUBICON_DOWNLOAD_DIPLAY_IMAGE;
							return;
						}
					}
				}
				else
				{
					eRubiconTransferState = RUBICON_PACKET_SEND;
					eRubiconUpdate = RUBICON_UPDATE_FILE;
				}				
			}
			else if (rubicon_ctrl_request != NULL)
			{
				if (rubicon_ctrl_request == RUBICON_UPDATE_FIRMWARE)
				{
					/* check for new firmware file on uSD card*/
					fw_status = (RUBICON_CheckNewFirmwareFile());
					
					if (fw_status == FILE_OK)
					{
						if(!GetDisplay_uSD_CardActivStatus()) DISPLAY_uSD_CardReadySet();
						RUBICON_PrepareFirmwareUpdatePacket();
						eRubiconTransferState = RUBICON_PACKET_SEND;
						eRubiconUpdate = RUBICON_UPDATE_BINARY;
						RUBICON_UpdateJournalCmdReset();
						RUBICON_RoomStatusUpdateReset();
						RUBICON_RoomStatusRequestReset();
						UpdateFileTransferDisplay();
					}
					else if(fw_status == FILE_SYS_ERROR)
					{
						CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE_FAIL, FILE_SYS_ERROR);
						RUBICON_FirmwareUpdatePacket.update_state = FILE_SYS_ERROR;
						UpdateFileTransferDisplay();
						RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
						eRubiconUpdate = RUBICON_NO_UPDATE;
						rubicon_ctrl_request = NULL;
						DISPLAY_FileProgressBarDeleteSet();
						DISPLAY_uSD_CardErrorSet();
					}
					else if(fw_status == FILE_DIR_ERROR)
					{
						CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE_FAIL, FILE_DIR_ERROR);
						RUBICON_FirmwareUpdatePacket.update_state = FILE_DIR_ERROR;
						UpdateFileTransferDisplay();
						RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
						eRubiconUpdate = RUBICON_NO_UPDATE;
						rubicon_ctrl_request = NULL;
						DISPLAY_FileProgressBarDeleteSet();
						DISPLAY_uSD_CardErrorSet();
					}
					else if(fw_status == FILE_ERROR)
					{
						CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE_FAIL, FILE_ERROR);
						RUBICON_FirmwareUpdatePacket.update_state = FILE_ERROR;
						UpdateFileTransferDisplay();
						RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
						eRubiconUpdate = RUBICON_NO_UPDATE;
						rubicon_ctrl_request = NULL;
						DISPLAY_FileProgressBarDeleteSet();
						DISPLAY_uSD_CardErrorSet();
					}
				}
				else if ((rubicon_ctrl_request == RUBICON_GET_LOG_LIST) || (rubicon_ctrl_request == RUBICON_DELETE_LOG_LIST))
				{
					eRubiconUpdate = RUBICON_NO_UPDATE;
				}
				else if (rubicon_ctrl_request == RUBICON_DOWNLOAD_DIPLAY_IMAGE)
				{
					if((RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_FIRMWARE) || (RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_BOOTLOADER))
					{
						p_rubicon_buffer = rubicon_ctrl_buffer;
						while(p_rubicon_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_rubicon_buffer++ = NULL;
						Int2Str((char *) rubicon_ctrl_buffer, rubicon_firmware_update_address_list[rubicon_firmware_update_list_cnt], 0);
						t = 0U;
						while((rubicon_ctrl_buffer[t]) != NULL) t++;
						++t;
						Int2Str((char *) &rubicon_ctrl_buffer[t], rubicon_file_update_list[rubicon_file_update_list_cnt], 0);
						
					}
					else if(RUBICON_FileUpdatePacket.update_command == RUBICON_DOWNLOAD_DIPLAY_IMAGE)
					{
						p_rubicon_buffer = rubicon_ctrl_buffer;
						while(p_rubicon_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_rubicon_buffer++ = NULL;
						Int2Str((char *) rubicon_ctrl_buffer, rubicon_image_update_address_list[rubicon_image_update_list_cnt], 0);
						t = 0U;
						while((rubicon_ctrl_buffer[t]) != NULL) t++;
						++t;
						Int2Str((char *) &rubicon_ctrl_buffer[t], rubicon_file_update_list[rubicon_file_update_list_cnt], 0);
					}
					
					fl_status = RUBICON_CheckNewImageFile();
					
					if (fl_status == FILE_OK)
					{
						if(!GetDisplay_uSD_CardActivStatus()) DISPLAY_uSD_CardReadySet();
						RUBICON_PrepareFileUpdatePacket();
						eRubiconTransferState = RUBICON_PACKET_SEND;
						eRubiconUpdate = RUBICON_UPDATE_FILE;
						UpdateFileTransferDisplay();
					}
					else if(fl_status == FILE_SYS_ERROR)
					{
						if(RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_FIRMWARE) CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE_FAIL, FILE_SYS_ERROR);
						else if(RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_BOOTLOADER) CONTROLLER_WriteLogEvent(RUBICON_BOOTLOADER_UPDATE_FAIL, FILE_SYS_ERROR);
						else if(RUBICON_FileUpdatePacket.update_command == RUBICON_DOWNLOAD_DIPLAY_IMAGE) CONTROLLER_WriteLogEvent(RUBICON_IMAGE_UPDATE_FAIL, FILE_SYS_ERROR);
						RUBICON_FileUpdatePacket.update_state = FILE_SYS_ERROR;
						UpdateFileTransferDisplay();
						RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IDLE;
						eRubiconUpdate = RUBICON_NO_UPDATE;
						rubicon_ctrl_request = NULL;
						DISPLAY_FileProgressBarDeleteSet();
						DISPLAY_uSD_CardErrorSet();
						
					}
					else if(fl_status == FILE_DIR_ERROR)
					{
						if(RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_FIRMWARE) CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE_FAIL, FILE_DIR_ERROR);
						else if(RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_BOOTLOADER) CONTROLLER_WriteLogEvent(RUBICON_BOOTLOADER_UPDATE_FAIL, FILE_DIR_ERROR);
						else if(RUBICON_FileUpdatePacket.update_command == RUBICON_DOWNLOAD_DIPLAY_IMAGE) CONTROLLER_WriteLogEvent(RUBICON_IMAGE_UPDATE_FAIL, FILE_DIR_ERROR);
						RUBICON_FileUpdatePacket.update_state = FILE_DIR_ERROR;
						UpdateFileTransferDisplay();
						RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IDLE;
						eRubiconUpdate = RUBICON_NO_UPDATE;
						rubicon_ctrl_request = NULL;
						DISPLAY_FileProgressBarDeleteSet();
						DISPLAY_uSD_CardErrorSet();
					}
					else if(fl_status == FILE_ERROR)
					{
						if(RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_FIRMWARE) CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE_FAIL, FILE_ERROR);
						else if(RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_BOOTLOADER) CONTROLLER_WriteLogEvent(RUBICON_BOOTLOADER_UPDATE_FAIL, FILE_ERROR);
						else if(RUBICON_FileUpdatePacket.update_command == RUBICON_DOWNLOAD_DIPLAY_IMAGE) CONTROLLER_WriteLogEvent(RUBICON_IMAGE_UPDATE_FAIL, FILE_ERROR);
						RUBICON_FileUpdatePacket.update_state = FILE_ERROR;
						UpdateFileTransferDisplay();
						RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IDLE;
						eRubiconUpdate = RUBICON_NO_UPDATE;
						rubicon_ctrl_request = NULL;
						DISPLAY_FileProgressBarDeleteSet();
						DISPLAY_uSD_CardErrorSet();
					}
				}
				else
				{
					/**
					*	prepare requested command packet in RS485 tx buffer
					*/
					if(RUBICON_FirmwareUpdatePacket.update_command == NULL)
					{
						rs485_rubicon_address = atoi((char *) rubicon_ctrl_buffer);
					}
					
					RUBICON_PrepareCommandPacket(rubicon_ctrl_request, rubicon_ctrl_buffer);
					eRubiconUpdate = RUBICON_HTTP_REQUEST;
					eRubiconTransferState = RUBICON_PACKET_SEND;
				}
				
				rubicon_ctrl_request = NULL;
			}			
			else if(IsRUBICON_RoomStatusUpdateSet() && IsRUBICON_RoomStatusRequestSet())
			{
				if(++rubicon_status_request_cnt >= RUBICON_MAX_ERRORS)
				{
					RUBICON_RoomStatusRequestReset();
					RUBICON_RoomStatusUpdateReset();
				}
				else
				{
					RUBICON_PrepareRoomStatusRequestPacket();
					eRubiconUpdate = RUBICON_UPDATE_ROOM_STATUS;
					eRubiconTransferState = RUBICON_PACKET_SEND;
				}
			}
			else if(IsRUBICON_UpdateJournalCmdSet())
			{
				RUBICON_UpdateJournalCmdReset();
				memset(tx_buffer, 0, DATA_BUF_SIZE);
				
				tx_buffer[0] = RUBICON_SOH;
				tx_buffer[1] = rs485_broadcast_address >> 8U;
				tx_buffer[2] = rs485_broadcast_address & 0xFFU;
				tx_buffer[3] = rs485_interface_address >> 8U;
				tx_buffer[4] = rs485_interface_address & 0xFFU;
				tx_buffer[6] = RUBICON_DOWNLOAD_JOURNAL_1;
				
				t = 7U;
				rubicon_journal_list_item = 0U;
				while(rubicon_journal_list_item < 48U)
				{
					Int2Str((char*)&tx_buffer[t], rubiconJournal_1[rubicon_journal_list_item], 0);
					rubicon_journal_list_item++;
					while(tx_buffer[t] != 0x00U) t++;
					tx_buffer[t] = ',';
					t++;
				}
				
				--t;
				tx_buffer[t] = ';';
				tx_buffer[5] = (t - 5U);
				
				rs485_packet_checksum = 0U;

				for (t = 6U; t < (tx_buffer[5] + 6U); t++)
				{
					rs485_packet_checksum += tx_buffer[t];
				}

				tx_buffer[tx_buffer[5] + 6] = rs485_packet_checksum >> 8U;
				tx_buffer[tx_buffer[5] + 7] = rs485_packet_checksum & 0xFFU;
				tx_buffer[tx_buffer[5] + 8] = RUBICON_EOT;
				eRubiconTransferState = RUBICON_PACKET_SEND;
			}
            else if((get_systick() - weather_forecast_update_timer) >= WEATHER_FORECAST_UPDATE_PERIOD)
			{
                weather_forecast_update_timer = get_systick();
                
                if(RTC_State != RTC_VALID) break;
                RTC_t dt;
                RTC_GetDateTime(&dt, RTC_Format_BIN);
                I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, EE_FORECAST_ADDRESS, WEATHER_FORECAST_DATA_SIZE);
                uint32_t wf_unit = ((i2c_ee_buffer[0] << 24U) + (i2c_ee_buffer[1] << 16U) + (i2c_ee_buffer[2] << 8U)  + i2c_ee_buffer[3]);
                
                if((dt.unix < (wf_unit + SECONDS_PER_DAY)) && (wf_unit < (dt.unix + SECONDS_PER_DAY)))
                {
                    memset(tx_buffer, 0U, sizeof (tx_buffer));
                    tx_buffer[0] = RUBICON_SOH;
                    tx_buffer[1] = rs485_broadcast_address >> 8U;
                    tx_buffer[2] = rs485_broadcast_address & 0xFFU;
                    tx_buffer[3] = rs485_interface_address >> 8U;
                    tx_buffer[4] = rs485_interface_address & 0xFFU;
                    tx_buffer[5] = WEATHER_FORECAST_DATA_SIZE + 1U;
                    tx_buffer[6] = RUBICON_DOWNLOAD_WEATHER_FORECAST;
                    memcpy(&tx_buffer[7], i2c_ee_buffer, WEATHER_FORECAST_DATA_SIZE);
                    rs485_packet_checksum = 0U;
                    
                    for (t = 6U; t < (tx_buffer[5] + 6U); t++)
                    {
                        rs485_packet_checksum += tx_buffer[t];
                    }
                    tx_buffer[tx_buffer[5] + 6] = rs485_packet_checksum >> 8U;
                    tx_buffer[tx_buffer[5] + 7] = rs485_packet_checksum & 0xFFU;
                    tx_buffer[tx_buffer[5] + 8] = RUBICON_EOT;
                    eRubiconUpdate = RUBICON_NO_UPDATE;
                    eRubiconTransferState = RUBICON_PACKET_SEND;
                }
			}
            else if(IsRUBICON_TimerExpired())
			{
				RUBICON_StartTimer(RUBICON_TIME_UPDATE_PERIOD);
				if      (eRubiconTimeUpdate == RUBICON_TIME_UPDATE_P2P)         rs485_rubicon_address = RUBICON_GetNextAddress();
				else if (eRubiconTimeUpdate == RUBICON_TIME_UPDATE_GROUP)       rs485_rubicon_address = rs485_group_address;
				else if (eRubiconTimeUpdate == RUBICON_TIME_UPDATE_BROADCAST)   rs485_rubicon_address = rs485_broadcast_address;
				RUBICON_PrepareTimeUpdatePacket();
				eRubiconUpdate = RUBICON_UPDATE_TIME;
				eRubiconTransferState = RUBICON_PACKET_SEND;
			}
			else 
			{
				rs485_rubicon_address = RUBICON_GetNextAddress();
				RUBICON_PrepareSysStatusRequestPacket();
				eRubiconUpdate = RUBICON_UPDATE_SYS_STATUS;
				eRubiconTransferState = RUBICON_PACKET_SEND;
			}			
			break;
		}
		
		case RUBICON_PACKET_SEND:
		{	
			if(!IsRUBICON_RxTimeoutTimerExpired()) break;
			
			rs485_rx_cnt = 0U;
			while(rs485_rx_cnt < DATA_BUF_SIZE) rx_buffer[rs485_rx_cnt++] = NULL;
			rs485_rx_cnt = 0U;
			old_rx_cnt = 0U;
			RS485_Send_Data(tx_buffer, (tx_buffer[5] + 9U));
			RUBICON_StartRxTimeoutTimer(RUBICON_RESPONSE_TIMEOUT);
			eRubiconTransferState = RUBICON_PACKET_PENDING;
		
			switch (eRubiconUpdate)
			{
				case RUBICON_UPDATE_TIME:
				{
					switch (eRubiconTimeUpdate)
					{						
						case RUBICON_TIME_UPDATE_P2P:
						{
							break;
						}
						
						case RUBICON_TIME_UPDATE_GROUP:
						{						
							RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
							eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;	
							break;
						}
						
						case RUBICON_TIME_UPDATE_BROADCAST:
						{
							RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
							eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;							
							break;
						}
						
						case RUBICON_NO_TIME_UPDATE:
						{
							RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
							eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;							
							break;
						}
						
						default:
						{
							RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
							eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;	
							break;
						}
					}
					break;
				}
					
				case RUBICON_UPDATE_SYS_STATUS:
				{
					break;
				}
				
				
				case RUBICON_UPDATE_ROOM_STATUS:
				{
					break;
				}
				
				
				case RUBICON_UPDATE_BINARY:
				{
					if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN)
					{
						if(RUBICON_FirmwareUpdatePacket.packet_send == 0)
						{
							RUBICON_StartFwUpdateTimer(RUBICON_FW_UPLOAD_TIMEOUT);
						}
					}
					else if(RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_BOOTLOADER)
					{
						RUBICON_StartFwUpdateTimer(RUBICON_BOOTLOADER_START_TIME);
					}
					else if(RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINISHED)
					{
						
						RUBICON_StartFwUpdateTimer(RUBICON_RESTART_TIME);
					}
					break;
				}
					
				case RUBICON_UPDATE_FILE:
				{
					RUBICON_StartRxTimeoutTimer(RUBICON_FILE_UPLOAD_TIMEOUT);
					break;
				}
				
				case RUBICON_UPDATE_LOG:
				{
					break;
				}
				
				case RUBICON_HTTP_REQUEST:
				{
					if((RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_FIRMWARE) || (RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_BOOTLOADER))
					{						
						if(RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_FIRMWARE) 
						{
							CONTROLLER_WriteLogEvent(RUBICON_FIRMWARE_UPDATE, rubicon_file_update_list[rubicon_file_update_list_cnt]);
							rubicon_ctrl_request = RUBICON_FirmwareUpdatePacket.update_state;
							RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_FINISHED;
							UpdateFileTransferDisplay();
							RUBICON_FirmwareUpdatePacket.update_state = rubicon_ctrl_request;
						}
						else if(RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_BOOTLOADER) 
						{
							CONTROLLER_WriteLogEvent(RUBICON_BOOTLOADER_UPDATED, rubicon_file_update_list[rubicon_file_update_list_cnt]);
							rubicon_ctrl_request = RUBICON_FirmwareUpdatePacket.update_state;
							RUBICON_FirmwareUpdatePacket.update_state = RUBICON_BOOTLOADER_UPDATED;
							UpdateFileTransferDisplay();
							RUBICON_FirmwareUpdatePacket.update_state = rubicon_ctrl_request;
						}
						
						rubicon_firmware_update_list_cnt++;
						
						if(rubicon_firmware_update_address_list[rubicon_firmware_update_list_cnt] == NULL) 
						{
							
							RUBICON_FirmwareUpdatePacket.update_command = NULL;
							RUBICON_StartRxTimeoutTimer(RUBICON_FW_EXE_BOOT_TIME);
						}
						else
						{
							rubicon_ctrl_request = RUBICON_DOWNLOAD_DIPLAY_IMAGE;
							RUBICON_StartRxTimeoutTimer(RUBICON_RESTART_TIME);
							eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
							return;
						}						
					}
					break;
				}
				
				case RUBICON_NO_UPDATE:
				{
					RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
					eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
					break;
				}
				
				default:
				{
					RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
					eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;	
					break;
				}
			}
			break;
		}
		case RUBICON_PACKET_PENDING:
		{
			if((rs485_rx_cnt != 0) && ((rx_buffer[0] != RUBICON_ACK) && (rx_buffer[0] != RUBICON_NAK)))
			{
				rs485_rx_cnt = 0;
				rx_buffer[0] = NULL;				
				RUBICON_StartRxTimeoutTimer(RUBICON_BYTE_RX_TIMEOUT);
			}
			else if(rx_buffer[0] == RUBICON_ACK)
			{
				if ((RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_BOOTLOADER) || \
				    (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN) || \
					((RUBICON_FileUpdatePacket.update_state >= FILE_UPDATE_IMAGE_1) && \
					(RUBICON_FileUpdatePacket.update_state <= FILE_UPDATE_IMAGE_29)))
				{
					eRubiconTransferState = RUBICON_PACKET_RECEIVED;
				}
				else
				{
					old_rx_cnt = rs485_rx_cnt;
					eRubiconTransferState = RUBICON_PACKET_RECEIVING;
					RUBICON_StartRxTimeoutTimer(RUBICON_BYTE_RX_TIMEOUT);
				}							
			}
			else if(rx_buffer[0] == RUBICON_NAK)
			{
				if ((RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_BOOTLOADER) || \
				    (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN))
				{
					++RUBICON_FirmwareUpdatePacket.send_attempt;
				}
				else if ((RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_QUERY_LIST) || \
						 (RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_DELETE_LOG))
				{
					++RUBICON_LogListTransfer.send_attempt;
				}
				else if ((RUBICON_FileUpdatePacket.update_state >= FILE_UPDATE_IMAGE_1) && \
						(RUBICON_FileUpdatePacket.update_state <= FILE_UPDATE_IMAGE_29))
				{
					++RUBICON_FileUpdatePacket.send_attempt;
				}
				else if(rubicon_http_cmd_state == HTTP_GET_RUBICON_TEMPERATURE)
				{
					rubicon_http_cmd_state = NULL;
				}
				
				RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
				eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
			}
			else if(IsRUBICON_RxTimeoutTimerExpired() && IsRUBICON_FwUpdateTimerExpired()) 
			{
				if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_BOOTLOADER)
				{
					RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_FAIL;
					DISPLAY_FileProgressBarDeleteSet();
					UpdateFileTransferDisplay();					
				}
				else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN)
				{
					if(RUBICON_FirmwareUpdatePacket.packet_send == 0)
					{
						RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_BOOTLOADER;
					}
					++RUBICON_FirmwareUpdatePacket.send_attempt;
				}
				else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINISHED)
				{
					RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
				}
				else if ((RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_QUERY_LIST) || \
						 (RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_DELETE_LOG))
				{
					++RUBICON_LogListTransfer.send_attempt;
				}
				else if ((RUBICON_FileUpdatePacket.update_state >= FILE_UPDATE_IMAGE_1) && \
						(RUBICON_FileUpdatePacket.update_state <= FILE_UPDATE_IMAGE_29))
				{
					++RUBICON_FileUpdatePacket.send_attempt;

				}
				
				RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
				eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
			}
			break;
		}
		case RUBICON_PACKET_RECEIVING:
		{	
			if(((rx_buffer[1] == (rs485_interface_address >> 8)) 	&& \
				(rx_buffer[2] == (rs485_interface_address & 0xff))) && \
				((rx_buffer[3] == (rs485_rubicon_address >> 8)) 	&& \
				(rx_buffer[4] == (rs485_rubicon_address & 0xff))) 	&& \
				(rx_buffer[rx_buffer[5] + 8] == RUBICON_EOT))
			{
				    rs485_packet_checksum = 0;

					for (j = 6; j < (rx_buffer[5] + 6); j++)
					{
						rs485_packet_checksum += rx_buffer[j];
					}

					if ((rx_buffer[rx_buffer[5] + 6] == (rs485_packet_checksum >> 8)) && \
						(rx_buffer[rx_buffer[5] + 7] == (rs485_packet_checksum & 0xff)))
					{
						eRubiconTransferState = RUBICON_PACKET_RECEIVED;
					}
					else
					{
						if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN)
						{
							++RUBICON_FirmwareUpdatePacket.send_attempt;
						}
						else if ((RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_QUERY_LIST) || \
								 (RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_DELETE_LOG))
						{
							++RUBICON_LogListTransfer.send_attempt;
						}
						else if(rubicon_http_cmd_state == HTTP_GET_RUBICON_TEMPERATURE)
						{
							rubicon_http_cmd_state = NULL;
						}
						
						RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
						eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
					}
				
			}
			else if (IsRUBICON_RxTimeoutTimerExpired())
			{
				if ((RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_QUERY_LIST) ||
						(RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_DELETE_LOG))
				{
					++RUBICON_LogListTransfer.send_attempt;
				}
				else if(rubicon_http_cmd_state == HTTP_GET_RUBICON_TEMPERATURE)
				{
					rubicon_http_cmd_state = NULL;
				}
				else if(rubicon_http_cmd_state == HTTP_GET_RUBICON_ROOM_STATUS)
				{
					rubicon_http_cmd_state = NULL;
				}
				else if(rubicon_http_cmd_state == HTTP_GET_RUBICON_SYS_INFO)
				{
					rubicon_http_cmd_state = NULL;
				}
				
				RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
				eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
			}
			
			if(old_rx_cnt != rs485_rx_cnt) 
			{
				old_rx_cnt = rs485_rx_cnt;
				RUBICON_StartRxTimeoutTimer(RUBICON_BYTE_RX_TIMEOUT);				
			}
			break;
		}
		case RUBICON_PACKET_RECEIVED:
		{
			switch (eRubiconUpdate)
			{
                case RUBICON_INIT:
                {
                    break;
                }
                
                case RUBICON_NO_UPDATE:
                {
                    break;
                }
                
                case RUBICON_UPDATE_TIME:
                {
                    break;
                }
                
				case RUBICON_UPDATE_SYS_STATUS:
				{
					if(rx_buffer[13] == '1')
					{
						RUBICON_FirmwareUpdatePacket.update_command = NULL;
						rubicon_ctrl_request = RUBICON_DOWNLOAD_DIPLAY_IMAGE;
						RUBICON_FileUpdatePacket.update_command = RUBICON_DOWNLOAD_DIPLAY_IMAGE;
						RUBICON_LogListTransfer.log_transfer_state = NULL;
						rubicon_image_update_list_cnt = 0;
						rubicon_image_update_address_list[rubicon_image_update_list_cnt] = rs485_rubicon_address;
						rubicon_image_update_list_cnt++;
						rubicon_image_update_address_list[rubicon_image_update_list_cnt] = NULL;
						rubicon_image_update_list_cnt = 0;
						rubicon_file_update_list_cnt = 0;
						j = 1;
						while(j <= 14) rubicon_file_update_list[rubicon_file_update_list_cnt++] = j++;
						rubicon_file_update_list[rubicon_file_update_list_cnt] = NULL;
						rubicon_file_update_list_cnt =  0;						
					}
					else if(rx_buffer[14] == '1')
					{
						RUBICON_FileUpdatePacket.update_command = NULL;
						rubicon_ctrl_request = RUBICON_DOWNLOAD_DIPLAY_IMAGE;
						RUBICON_FirmwareUpdatePacket.update_command = RUBICON_UPDATE_FIRMWARE;
						rubicon_file_update_list[0] = (FILE_UPDATE_IMAGE_20 - 40);
						rubicon_file_update_list[1] = NULL;
						rubicon_file_update_list_cnt = 0;
						rubicon_firmware_update_list_cnt = 0;
						rubicon_firmware_update_address_list[rubicon_firmware_update_list_cnt] = rs485_rubicon_address;
						rubicon_firmware_update_list_cnt++;
						rubicon_firmware_update_address_list[rubicon_firmware_update_list_cnt] =  NULL;
						rubicon_firmware_update_list_cnt = 0;	
					}
					else if ((rx_buffer[7] == '1') || (rx_buffer[8] == '1'))
					{
						RUBICON_LogListTransfer.log_transfer_state = LOG_TRANSFER_QUERY_LIST;
						RUBICON_LogListTransfer.send_attempt = 0;
					}
					else
					{
						/**
						* 	addressed controller responded, check  room status for journal update
						*/
						if(IsRUBICON_RoomStatusUpdateSet() && !IsRUBICON_RoomStatusRequestSet())
						{
							RUBICON_RoomStatusRequestSet();
							rubicon_status_request_cnt = 0;
						}
					}
					
//					RUBICON_StopRxTimeoutTimer();
					break;
				}
				
				
				case RUBICON_UPDATE_ROOM_STATUS:
				{
					if ((rx_buffer[0] == 0x06) && (rx_buffer[6] == RUBICON_GET_ROOM_STATUS))
					{
						RUBICON_RoomStatusRequestReset();
						
						rubicon_journal_list_item = 0;
						
						while(rs485_rubicon_address > 199)
						{
							rs485_rubicon_address -= 100;
							rubicon_journal_list_item++;
						}
						
						switch(rx_buffer[7])	// room status
						{
							case RUBICON_ROOM_CLEANING:
							{
								rubiconJournal_1[(rubicon_journal_list_item * 4)] += 1;
								rubiconJournal_1[(rubicon_journal_list_item * 4) + 3] += 1;
								rubiconJournal_1[20] += 1;
								rubiconJournal_1[23] += 1;
								rubiconJournal_1[44] += 1;
								rubiconJournal_1[47] += 1;
								break;
							}
							
							case RUBICON_ROOM_BEDDING_REPLACEMENT:
							{
								rubiconJournal_1[(rubicon_journal_list_item * 4) + 1] += 1;
								rubiconJournal_1[(rubicon_journal_list_item * 4) + 3] += 1;
								rubiconJournal_1[21] += 1;
								rubiconJournal_1[23] += 1;
								rubiconJournal_1[45] += 1;
								rubiconJournal_1[47] += 1;
								break;
							}
							
							case RUBICON_ROOM_GENERAL_CLEANING:
							{
								rubiconJournal_1[(rubicon_journal_list_item * 4) + 2] += 1;
								rubiconJournal_1[(rubicon_journal_list_item * 4) + 3] += 1;
								rubiconJournal_1[22] += 1;
								rubiconJournal_1[23] += 1;
								rubiconJournal_1[46] += 1;
								rubiconJournal_1[47] += 1;
								break;
							}
							
							default:
							{
								break;
							}
						}
					}
					break;
				}
				
				
				case RUBICON_UPDATE_BINARY:
				{
					if (RUBICON_FirmwareUpdatePacket.packet_send == RUBICON_FirmwareUpdatePacket.packet_total)
					{
						RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_FINISHED;
						DISPLAY_FileProgressBarDeleteSet();
						UpdateFileTransferDisplay();
						f_close(&file_SD);
						f_mount(NULL,"0:",0);						
					}
					else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN)
					{
						if((RUBICON_FirmwareUpdatePacket.packet_send == 0) && (!IsRUBICON_FwUpdateTimerExpired()))
						{
							return;
						}
						
						RUBICON_FirmwareUpdatePacket.last_packet_send = RUBICON_FirmwareUpdatePacket.packet_send;
						++RUBICON_FirmwareUpdatePacket.packet_send;	
						RUBICON_FirmwareUpdatePacket.send_attempt = 1;					
					}
					else if ((RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_BOOTLOADER) && \
							(!IsRUBICON_FwUpdateTimerExpired())) 
					{
						return;				
					}
					break;
				}
				
				case RUBICON_UPDATE_FILE:
				{
					if (RUBICON_FileUpdatePacket.packet_send == RUBICON_FileUpdatePacket.packet_total)
					{
						RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_FINISHED;
						DISPLAY_FileProgressBarDeleteSet();
						UpdateFileTransferDisplay();
						RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IDLE;
						eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
						eRubiconUpdate = RUBICON_NO_UPDATE;
						rubicon_ctrl_request = NULL;
						f_close(&file_SD);
						f_mount(NULL,"0:",0);
						
						if(RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_FIRMWARE)
						{
							rs485_rubicon_address = rubicon_firmware_update_address_list[rubicon_firmware_update_list_cnt];
							rubicon_ctrl_request = RUBICON_START_BOOTLOADER;
							RUBICON_StartRxTimeoutTimer(RUBICON_BOOTLOADER_START_TIME);
							eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
							return;
						}
						else if(RUBICON_FirmwareUpdatePacket.update_command == RUBICON_UPDATE_BOOTLOADER)
						{
							rs485_rubicon_address = rubicon_firmware_update_address_list[rubicon_firmware_update_list_cnt];
							rubicon_ctrl_request = RUBICON_UPDATE_BOOTLOADER;
							RUBICON_StartRxTimeoutTimer(RUBICON_BOOTLOADER_START_TIME);
							eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
							return;
						}
						else if(RUBICON_FileUpdatePacket.update_command == RUBICON_DOWNLOAD_DIPLAY_IMAGE)
						{
							CONTROLLER_WriteLogEvent(RUBICON_IMAGE_UPDATED, rubicon_file_update_list[rubicon_file_update_list_cnt]);
							rubicon_file_update_list_cnt++;
							
							if(rubicon_file_update_list[rubicon_file_update_list_cnt] == NULL)
							{
								rubicon_file_update_list_cnt = 0;
								rubicon_image_update_list_cnt++;
					
								if(rubicon_image_update_address_list[rubicon_image_update_list_cnt] == NULL) 
								{
									
									RUBICON_FileUpdatePacket.update_command = NULL;
									RUBICON_StartRxTimeoutTimer(RUBICON_IMAGE_COPY_TIME);
								}
								else
								{
									rubicon_ctrl_request = RUBICON_DOWNLOAD_DIPLAY_IMAGE;
									RUBICON_StartRxTimeoutTimer(RUBICON_IMAGE_COPY_TIME);
								}
							}
							else 
							{
								rs485_rubicon_address = rubicon_image_update_address_list[rubicon_image_update_list_cnt];
								rubicon_ctrl_request = RUBICON_DOWNLOAD_DIPLAY_IMAGE;
								RUBICON_StartRxTimeoutTimer(RUBICON_IMAGE_COPY_TIME);	
							}
							
							eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
							return;
						}
					
					
					}
					if ((RUBICON_FileUpdatePacket.update_state >= FILE_UPDATE_IMAGE_1) && \
						(RUBICON_FileUpdatePacket.update_state <= FILE_UPDATE_IMAGE_29))
					{
						RUBICON_FileUpdatePacket.last_packet_send = RUBICON_FileUpdatePacket.packet_send;
						++RUBICON_FileUpdatePacket.packet_send;
						RUBICON_FileUpdatePacket.send_attempt = 1;
					}
					break;
				}
				
				case RUBICON_UPDATE_LOG:
				{
					if ((rx_buffer[0] == 0x06) && (rx_buffer[5] == 0x01) && (rx_buffer[6] == RUBICON_GET_LOG_LIST))
					{
						RUBICON_LogListTransfer.log_transfer_state = LOG_TRANSFER_IDLE;
						if(IsRUBICON_RoomStatusUpdateSet()) eRubiconUpdate = RUBICON_UPDATE_ROOM_STATUS;
					}
					else if ((rx_buffer[0] == 0x06) && (rx_buffer[5] == 0x12) && (rx_buffer[6] == RUBICON_GET_LOG_LIST))
					{
						RUBICON_WriteLogToList(0);
						UpdateLogDisplay();
						
						/***************************************************************
                        *
                        *   R O O M     C O N T R O L L E R     E R R O R S
                        *
                        *   RUBICON_SPI_DRIVER_FAIL                 1
                        *   RUBICON_I2C_DRIVER_FAIL                 2
                        *   RUBICON_USART_DRIVER_FAIL               3
                        *   RUBICON_RTC_DRIVER_FAIL                 4
                        *   RUBICON_TMR_DRIVER_FAIL                 5
                        *
                        *   RUBICON_CAP_FUNC_FAIL                   11
                        *   RUBICON_RC522_FUNC_FAIL                 12
                        *   RUBICON_ONEWIRE_FUNC_FAIL               13
                        *   RUBICON_RS485_FUNC_FAIL                 14
                        *   RUBICON_MAIN_FUNC_FAIL                  15 
                        *   RUBICON_DISPLAY_FUNC_FAIL               16
                        *   RUBICON_LOGGER_FUNC_FAIL                17
                        *   RUBICON_DIO_FUNC_FAIL                   18
                        *   RUBICON_EEPROM_FUNC_FAIL                19
                        *   RUBICON_SIGNAL_FUNC_FAIL                20
                        *
                        *--------------------------------------------------------
                        *   T H E R M O S T A T     E R R O R S     L I S T
                        *
                        * 	THERMOSTAT_FANCOIL_RPM_SENSOR_ERROR    ((uint8_t)0xc0)
                        * 	THERMOSTAT_FANCOIL_NTC_SENSOR_ERROR    ((uint8_t)0xc1)
                        * 	THERMOSTAT_FANCOIL_LO_TEMP_ERROR       ((uint8_t)0xc2)
                        * 	THERMOSTAT_FANCOIL_HI_TEMP_ERROR       ((uint8_t)0xc3)
                        * 	THERMOSTAT_FANCOIL_FREEZING_PROTECTION ((uint8_t)0xc4)
                        * 	THERMOSTAT_AMBIENT_NTC_SENSOR_ERROR    ((uint8_t)0xc5)
                        * 	THERMOSTAT_THERMOSTAT_ERROR            ((uint8_t)0xc6)
                        *
                        *--------------------------------------------------------
                        *   R U B I C O N       E V E N T S     L I S T
                        *
						* 	RUBICON_FIRMWARE_UPDATE_FAIL			((uint8_t)0xce)
						*	RUBICON_IMAGE_UPDATE_FAIL				((uint8_t)0xcf)	
						*	RUBICON_PIN_RESET						((uint8_t)0xd0)
						*	RUBICON_POWER_ON_RESET					((uint8_t)0xd1)
						*	RUBICON_SOFTWARE_RESET					((uint8_t)0xd2)
						*	RUBICON_IWDG_RESET						((uint8_t)0xd3)
						*	RUBICON_WWDG_RESET						((uint8_t)0xd4)
						*	RUBICON_LOW_POWER_RESET					((uint8_t)0xd5)
                        *   RUBICON_FIRMWARE_UPDATE					((uint8_t)0xd6)
                        *   RUBICON_FIRMWARE_UPDATE_FAIL		    ((uint8_t)0xd7)
                        *   RUBICON_BOOTLOADER_UPDATED			    ((uint8_t)0xd8)
                        *   RUBICON_BOOTLOADER_UPDATE_FAIL		    ((uint8_t)0xd9)
                        *   RUBICON_IMAGE_UPDATED				    ((uint8_t)0xda)
                        *   RUBICON_IMAGE_UPDATE_FAIL			    ((uint8_t)0xdb)
                        *   RUBICON_DISPLAY_FAIL				    ((uint8_t)0xdc)
                        *   RUBICON_DRIVER_OR_FUNCTION_FAIL         ((uint8_t)0xdd)
                        *   RUBICON_ONEWIRE_BUS_EXCESSIVE_ERROR     ((uint8_t)0xde)
                        *
                        *---------------------------------------------------------
                        *   R U B I C O N       L O G       L I S T
                        *
						*	RUBICON_LOG_NO_EVENT                	((uint8_t)0xe0)
						*	RUBICON_LOG_GUEST_CARD_VALID        	((uint8_t)0xe1)
						*	RUBICON_LOG_GUEST_CARD_INVALID      	((uint8_t)0xe2)
						*	RUBICON_LOG_HANDMAID_CARD_VALID     	((uint8_t)0xe3)
						*	RUBICON_LOG_ENTRY_DOOR_CLOSED			((uint8_t)0xe4)
						*	RUBICON_LOG_PRESET_CARD					((uint8_t)0xe5)
						*	RUBICON_LOG_HANDMAID_SERVICE_END    	((uint8_t)0xe6)
						*	RUBICON_LOG_MANAGER_CARD            	((uint8_t)0xe7)
						*	RUBICON_LOG_SERVICE_CARD            	((uint8_t)0xe8)
						*	RUBICON_LOG_ENTRY_DOOR_OPENED          	((uint8_t)0xe9)
						*	RUBICON_LOG_MINIBAR_USED            	((uint8_t)0xea)
						*	RUBICON_LOG_BALCON_DOOR_OPENED			((uint8_t)0xeb)
						*	RUBICON_LOG_BALCON_DOOR_CLOSED			((uint8_t)0xec)
						*	RUBICON_LOG_CARD_STACKER_ON				((uint8_t)0xed)		
						*	RUBICON_LOG_CARD_STACKER_OFF			((uint8_t)0xee)
						*	RUBICON_LOG_DO_NOT_DISTURB_SWITCH_ON 	((uint8_t)0xef)
						*	RUBICON_LOG_DO_NOT_DISTURB_SWITCH_OFF	((uint8_t)0xf0)
						*	RUBICON_LOG_HANDMAID_SWITCH_ON			((uint8_t)0xf1)
						*	RUBICON_LOG_HANDMAID_SWITCH_OFF			((uint8_t)0xf2)
						*	RUBICON_LOG_SOS_ALARM_TRIGGER			((uint8_t)0xf3)
						*	RUBICON_LOG_SOS_ALARM_RESET				((uint8_t)0xf4)
						*	RUBICON_LOG_FIRE_ALARM_TRIGGER			((uint8_t)0xf5)
						*	RUBICON_LOG_FIRE_ALARM_RESET          	((uint8_t)0xf6)
						*	RUBICON_LOG_UNKNOWN_CARD				((uint8_t)0xf7)
						*	RUBICON_LOG_CARD_EXPIRED				((uint8_t)0xf8)
						* 	RUBICON_LOG_WRONG_ROOM					((uint8_t)0xf9)
						*	RUBICON_LOG_WRONG_SYSTEM_ID				((uint8_t)0xfa)
						*	RUBICON_CONTROLLER_RESET				((uint8_t)0xfb)
						*	RUBICON_ENTRY_DOOR_NOT_CLOSED			((uint8_t)0xfc)
						*	RUBICON_DOOR_BELL_ACTIVE				((uint8_t)0xfd)
                        *   RUBICON_DOOR_LOCK_USER_OPEN	            ((uint8_t)0xfe)
						*
						***********************************************************************************/
					}
                    else if ((rx_buffer[0] == 0x06) && (rx_buffer[5] == 0x01) && (rx_buffer[6] == RUBICON_DELETE_LOG_LIST))
					{
                        
                    }
					else 
                    {
                        ++RUBICON_LogListTransfer.send_attempt;
                    }
					break;
				}
				
				case RUBICON_HTTP_REQUEST:
				{
					if(rubicon_http_cmd_state == HTTP_GET_RUBICON_ROOM_STATUS)
					{
						rubicon_http_cmd_state = HTTP_RUBICON_ROOM_STATUS_READY;
					}
					break;
				}
			}

			RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
			eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
			break;
		}
		case RUBICON_PACKET_ERROR:
		{
			RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
			eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
			break;
		}
		default:
		{
			RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
			eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
			break;
		}
    }
}


void RUBICON_PrepareTimeUpdatePacket(void)
{
    uint8_t i;
    RTC_GetDate(RTC_Format_BCD, &RTC_Date);
    RTC_GetTime(RTC_Format_BCD, &RTC_Time);

    tx_buffer[0] = RUBICON_SOH;
    tx_buffer[1] = rs485_rubicon_address >> 8;
    tx_buffer[2] = rs485_rubicon_address & 0x00ff;
    tx_buffer[3] = rs485_interface_address >> 8;
    tx_buffer[4] = rs485_interface_address & 0x00ff;
    tx_buffer[5] = 0x0d;
    tx_buffer[6] = RUBICON_SET_RTC_DATE_TIME;
    tx_buffer[7] = (RTC_Date.RTC_Date >> 4) + 48;
    tx_buffer[8] = (RTC_Date.RTC_Date & 0x0f) + 48;
    tx_buffer[9] = (RTC_Date.RTC_Month >> 4) + 48;
    tx_buffer[10] = (RTC_Date.RTC_Month & 0x0f) + 48;
    tx_buffer[11] = (RTC_Date.RTC_Year >> 4) + 48;
    tx_buffer[12] = (RTC_Date.RTC_Year & 0x0f) + 48;
    tx_buffer[13] = (RTC_Time.RTC_Hours >> 4) + 48;
    tx_buffer[14] = (RTC_Time.RTC_Hours & 0x0f) + 48;
    tx_buffer[15] = (RTC_Time.RTC_Minutes >> 4) + 48;
    tx_buffer[16] = (RTC_Time.RTC_Minutes & 0x0f) + 48;
    tx_buffer[17] = (RTC_Time.RTC_Seconds >> 4) + 48;
    tx_buffer[18] = (RTC_Time.RTC_Seconds & 0x0f) + 48;

    rs485_packet_checksum = 0;

    for (i = 6; i < 19; i++)
    {
        rs485_packet_checksum += tx_buffer[i];
    }

    tx_buffer[19] = rs485_packet_checksum >> 8;
    tx_buffer[20] = rs485_packet_checksum;
    tx_buffer[21] = RUBICON_EOT;
}

void RUBICON_PrepareSysStatusRequestPacket(void)
{
    tx_buffer[0] = RUBICON_SOH;
    tx_buffer[1] = rs485_rubicon_address >> 8;
    tx_buffer[2] = rs485_rubicon_address & 0x00ff;
    tx_buffer[3] = rs485_interface_address >> 8;
    tx_buffer[4] = rs485_interface_address & 0x00ff;
    tx_buffer[5] = 0x01;
    tx_buffer[6] = RUBICON_GET_SYS_STATUS;
    tx_buffer[7] = 0x00;
    tx_buffer[8] = tx_buffer[6];
    tx_buffer[9] = RUBICON_EOT;
}


void RUBICON_PrepareRoomStatusRequestPacket(void)
{
    tx_buffer[0] = RUBICON_SOH;
    tx_buffer[1] = rs485_rubicon_address >> 8;
    tx_buffer[2] = rs485_rubicon_address & 0x00ff;
    tx_buffer[3] = rs485_interface_address >> 8;
    tx_buffer[4] = rs485_interface_address & 0x00ff;
    tx_buffer[5] = 0x01;
    tx_buffer[6] = RUBICON_GET_ROOM_STATUS;
    tx_buffer[7] = 0x00;
    tx_buffer[8] = RUBICON_GET_ROOM_STATUS;
    tx_buffer[9] = RUBICON_EOT;
}

uint8_t RUBICON_CheckConfigFile(void)
{
	UINT bytes_rd;
	char *ret;
	
	bytes_rd = 0;
	config_file_byte_cnt = 0;
		
	if (f_mount(&filesystem, "0:", 0) != FR_OK)
	{
		return FILE_SYS_ERROR;
	}
	
	if (f_opendir(&dir_1, "/") != FR_OK)
	{
		f_mount(NULL,"0:",0);
		return FILE_DIR_ERROR;
	}
	
	if (f_open(&file_SD, "UPDATE.CFG", FA_READ) != FR_OK) 
	{
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	if(f_read (&file_SD, rubicon_ctrl_buffer, RUBICON_CONFIG_FILE_MAX_SIZE, &bytes_rd) != FR_OK)
	{
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	p_rubicon_buffer = config_file_buffer;			
	while(p_rubicon_buffer < config_file_buffer + sizeof(config_file_buffer)) *p_rubicon_buffer++ = NULL;
	p_rubicon_buffer = config_file_buffer;
	
	ret = strstr((const char *) rubicon_ctrl_buffer, "<HWI>");
		
	if(ret == NULL)
	{
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	ret += RUBICON_CONFIG_FILE_TAG_LENGHT;
	
	while((*ret != '<') && (*ret != NULL))
	{
		*p_rubicon_buffer++ = *ret++;
	}
	
	++p_rubicon_buffer;
	
	ret = strstr((const char *) rubicon_ctrl_buffer, "<FWI>");
		
	if(ret == NULL)
	{
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	ret += RUBICON_CONFIG_FILE_TAG_LENGHT;
	
	while((*ret != '<') && (*ret != NULL))
	{
		*p_rubicon_buffer++ = *ret++;
	}
	
	++p_rubicon_buffer;
	
	ret = strstr((const char *) rubicon_ctrl_buffer, "<UDT>");
		
	if(ret == NULL)
	{
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	ret += RUBICON_CONFIG_FILE_TAG_LENGHT;
	
	while((*ret != '<') && (*ret != NULL))
	{
		*p_rubicon_buffer++ = *ret++;
	}
	
	config_file_byte_cnt = p_rubicon_buffer - config_file_buffer;
	
	return (FILE_OK);
}

uint8_t RUBICON_CreateUpdateAddresseList(void)
{
	UINT brd;
	char *rtn;
	uint8_t fual_add[8];
	uint16_t tmp_add, fual_cnt, tmp_cnt;
	
	brd = 0;
	tmp_cnt = 0;
	tmp_add = 0;
	fual_cnt = 0;

	if(f_lseek (&file_SD, config_file_byte_cnt) != FR_OK)
	{
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	if(f_read (&file_SD, rubicon_ctrl_buffer, RUBICON_CONFIG_FILE_MAX_SIZE, &brd) != FR_OK)
	{
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	rtn = strstr((const char *) rubicon_ctrl_buffer, "<UFA>");
		
	if(rtn == NULL)
	{
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	p_rubicon_buffer = fual_add;			
	while(p_rubicon_buffer < fual_add + sizeof(fual_add)) *p_rubicon_buffer++ = NULL;
	p_rubicon_buffer = fual_add;
	
	rtn += RUBICON_CONFIG_FILE_TAG_LENGHT;
	
	while((*rtn != '<') && (*rtn != NULL))
	{
		*p_rubicon_buffer++ = *rtn++;
		
		if((*rtn == ',') || (*rtn == '<'))
		{
			if(*rtn == ',') rtn++;
			
			tmp_add = atoi((char *) fual_add);
			tmp_cnt = 0;
			
			while(rubicon_address_list[tmp_cnt] != 0x0000)
			{
				if(rubicon_address_list[tmp_cnt] == tmp_add)
				{
					rubicon_firmware_update_address_list[fual_cnt] = tmp_add;
					p_rubicon_buffer = fual_add;			
					while(p_rubicon_buffer < fual_add + sizeof(fual_add)) *p_rubicon_buffer++ = NULL;
					p_rubicon_buffer = fual_add;
					fual_cnt++;
				}
				tmp_cnt++;
			}
		}
	}
	
	rtn++;
	brd = 0;
	tmp_cnt = 0;
	tmp_add = 0;
	fual_cnt = 0;
	
	config_file_byte_cnt = ((uint8_t *)rtn - rubicon_ctrl_buffer) + config_file_byte_cnt;
	
	
	if(f_lseek (&file_SD, config_file_byte_cnt) != FR_OK)
	{
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	if(f_read (&file_SD, rubicon_ctrl_buffer, RUBICON_CONFIG_FILE_MAX_SIZE, &brd) != FR_OK)
	{
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	if(brd >= RUBICON_CONFIG_FILE_MAX_SIZE)
	{
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	rtn = strstr((const char *) rubicon_ctrl_buffer, "<UIA>");
		
	if(rtn == NULL)
	{
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	p_rubicon_buffer = fual_add;			
	while(p_rubicon_buffer < fual_add + sizeof(fual_add)) *p_rubicon_buffer++ = NULL;
	p_rubicon_buffer = fual_add;
	
	rtn += RUBICON_CONFIG_FILE_TAG_LENGHT;
	
	while((*rtn != '<') && (*rtn != NULL))
	{
		*p_rubicon_buffer++ = *rtn++;
		
		if((*rtn == ',') || (*rtn == '<'))
		{
			if(*rtn == ',') rtn++;
			
			tmp_add = atoi((char *) fual_add);
			tmp_cnt = 0;
			
			while(rubicon_address_list[tmp_cnt] != 0x0000)
			{
				if(rubicon_address_list[tmp_cnt] == tmp_add)
				{
					rubicon_image_update_address_list[fual_cnt] = tmp_add;
					p_rubicon_buffer = fual_add;			
					while(p_rubicon_buffer < fual_add + sizeof(fual_add)) *p_rubicon_buffer++ = NULL;
					p_rubicon_buffer = fual_add;
					fual_cnt++;
				}
				tmp_cnt++;
			}
		}
	}
	
	f_close(&file_SD);
	
	return FILE_OK;
}

uint8_t RUBICON_CheckNewFirmwareFile(void)
{
	if (f_mount(&filesystem, "0:", 0) != FR_OK)
	{
		return FILE_SYS_ERROR;
	}

	if (f_opendir(&dir_1, "/") != FR_OK)
	{
		f_mount(NULL,"0:",0);
		return FILE_DIR_ERROR;
	}
		
	if (f_open(&file_SD, "NEW.BIN", FA_READ) != FR_OK) 
	{
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
    rs485_rubicon_address = atoi((char *) rubicon_ctrl_buffer);

	
    RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_INIT;
    RUBICON_FirmwareUpdatePacket.send_attempt = 0;
    RUBICON_FirmwareUpdatePacket.packet_send = 0;
    RUBICON_FirmwareUpdatePacket.last_packet_send = 0;
	RUBICON_FirmwareUpdatePacket.packet_total = file_SD.obj.objsize / RUBICON_PACKET_BUFFER_SIZE;

    if ((RUBICON_FirmwareUpdatePacket.packet_total * RUBICON_PACKET_BUFFER_SIZE) < file_SD.obj.objsize)
    {
        ++RUBICON_FirmwareUpdatePacket.packet_total;
    }

	DISPLAY_FileProgressBarShowSet();
    return FILE_OK;	
}

uint8_t RUBICON_CheckNewImageFile(void)
{
	uint32_t k, i;
	UINT dr;
	char file_name[16];
	char directory_name[16];
	
	if (f_mount(&filesystem, "0:", 0) != FR_OK)
	{
		return FILE_SYS_ERROR;
	}
	/**
	*
	* copy user data to temp buffer
	*
	*/
	i = 0;
	while(i < 16) 
	{
		file_name[i] = NULL;
		directory_name[i] = NULL;
		i++;
	}	
	/**
	*
	* release buffer for local usage
	*
	*/
	rs485_rubicon_address = atoi((char *) rubicon_ctrl_buffer);
	

	if(RUBICON_FileUpdatePacket.update_command == RUBICON_DOWNLOAD_DIPLAY_IMAGE)
	{				
		directory_name[0] = '/';
		Int2Str((char *)file_name, rubicon_image_update_address_list[rubicon_image_update_list_cnt], 0U);
		Int2Str((char *)&directory_name[1], rubicon_image_update_address_list[rubicon_image_update_list_cnt], 0U);
		i = strlen(file_name);
		file_name[i++] = '/';
		Int2Str((char *)&file_name[i], rubicon_image_update_address_list[rubicon_image_update_list_cnt], 0U);
		i = strlen(file_name);
		file_name[i++] = '_';
		file_name[i] = NULL;
		Int2Str((char *)&file_name[i], rubicon_file_update_list[rubicon_file_update_list_cnt], 0U);
		i = strlen(file_name);
		
		if (f_opendir(&dir_1, directory_name) != FR_OK)
		{
			f_mount(NULL,"0:",0);
			return FILE_DIR_ERROR;
		}
		
		strncat(&file_name[i], ".RAW", 4);
		
		if (f_open(&file_SD, file_name, FA_READ) == FR_OK)					
		{
			RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IDLE + rubicon_file_update_list[rubicon_file_update_list_cnt];	
		}
		else
		{
			f_mount(NULL,"0:",0);
			return FILE_ERROR;
		}
	}
	else
	{	
		k = 0;
		
		while(k < sizeof(rubicon_ctrl_buffer))
		{
			if(rubicon_ctrl_buffer[k] == NULL) break;
			else k++;
		}
		k++;
		
		if(rubicon_ctrl_buffer[k] == '1')
		{
			k++;
			
			if(rubicon_ctrl_buffer[k] == NULL)
			{
				if (f_open(&file_SD, "IMG1.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_1;
					memcpy(file_name, "IMG1.RAW", 8);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '0')
			{
				if (f_open(&file_SD, "IMG10.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}			
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_10;
					memcpy(file_name, "IMG10.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '1')
			{
				if (f_open(&file_SD, "IMG11.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_11;
					memcpy(file_name, "IMG11.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '2')
			{
				if (f_open(&file_SD, "IMG12.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_12;
					memcpy(file_name, "IMG12.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '3')
			{
				if (f_open(&file_SD, "IMG13.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_13;
					memcpy(file_name, "IMG13.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '4')
			{
				if (f_open(&file_SD, "IMG14.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_14;
					memcpy(file_name, "IMG14.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '5')
			{
				if (f_open(&file_SD, "IMG15.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_15;
					memcpy(file_name, "IMG15.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '6')
			{
				if (f_open(&file_SD, "IMG16.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_16;
					memcpy(file_name, "IMG16.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '7')
			{
				if (f_open(&file_SD, "IMG17.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_17;
					memcpy(file_name, "IMG17.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '8')
			{
				if (f_open(&file_SD, "IMG18.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_18;
					memcpy(file_name, "IMG18.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '9')
			{
				if (f_open(&file_SD, "IMG19.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_19;
					memcpy(file_name, "IMG19.RAW", 9);
				}
			}
		}
		else if(rubicon_ctrl_buffer[k] == '2') 
		{
			k++;
		
			if(rubicon_ctrl_buffer[k] == NULL)
			{
				if (f_open(&file_SD, "IMG2.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_2;
					memcpy(file_name, "IMG2.RAW", 8);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '0')
			{
				if (f_open(&file_SD, "IMG20.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}			
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_20;
					memcpy(file_name, "IMG20.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '1')
			{
				if (f_open(&file_SD, "IMG21.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}			
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_21;
					memcpy(file_name, "IMG21.RAW", 9);
				}
			}		
			else if(rubicon_ctrl_buffer[k] == '2')
			{
				if (f_open(&file_SD, "IMG22.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}			
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_22;
					memcpy(file_name, "IMG22.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '3')
			{
				if (f_open(&file_SD, "IMG23.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}			
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_23;
					memcpy(file_name, "IMG23.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '4')
			{
				if (f_open(&file_SD, "IMG24.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}			
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_24;
					memcpy(file_name, "IMG24.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '5')
			{
				if (f_open(&file_SD, "IMG25.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}			
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_25;
					memcpy(file_name, "IMG25.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '6')
			{
				if (f_open(&file_SD, "IMG26.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}			
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_26;
					memcpy(file_name, "IMG26.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '7')
			{
				if (f_open(&file_SD, "IMG27.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}			
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_27;
					memcpy(file_name, "IMG27.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '8')
			{
				if (f_open(&file_SD, "IMG28.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}			
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_28;
					memcpy(file_name, "IMG28.RAW", 9);
				}
			}
			else if(rubicon_ctrl_buffer[k] == '9')
			{
				if (f_open(&file_SD, "IMG29.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}			
				else 
				{
					RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_29;
					memcpy(file_name, "IMG29.RAW", 9);
				}
			}
		}
		else if(rubicon_ctrl_buffer[k] == '3') 
		{
			if (f_open(&file_SD, "IMG3.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else 
			{
				RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_3;
				memcpy(file_name, "IMG3.RAW", 8);
			}
		}
		else if(rubicon_ctrl_buffer[k] == '4') 
		{
			if (f_open(&file_SD, "IMG4.RAW", FA_READ) != FR_OK)
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else 
			{
				RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_4;
				memcpy(file_name, "IMG4.RAW", 8);
			}
		}
		else if(rubicon_ctrl_buffer[k] == '5') 
		{
			if (f_open(&file_SD, "IMG5.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else 
			{
				RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_5;
				memcpy(file_name, "IMG5.RAW", 8);
			}
		}
		else if(rubicon_ctrl_buffer[k] == '6') 
		{
			if (f_open(&file_SD, "IMG6.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else 
			{
				RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_6;
				memcpy(file_name, "IMG6.RAW", 8);
			}
		}
		else if(rubicon_ctrl_buffer[k] == '7') 
		{
			if (f_open(&file_SD, "IMG7.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else 
			{
				RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_7;
				memcpy(file_name, "IMG7.RAW", 8);
			}
		}
		else if(rubicon_ctrl_buffer[k] == '8') 
		{
			if (f_open(&file_SD, "IMG8.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else 
			{
				RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_8;
				memcpy(file_name, "IMG8.RAW", 8);
			}
		}
		else if(rubicon_ctrl_buffer[k] == '9') 
		{
			if (f_open(&file_SD, "IMG9.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else 
			{
				RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_9;
				memcpy(file_name, "IMG9.RAW", 8);
			}
		}
		else return FILE_ERROR;
	}
	
    /** ==========================================================================*/
    /**                                                                           */
    /**     C A L C U L A T E    C R C  3 2    F O R    I M A G E   F I L E       */ 
    /**                                                                           */
    /** ==========================================================================*/
	CRC_ResetDR();
	crc_32_file = 0;
	dr = 0;
	RUBICON_FileUpdatePacket.file_data_read = 0;
	
	while(RUBICON_FileUpdatePacket.file_data_read != file_SD.obj.objsize)
	{
		f_read(&file_SD, rubicon_ctrl_buffer, RUBICON_BUFFER_SIZE, &dr);
		RUBICON_FileUpdatePacket.file_data_read += dr;
		crc_32_file = CRC_Calculate8(rubicon_ctrl_buffer, dr);
	}
	
	/*
	* reopen file
	*/
	f_close(&file_SD);
	
	if (f_open(&file_SD, file_name, FA_READ) != FR_OK)
	{
		f_mount(NULL,"0:",0);
		return FILE_ERROR;
	}
	
	RUBICON_FileUpdatePacket.send_attempt = 0;
    RUBICON_FileUpdatePacket.packet_send = 0;
    RUBICON_FileUpdatePacket.last_packet_send = 0;
	RUBICON_FileUpdatePacket.packet_total = file_SD.obj.objsize / RUBICON_PACKET_BUFFER_SIZE;

	if ((RUBICON_FileUpdatePacket.packet_total * RUBICON_PACKET_BUFFER_SIZE) < file_SD.obj.objsize)
    {
        ++RUBICON_FileUpdatePacket.packet_total;
    }
	
	rubicon_file_progress = 0;
	DISPLAY_FileProgressBarShowSet();
    return FILE_OK;
}

void RUBICON_PrepareFirmwareUpdatePacket(void)
{
	uint32_t i;

    if (RUBICON_FirmwareUpdatePacket.send_attempt >= RUBICON_MAX_ERRORS)
    {
        RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_FAIL;
    }

    if ((RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FAIL) || \
	   (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINISHED))
    {
        RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
        f_close(&file_SD);
		f_mount(NULL,"0:",0);
        return;
    }
    else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_INIT)
    {
        tx_buffer[0] = RUBICON_SOH;
        tx_buffer[5] = 0x01;
        tx_buffer[6] = RUBICON_START_BOOTLOADER;
        RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_BOOTLOADER;
    }
    else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_BOOTLOADER)
    {
        tx_buffer[0] = RUBICON_SOH;
        tx_buffer[5] = 0x03;
        tx_buffer[6] = RUBICON_DOWNLOAD_FIRMWARE;
        tx_buffer[7] = RUBICON_FirmwareUpdatePacket.packet_total >> 8;
        tx_buffer[8] = RUBICON_FirmwareUpdatePacket.packet_total & 0xff;
        RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_RUN;
    }
    else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN)
    {
        tx_buffer[0] = RUBICON_STX;
        tx_buffer[5] = 0x42;
        tx_buffer[6] = RUBICON_FirmwareUpdatePacket.packet_send >> 8;
        tx_buffer[7] = RUBICON_FirmwareUpdatePacket.packet_send & 0xff;

        f_read(&file_SD, (uint8_t*) &tx_buffer[8], RUBICON_PACKET_BUFFER_SIZE, (UINT*) (&RUBICON_FirmwareUpdatePacket.file_data_read));
		
		if (RUBICON_FileUpdatePacket.packet_send == 1) delay(500);		
	}

    tx_buffer[1] = rs485_rubicon_address >> 8;
    tx_buffer[2] = rs485_rubicon_address & 0x00ff;
    tx_buffer[3] = rs485_interface_address >> 8;
    tx_buffer[4] = rs485_interface_address & 0x00ff;

    rs485_packet_checksum = 0;

    for (i = 6; i < (tx_buffer[5] + 6); i++)
    {
        rs485_packet_checksum += tx_buffer[i];
    }

    tx_buffer[tx_buffer[5] + 6] = rs485_packet_checksum >> 8;
    tx_buffer[tx_buffer[5] + 7] = rs485_packet_checksum;
    tx_buffer[tx_buffer[5] + 8] = RUBICON_EOT;
	
	rubicon_file_progress = ((RUBICON_FirmwareUpdatePacket.packet_send * 100) / RUBICON_FirmwareUpdatePacket.packet_total);
	PROGBAR_SetValue(hPROGBAR_FileTransfer, rubicon_file_progress);
}

void RUBICON_PrepareFileUpdatePacket(void)
{
	uint32_t i;
	UINT rd;

    if (RUBICON_FileUpdatePacket.send_attempt >= RUBICON_MAX_ERRORS)
    {
        RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_FAIL;
    }

    if ((RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_FAIL) || \
	   (RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_FINISHED))
    {
		UpdateFileTransferDisplay();
        RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IDLE;
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
        return;
    }
	else if (RUBICON_FileUpdatePacket.packet_send == 0)
    {
        tx_buffer[0] = RUBICON_SOH;
        tx_buffer[5] = 0x0b;
		
        if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_1) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_1;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_2) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_2;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_3) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_3;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_4) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_4;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_5) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_5;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_6) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_6;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_7) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_7;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_8) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_8;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_9) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_9;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_10) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_10;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_11) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_11;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_12) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_12;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_13) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_13;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_14) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_14;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_15) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_15;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_16) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_16;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_17) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_17;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_18) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_18;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_19) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_19;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_20) tx_buffer[6] = RUBICON_DOWNLOAD_FIRMWARE_IMAGE;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_21) tx_buffer[6] = RUBICON_DOWNLOAD_BOOTLOADER_IMAGE;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_22) tx_buffer[6] = RUBICON_DOWNLOAD_SMALL_FONT;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_23) tx_buffer[6] = RUBICON_DOWNLOAD_MIDDLE_FONT;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_24) tx_buffer[6] = RUBICON_DOWNLOAD_BIG_FONT;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_25) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_25;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_26) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_26;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_27) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_27;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_28) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_28;
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_29) tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_29;
		
        tx_buffer[7] = RUBICON_FileUpdatePacket.packet_total >> 8;
        tx_buffer[8] = RUBICON_FileUpdatePacket.packet_total & 0xff;
		tx_buffer[9] = (file_SD.obj.objsize >> 24);
		tx_buffer[10] = (file_SD.obj.objsize >> 16);
		tx_buffer[11] = (file_SD.obj.objsize >> 8);
		tx_buffer[12] = (file_SD.obj.objsize & 0xff);
		tx_buffer[13] = (crc_32_file >> 24);
		tx_buffer[14] = (crc_32_file >> 16);
		tx_buffer[15] = (crc_32_file >> 8);
		tx_buffer[16] = (crc_32_file & 0xff);
    }
    else
    {
        f_read(&file_SD, (uint8_t*) &tx_buffer[8], RUBICON_PACKET_BUFFER_SIZE, &rd);
		tx_buffer[0] = RUBICON_STX;
        tx_buffer[5] = (rd + 2);
        tx_buffer[6] = RUBICON_FileUpdatePacket.packet_send >> 8;
        tx_buffer[7] = RUBICON_FileUpdatePacket.packet_send & 0xff;
		
        		
		if (RUBICON_FileUpdatePacket.packet_send == 1) delay(500);	
	}

    tx_buffer[1] = rs485_rubicon_address >> 8;
    tx_buffer[2] = rs485_rubicon_address & 0x00ff;
    tx_buffer[3] = rs485_interface_address >> 8;
    tx_buffer[4] = rs485_interface_address & 0x00ff;

    rs485_packet_checksum = 0;

    for (i = 6; i < (tx_buffer[5] + 6); i++)
    {
        rs485_packet_checksum += tx_buffer[i];
    }

    tx_buffer[tx_buffer[5] + 6] = rs485_packet_checksum >> 8;
    tx_buffer[tx_buffer[5] + 7] = rs485_packet_checksum;
    tx_buffer[tx_buffer[5] + 8] = RUBICON_EOT;
	
	rubicon_file_progress = ((RUBICON_FileUpdatePacket.packet_send * 100) / RUBICON_FileUpdatePacket.packet_total);
	PROGBAR_SetValue(hPROGBAR_FileTransfer, rubicon_file_progress);
}

void RUBICON_PrepareCommandPacket(uint8_t command, uint8_t *ibuff)
{
    uint32_t i;
	uint8_t hex[6];

    tx_buffer[0] = RUBICON_SOH;
    tx_buffer[1] = rs485_rubicon_address >> 8;
    tx_buffer[2] = rs485_rubicon_address & 0x00ff;
    tx_buffer[3] = rs485_interface_address >> 8;
    tx_buffer[4] = rs485_interface_address & 0x00ff;
    tx_buffer[5] = 0x01;
    tx_buffer[6] = command;
	
    if (command == RUBICON_SET_DISPLAY_BRIGHTNESS)
    {		
        tx_buffer[5] = 0x03;
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
        tx_buffer[7] = i >> 8;		// display brightness MSB
        tx_buffer[8] = i & 0xff;	// display brightness LSB
    }
    else if (command == RUBICON_DOWNLOAD_JOURNAL_1)
    {	
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = 7;
		while((*ibuff != NULL) && (*ibuff != ';')) tx_buffer[i++] = *ibuff++;
		
		tx_buffer[i++] = *ibuff++;
		tx_buffer[i] = NULL;
		tx_buffer[5] = (i - 5);
    }
	else if (command == RUBICON_SET_RS485_CONFIG)
    {
        tx_buffer[5] = 14;
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
		hex[0] = i >> 8;
		hex[1] = i & 0xff;
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
		hex[2] = i >> 8;
		hex[3] = i & 0xff;
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
		hex[4] = i >> 8;
		hex[5] = i & 0xff;
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		Hex2Str((char *)&tx_buffer[7], hex, 12U);
		tx_buffer[19] = *ibuff;
    }
	else if (command == RUBICON_SET_BEDDING_REPLACEMENT_PERIOD)
    {
		while(*ibuff != NULL) ++ibuff;		
		++ibuff;
        tx_buffer[5] = 3;
		tx_buffer[7] = *ibuff++;
		if(*ibuff == NULL)
		{
			tx_buffer[8] = tx_buffer[7];
			tx_buffer[7] = '0';
		}
		else tx_buffer[8] = *ibuff;
    }
    else if (command == RUBICON_SET_DOUT_STATE)
    {
        tx_buffer[5] = 10;
		
		while(*ibuff != NULL) ++ibuff;
		
		++ibuff;
        i = 7;
        while (i < 23) tx_buffer[i++] = *ibuff++;
    }
    else if ((command == RUBICON_SET_MIFARE_KEY_A) || (command == RUBICON_SET_MIFARE_KEY_B))
    {
        tx_buffer[5] = 13;
    }
    else if (command == RUBICON_SET_MIFARE_PERMITED_GROUP)
    {
        tx_buffer[5] = 17;
    }
	else if (command == RUBICON_SET_ROOM_STATUS)
    {		
        tx_buffer[5] = 0x02;
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
        tx_buffer[7] = *ibuff;
		++ibuff;
		if(*ibuff != NULL) 
		{
			tx_buffer[5] = 0x03;
			tx_buffer[8] = *ibuff;
		}
    }
	else if (command == RUBICON_RESET_SOS_ALARM)
    {
        tx_buffer[5] = 2;
		tx_buffer[7] = '1';
    }
	else if (command == RUBICON_SET_ROOM_TEMPERATURE)
    {
        tx_buffer[5] = 8;
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
		
		if (i > 127)
		{
			tx_buffer[7] = 'E';
			i -= 128;
		}
		else
		{
			tx_buffer[7] = 'D';
		}
		
		if (i > 63)
		{
			tx_buffer[8] = 'H';
			i -= 64;
		}
		else
		{
			tx_buffer[8] = 'C';
		}
		
		tx_buffer[9] = NULL;
		tx_buffer[10] = NULL;
		
		while(i > 9)
		{
			++tx_buffer[9];
			i -= 10;
		}
		
		tx_buffer[9] += 48;
		tx_buffer[10] = i + 48;		
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
		
		if (i > 127)
		{
			tx_buffer[7] = 'O';
			i -= 128;
		}		
		tx_buffer[11] = NULL;
		tx_buffer[12] = NULL;
		tx_buffer[13] = NULL;
		
		while(i > 99)
		{
			++tx_buffer[11];
			i -= 100;
		}
		
		tx_buffer[11] += 48;
		
		while(i > 9)
		{
			++tx_buffer[12];
			i -= 10;
		}
		
		tx_buffer[12] += 48;
		tx_buffer[13] = i + 48;	
    }
	else if (command == RUBICON_SET_SYSTEM_ID)
    {		
        tx_buffer[5] = 0x03;
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
        tx_buffer[7] = i >> 8;		// system id MSB
        tx_buffer[8] = i & 0xff;	// system id LSB
    }
	else if (command == RUBICON_SET_ROOM_THERMOSTAT_IMAGE)
    {		
        tx_buffer[5] = 0x06;
        while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
        tx_buffer[7] = i & 0xff;	// room thermostat address id 0 - 255
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
        tx_buffer[8] = i & 0xff;	// room thermostat display image id 0 - 255
        while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
        tx_buffer[9] = i & 0xff;	// room thermostat display image time 0 - 255
        while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
        tx_buffer[10] = i & 0xff;	// room thermostat buzzer mode 0 - 255
        while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
        tx_buffer[11] = i & 0xff;	// room thermostat buzzer repeat time 0 - 255
    }
    
    rs485_packet_checksum = 0;

    for (i = 6; i < (tx_buffer[5] + 6); i++)
    {
        rs485_packet_checksum += tx_buffer[i];
    }

    tx_buffer[tx_buffer[5] + 6] = rs485_packet_checksum >> 8;
    tx_buffer[tx_buffer[5] + 7] = rs485_packet_checksum;
    tx_buffer[tx_buffer[5] + 8] = RUBICON_EOT;
}

void CONTROLLER_WriteLogEvent(uint8_t log_event, uint8_t event_detail)
{
	static uint16_t log_id = 0;
	
	RTC_GetDate(RTC_Format_BCD, &RTC_Date);
    RTC_GetTime(RTC_Format_BCD, &RTC_Time);
	
	log_id++;
	i2c_ee_buffer[0] = (log_id >> 8);
	i2c_ee_buffer[1] = (log_id & 0xff);
	i2c_ee_buffer[2] = log_event;
	i2c_ee_buffer[3] = NULL;
	i2c_ee_buffer[4] = NULL;
	i2c_ee_buffer[5] = event_detail;
	i2c_ee_buffer[6] = NULL;
	i2c_ee_buffer[7] = NULL;
	i2c_ee_buffer[8] = NULL;
	i2c_ee_buffer[9] = NULL;
	i2c_ee_buffer[10] = RTC_Date.RTC_Date;
    i2c_ee_buffer[11] = RTC_Date.RTC_Month;
    i2c_ee_buffer[12] = RTC_Date.RTC_Year;
    i2c_ee_buffer[13] = RTC_Time.RTC_Hours;
    i2c_ee_buffer[14] = RTC_Time.RTC_Minutes;
    i2c_ee_buffer[15] = RTC_Time.RTC_Seconds;
	RUBICON_WriteLogToList(1);
}

void RUBICON_PrepareLogUpdatePacket(void)
{
    if (RUBICON_LogListTransfer.send_attempt >= RUBICON_MAX_ERRORS)
    {
        RUBICON_LogListTransfer.log_transfer_state = LOG_TRANSFER_IDLE;
        return;
    }
    else if (RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_QUERY_LIST)
    {
        tx_buffer[6] = RUBICON_GET_LOG_LIST;
    }
    else if (RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_DELETE_LOG)
    {
        tx_buffer[6] = RUBICON_DELETE_LOG_LIST;
    }

    tx_buffer[0] = RUBICON_SOH;
    tx_buffer[1] = rs485_rubicon_address >> 8;
    tx_buffer[2] = rs485_rubicon_address & 0x00ff;
    tx_buffer[3] = rs485_interface_address >> 8;
    tx_buffer[4] = rs485_interface_address & 0x00ff;
    tx_buffer[5] = 0x01;
    tx_buffer[7] = 0x00;
    tx_buffer[8] = tx_buffer[6];
    tx_buffer[9] = RUBICON_EOT;
}

void RUBICON_WriteLogToList(uint8_t source)
{
	uint8_t e;
	
	
	e = 0;
	
	if(source == 0)
	{
		while(e < RUBICON_LOG_SIZE) 
		{
			i2c_ee_buffer[e] = rx_buffer[7 + e];
			++e;
		}		
	}
	
	i2c_ee_buffer[3] = rs485_rubicon_address >> 8;
	i2c_ee_buffer[4] = rs485_rubicon_address & 0x00ff;
			
	/**
	*	LOG_LIST_TYPE_1 -> log list is empty and next log address is first address
	*	0000000000000000000000000000000000000000000000000000000000000000000000000
	*
	*	LOG_LIST_TYPE_2 -> log list start at some addres, it's full till last address, next log address is first address and is free for write 
	*	000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
	*
	* 	LOG_LIST_TYPE_3 -> log list start at some addres, end at upper address, next log address is next upper from end address and is free for write
	*	000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxx000000000000000000000000000
	*
	*	LOG_LIST_TYPE_4 -> log list start at first address, end at last address, it's full, next log address is first memory address, write is forbiden
	*	xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
	*	
	*	LOG_LIST_TYPE_5 -> log list start at first address, end at upper address, and next upper log address is free for write
	*	xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00000000000000000000000000000000000000000
	*
	*	LOG_LIST_TYPE_6 -> log list start at upper address, end at lower address and next upper from end address is free for write
	*	xxxxxxxxxxxx0000000000000000000000000000000000000000000000xxxxxxxxxxxxxxx
	*/
	
	switch (RUBICON_LogMemory.LOG_MemoryFragment)
	{
		case LOG_LIST_UNDEFINED:
			/** should newer get here */
			break;
		
		
		case LOG_LIST_TYPE_1:
			
			I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, RUBICON_LogMemory.next_log_address, RUBICON_LOG_SIZE);
			delay(I2C_EE_WRITE_DELAY);	
			++RUBICON_LogMemory.log_list_cnt;
			RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_5;
			RUBICON_LogMemory.next_log_address += RUBICON_LOG_SIZE;
			break;
		
		
		case LOG_LIST_TYPE_2:
			
			if(RUBICON_LogMemory.next_log_address < I2C_EE_PAGE_SIZE)
			{
				I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, RUBICON_LogMemory.next_log_address, RUBICON_LOG_SIZE);
				delay(I2C_EE_WRITE_DELAY);
			}
			else
			{
				I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_1, RUBICON_LogMemory.next_log_address, RUBICON_LOG_SIZE);
				delay(I2C_EE_WRITE_DELAY);
			}
			
			RUBICON_LogMemory.last_log_address = RUBICON_LogMemory.next_log_address;
			RUBICON_LogMemory.next_log_address += RUBICON_LOG_SIZE;
			++RUBICON_LogMemory.log_list_cnt;
			
			if (RUBICON_LogMemory.next_log_address == RUBICON_LogMemory.first_log_address)
			{
				RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_4;
			}			
			break;
		
			
		case LOG_LIST_TYPE_3:
			
			if(RUBICON_LogMemory.next_log_address < I2C_EE_PAGE_SIZE)
			{
				I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, RUBICON_LogMemory.next_log_address, RUBICON_LOG_SIZE);
				delay(I2C_EE_WRITE_DELAY);
			}
			else
			{
				I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_1, RUBICON_LogMemory.next_log_address, RUBICON_LOG_SIZE);
				delay(I2C_EE_WRITE_DELAY);
			}
			
			RUBICON_LogMemory.last_log_address = RUBICON_LogMemory.next_log_address;
			RUBICON_LogMemory.next_log_address += RUBICON_LOG_SIZE;
			++RUBICON_LogMemory.log_list_cnt;
			
			if (RUBICON_LogMemory.next_log_address > (I2C_EE_LOG_LIST_END_ADDRESS - RUBICON_LOG_SIZE))
			{
				RUBICON_LogMemory.next_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
			}
			else if (RUBICON_LogMemory.next_log_address == RUBICON_LogMemory.first_log_address)
			{
				RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_4;
			}
			break;
		
			
		case LOG_LIST_TYPE_4:
			
			break;
		
		
		case LOG_LIST_TYPE_5:
			
			if(RUBICON_LogMemory.next_log_address < I2C_EE_PAGE_SIZE)
			{
				I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, RUBICON_LogMemory.next_log_address, RUBICON_LOG_SIZE);
				delay(I2C_EE_WRITE_DELAY);
			}
			else
			{
				I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_1, RUBICON_LogMemory.next_log_address, RUBICON_LOG_SIZE);
				delay(I2C_EE_WRITE_DELAY);
			}
			
			RUBICON_LogMemory.last_log_address = RUBICON_LogMemory.next_log_address;
			RUBICON_LogMemory.next_log_address += RUBICON_LOG_SIZE;
			++RUBICON_LogMemory.log_list_cnt;			
			
			if (RUBICON_LogMemory.next_log_address > (I2C_EE_LOG_LIST_END_ADDRESS - RUBICON_LOG_SIZE))
			{
				RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_4;
			}
			break;
		
			
		case LOG_LIST_TYPE_6:
			
			if(RUBICON_LogMemory.next_log_address < I2C_EE_PAGE_SIZE)
			{
				I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, RUBICON_LogMemory.next_log_address, RUBICON_LOG_SIZE);
				delay(I2C_EE_WRITE_DELAY);
			}
			else
			{
				I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_1, RUBICON_LogMemory.next_log_address, RUBICON_LOG_SIZE);
				delay(I2C_EE_WRITE_DELAY);
			}
			
			RUBICON_LogMemory.last_log_address = RUBICON_LogMemory.next_log_address;
			RUBICON_LogMemory.next_log_address += RUBICON_LOG_SIZE;
			++RUBICON_LogMemory.log_list_cnt;
			
			if (RUBICON_LogMemory.next_log_address == RUBICON_LogMemory.first_log_address)
			{
				RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_4;
			}
			break;
		
			
		default:
			
			break;
		
	}
}

void RUBICON_DeleteBlockFromLogList(void)
{
	uint16_t x_cnt;
	uint32_t delete_cnt;
	/**
	*	LOG_LIST_TYPE_1 -> log list is empty and next log address is first address
	*	0000000000000000000000000000000000000000000000000000000000000000000000000
	*
	*	LOG_LIST_TYPE_2 -> log list start at some addres, it's full till last address, next log address is first address and is free for write 
	*	000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
	*
	* 	LOG_LIST_TYPE_3 -> log list start at some addres, end at upper address, next log address is next upper from end address and is free for write
	*	000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxx000000000000000000000000000
	*
	*	LOG_LIST_TYPE_4 -> log list start at first address, end at last address, it's full, next log address is first memory address, write is forbiden
	*	xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
	*	
	*	LOG_LIST_TYPE_5 -> log list start at first address, end at upper address, and next upper log address is free for write
	*	xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00000000000000000000000000000000000000000
	*
	*	LOG_LIST_TYPE_6 -> log list start at upper address, end at lower address and next upper from end address is free for write
	*	xxxxxxxxxxxx0000000000000000000000000000000000000000000000xxxxxxxxxxxxxxx
	*/
	
	switch (RUBICON_LogMemory.LOG_MemoryFragment)
	{
		case LOG_LIST_UNDEFINED:
			/** should newer get here */
			RUBICON_StartDisplayTimer();
			LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log list undefined state");
			break;
		
		
		case LOG_LIST_TYPE_1:
			
			break;
		
		
		case LOG_LIST_TYPE_2:
			
			if(HTTP_LogListTransfer.log_transfer_state == HTTP_DELETE_LOG_LIST)
			{				
				x_cnt = 0;				
				while(x_cnt < I2C_EE_BLOCK_SIZE) i2c_ee_buffer[x_cnt++] = NULL;				
				delete_cnt = RUBICON_LogMemory.first_log_address;				
				while(delete_cnt >= I2C_EE_BLOCK_SIZE) delete_cnt -= I2C_EE_BLOCK_SIZE;				
				if(delete_cnt != 0) delete_cnt = ((I2C_EE_BLOCK_SIZE - delete_cnt) - 1);
				else delete_cnt = I2C_EE_BLOCK_SIZE - 1;
				
				/**
				*	delete current block
				*/
				if(RUBICON_LogMemory.first_log_address < I2C_EE_PAGE_SIZE)
				{
					I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, RUBICON_LogMemory.first_log_address, delete_cnt);
					delay(I2C_EE_WRITE_DELAY);
				}
				else
				{
					I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_1, RUBICON_LogMemory.first_log_address, delete_cnt);
					delay(I2C_EE_WRITE_DELAY);
				}
				/**
				*	set first log address
				*/
				if((RUBICON_LogMemory.first_log_address + delete_cnt + 1) >= I2C_EE_LOG_LIST_END_ADDRESS)
				{
					/**
					*	set memory fragmentation type
					*/
					if(RUBICON_LogMemory.next_log_address != I2C_EE_LOG_LIST_START_ADDRESS)
					{
						RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_5;
					}
					else
					{
						RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_1;
					}
					
					RUBICON_LogMemory.first_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
				}
				else 
				{
					/**
					*	set memory fragmentation type
					*/
					if(RUBICON_LogMemory.next_log_address != I2C_EE_LOG_LIST_START_ADDRESS)
					{
						RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_6;
					}
					
					RUBICON_LogMemory.first_log_address += delete_cnt + 1;
				}
				
				HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_DELETED;
				RUBICON_LogMemory.log_list_cnt -= ((delete_cnt + 1) / RUBICON_LOG_SIZE);				
			}
			break;
		
			
		case LOG_LIST_TYPE_3:
			
			if(HTTP_LogListTransfer.log_transfer_state == HTTP_DELETE_LOG_LIST)
			{				
				x_cnt = 0;				
				while(x_cnt < I2C_EE_BLOCK_SIZE) i2c_ee_buffer[x_cnt++] = NULL;				
				delete_cnt = RUBICON_LogMemory.first_log_address;				
				while(delete_cnt >= I2C_EE_BLOCK_SIZE) delete_cnt -= I2C_EE_BLOCK_SIZE;				
				if(delete_cnt != 0) delete_cnt = ((I2C_EE_BLOCK_SIZE - delete_cnt) - 1);
				else delete_cnt = I2C_EE_BLOCK_SIZE - 1;
				
				if((RUBICON_LogMemory.first_log_address + delete_cnt + 1) >= (RUBICON_LogMemory.last_log_address + RUBICON_LOG_SIZE))
				{
					delete_cnt = ((RUBICON_LogMemory.last_log_address + RUBICON_LOG_SIZE) - RUBICON_LogMemory.first_log_address) - 1;
					if(RUBICON_LogMemory.first_log_address < I2C_EE_PAGE_SIZE)
					{
						I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, RUBICON_LogMemory.first_log_address, delete_cnt);
						delay(I2C_EE_WRITE_DELAY);
					}
					else
					{
						I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_1, RUBICON_LogMemory.first_log_address, delete_cnt);
						delay(I2C_EE_WRITE_DELAY);
					}
					RUBICON_LogMemory.first_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
					RUBICON_LogMemory.last_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
					RUBICON_LogMemory.next_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
					RUBICON_LogMemory.log_list_cnt = 0;
					RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_1;
				}
				else
				{
					if(RUBICON_LogMemory.first_log_address < I2C_EE_PAGE_SIZE)
					{
						I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, RUBICON_LogMemory.first_log_address, delete_cnt);
						delay(I2C_EE_WRITE_DELAY);
					}
					else
					{
						I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_1, RUBICON_LogMemory.first_log_address, delete_cnt);
						delay(I2C_EE_WRITE_DELAY);
					}
					RUBICON_LogMemory.log_list_cnt -= ((delete_cnt + 1) / RUBICON_LOG_SIZE);
					RUBICON_LogMemory.first_log_address += delete_cnt + 1;
				}
				/**
				*	delete current block
				*/
				
				
				HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_DELETED;
								
			}
			break;
		
			
		case LOG_LIST_TYPE_4:
			
			if(HTTP_LogListTransfer.log_transfer_state == HTTP_DELETE_LOG_LIST)
			{
				x_cnt = 0;
				 
				while(x_cnt < I2C_EE_BLOCK_SIZE) i2c_ee_buffer[x_cnt++] = NULL;
				
				I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, I2C_EE_LOG_LIST_START_ADDRESS, I2C_EE_BLOCK_SIZE - 1);
				delay(I2C_EE_WRITE_DELAY);
				
				HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_DELETED;
				RUBICON_LogMemory.first_log_address = HTTP_LogListTransfer.log_transfer_end_address;
				RUBICON_LogMemory.last_log_address = I2C_EE_LOG_LIST_END_ADDRESS - RUBICON_LOG_SIZE;
				RUBICON_LogMemory.next_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
				RUBICON_LogMemory.log_list_cnt -= (I2C_EE_BLOCK_SIZE / RUBICON_LOG_SIZE);
				RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_2;
			}
			break;
		
		
		case LOG_LIST_TYPE_5:
			
			if(HTTP_LogListTransfer.log_transfer_state == HTTP_DELETE_LOG_LIST)
			{				
				x_cnt = 0;				
				while(x_cnt < I2C_EE_BLOCK_SIZE) i2c_ee_buffer[x_cnt++] = NULL;				
				delete_cnt = RUBICON_LogMemory.first_log_address;				
				while(delete_cnt >= I2C_EE_BLOCK_SIZE) delete_cnt -= I2C_EE_BLOCK_SIZE;				
				if(delete_cnt != 0) delete_cnt = ((I2C_EE_BLOCK_SIZE - delete_cnt) - 1);
				else delete_cnt = I2C_EE_BLOCK_SIZE - 1;
				
				if((RUBICON_LogMemory.first_log_address + delete_cnt + 1) >= (RUBICON_LogMemory.last_log_address + RUBICON_LOG_SIZE))
				{
					delete_cnt = ((RUBICON_LogMemory.last_log_address + RUBICON_LOG_SIZE) - RUBICON_LogMemory.first_log_address) - 1;
					/**
					*	delete current block
					*/
					if(RUBICON_LogMemory.first_log_address < I2C_EE_PAGE_SIZE)
					{
						I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, RUBICON_LogMemory.first_log_address, delete_cnt);
						delay(I2C_EE_WRITE_DELAY);
					}
					else
					{
						I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_1, RUBICON_LogMemory.first_log_address, delete_cnt);
						delay(I2C_EE_WRITE_DELAY);
					}
					RUBICON_LogMemory.first_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
					RUBICON_LogMemory.last_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
					RUBICON_LogMemory.next_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
					RUBICON_LogMemory.log_list_cnt = 0;
					RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_1;
				}
				else
				{
					/**
					*	delete current block
					*/
					if(RUBICON_LogMemory.first_log_address < I2C_EE_PAGE_SIZE)
					{
						I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, RUBICON_LogMemory.first_log_address, delete_cnt);
						delay(I2C_EE_WRITE_DELAY);
					}
					else
					{
						I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_1, RUBICON_LogMemory.first_log_address, delete_cnt);
						delay(I2C_EE_WRITE_DELAY);
					}
					RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_3;
					RUBICON_LogMemory.log_list_cnt -= ((delete_cnt + 1) / RUBICON_LOG_SIZE);
					RUBICON_LogMemory.first_log_address += delete_cnt + 1;
				}
				
				
				HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_DELETED;								
			}
			break;
		
			
		case LOG_LIST_TYPE_6:
			
			if(HTTP_LogListTransfer.log_transfer_state == HTTP_DELETE_LOG_LIST)
			{				
				x_cnt = 0;				
				while(x_cnt < I2C_EE_BLOCK_SIZE) i2c_ee_buffer[x_cnt++] = NULL;				
				delete_cnt = RUBICON_LogMemory.first_log_address;				
				while(delete_cnt >= I2C_EE_BLOCK_SIZE) delete_cnt -= I2C_EE_BLOCK_SIZE;				
				if(delete_cnt != 0) delete_cnt = ((I2C_EE_BLOCK_SIZE - delete_cnt) - 1);
				else delete_cnt = I2C_EE_BLOCK_SIZE - 1;
				
				/**
				*	delete current block
				*/
				if(RUBICON_LogMemory.first_log_address < I2C_EE_PAGE_SIZE)
				{
					I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, RUBICON_LogMemory.first_log_address, delete_cnt);
					delay(I2C_EE_WRITE_DELAY);
				}
				else
				{
					I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_1, RUBICON_LogMemory.first_log_address, delete_cnt);
					delay(I2C_EE_WRITE_DELAY);
				}
				/**
				*	set first log address
				*/
				if((RUBICON_LogMemory.first_log_address + delete_cnt + 1) >= I2C_EE_LOG_LIST_END_ADDRESS)
				{
					/**
					*	set memory fragmentation type
					*/
					RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_5;
					RUBICON_LogMemory.first_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
				}
				else 
				{
					RUBICON_LogMemory.first_log_address += delete_cnt + 1;
				}
				
				HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_DELETED;
				RUBICON_LogMemory.log_list_cnt -= ((delete_cnt + 1) / RUBICON_LOG_SIZE);			
			}
			break;
		
			
		default:
			
			RUBICON_StartDisplayTimer();
			LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Shit just happen, puta madre");
			break;
		
	}// End of switch
}


void RUBICON_FormatLogList(void)
{
	uint32_t delete_cnt;

	if(HTTP_LogListTransfer.log_transfer_state == HTTP_FORMAT_LOG_LIST)
	{				
		delete_cnt = 0;				
		while(delete_cnt < I2C_EE_BLOCK_SIZE) i2c_ee_buffer[delete_cnt++] = NULL;				
		delete_cnt = I2C_EE_LOG_LIST_START_ADDRESS;
		
		while(delete_cnt < I2C_EE_LOG_LIST_END_ADDRESS)
		{
			if(delete_cnt < I2C_EE_PAGE_SIZE) I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, delete_cnt, (I2C_EE_BLOCK_SIZE - 1));
			else I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_1, delete_cnt, (I2C_EE_BLOCK_SIZE - 1));
			
			delay(I2C_EE_WRITE_DELAY);
			delete_cnt += I2C_EE_BLOCK_SIZE;
		}
		
		RUBICON_LogMemory.log_list_cnt = 0;
		RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_1;		
		RUBICON_LogMemory.first_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
		RUBICON_LogMemory.last_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
		RUBICON_LogMemory.next_log_address = I2C_EE_LOG_LIST_START_ADDRESS;
		HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_FORMATED;				
	}
}


void RUBICON_ReadBlockFromLogList(void)
{
	uint32_t read_cnt;
	/**
	*	LOG_LIST_TYPE_1 -> log list is empty and next log address is first address
	*	0000000000000000000000000000000000000000000000000000000000000000000000000
	*
	*	LOG_LIST_TYPE_2 -> log list start at some addres, it's full till last address, next log address is first address and is free for write 
	*	000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
	*
	* 	LOG_LIST_TYPE_3 -> log list start at some addres, end at upper address, next log address is next upper from end address and is free for write
	*	000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxx000000000000000000000000000
	*
	*	LOG_LIST_TYPE_4 -> log list start at first address, end at last address, it's full, next log address is first memory address, write is forbiden
	*	xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
	*	
	*	LOG_LIST_TYPE_5 -> log list start at first address, end at upper address, and next upper log address is free for write
	*	xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00000000000000000000000000000000000000000
	*
	*	LOG_LIST_TYPE_6 -> log list start at upper address, end at lower address and next upper from end address is free for write
	*	xxxxxxxxxxxx0000000000000000000000000000000000000000000000xxxxxxxxxxxxxxx
	*/
	
	switch (RUBICON_LogMemory.LOG_MemoryFragment)
	{
		case LOG_LIST_UNDEFINED:
			/** should newer get here */
			RUBICON_StartDisplayTimer();
			LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log list undefined state");
			break;
		
		
		case LOG_LIST_TYPE_1:
			
			break;
		
		
		case LOG_LIST_TYPE_2:
			
			if(HTTP_LogListTransfer.log_transfer_state == HTTP_GET_LOG_LIST)
			{
				read_cnt = RUBICON_LogMemory.first_log_address;				
				while(read_cnt >= I2C_EE_BLOCK_SIZE) read_cnt -= I2C_EE_BLOCK_SIZE;				
				if(read_cnt != 0) read_cnt = ((I2C_EE_BLOCK_SIZE - read_cnt) - 1);
				else read_cnt = I2C_EE_BLOCK_SIZE - 1;
				
				if(RUBICON_LogMemory.first_log_address < I2C_EE_PAGE_SIZE)
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, RUBICON_LogMemory.first_log_address, read_cnt);
				}
				else
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_1, RUBICON_LogMemory.first_log_address, read_cnt);
				}
				
                Hex2Str((char *)rubicon_ctrl_buffer, i2c_ee_buffer, (I2C_EE_BLOCK_SIZE * 2U));		
				HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_READY;				
			}
			break;
		
			
		case LOG_LIST_TYPE_3:
			
			if(HTTP_LogListTransfer.log_transfer_state == HTTP_GET_LOG_LIST)
			{
				read_cnt = RUBICON_LogMemory.first_log_address;				
				while(read_cnt >= I2C_EE_BLOCK_SIZE) read_cnt -= I2C_EE_BLOCK_SIZE;				
				if(read_cnt != 0) read_cnt = ((I2C_EE_BLOCK_SIZE - read_cnt) - 1);
				else read_cnt = I2C_EE_BLOCK_SIZE - 1;
				
				if(RUBICON_LogMemory.first_log_address < I2C_EE_PAGE_SIZE)
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, RUBICON_LogMemory.first_log_address, read_cnt);
				}
				else
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_1, RUBICON_LogMemory.first_log_address, read_cnt);
				}
				
				Hex2Str((char *)rubicon_ctrl_buffer, i2c_ee_buffer, (I2C_EE_BLOCK_SIZE * 2U));					
				HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_READY;				
			}
			break;
		
			
		case LOG_LIST_TYPE_4:
			
			if(HTTP_LogListTransfer.log_transfer_state == HTTP_GET_LOG_LIST)
			{
				I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, RUBICON_LogMemory.first_log_address, I2C_EE_BLOCK_SIZE - 1);
				Hex2Str((char *)rubicon_ctrl_buffer, i2c_ee_buffer, (I2C_EE_BLOCK_SIZE * 2U));		
				HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_READY;
				HTTP_LogListTransfer.log_transfer_end_address = I2C_EE_LOG_LIST_START_ADDRESS + I2C_EE_BLOCK_SIZE;
			}
			break;
		
		
		case LOG_LIST_TYPE_5:
			
			if(HTTP_LogListTransfer.log_transfer_state == HTTP_GET_LOG_LIST)
			{
				read_cnt = RUBICON_LogMemory.first_log_address;				
				while(read_cnt >= I2C_EE_BLOCK_SIZE) read_cnt -= I2C_EE_BLOCK_SIZE;				
				if(read_cnt != 0) read_cnt = ((I2C_EE_BLOCK_SIZE - read_cnt) - 1);
				else read_cnt = I2C_EE_BLOCK_SIZE - 1;
				
				if(RUBICON_LogMemory.first_log_address < I2C_EE_PAGE_SIZE)
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, RUBICON_LogMemory.first_log_address, read_cnt);
				}
				else
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_1, RUBICON_LogMemory.first_log_address, read_cnt);
				}
									
				Hex2Str((char *)rubicon_ctrl_buffer, i2c_ee_buffer, (I2C_EE_BLOCK_SIZE * 2U));					
				HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_READY;				
			}
			break;
		
			
		case LOG_LIST_TYPE_6:
			
			if(HTTP_LogListTransfer.log_transfer_state == HTTP_GET_LOG_LIST)
			{
				read_cnt = RUBICON_LogMemory.first_log_address;
				while(read_cnt >= I2C_EE_BLOCK_SIZE) read_cnt -= I2C_EE_BLOCK_SIZE;
				if(read_cnt != 0) read_cnt = ((I2C_EE_BLOCK_SIZE - read_cnt) - 1);
				else read_cnt = I2C_EE_BLOCK_SIZE - 1;
				
				if(RUBICON_LogMemory.first_log_address < I2C_EE_PAGE_SIZE)
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, RUBICON_LogMemory.first_log_address, read_cnt);
				}
				else
				{
					I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_1, RUBICON_LogMemory.first_log_address, read_cnt);
				}
				
				Hex2Str((char *)rubicon_ctrl_buffer, i2c_ee_buffer, (I2C_EE_BLOCK_SIZE * 2U));
				HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_READY;				
			}
			break;
		
			
		default:
			
			RUBICON_StartDisplayTimer();
			LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Shit just happen, puta madre");
			break;
		
	}// End of switch	
}







