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
#include "hotel_room_controller.h"
#include "rubicon_address_list.h"
#include "stm32f429i_lcd.h"
#include "common.h"
#include "uart.h"
#include <stdlib.h>
#include "ff.h"
#include "i2c_eeprom.h"
#include "buzzer.h"
#include "W25Q16.h"


/* Private typedef -----------------------------------------------------------*/
eRubiconStateTypeDef eRubiconTransferState = RUBICON_INIT;
eRubiconTimeUpdateTypeDef eRubiconTimeUpdate = RUBICON_TIME_UPDATE_INIT;
eRubiconUpdateTypeDef eRubiconUpdate = RUBICON_UPDATE_INIT;

typedef struct
{
    uint8_t update_state;
    uint8_t send_attempt;
    uint32_t packet_total;
    uint32_t packet_send;
    uint32_t last_packet_send;
    uint16_t file_data_read;

} RUBICON_UpdatePacketTypeDef;


/* Private define ------------------------------------------------------------*/
RUBICON_UpdatePacketTypeDef RUBICON_FileUpdatePacket;
RUBICON_UpdatePacketTypeDef RUBICON_FirmwareUpdatePacket;
RUBICON_LogListTransferTypeDef RUBICON_LogListTransfer;
RUBICON_LogListTransferTypeDef HTTP_LogListTransfer;
RUBICON_LogMemoryTypeDef RUBICON_LogMemory;


/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint16_t rs485_rubicon_address;
uint16_t rs485_interface_address;

volatile uint32_t rubicon_display_timer;
volatile uint32_t rubicon_timer;
volatile uint32_t rubicon_flags;
volatile uint32_t rubicon_rx_timer;
volatile uint32_t rubicon_fw_update_timer;
volatile uint32_t rubicon_tftp_file;
volatile uint32_t rubicon_response_timer;

uint16_t rs485_packet_checksum;
uint16_t rs485_packet_lenght;
uint16_t rubicon_address_list_cnt;
uint8_t rubicon_ctrl_buffer[RUBICON_BUFFER_SIZE];
uint8_t *p_rubicon_ctrl_buffer;
uint8_t file_size[4];
uint8_t rubicon_ctrl_request;
uint8_t rubicon_http_cmd_state;

FATFS RUBICON_fatfs;
DIR RUBICON_dir;
FIL RUBICON_file;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
static void delay(__IO uint32_t nCount)
{
    __IO uint32_t index = 0;

    for (index = 80000 * nCount; index != 0; index--)
    {
    }
}

void RUBICON_Init(void)
{
	uint32_t temp_log_list_scan;
	
	RS485ModeGpio_Init();
	RS485_MODE(RS485_RX);
	
	p_comm_buffer = rx_buffer;
	while (p_comm_buffer < rx_buffer + sizeof (rx_buffer)) *p_comm_buffer++ = NULL;
	
	p_comm_buffer = tx_buffer;
	while (p_comm_buffer < tx_buffer + sizeof (tx_buffer)) *p_comm_buffer++ = NULL;
	
	p_rubicon_ctrl_buffer = rubicon_ctrl_buffer;
	while (p_rubicon_ctrl_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_rubicon_ctrl_buffer++ = NULL;

	p_i2c_ee_buffer = i2c_ee_buffer;
	while (p_i2c_ee_buffer < i2c_ee_buffer + sizeof(i2c_ee_buffer)) *p_i2c_ee_buffer++ = NULL;
	
	rubicon_address_list_cnt = 0;
	rs485_interface_address = RS485_INTERFACE_DEFAULT_ADDRESS;
	
	eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
	eRubiconUpdate = RUBICON_NO_UPDATE;
	eRubiconTimeUpdate = RUBICON_TIME_UPDATE_BROADCAST;
	
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
    static uint8_t display_flag;
	static uint8_t old_rx_cnt;
	uint8_t j, disp_add[16];
    /**
     *	
     */
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
				eRubiconUpdate = RUBICON_UPDATE_FIRMWARE;
				RUBICON_PrepareFirmwareUpdatePacket();
				eRubiconTransferState = RUBICON_PACKET_SEND;
				
			}
			else if ((RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FAIL) || \
					   (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINSH))
			{	
				if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FAIL)
				{
					display_flag = 0;
					RUBICON_StartDisplayTimer();
					LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "Firmware update failed");
					RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
					eRubiconUpdate = RUBICON_NO_UPDATE;
				}
				else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINSH)
				{
					display_flag = 0;
					RUBICON_StartDisplayTimer();
					LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "Firmware update finished");
					RUBICON_PrepareCommandPacket(RUBICON_EXECUTE_APPLICATION, rubicon_ctrl_buffer);
					eRubiconTransferState = RUBICON_PACKET_SEND;
					eRubiconUpdate = RUBICON_UPDATE_FIRMWARE;
				}				
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
			else if ((RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_1) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_2) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_3) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_4) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_5) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_6) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_7) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_8) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_9) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_10) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_11) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_12) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_13) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_14) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_15) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_16) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_17) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_18) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_19))
			{
				RUBICON_PrepareFileUpdatePacket();
				RUBICON_FileUpdatePacket.last_packet_send = RUBICON_FileUpdatePacket.packet_send;
				eRubiconTransferState = RUBICON_PACKET_SEND;
				eRubiconUpdate = RUBICON_UPDATE_FILE;
			}
			else if (rubicon_ctrl_request != NULL)
			{
				if (rubicon_ctrl_request == RUBICON_DOWNLOAD_FIRMWARE)
				{
					/* check for new firmware file on uSD card*/
					if (RUBICON_CheckNewFirmwareFile() == FILE_OK)
					{
						RUBICON_PrepareFirmwareUpdatePacket();
						eRubiconTransferState = RUBICON_PACKET_SEND;
						eRubiconUpdate = RUBICON_UPDATE_FIRMWARE;
					}
				}
				else if ((rubicon_ctrl_request == RUBICON_GET_LOG_LIST) || (rubicon_ctrl_request == RUBICON_DELETE_LOG_LIST))
				{
					eRubiconUpdate = RUBICON_NO_UPDATE;
				}
				else if (rubicon_ctrl_request == RUBICON_DOWNLOAD_DIPLAY_IMAGE)
				{
					
					if (RUBICON_CheckNewImageFile() == FILE_OK)
					{
						RUBICON_PrepareFileUpdatePacket();
						eRubiconTransferState = RUBICON_PACKET_SEND;
						eRubiconUpdate = RUBICON_UPDATE_FILE;
					}
				}
				else
				{
					rs485_rubicon_address = atoi((char *) rubicon_ctrl_buffer);
					RUBICON_PrepareCommandPacket(rubicon_ctrl_request, rubicon_ctrl_buffer);
					eRubiconUpdate = RUBICON_HTTP_REQUEST;
					eRubiconTransferState = RUBICON_PACKET_SEND;
				}

				rubicon_ctrl_request = NULL;
			}
			else if (IsRUBICON_TimerExpired())
			{
				RUBICON_StartTimer(RUBICON_TIME_UPDATE_PERIOD);
				if (eRubiconTimeUpdate == RUBICON_TIME_UPDATE_P2P) rs485_rubicon_address = RUBICON_GetNextAddress();
				else if (eRubiconTimeUpdate == RUBICON_TIME_UPDATE_GROUP) rs485_rubicon_address = RUBICON_GetGroupAddress(0);
				else if (eRubiconTimeUpdate == RUBICON_TIME_UPDATE_BROADCAST) rs485_rubicon_address = RUBICON_GetBroadcastAddress();
				RUBICON_PrepareTimeUpdatePacket();
				eRubiconUpdate = RUBICON_UPDATE_TIME;
				eRubiconTransferState = RUBICON_PACKET_SEND;
			}
			else
			{
				rs485_rubicon_address = RUBICON_GetNextAddress();
				RUBICON_PrepareStatusUpdatePacket();
				eRubiconUpdate = RUBICON_UPDATE_STATUS;
				eRubiconTransferState = RUBICON_PACKET_SEND;
			}

			if (IsRUBICON_DisplayTimerExpired() && (display_flag == 0))
			{
				display_flag = 1;
				LCD_DisplayStringLine(LCD_LINE_6, (uint8_t*) "                          ");
				LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "                          ");
			}
			break;
		}
		
		case RUBICON_PACKET_SEND:
		{	
			if(!IsRUBICON_RxTimeoutTimerExpired()) break;
			
			rs485_rx_cnt = 0;
			while(rs485_rx_cnt < DATA_BUF_SIZE) rx_buffer[rs485_rx_cnt++] = NULL;
			rs485_rx_cnt = 0;
			old_rx_cnt = 0;
			RS485_Send_Data(tx_buffer, (tx_buffer[5] + 9));
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
							display_flag = 0;
							RUBICON_StartDisplayTimer();
							LCD_DisplayStringLine(LCD_LINE_6, (uint8_t*) "  P2P time updated");
							break;
						}
						
						case RUBICON_TIME_UPDATE_GROUP:
						{
							display_flag = 0;
							RUBICON_StartDisplayTimer();
							LCD_DisplayStringLine(LCD_LINE_6, (uint8_t*) "  Group time updated");						
							RUBICON_StartRxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
							eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;	
							break;
						}
						
						case RUBICON_TIME_UPDATE_BROADCAST:
						{
							display_flag = 0;
							RUBICON_StartDisplayTimer();
							LCD_DisplayStringLine(LCD_LINE_6, (uint8_t*) "  Broadcast time updated");
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
					
				case RUBICON_UPDATE_STATUS:
				{
					break;
				}
				
				case RUBICON_UPDATE_FIRMWARE:
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
					else if(RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINSH)
					{
						RUBICON_StartFwUpdateTimer(RUBICON_FW_EXE_BOOT_TIME);
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
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_1) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_2) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_3) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_4) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_5) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_6) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_7) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_8) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_9) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_10) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_11) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_12) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_13) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_14) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_15) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_16) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_17) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_18) || \
					(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_19))
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
				else if ((RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_1) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_2) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_3) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_4) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_5) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_6) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_7) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_8) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_9) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_10) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_11) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_12) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_13) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_14) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_15) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_16) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_17) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_18) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_19))
				{
					++RUBICON_FileUpdatePacket.send_attempt;
				}
				else if(rubicon_http_cmd_state == HTTP_GET_RUBICON_TEMPERATURE)
				{
					display_flag = 0;
					RUBICON_StartDisplayTimer();
					LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Temperature request fail");
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
				}
				else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN)
				{
					if(RUBICON_FirmwareUpdatePacket.packet_send == 0)
					{
						RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_BOOTLOADER;
					}
					++RUBICON_FirmwareUpdatePacket.send_attempt;
				}
				else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINSH)
				{
					RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
				}
				else if ((RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_QUERY_LIST) || \
						 (RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_DELETE_LOG))
				{
					++RUBICON_LogListTransfer.send_attempt;
				}
				else if ((RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_1) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_2) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_3) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_4) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_5) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_6) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_7) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_8) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_9) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_10) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_11) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_12) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_13) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_14) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_15) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_16) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_17) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_18) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_19))
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
			if(((rx_buffer[1] == (rs485_interface_address >> 8)) && (rx_buffer[2] == (rs485_interface_address & 0xff))) && \
				   ((rx_buffer[3] == (rs485_rubicon_address >> 8)) && (rx_buffer[4] == (rs485_rubicon_address & 0xff))) && \
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
					display_flag = 0;
					RUBICON_StartDisplayTimer();
					LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log list transfer failed");
					++RUBICON_LogListTransfer.send_attempt;
				}
				else if(rubicon_http_cmd_state == HTTP_GET_RUBICON_TEMPERATURE)
				{
					display_flag = 0;
					RUBICON_StartDisplayTimer();
					LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Temperature request failed");
					rubicon_http_cmd_state = NULL;
				}
				else if(rubicon_http_cmd_state == HTTP_GET_RUBICON_ROOM_STATUS)
				{
					display_flag = 0;
					RUBICON_StartDisplayTimer();
					LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Status request failed     ");
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
				case RUBICON_UPDATE_STATUS:
				{
					if ((rx_buffer[7] == '1') || (rx_buffer[8] == '1'))
					{
						RUBICON_LogListTransfer.log_transfer_state = LOG_TRANSFER_QUERY_LIST;
						RUBICON_LogListTransfer.send_attempt = 0;
					}
					else
					{
						display_flag = 0;
						RUBICON_StartDisplayTimer();

						j = 0;
						while (rs485_rubicon_address > 9999)
						{
							rs485_rubicon_address -= 10000;
							j++;
						}
						disp_add[0] = j + 48;
						j = 0;
						while (rs485_rubicon_address > 999)
						{
							rs485_rubicon_address -= 1000;
							j++;
						}
						disp_add[1] = j + 48;
						j = 0;
						while (rs485_rubicon_address > 99)
						{
							rs485_rubicon_address -= 100;
							j++;
						}
						disp_add[2] = j + 48;
						j = 0;
						while (rs485_rubicon_address > 9)
						{
							rs485_rubicon_address -= 10;
							j++;
						}
						disp_add[3] = j + 48;
						disp_add[4] = rs485_rubicon_address + 48;
						disp_add[5] = ' ';
						disp_add[6] = ' ';
						disp_add[7] = 's';
						disp_add[8] = 't';
						disp_add[9] = 'a';
						disp_add[10] = 't';
						disp_add[11] = 'u';
						disp_add[12] = 's';
						disp_add[13] = ' ';
						disp_add[14] = '0';
						disp_add[15] = NULL;
						LCD_DisplayStringLine(LCD_LINE_10, disp_add);
					}

					RUBICON_StopRxTimeoutTimer();
					break;
				}
				
				case RUBICON_UPDATE_FIRMWARE:
				{
					if (RUBICON_FirmwareUpdatePacket.packet_send == RUBICON_FirmwareUpdatePacket.packet_total)
					{
						RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_FINSH;
					}
					else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN)
					{
						if((RUBICON_FirmwareUpdatePacket.packet_send == 0) && (!IsRUBICON_FwUpdateTimerExpired()))
						{
							return;
						}
						
						++RUBICON_FirmwareUpdatePacket.packet_send;
						RUBICON_FirmwareUpdatePacket.last_packet_send = RUBICON_FirmwareUpdatePacket.packet_send;
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
						RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_FINSH;
						rubicon_ctrl_request = RUBICON_START_BOOTLOADER;
					}
					if ((RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_1) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_2) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_3) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_4) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_5) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_6) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_7) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_8) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_9) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_10) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_11) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_12) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_13) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_14) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_15) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_16) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_17) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_18) || \
						(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_19))
					{
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
						display_flag = 0;
						RUBICON_StartDisplayTimer();
						LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log list transfered");
					}
					else if ((rx_buffer[0] == 0x06) && (rx_buffer[5] == 0x12) && (rx_buffer[6] == RUBICON_GET_LOG_LIST))
					{
						RUBICON_WriteLogToList();
						
						/***********************************************************************************
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
						*
						***********************************************************************************/
						
						
						switch (rx_buffer[9])
						{
							
							case RUBICON_LOG_NO_EVENT:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Log event empty      ");
								break;
							
							case RUBICON_LOG_GUEST_CARD_VALID:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Guest card valid    ");
								break;
							
							case RUBICON_LOG_GUEST_CARD_INVALID:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Guest card invalid   ");
								break;
								
							case RUBICON_LOG_HANDMAID_CARD_VALID:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Handmaid card valid  ");
								break;
							
							case RUBICON_LOG_ENTRY_DOOR_CLOSED:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Entry door closed    ");
								break;
							
							case RUBICON_LOG_PRESET_CARD:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Preset card          ");
								break;
							
							case RUBICON_LOG_HANDMAID_SERVICE_END:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Handmaid service end ");
								break;
							
							case RUBICON_LOG_MANAGER_CARD:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Manager card used    ");
								break;
							
							case RUBICON_LOG_SERVICE_CARD:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Service card used    ");
								break;
							
							case RUBICON_LOG_ENTRY_DOOR_OPENED:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Entry door opened    ");
								break;
							
							case RUBICON_LOG_MINIBAR_USED:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Minibar used         ");
								break;
							
							case RUBICON_LOG_BALCON_DOOR_OPENED:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Balcon door opened   ");
								break;
							
							case RUBICON_LOG_BALCON_DOOR_CLOSED:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Balcon door closed   ");
								break;
							
							case RUBICON_LOG_CARD_STACKER_ON:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Card in stacker      ");
								break;							
							
							case RUBICON_LOG_CARD_STACKER_OFF:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Card out of stacker  ");
								break;
							
							case RUBICON_LOG_HANDMAID_SWITCH_ON:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Handmaid switch ON   ");
								break;
							
							case RUBICON_LOG_HANDMAID_SWITCH_OFF:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Handmaid switch OFF  ");
								break;
							
							case RUBICON_LOG_SOS_ALARM_TRIGGER:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "SOS alarm trigger    ");
								BUZZER_On();
								break;
									
							case RUBICON_LOG_SOS_ALARM_RESET:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "SOS alarm reset      ");
								break;
							
							case RUBICON_LOG_FIRE_ALARM_TRIGGER:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Fire alarm trigger   ");
								break;
							
							case RUBICON_LOG_FIRE_ALARM_RESET:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Fire alarm reset     ");
								break;
							
							case RUBICON_LOG_UNKNOWN_CARD:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Unknown card         ");
								break;
							
							case RUBICON_LOG_DO_NOT_DISTURB_SWITCH_ON:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Do Not Disturb ON    ");
								break;

							case RUBICON_LOG_DO_NOT_DISTURB_SWITCH_OFF:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Do Not Disturb OFF   ");
								break;
							
							case RUBICON_LOG_CARD_EXPIRED:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Card Time Expired    ");
								break;
							
							case RUBICON_LOG_WRONG_ROOM:
								LCD_DisplayStringLine(LCD_LINE_9, (uint8_t*) "Wrong Room   	 	   ");
								break;
						}
						display_flag = 0;
						RUBICON_StartDisplayTimer();
						LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log transfered            ");
					}
					else if ((rx_buffer[0] == 0x06) && (rx_buffer[5] == 0x01) && (rx_buffer[6] == RUBICON_DELETE_LOG_LIST))
					{
						display_flag = 0;
						RUBICON_StartDisplayTimer();
						LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log deleted               ");
					}
					else
					{
						display_flag = 0;
						RUBICON_StartDisplayTimer();
						LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log list transfer failed  ");
						++RUBICON_LogListTransfer.send_attempt;
					}
					break;
				}
				
				case RUBICON_HTTP_REQUEST:
				{
					if(rubicon_http_cmd_state == HTTP_GET_RUBICON_TEMPERATURE)
					{
						rubicon_http_cmd_state = HTTP_RUBICON_TEMPERATURE_READY;
					}
					else if(rubicon_http_cmd_state == HTTP_GET_RUBICON_ROOM_STATUS)
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

uint16_t RUBICON_GetNextAddress(void)
{
    uint16_t current_address;


    current_address = rubicon_address_list[rubicon_address_list_cnt];

    ++rubicon_address_list_cnt;

    if (rubicon_address_list[rubicon_address_list_cnt] == 0x0000)
    {
        rubicon_address_list_cnt = 0;
    }

    return current_address;
}

uint16_t RUBICON_GetGroupAddress(uint16_t group)
{
    return 0x6776;
}

uint16_t RUBICON_GetBroadcastAddress(void)
{
    return 0x9999;
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

void RUBICON_PrepareStatusUpdatePacket(void)
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

uint8_t RUBICON_CheckNewFirmwareFile(void)
{
	
	W25Qxx_Read((W25QXX_FW_END_ADDRESS - 3), file_size, 4);
	
	if((file_size[0] == 0) && (file_size[1] == 0) && (file_size[2] == 0) && (file_size[3] == 0)) return FILE_SYS_ERROR;
	else if((file_size[0] == 0xff) && (file_size[1] == 0xff) && (file_size[2] == 0xff) && (file_size[3] == 0xff)) return FILE_SYS_ERROR;
	
//    if (f_mount(0, &RUBICON_fatfs) != FR_OK)
//    {
//        return FILE_SYS_ERROR;
//    }

//    if (f_opendir(&RUBICON_dir, "FW") != FR_OK)
//    {
//        return FILE_DIR_ERROR;
//    }

//    printf("directory opened\r\n");

//    if (f_open(&RUBICON_file, "FW/NEW.BIN", FA_READ) != FR_OK)
//    {
//        return FILE_ERROR;
//    }

    rs485_rubicon_address = atoi((char *) rubicon_ctrl_buffer);
    RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_INIT;
    RUBICON_FirmwareUpdatePacket.send_attempt = 0;
    RUBICON_FirmwareUpdatePacket.packet_send = 0;
    RUBICON_FirmwareUpdatePacket.last_packet_send = 0;
    RUBICON_FirmwareUpdatePacket.packet_total = ((file_size[0] << 24) + (file_size[1] << 16) + (file_size[2] << 8) + file_size[3]) / RUBICON_PACKET_BUFFER_SIZE;

    if ((RUBICON_FirmwareUpdatePacket.packet_total * RUBICON_PACKET_BUFFER_SIZE) < ((file_size[0] << 24) + (file_size[1] << 16) + (file_size[2] << 8) + file_size[3]))
    {
        ++RUBICON_FirmwareUpdatePacket.packet_total;
    }

    return FILE_OK;
	
}

uint8_t RUBICON_CheckNewImageFile(void)
{
	uint32_t k;
	
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
			W25Qxx_Read((W25QXX_IMAGE_1_END_ADDRESS - 3), file_size, 4);
			RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_1;
		}
		else if(rubicon_ctrl_buffer[k] == '0')
		{
			W25Qxx_Read((W25QXX_IMAGE_10_END_ADDRESS - 3), file_size, 4);
			RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_10;
		}
		else if(rubicon_ctrl_buffer[k] == '1')
		{
			W25Qxx_Read((W25QXX_IMAGE_11_END_ADDRESS - 3), file_size, 4);
			RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_11;
		}
		else if(rubicon_ctrl_buffer[k] == '2')
		{
			W25Qxx_Read((W25QXX_IMAGE_12_END_ADDRESS - 3), file_size, 4);
			RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_12;
		}
		else if(rubicon_ctrl_buffer[k] == '3')
		{
			W25Qxx_Read((W25QXX_IMAGE_13_END_ADDRESS - 3), file_size, 4);
			RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_13;
		}
		else if(rubicon_ctrl_buffer[k] == '4')
		{
			W25Qxx_Read((W25QXX_IMAGE_14_END_ADDRESS - 3), file_size, 4);
			RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_14;
		}
		else if(rubicon_ctrl_buffer[k] == '5')
		{
			W25Qxx_Read((W25QXX_IMAGE_15_END_ADDRESS - 3), file_size, 4);
			RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_15;
		}
		else if(rubicon_ctrl_buffer[k] == '6')
		{
			W25Qxx_Read((W25QXX_IMAGE_16_END_ADDRESS - 3), file_size, 4);
			RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_16;
		}
		else if(rubicon_ctrl_buffer[k] == '7')
		{
			W25Qxx_Read((W25QXX_IMAGE_17_END_ADDRESS - 3), file_size, 4);
			RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_17;
		}
		else if(rubicon_ctrl_buffer[k] == '8')
		{
			W25Qxx_Read((W25QXX_IMAGE_18_END_ADDRESS - 3), file_size, 4);
			RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_18;
		}
		else if(rubicon_ctrl_buffer[k] == '9')
		{
			W25Qxx_Read((W25QXX_IMAGE_19_END_ADDRESS - 3), file_size, 4);
			RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_19;
		}
	}
	else if(rubicon_ctrl_buffer[k] == '2') 
	{
		W25Qxx_Read((W25QXX_IMAGE_2_END_ADDRESS - 3), file_size, 4);
		RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_2;
	}
	else if(rubicon_ctrl_buffer[k] == '3') 
	{
		W25Qxx_Read((W25QXX_IMAGE_3_END_ADDRESS - 3), file_size, 4);
		RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_3;
	}
	else if(rubicon_ctrl_buffer[k] == '4') 
	{
		W25Qxx_Read((W25QXX_IMAGE_4_END_ADDRESS - 3), file_size, 4);
		RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_4;
	}
	else if(rubicon_ctrl_buffer[k] == '5') 
	{
		W25Qxx_Read((W25QXX_IMAGE_5_END_ADDRESS - 3), file_size, 4);
		RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_5;
	}
	else if(rubicon_ctrl_buffer[k] == '6') 
	{
		W25Qxx_Read((W25QXX_IMAGE_6_END_ADDRESS - 3), file_size, 4);
		RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_6;
	}
	else if(rubicon_ctrl_buffer[k] == '7') 
	{
		W25Qxx_Read((W25QXX_IMAGE_7_END_ADDRESS - 3), file_size, 4);
		RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_7;
	}
	else if(rubicon_ctrl_buffer[k] == '8') 
	{
		W25Qxx_Read((W25QXX_IMAGE_8_END_ADDRESS - 3), file_size, 4);
		RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_8;
	}
	else if(rubicon_ctrl_buffer[k] == '9') 
	{
		W25Qxx_Read((W25QXX_IMAGE_9_END_ADDRESS - 3), file_size, 4);
		RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_9;
	}

	else return FILE_SYS_ERROR;
	
	if((file_size[0] == 0) && (file_size[1] == 0) && (file_size[2] == 0) && (file_size[3] == 0)) return FILE_SYS_ERROR;
	else if((file_size[0] == 0xff) && (file_size[1] == 0xff) && (file_size[2] == 0xff) && (file_size[3] == 0xff)) return FILE_SYS_ERROR;
	
	rs485_rubicon_address = atoi((char *) rubicon_ctrl_buffer);
    RUBICON_FileUpdatePacket.send_attempt = 0;
    RUBICON_FileUpdatePacket.packet_send = 0;
    RUBICON_FileUpdatePacket.last_packet_send = 0;
    RUBICON_FileUpdatePacket.packet_total = ((file_size[0] << 24) + (file_size[1] << 16) + (file_size[2] << 8) + file_size[3]) / RUBICON_PACKET_BUFFER_SIZE;

    if ((RUBICON_FileUpdatePacket.packet_total * RUBICON_PACKET_BUFFER_SIZE) < ((file_size[0] << 24) + (file_size[1] << 16) + (file_size[2] << 8) + file_size[3]))
    {
        ++RUBICON_FileUpdatePacket.packet_total;
    }

    return FILE_OK;
}

void RUBICON_PrepareFirmwareUpdatePacket(void)
{
    static uint32_t fwup_address = 0;
	uint32_t i;

    //FW_UPDATE_IDLE			16
    //FW_UPDATE_INIT 			17
    //FW_UPDATE_BOOTLOADER 		18
    //FW_UPDATE_RUN				19
    //FW_UPDATE_FINSH			20
    //FW_UPDATE_FAIL			21

    if (RUBICON_FirmwareUpdatePacket.send_attempt >= MAX_QUERY_ATTEMPTS)
    {
        RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_FAIL;
    }

    if ((RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FAIL) || \
	   (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINSH))
    {
        RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
        //f_close(&RUBICON_file);
        //f_mount(0, NULL);
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
		fwup_address = W25QXX_FW_START_ADDRESS;
    }
    else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN)
    {
        tx_buffer[0] = RUBICON_STX;
        tx_buffer[5] = 0x42;
        tx_buffer[6] = RUBICON_FirmwareUpdatePacket.packet_send >> 8;
        tx_buffer[7] = RUBICON_FirmwareUpdatePacket.packet_send & 0xff;

        //f_read(&RUBICON_file, (uint8_t*) &tx_buffer[8], RUBICON_PACKET_BUFFER_SIZE, (UINT*) (&RUBICON_FirmwareUpdatePacket.file_data_read));
		
		W25Qxx_Read(fwup_address, &tx_buffer[8], RUBICON_PACKET_BUFFER_SIZE);
		fwup_address += RUBICON_PACKET_BUFFER_SIZE;	
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
}

void RUBICON_PrepareFileUpdatePacket(void)
{
	static uint32_t pfup_address = 0;
	uint32_t i;

	//	FILE_UPDATE_IDLE			40
	//	FILE_UPDATE_INIT 			41
	//	FILE_UPDATE_RUN				42
	//	FILE_UPDATE_FINSH			43
	//	FILE_UPDATE_FAIL			44

    if (RUBICON_FileUpdatePacket.send_attempt >= MAX_QUERY_ATTEMPTS)
    {
        RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_FAIL;
    }

    if ((RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_FAIL) || \
	   (RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_FINSH))
    {
        RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IDLE;
        //f_close(&RUBICON_file);
        //f_mount(0, NULL);
        return;
    }
	else if (RUBICON_FileUpdatePacket.packet_send == 0)
    {
        tx_buffer[0] = RUBICON_SOH;
        tx_buffer[5] = 0x03;
		
        if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_1) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_1;
			pfup_address = W25QXX_IMAGE_1_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_2) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_2;
			pfup_address = W25QXX_IMAGE_2_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_3) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_3;
			pfup_address = W25QXX_IMAGE_3_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_4) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_4;
			pfup_address = W25QXX_IMAGE_4_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_5) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_5;
			pfup_address = W25QXX_IMAGE_5_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_6) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_6;
			pfup_address = W25QXX_IMAGE_6_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_7) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_7;
			pfup_address = W25QXX_IMAGE_7_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_8) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_8;
			pfup_address = W25QXX_IMAGE_8_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_9) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_9;
			pfup_address = W25QXX_IMAGE_9_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_10) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_10;
			pfup_address = W25QXX_IMAGE_10_START_ADDRESS;
		}
		if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_11) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_11;
			pfup_address = W25QXX_IMAGE_11_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_12) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_12;
			pfup_address = W25QXX_IMAGE_12_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_13) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_13;
			pfup_address = W25QXX_IMAGE_13_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_14) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_14;
			pfup_address = W25QXX_IMAGE_14_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_15) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_15;
			pfup_address = W25QXX_IMAGE_15_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_16) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_16;
			pfup_address = W25QXX_IMAGE_16_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_17) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_17;
			pfup_address = W25QXX_IMAGE_17_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_18) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_18;
			pfup_address = W25QXX_IMAGE_18_START_ADDRESS;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_19) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_19;
			pfup_address = W25QXX_IMAGE_19_START_ADDRESS;
		}
		
        tx_buffer[7] = RUBICON_FileUpdatePacket.packet_total >> 8;
        tx_buffer[8] = RUBICON_FileUpdatePacket.packet_total & 0xff;
		
		
    }
    else
    {
        tx_buffer[0] = RUBICON_STX;
        tx_buffer[5] = 0x42;
        tx_buffer[6] = RUBICON_FileUpdatePacket.packet_send >> 8;
        tx_buffer[7] = RUBICON_FileUpdatePacket.packet_send & 0xff;

        //f_read(&RUBICON_file, (uint8_t*) &tx_buffer[8], RUBICON_PACKET_BUFFER_SIZE, (UINT*) (&RUBICON_FirmwareUpdatePacket.file_data_read));
		
		W25Qxx_Read(pfup_address, &tx_buffer[8], RUBICON_PACKET_BUFFER_SIZE);
		
		if(RUBICON_FileUpdatePacket.last_packet_send < RUBICON_FileUpdatePacket.packet_send)
		{
			pfup_address += RUBICON_PACKET_BUFFER_SIZE;
		}
			
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
}

void RUBICON_PrepareCommandPacket(uint8_t command, uint8_t *ibuff)
{
    uint32_t i;
	unsigned long card_id;
	uint8_t hex[6];

    tx_buffer[0] = RUBICON_SOH;
    tx_buffer[1] = rs485_rubicon_address >> 8;
    tx_buffer[2] = rs485_rubicon_address & 0x00ff;
    tx_buffer[3] = rs485_interface_address >> 8;
    tx_buffer[4] = rs485_interface_address & 0x00ff;
    tx_buffer[5] = 0x01;
    tx_buffer[6] = command;
    /** 
     *	command list without formating 
     */
    // RUBICON_FLASH_PROTECTION_ENABLE)
    // RUBICON_FLASH_PROTECTION_DISABLE)
    // RUBICON_START_BOOTLOADER)
    // RUBICON_EXECUTE_APPLICATION)
    // RUBICON_GET_SYS_STATUS)
    // RUBICON_GET_SYS_INFO)
    // RUBICON_GET_DISPLAY_BRIGHTNESS)
    // RUBICON_GET_RTC_DATE_TIME
    // RUBICON_GET_LOG_LIST
    // RUBICON_DELETE_LOG_LIST
    // RUBICON_GET_RS485_CONFIG
    // RUBICON_GET_DIN_STATE
    // RUBICON_GET_DOUT_STATE
    // RUBICON_GET_PCB_TEMPERATURE
    // RUBICON_GET_TEMP_CARD_BUFFER	
    // RUBICON_GET_MIFARE_AUTHENTICATION_KEY_A
    // RUBICON_GET_MIFARE_AUTHENTICATION_KEY_B
    // RUBICON_GET_MIFARE_PERMITED_GROUP
    // RUBICON_GET_MIFARE_PERMITED_CARD_1
    // RUBICON_GET_MIFARE_PERMITED_CARD_2
    // RUBICON_GET_MIFARE_PERMITED_CARD_3
    // RUBICON_GET_MIFARE_PERMITED_CARD_4
    // RUBICON_GET_MIFARE_PERMITED_CARD_5
    // RUBICON_GET_MIFARE_PERMITED_CARD_6
    // RUBICON_GET_MIFARE_PERMITED_CARD_7
    // RUBICON_GET_MIFARE_PERMITED_CARD_8
    // RUBICON_GET_ROOM_STATUS
	// RUBICON_GET_ROOM_TEMPERATURE
	// RUBICON_SET_ROOM_TEMPERATURE
	
    if (command == RUBICON_SET_DISPLAY_BRIGHTNESS)
    {		
        tx_buffer[5] = 0x03;
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
        tx_buffer[7] = i >> 8;		// display brightness MSB
        tx_buffer[8] = i & 0xff;	// display brightness LSB
    }
    else if (command == RUBICON_SET_RS485_CONFIG)
    {
        tx_buffer[5] = 13;
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		i = atoi((char *) ibuff);
		hex[0] = i >> 8;
		hex[1] = i & 0xff;
		hex[2] = RUBICON_DEFFAULT_GROUP_ADDRESS >> 8;
		hex[3] = RUBICON_DEFFAULT_GROUP_ADDRESS & 0xff;
		hex[4] = RUBICON_DEFFAULT_BROADCAST_ADDRESS >> 8;
		hex[5] = RUBICON_DEFFAULT_BROADCAST_ADDRESS & 0xff;
		Hex2Str(hex, 6, &tx_buffer[7]);		
    }
    else if (command == RUBICON_SET_DOUT_STATE)
    {
        tx_buffer[5] = 17;
		
		while(*ibuff != NULL) ++ibuff;
		
		++ibuff;
        i = 7;
        while (i < 23) tx_buffer[i++] = *ibuff++;
    }
    else if ((command == RUBICON_SET_MIFARE_AUTHENTICATION_KEY_A) || \
			 (command == RUBICON_SET_MIFARE_AUTHENTICATION_KEY_B))
    {
        tx_buffer[5] = 13;
    }
    else if (command == RUBICON_SET_MIFARE_PERMITED_GROUP)
    {
        tx_buffer[5] = 17;
    }
    else if (command == RUBICON_SET_MIFARE_PERMITED_CARD)
    {
        tx_buffer[5] = 21;
		
		while(*ibuff != NULL) ++ibuff;
		
		++ibuff;
		
		if(*ibuff == '1') tx_buffer[6] = RUBICON_SET_MIFARE_PERMITED_CARD_1;
		else if(*ibuff == '2') tx_buffer[6] = RUBICON_SET_MIFARE_PERMITED_CARD_2;
		else if(*ibuff == '3') tx_buffer[6] = RUBICON_SET_MIFARE_PERMITED_CARD_3;
		else if(*ibuff == '4') tx_buffer[6] = RUBICON_SET_MIFARE_PERMITED_CARD_4;
		else if(*ibuff == '5') tx_buffer[6] = RUBICON_SET_MIFARE_PERMITED_CARD_5;
		else if(*ibuff == '6') tx_buffer[6] = RUBICON_SET_MIFARE_PERMITED_CARD_6;
		else if(*ibuff == '7') tx_buffer[6] = RUBICON_SET_MIFARE_PERMITED_CARD_7;
		else if(*ibuff == '8') tx_buffer[6] = RUBICON_SET_MIFARE_PERMITED_CARD_8;
			
		while(*ibuff != NULL) ++ibuff;
		++ibuff;
		while(*ibuff == '0') ++ibuff;
		card_id = strtoul((char *) ibuff, NULL, 0);
		hex[0] = card_id  & 0xff;
		hex[1] = (card_id >> 8) & 0xff;
		hex[2] = (card_id >> 16) & 0xff;
		hex[3] = (card_id >> 24) & 0xff;				
		Hex2Str(hex, 4, &tx_buffer[7]);
		tx_buffer[15] = NULL;
		tx_buffer[16] = NULL;			
		
		while(*ibuff != NULL) ++ibuff;
		
		++ibuff;		
        i = 17;
        while (i < 27) tx_buffer[i++] = *ibuff++;
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
	
    rs485_packet_checksum = 0;

    for (i = 6; i < (tx_buffer[5] + 6); i++)
    {
        rs485_packet_checksum += tx_buffer[i];
    }

    tx_buffer[tx_buffer[5] + 6] = rs485_packet_checksum >> 8;
    tx_buffer[tx_buffer[5] + 7] = rs485_packet_checksum;
    tx_buffer[tx_buffer[5] + 8] = RUBICON_EOT;
}

void RUBICON_PrepareLogUpdatePacket(void)
{
    if (RUBICON_LogListTransfer.send_attempt >= MAX_QUERY_ATTEMPTS)
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

void RUBICON_WriteLogToList(void)
{
	uint8_t e;
	
	e = 0;
		
	while(e < RUBICON_LOG_SIZE) 
	{
		i2c_ee_buffer[e] = rx_buffer[7 + e];
		++e;
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
			RUBICON_StartDisplayTimer();
			LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log list undefined state");
			break;
		
		
		case LOG_LIST_TYPE_1:
			
			I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, RUBICON_LogMemory.next_log_address, RUBICON_LOG_SIZE);
			delay(I2C_EE_WRITE_DELAY);	
			++RUBICON_LogMemory.log_list_cnt;
			RUBICON_LogMemory.LOG_MemoryFragment = LOG_LIST_TYPE_5;
			RUBICON_LogMemory.next_log_address += RUBICON_LOG_SIZE;
			RUBICON_StartDisplayTimer();
			LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log switch to type 5");
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
				RUBICON_StartDisplayTimer();
				LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log list full !!!");
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
				RUBICON_StartDisplayTimer();
				LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log list full !!!");
			}
			break;
		
			
		case LOG_LIST_TYPE_4:
			
			RUBICON_StartDisplayTimer();
			LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log list full !!!");
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
				RUBICON_StartDisplayTimer();
				LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log list full !!!");
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
				RUBICON_StartDisplayTimer();
				LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Log list full !!!");
			}
			break;
		
			
		default:
			
			RUBICON_StartDisplayTimer();
			LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Shit just happen, puta madre");
			break;
		
	}// End of switch
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
				
				Hex2Str(i2c_ee_buffer, I2C_EE_BLOCK_SIZE, rubicon_ctrl_buffer);				
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
				
				Hex2Str(i2c_ee_buffer, I2C_EE_BLOCK_SIZE, rubicon_ctrl_buffer);				
				HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_READY;				
			}
			break;
		
			
		case LOG_LIST_TYPE_4:
			
			if(HTTP_LogListTransfer.log_transfer_state == HTTP_GET_LOG_LIST)
			{
				I2C_EERPOM_ReadBytes16(I2C_EE_READ_PAGE_0, RUBICON_LogMemory.first_log_address, I2C_EE_BLOCK_SIZE - 1);
			
				Hex2Str(i2c_ee_buffer, I2C_EE_BLOCK_SIZE, rubicon_ctrl_buffer);
				
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
									
				Hex2Str(i2c_ee_buffer, I2C_EE_BLOCK_SIZE, rubicon_ctrl_buffer);				
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
				
				Hex2Str(i2c_ee_buffer, I2C_EE_BLOCK_SIZE, rubicon_ctrl_buffer);				
				HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_LIST_READY;				
			}
			break;
		
			
		default:
			
			RUBICON_StartDisplayTimer();
			LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "  Shit just happen, puta madre");
			break;
		
	}// End of switch	
}







