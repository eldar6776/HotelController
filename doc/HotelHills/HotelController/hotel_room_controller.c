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
#include <string.h>
#include "ff.h"
#include "i2c_eeprom.h"
#include "buzzer.h"
#include "W25Q16.h"
#include "Display.h"

/* Private typedef -----------------------------------------------------------*/

struct s_rs485_pcb
{
	uint8_t cmd;				/* requested command  */
	const char *cmd_args_p;		/* pointer to buffer with command arguments  */
	uint8_t cmd_attempt;		/* number of attempts to execute command before returning failure code */
	uint8_t cmd_state;			/* current state of command execution */
	uint8_t cmd_rtn;			/* command execution return flag */
	uint16_t cmd_cycle;			/* command cycle counter for repetitive command execution */
	uint32_t tmr_preset;		/* value for command timeout timer */
	uint8_t pck_mode;			/* scope of transmitted packet: P2P, group or broadcast  */
	uint32_t pck_total;			/* total number of packet to send */
    uint32_t pck_sent;			/* number of sent packets witch are confirmed as valid from receiver */
    uint8_t *in_buff_p;			/* pointer to buffer with received room controller response  */
	uint8_t flag;				/* packet control block flags */
	uint8_t *out_buff_p;		/* pointer to buffer for room controller request forming */
	uint32_t byte_cnt;			/* request buffer byte counter*/
	uint16_t rec_addr;			/* recipient room controller address */
	uint16_t *addr_index;		/* pointer to room controller address list */
};


eRubiconStateTypeDef eRubiconTransferState = RUBICON_INIT;
eRubiconTimeUpdateTypeDef eRubiconTimeUpdate = RUBICON_TIME_UPDATE_INIT;
eRubiconUpdateTypeDef eRubiconUpdate = RUBICON_UPDATE_INIT;

uint16_t rs485_rubicon_address;
uint16_t rs485_interface_address;
uint16_t rs485_broadcast_address;
uint16_t rs485_group_address;


/* Private define ------------------------------------------------------------*/
RUBICON_UpdatePacketTypeDef RUBICON_FileUpdatePacket;
RUBICON_UpdatePacketTypeDef RUBICON_FirmwareUpdatePacket;
RUBICON_LogListTransferTypeDef RUBICON_LogListTransfer;
RUBICON_LogListTransferTypeDef HTTP_LogListTransfer;
RUBICON_LogMemoryTypeDef RUBICON_LogMemory;

t_rs485_pcb *rs485_pcb_p;
t_rs485_pcb rs485_pcb;

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
volatile uint32_t rubicon_display_timer;
volatile uint32_t rubicon_timer;
volatile uint32_t rubicon_flags;
volatile uint32_t rubicon_rx_tx_timer;
volatile uint32_t rubicon_fw_update_timer;
volatile uint32_t rubicon_tftp_file;
volatile uint32_t rubicon_response_timer;

uint16_t rs485_packet_checksum;
uint16_t rs485_packet_lenght;
uint16_t rubicon_address_list_cnt;
uint8_t rubicon_ctrl_buffer[RUBICON_BUFFER_SIZE];
uint8_t *p_rubicon_buffer;
uint8_t config_file_buffer[RUBICON_CONFIG_FILE_BUFFER_SIZE];
uint8_t config_file_image_cnt;
uint32_t config_file_byte_cnt;
uint8_t rubicon_cmd_request;
uint8_t rubicon_http_cmd_state;

extern FATFS filesystem;
extern FIL file_SD, file_CR;
extern DIR dir_1, dir_2;

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
	
	rs485_pcb_p = &rs485_pcb;
	
	rs485_interface_address = RS485_INTERFACE_DEFAULT_ADDRESS;
	rs485_broadcast_address = RUBICON_DEFFAULT_BROADCAST_ADDRESS;
	rs485_group_address = RUBICON_DEFFAULT_GROUP_ADDRESS;
	
	rs485_pcb_p->cmd = CMD_IDLE;
	rs485_pcb_p->cmd_rtn = CMD_IDLE;
	rs485_pcb_p->cmd_state = CMD_IDLE;
	rs485_pcb_p->pck_mode = CMD_P2P;
	
	RS485ModeGpio_Init();
	RS485_MODE(RS485_RX);
	
	p_comm_buffer = rx_buffer;
	while (p_comm_buffer < rx_buffer + sizeof (rx_buffer)) *p_comm_buffer++ = NULL;
	
	p_comm_buffer = tx_buffer;
	while (p_comm_buffer < tx_buffer + sizeof (tx_buffer)) *p_comm_buffer++ = NULL;
	
	p_rubicon_buffer = rubicon_ctrl_buffer;
	while (p_rubicon_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_rubicon_buffer++ = NULL;

	p_i2c_ee_buffer = i2c_ee_buffer;
	while (p_i2c_ee_buffer < i2c_ee_buffer + sizeof(i2c_ee_buffer)) *p_i2c_ee_buffer++ = NULL;
	
	rubicon_address_list_cnt = 0;
	rs485_interface_address = RS485_INTERFACE_DEFAULT_ADDRESS;
	
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
/**void RUBICON_ProcessService(void)
*
*
void RUBICON_ProcessService(void)
{
    static uint8_t display_flag = 0;
	static uint8_t i_cnt = 0;
	static uint8_t old_rx_cnt;
	uint8_t j, t, fw_status, fl_status;
    
    switch (eRubiconTransferState)
    {
		case RUBICON_INIT:
		{
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
					   (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINISHED))
			{	
				if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FAIL)
				{
					display_flag = 0;
					RUBICON_StartDisplayTimer();
					LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "Firmware update failed");
					RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
					eRubiconUpdate = RUBICON_NO_UPDATE;
				}
				else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINISHED)
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
			else if ((RUBICON_FileUpdatePacket.update_state >= FILE_UPDATE_IMAGE_1) && \
					(RUBICON_FileUpdatePacket.update_state <= FILE_UPDATE_IMAGE_19))
			{
				RUBICON_PrepareFileUpdatePacket();
				RUBICON_FileUpdatePacket.last_packet_send = RUBICON_FileUpdatePacket.packet_send;
				eRubiconTransferState = RUBICON_PACKET_SEND;
				eRubiconUpdate = RUBICON_UPDATE_FILE;
			}
			else if (rubicon_cmd_request != NULL)
			{
				if (rubicon_cmd_request == RUBICON_DOWNLOAD_FIRMWARE)
				{
					if (RUBICON_CheckNewFirmwareFile() == FILE_OK)
					{
						RUBICON_PrepareFirmwareUpdatePacket();
						eRubiconTransferState = RUBICON_PACKET_SEND;
						eRubiconUpdate = RUBICON_UPDATE_FIRMWARE;
					}
				}
				else if ((rubicon_cmd_request == RUBICON_GET_LOG_LIST) || (rubicon_cmd_request == RUBICON_DELETE_LOG_LIST))
				{
					eRubiconUpdate = RUBICON_NO_UPDATE;
				}
				else if ((rubicon_cmd_request == RUBICON_UPLOAD_ALL_IMAGE_TO_SELECTED) || \
						 (rubicon_cmd_request == RUBICON_UPLOAD_ALL_IMAGE_TO_ALL_IN_RANGE))
				{
					if (RUBICON_CheckNewImageFile() == FILE_OK)
					{
						RUBICON_PrepareFileUpdatePacket();
						eRubiconTransferState = RUBICON_PACKET_SEND;
						eRubiconUpdate = RUBICON_UPDATE_FILE;
					}
				}
				else if (rubicon_cmd_request == RUBICON_UPLOAD_USING_CONFIG_FILE)
				{					
					if ((RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IDLE) && \
						(RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_IDLE))
					{
						if(RUBICON_CheckConfigFile() != FILE_OK) 
						{
							rubicon_cmd_request = NULL;
							break;
						}
						
						if(RUBICON_CreateUpdateAddresseList() != FILE_OK) 
						{
							rubicon_cmd_request = NULL;
							break;
						}
						
						rubicon_address_list_cnt = 0;
						fw_status = RUBICON_CheckNewFirmwareFile();
						
						if (fw_status == FILE_OK)
						{
							RUBICON_PrepareFirmwareUpdatePacket();
							eRubiconTransferState = RUBICON_PACKET_SEND;
							eRubiconUpdate = RUBICON_UPDATE_FIRMWARE;
						}
						else if((fw_status == FILE_ERROR) || (fw_status == FILE_DIR_ERROR) || \
							(fw_status == FILE_SYS_ERROR) || (fw_status == FW_UPDATE_FINISHED))
						{
							RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_FROM_CONFIG_FILE;
							RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
							eRubiconUpdate = RUBICON_NO_UPDATE;
							rubicon_address_list_cnt = 0;
							config_file_image_cnt = 1;
						}
					}
					else if ((RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FROM_CONFIG_FILE) && \
							(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IDLE))
					{
						++rubicon_address_list_cnt;
						fw_status = RUBICON_CheckNewFirmwareFile();
						
						if (fw_status == FILE_OK)
						{
							RUBICON_PrepareFirmwareUpdatePacket();
							eRubiconTransferState = RUBICON_PACKET_SEND;
							eRubiconUpdate = RUBICON_UPDATE_FIRMWARE;
						}
						else if((fw_status == FILE_ERROR) || (fw_status == FILE_DIR_ERROR) || \
							(fw_status == FILE_SYS_ERROR) || (fw_status == FW_UPDATE_FINISHED))
						{
							RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
							RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_FROM_CONFIG_FILE;
							eRubiconUpdate = RUBICON_NO_UPDATE;
							rubicon_address_list_cnt = 0;
							config_file_image_cnt = 1;
						}
					}
					else if ((RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_FROM_CONFIG_FILE) && \
							(RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_IDLE))
					{
						fl_status = RUBICON_CheckNewImageFile();
						
						if (fl_status == FILE_OK)
						{
							RUBICON_PrepareFileUpdatePacket();
							eRubiconTransferState = RUBICON_PACKET_SEND;
							eRubiconUpdate = RUBICON_UPDATE_FILE;
						}
						
						if(++i_cnt >= 2) 
						{
							i_cnt = 0;
							
							if(++config_file_image_cnt >= 14) 
							{
								config_file_image_cnt = 1;
								++rubicon_address_list_cnt;
							}
						}
						
						if ((rubicon_image_update_address_list[rubicon_address_list_cnt] == 0x0000) || \
							(fl_status == FILE_ERROR) || (fl_status == FILE_DIR_ERROR) || \
							(fl_status == FILE_SYS_ERROR) || (fl_status == FILE_UPDATE_FINISHED))
						{
							RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IDLE;
							RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
							eRubiconUpdate = RUBICON_NO_UPDATE;
							rubicon_cmd_request = NULL;
							rubicon_address_list_cnt = 0;
							config_file_image_cnt = 0;
						}
					}
				}
				else
				{
					
					rs485_rubicon_address = atoi((char *) rubicon_ctrl_buffer);
					RUBICON_PrepareCommandPacket(rubicon_cmd_request, rubicon_ctrl_buffer);
					eRubiconUpdate = RUBICON_HTTP_REQUEST;
					eRubiconTransferState = RUBICON_PACKET_SEND;
				}
				
				if((rubicon_cmd_request != RUBICON_UPLOAD_ALL_IMAGE_TO_ALL_IN_RANGE) && \
					(rubicon_cmd_request != RUBICON_UPLOAD_USING_CONFIG_FILE))
				{
					rubicon_cmd_request = NULL;
				}
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
			if(!IsRUBICON_RxTxTimeoutTimerExpired()) break;
			
			rs485_rx_cnt = 0;
			while(rs485_rx_cnt < DATA_BUF_SIZE) rx_buffer[rs485_rx_cnt++] = NULL;
			rs485_rx_cnt = 0;
			old_rx_cnt = 0;
			RS485_Send_Data(tx_buffer, (tx_buffer[5] + 9));
			RUBICON_StartRxTxTimeoutTimer(RUBICON_RESPONSE_TIMEOUT);
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
							RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
							eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;	
							break;
						}
						
						case RUBICON_TIME_UPDATE_BROADCAST:
						{
							display_flag = 0;
							RUBICON_StartDisplayTimer();
							LCD_DisplayStringLine(LCD_LINE_6, (uint8_t*) "  Broadcast time updated");
							RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
							eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;							
							break;
						}
						
						case RUBICON_NO_TIME_UPDATE:
						{
							RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
							eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;							
							break;
						}
						
						default:
						{
							RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
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
					else if(RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_FINISHED)
					{
						if (rubicon_cmd_request == RUBICON_UPLOAD_USING_CONFIG_FILE) RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_FROM_CONFIG_FILE;
						RUBICON_StartFwUpdateTimer(RUBICON_RESTART_TIME);
					}
					break;
				}
					
				case RUBICON_UPDATE_FILE:
				{
					RUBICON_StartRxTxTimeoutTimer(RUBICON_FILE_UPLOAD_TIMEOUT);
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
					RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
					eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
					break;
				}
				
				default:
				{
					RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
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
				RUBICON_StartRxTxTimeoutTimer(RUBICON_BYTE_RX_TIMEOUT);
			}
			else if(rx_buffer[0] == RUBICON_ACK)
			{
				if ((RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_BOOTLOADER) || \
				    (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN) || \
					((RUBICON_FileUpdatePacket.update_state >= FILE_UPDATE_IMAGE_1) && \
					(RUBICON_FileUpdatePacket.update_state <= FILE_UPDATE_IMAGE_19)))
				{
					eRubiconTransferState = RUBICON_PACKET_RECEIVED;
				}
				else
				{
					old_rx_cnt = rs485_rx_cnt;
					eRubiconTransferState = RUBICON_PACKET_RECEIVING;
					RUBICON_StartRxTxTimeoutTimer(RUBICON_BYTE_RX_TIMEOUT);
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
						(RUBICON_FileUpdatePacket.update_state <= FILE_UPDATE_IMAGE_19))
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
				
				RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
				eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
			}
			else if(IsRUBICON_RxTxTimeoutTimerExpired() && IsRUBICON_FwUpdateTimerExpired()) 
			{
				if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_BOOTLOADER)
				{
					if (rubicon_cmd_request == RUBICON_UPLOAD_USING_CONFIG_FILE) RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_FROM_CONFIG_FILE;
					else RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_FAIL;
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
					if (rubicon_cmd_request == RUBICON_UPLOAD_USING_CONFIG_FILE) RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_FROM_CONFIG_FILE;
					else RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_IDLE;
				}
				else if ((RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_QUERY_LIST) || \
						 (RUBICON_LogListTransfer.log_transfer_state == LOG_TRANSFER_DELETE_LOG))
				{
					++RUBICON_LogListTransfer.send_attempt;
				}
				else if ((RUBICON_FileUpdatePacket.update_state >= FILE_UPDATE_IMAGE_1) && \
						(RUBICON_FileUpdatePacket.update_state <= FILE_UPDATE_IMAGE_19))
				{
					++RUBICON_FileUpdatePacket.send_attempt;
					if (rubicon_cmd_request == RUBICON_UPLOAD_USING_CONFIG_FILE) RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_FROM_CONFIG_FILE;
				}
				
				RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
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
						
						RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
						eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
					}
				
			}
			else if (IsRUBICON_RxTxTimeoutTimerExpired())
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
				
				RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
				eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
			}
			
			if(old_rx_cnt != rs485_rx_cnt) 
			{
				old_rx_cnt = rs485_rx_cnt;
				RUBICON_StartRxTxTimeoutTimer(RUBICON_BYTE_RX_TIMEOUT);				
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

					RUBICON_StopRxTxTimeoutTimer();
					break;
				}
				
				case RUBICON_UPDATE_FIRMWARE:
				{
					if (RUBICON_FirmwareUpdatePacket.packet_send == RUBICON_FirmwareUpdatePacket.packet_total)
					{
						RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_FINISHED;
						f_close(&file_SD);
						f_mount(NULL,"0:",0);
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
						if(rubicon_cmd_request == RUBICON_UPLOAD_USING_CONFIG_FILE)
						{
							RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_FROM_CONFIG_FILE;
							f_close(&file_SD);
							f_mount(NULL,"0:",0);
						}									
						else if(rubicon_cmd_request == RUBICON_UPLOAD_ALL_IMAGE_TO_ALL_IN_RANGE)
						{
							t = 0;
							
							for(j = 0; j < 100; j++)
							{
								if(rubicon_ctrl_buffer[t] == NULL) j = 100;
								t++;
							}
							
							if((rubicon_ctrl_buffer[t] == '1') && (rubicon_ctrl_buffer[t + 1] == '3')) 
							{
								RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_FINISHED;
								f_close(&file_SD);
								f_mount(NULL,"0:",0);
								rubicon_cmd_request = NULL;
							}
							else if ((rubicon_ctrl_buffer[t] > '0') && (rubicon_ctrl_buffer[t] < '9')  && (rubicon_ctrl_buffer[t + 1] == NULL))
							{
								++rubicon_ctrl_buffer[t];
							}
							else if (rubicon_ctrl_buffer[t] == '9')
							{
								rubicon_ctrl_buffer[t] = '1';
								rubicon_ctrl_buffer[t + 1] = '0';
							}
							else if ((rubicon_ctrl_buffer[t] == '1') && (rubicon_ctrl_buffer[t + 1] < '9'))
							{
								rubicon_ctrl_buffer[t + 1] += 1;
							}							
						}
						else
						{
							RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_FINISHED;
							f_close(&file_SD);
							f_mount(NULL,"0:",0);
						}
						rubicon_cmd_request = RUBICON_START_BOOTLOADER;
					}
					if ((RUBICON_FileUpdatePacket.update_state >= FILE_UPDATE_IMAGE_1) && \
						(RUBICON_FileUpdatePacket.update_state <= FILE_UPDATE_IMAGE_19))
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
						LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "Log list transfered");
					}
					else if ((rx_buffer[0] == 0x06) && (rx_buffer[5] == 0x12) && (rx_buffer[6] == RUBICON_GET_LOG_LIST))
					{
						RUBICON_WriteLogToList();
						UpdateLogDisplay();					
					}
					else if ((rx_buffer[0] == 0x06) && (rx_buffer[5] == 0x01) && (rx_buffer[6] == RUBICON_DELETE_LOG_LIST))
					{
						display_flag = 0;
						RUBICON_StartDisplayTimer();
						LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "Log deleted               ");
					}
					else
					{
						display_flag = 0;
						RUBICON_StartDisplayTimer();
						LCD_DisplayStringLine(LCD_LINE_10, (uint8_t*) "Log list transfer failed  ");
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

			RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
			eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
			break;
		}
		case RUBICON_PACKET_ERROR:
		{
			RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
			eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
			break;
		}
		default:
		{
			RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
			eRubiconTransferState = RUBICON_PACKET_ENUMERATOR;
			break;
		}
    }
}
*
**/

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
    return rs485_group_address;
}

uint16_t RUBICON_GetBroadcastAddress(void)
{
    return rs485_broadcast_address;
}

int RUBICON_ScanRS485_Bus(uint16_t start_address, uint16_t end_address, uint8_t option)
{
	int new_fnd;
	static uint16_t address_offset;
	uint8_t scn_pcnt;
	uint8_t tmp_j;
	uint16_t tmp_address, rx_chksm;
	
	static enum
	{
		SCAN_INIT 		= 0x00,
		SCAN_SEND 		= 0x01,
		SCAN_PENDING	= 0x02,
		SCAN_RECEIVE	= 0x03,
		SCAN_SETUP		= 0x04
		
	}eBusScaningState;
	
	if(option == RS485_SCANNER_FIND_NEXT) eBusScaningState = SCAN_SETUP;
	else eBusScaningState = SCAN_INIT;
	
	rs485_rx_cnt = 0;
	scn_pcnt = 0;
	
	while(scn_pcnt == 0)
	{
		switch(eBusScaningState)
		{
			case SCAN_INIT:
			{
				if ((start_address <= RS485_INTERFACE_DEFAULT_ADDRESS) || (start_address >= end_address)) return (-1);
				p_comm_buffer = rx_buffer;
				while(p_comm_buffer < rx_buffer + sizeof(rx_buffer)) *p_comm_buffer++ = NULL;
				p_comm_buffer = tx_buffer;
				while(p_comm_buffer < tx_buffer + sizeof(tx_buffer)) *p_comm_buffer++ = NULL;
				new_fnd = 0;
				address_offset = 0;
				eBusScaningState = SCAN_SETUP;
				break;	
			}
			
			
			case SCAN_SEND:
			{
				if(!IsRUBICON_RxTxTimeoutTimerExpired()) break;				
				RS485_Send_Data(tx_buffer, (tx_buffer[5] + 9));
				RUBICON_StartRxTxTimeoutTimer(RUBICON_RESPONSE_TIMEOUT);
				eBusScaningState = SCAN_PENDING;				
				break;	
			}
			
			
			case SCAN_PENDING:
			{	
				if(((rx_buffer[1] == (rs485_interface_address >> 8)) && \
					(rx_buffer[2] == (rs485_interface_address & 0xff))) && \
					((rx_buffer[3] == (rs485_rubicon_address >> 8)) && \
					(rx_buffer[4] == (rs485_rubicon_address & 0xff))) && \
					(rx_buffer[rx_buffer[5] + 8] == RUBICON_EOT))
				{
					rx_chksm = 0;
					for (tmp_j = 6; tmp_j < (rx_buffer[5] + 6); tmp_j++) rx_chksm += rx_buffer[tmp_j];

					if ((rx_buffer[rx_buffer[5] + 6] == (rx_chksm >> 8)) && \
						(rx_buffer[rx_buffer[5] + 7] == (rx_chksm & 0xff)))
					{
						eBusScaningState = SCAN_RECEIVE;
					}
				}
				else if(IsRUBICON_RxTxTimeoutTimerExpired())
				{
					
					if(option == RS485_SCANNER_FIND_ADDRESSED)
					{
						return (0);
					}
					else
					{
						eBusScaningState = SCAN_SETUP;
					}
				}
				break;
			}
			
			
			case SCAN_RECEIVE:
			{	
				rubicon_firmware_update_address_list[new_fnd] = rs485_rubicon_address;
				
				if((option == RS485_SCANNER_FIND_FIRST) || \
					(option == RS485_SCANNER_FIND_NEXT) || \
					(option == RS485_SCANNER_FIND_ADDRESSED))
				{
					return (1);
				}
				else
				{
					RUBICON_StartRxTxTimeoutTimer(RUBICON_RESPONSE_TIMEOUT);
					eBusScaningState = SCAN_SETUP;
					++new_fnd;
				}	
				break;
			}
			
			
			case SCAN_SETUP:
			{
				if(option == RS485_SCANNER_FIND_NEW)
				{
					tmp_address = 0;
					rubicon_address_list_cnt = 0;
					
					while(rubicon_address_list[rubicon_address_list_cnt + 1] != 0x0000)	// find if call address is allready used 
					{
						tmp_address = RUBICON_GetNextAddress();					
						if((start_address + address_offset) == tmp_address)
						{
							tmp_address = 0;
							++address_offset;
							rubicon_address_list_cnt = 0;
						}
					}
				}				
				else if(option == RS485_SCANNER_FIND_ADDRESSED)
				{
					address_offset = 0;
				}
				
				rs485_rubicon_address = (start_address + address_offset);
				if((start_address + address_offset) > end_address) scn_pcnt = 1;
				rs485_interface_address = RS485_INTERFACE_DEFAULT_ADDRESS;
				++address_offset;
				RUBICON_PrepareCommandPacket(RUBICON_GET_SYS_INFO, rx_buffer);			
				eBusScaningState = SCAN_SEND;			
				break;
			}
		}
	}
	
	return (new_fnd);
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
	
	if(bytes_rd >= RUBICON_CONFIG_FILE_MAX_SIZE)
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
	
//	if (f_mount(&filesystem, "0:", 0) != FR_OK)
//	{
//		return FILE_SYS_ERROR;
//	}
//	
//	if (f_opendir(&dir_1, "/") != FR_OK)
//	{
//		f_mount(NULL,"0:",0);
//		return FILE_DIR_ERROR;
//	}
//	
//	if (f_open(&file_SD, "UPDATE.CFG", FA_READ) != FR_OK) 
//	{
//		f_mount(NULL,"0:",0);
//		return FILE_ERROR;
//	}
	
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

/**uint8_t RUBICON_CheckNewFirmwareFile(void)
*
uint8_t RUBICON_CheckNewFirmwareFile(void)
{
	
	W25Qxx_Read((W25QXX_FW_END_ADDRESS - 3), file_size, 4);
	
	if((file_size[0] == 0) && (file_size[1] == 0) && (file_size[2] == 0) && (file_size[3] == 0)) return FILE_SYS_ERROR;
	else if((file_size[0] == 0xff) && (file_size[1] == 0xff) && (file_size[2] == 0xff) && (file_size[3] == 0xff)) return FILE_SYS_ERROR;

	
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
	
    if (rubicon_cmd_request != RUBICON_UPLOAD_USING_CONFIG_FILE) 
	{
		rs485_rubicon_address = atoi((char *) rubicon_ctrl_buffer);
	}
	else if(rubicon_firmware_update_address_list[rubicon_address_list_cnt] != 0x0000)
	{
		rs485_rubicon_address = rubicon_firmware_update_address_list[rubicon_address_list_cnt];
	}
	else
	{
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
		return FW_UPDATE_FINISHED;
	}
	
    RUBICON_FirmwareUpdatePacket.update_state = FW_UPDATE_INIT;
    RUBICON_FirmwareUpdatePacket.send_attempt = 0;
    RUBICON_FirmwareUpdatePacket.packet_send = 0;
    RUBICON_FirmwareUpdatePacket.last_packet_send = 0;
	RUBICON_FirmwareUpdatePacket.packet_total = ((file_size[0] << 24) + (file_size[1] << 16) + (file_size[2] << 8) + file_size[3]) / RUBICON_PACKET_BUFFER_SIZE;

    if ((RUBICON_FirmwareUpdatePacket.packet_total * RUBICON_PACKET_BUFFER_SIZE) < ((file_size[0] << 24) + (file_size[1] << 16) + (file_size[2] << 8) + file_size[3]))
    {
        ++RUBICON_FirmwareUpdatePacket.packet_total;
    }
    RUBICON_FirmwareUpdatePacket.packet_total = file_SD.obj.objsize / RUBICON_PACKET_BUFFER_SIZE;

    if ((RUBICON_FirmwareUpdatePacket.packet_total * RUBICON_PACKET_BUFFER_SIZE) < file_SD.obj.objsize)
    {
        ++RUBICON_FirmwareUpdatePacket.packet_total;
    }

    return FILE_OK;	
}
*
**/

/**uint8_t RUBICON_CheckNewImageFile(void)
*
*
uint8_t RUBICON_CheckNewImageFile(void)
{
	uint32_t k;
	
	if (f_mount(&filesystem, "0:", 0) != FR_OK)
	{
		return FILE_SYS_ERROR;
	}

	if (f_opendir(&dir_1, "/") != FR_OK)
	{
		f_mount(NULL,"0:",0);
		return FILE_DIR_ERROR;
	}

	if (rubicon_cmd_request == RUBICON_UPLOAD_USING_CONFIG_FILE)
	{
		if(config_file_image_cnt == 1)
		{
			if (f_open(&file_SD, "IMG1.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_1;
		}
		else if(config_file_image_cnt == 2)
		{
			if (f_open(&file_SD, "IMG2.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_2;
		}
		else if(config_file_image_cnt == 3)
		{
			if (f_open(&file_SD, "IMG3.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_3;
		}
		else if(config_file_image_cnt == 4)
		{
			if (f_open(&file_SD, "IMG4.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_4;
		}
		else if(config_file_image_cnt == 5)
		{
			if (f_open(&file_SD, "IMG5.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_5;
		}
		else if(config_file_image_cnt == 6)
		{
			if (f_open(&file_SD, "IMG6.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_6;
		}
		else if(config_file_image_cnt == 7)
		{
			if (f_open(&file_SD, "IMG7.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_7;
		}
		else if(config_file_image_cnt == 8)
		{
			if (f_open(&file_SD, "IMG8.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_8;
		}
		else if(config_file_image_cnt == 9)
		{
			if (f_open(&file_SD, "IMG9.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_9;
		}
		else if(config_file_image_cnt == 10)
		{
			if (f_open(&file_SD, "IMG10.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_10;
		}
		else if(config_file_image_cnt == 11)
		{
			if (f_open(&file_SD, "IMG11.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_11;
		}
		else if(config_file_image_cnt == 12)
		{
			if (f_open(&file_SD, "IMG12.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_12;
		}
		else if(config_file_image_cnt == 13)
		{
			if (f_open(&file_SD, "IMG13.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_13;
		}
		else if(config_file_image_cnt == 14)
		{
			if (f_open(&file_SD, "IMG14.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_14;
		}
		else if(config_file_image_cnt == 15)
		{
			if (f_open(&file_SD, "IMG15.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_15;
		}
		else if(config_file_image_cnt == 16)
		{
			if (f_open(&file_SD, "IMG16.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_16;
		}
		else if(config_file_image_cnt == 17)
		{
			if (f_open(&file_SD, "IMG17.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_17;
		}
		else if(config_file_image_cnt == 18)
		{
			if (f_open(&file_SD, "IMG18.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_18;
		}
		else if(config_file_image_cnt == 19)
		{
			if (f_open(&file_SD, "IMG19.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_19;
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
				else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_1;
			}
			else if(rubicon_ctrl_buffer[k] == '0')
			{
				if (f_open(&file_SD, "IMG10.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}			
				else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_10;
			}
			else if(rubicon_ctrl_buffer[k] == '1')
			{
				if (f_open(&file_SD, "IMG11.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_11;
			}
			else if(rubicon_ctrl_buffer[k] == '2')
			{
				if (f_open(&file_SD, "IMG12.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_12;
			}
			else if(rubicon_ctrl_buffer[k] == '3')
			{
				if (f_open(&file_SD, "IMG13.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_13;
			}
			else if(rubicon_ctrl_buffer[k] == '4')
			{
				if (f_open(&file_SD, "IMG14.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_14;
			}
			else if(rubicon_ctrl_buffer[k] == '5')
			{
				if (f_open(&file_SD, "IMG15.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_15;
			}
			else if(rubicon_ctrl_buffer[k] == '6')
			{
				if (f_open(&file_SD, "IMG16.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_16;
			}
			else if(rubicon_ctrl_buffer[k] == '7')
			{
				if (f_open(&file_SD, "IMG17.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_17;
			}
			else if(rubicon_ctrl_buffer[k] == '8')
			{
				if (f_open(&file_SD, "IMG18.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_18;
			}
			else if(rubicon_ctrl_buffer[k] == '9')
			{
				if (f_open(&file_SD, "IMG19.RAW", FA_READ) != FR_OK) 
				{
					f_mount(NULL,"0:",0);
					return FILE_ERROR;
				}
				else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_19;
			}
		}
		else if(rubicon_ctrl_buffer[k] == '2') 
		{
			if (f_open(&file_SD, "IMG2.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_2;
		}
		else if(rubicon_ctrl_buffer[k] == '3') 
		{
			if (f_open(&file_SD, "IMG3.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_3;
		}
		else if(rubicon_ctrl_buffer[k] == '4') 
		{
			if (f_open(&file_SD, "IMG4.RAW", FA_READ) != FR_OK)
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_4;
		}
		else if(rubicon_ctrl_buffer[k] == '5') 
		{
			if (f_open(&file_SD, "IMG5.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_5;
		}
		else if(rubicon_ctrl_buffer[k] == '6') 
		{
			if (f_open(&file_SD, "IMG6.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_6;
		}
		else if(rubicon_ctrl_buffer[k] == '7') 
		{
			if (f_open(&file_SD, "IMG7.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_7;
		}
		else if(rubicon_ctrl_buffer[k] == '8') 
		{
			if (f_open(&file_SD, "IMG8.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_8;
		}
		else if(rubicon_ctrl_buffer[k] == '9') 
		{
			if (f_open(&file_SD, "IMG9.RAW", FA_READ) != FR_OK) 
			{
				f_mount(NULL,"0:",0);
				return FILE_ERROR;
			}
			else RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IMAGE_9;
		}
		else return FILE_ERROR;
	}
	
	if((file_size[0] == 0) && (file_size[1] == 0) && (file_size[2] == 0) && (file_size[3] == 0)) return FILE_SYS_ERROR;
	else if((file_size[0] == 0xff) && (file_size[1] == 0xff) && (file_size[2] == 0xff) && (file_size[3] == 0xff)) return FILE_SYS_ERROR;
	
	if (rubicon_cmd_request != RUBICON_UPLOAD_USING_CONFIG_FILE) 
	{
		rs485_rubicon_address = atoi((char *) rubicon_ctrl_buffer);
	}
	else if(rubicon_image_update_address_list[rubicon_address_list_cnt] != 0x0000)
	{
		rs485_rubicon_address = rubicon_image_update_address_list[rubicon_address_list_cnt];
	}
	else
	{
		f_close(&file_SD);
		return FILE_UPDATE_FINISHED;
	}
	
	//rs485_rubicon_address = atoi((char *) rubicon_ctrl_buffer);
    RUBICON_FileUpdatePacket.send_attempt = 0;
    RUBICON_FileUpdatePacket.packet_send = 0;
    RUBICON_FileUpdatePacket.last_packet_send = 0;
    RUBICON_FileUpdatePacket.packet_total = ((file_size[0] << 24) + (file_size[1] << 16) + (file_size[2] << 8) + file_size[3]) / RUBICON_PACKET_BUFFER_SIZE;
	RUBICON_FileUpdatePacket.packet_total = file_SD.obj.objsize / RUBICON_PACKET_BUFFER_SIZE;
    if ((RUBICON_FileUpdatePacket.packet_total * RUBICON_PACKET_BUFFER_SIZE) < ((file_size[0] << 24) + (file_size[1] << 16) + (file_size[2] << 8) + file_size[3]))
    {
        ++RUBICON_FileUpdatePacket.packet_total;
    }
	if ((RUBICON_FileUpdatePacket.packet_total * RUBICON_PACKET_BUFFER_SIZE) < file_SD.obj.objsize)
    {
        ++RUBICON_FileUpdatePacket.packet_total;
    }
    return FILE_OK;
}
*
**/

void RUBICON_PrepareFirmwareUpdatePacket(void)
{
//    static uint32_t fwup_address = 0;
	uint32_t i;

    //FW_UPDATE_IDLE			16
    //FW_UPDATE_INIT 			17
    //FW_UPDATE_BOOTLOADER 		18
    //FW_UPDATE_RUN				19
    //FW_UPDATE_FINISHED		20
    //FW_UPDATE_FAIL			21

    if (RUBICON_FirmwareUpdatePacket.send_attempt >= MAX_QUERY_ATTEMPTS)
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
		//fwup_address = W25QXX_FW_START_ADDRESS;
    }
    else if (RUBICON_FirmwareUpdatePacket.update_state == FW_UPDATE_RUN)
    {
        tx_buffer[0] = RUBICON_STX;
        tx_buffer[5] = 0x42;
        tx_buffer[6] = RUBICON_FirmwareUpdatePacket.packet_send >> 8;
        tx_buffer[7] = RUBICON_FirmwareUpdatePacket.packet_send & 0xff;

        f_read(&file_SD, (uint8_t*) &tx_buffer[8], RUBICON_PACKET_BUFFER_SIZE, (UINT*) (&RUBICON_FirmwareUpdatePacket.file_data_read));
		
		if (RUBICON_FileUpdatePacket.packet_send == 1) delay(500);
		
//		W25Qxx_Read(fwup_address, &tx_buffer[8], RUBICON_PACKET_BUFFER_SIZE);
//		fwup_address += RUBICON_PACKET_BUFFER_SIZE;	
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
//	static uint32_t pfup_address = 0;
	uint32_t i;

	//	FILE_UPDATE_IDLE			40
	//	FILE_UPDATE_INIT 			41
	//	FILE_UPDATE_RUN				42
	//	FILE_UPDATE_FINISHED		43
	//	FILE_UPDATE_FAIL			44

    if (RUBICON_FileUpdatePacket.send_attempt >= MAX_QUERY_ATTEMPTS)
    {
        RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_FAIL;
    }

    if ((RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_FAIL) || \
	   (RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_FINISHED))
    {
        RUBICON_FileUpdatePacket.update_state = FILE_UPDATE_IDLE;
		f_close(&file_SD);
		f_mount(NULL,"0:",0);
        return;
    }
	else if (RUBICON_FileUpdatePacket.packet_send == 0)
    {
        tx_buffer[0] = RUBICON_SOH;
        tx_buffer[5] = 0x03;
		
        if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_1) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_1;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_2) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_2;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_3) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_3;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_4) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_4;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_5) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_5;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_6) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_6;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_7) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_7;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_8) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_8;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_9) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_9;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_10) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_10;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_11) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_11;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_12) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_12;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_13) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_13;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_14) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_14;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_15) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_15;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_16) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_16;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_17) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_17;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_18) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_18;
		}
		else if(RUBICON_FileUpdatePacket.update_state == FILE_UPDATE_IMAGE_19) 
		{
			tx_buffer[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_19;
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

        f_read(&file_SD, (uint8_t*) &tx_buffer[8], RUBICON_PACKET_BUFFER_SIZE, (UINT*) (&RUBICON_FileUpdatePacket.file_data_read));
		
		if (RUBICON_FileUpdatePacket.packet_send == 1) delay(500);
		
//		W25Qxx_Read(pfup_address, &tx_buffer[8], RUBICON_PACKET_BUFFER_SIZE);
	
//		if(RUBICON_FileUpdatePacket.last_packet_send < RUBICON_FileUpdatePacket.packet_send)
//		{
//			pfup_address += RUBICON_PACKET_BUFFER_SIZE;
//		}
			
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
	/**	Full command list switch states
	*
	*
	switch(rubicon_cmd_request)
	{
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_1:
		{
			
			break;
		}
			
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_2:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_3:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_4:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_5:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_6:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_7:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_8:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_9:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_10:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_SMALL_FONTS:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_MIDDLE_FONTS:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_BIG_FONTS:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_TEXT_DATE_TIME:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_TEXT_EVENTS:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_FIRMWARE:
		{
			
			break;
		}
		
		
		case RUBICON_FLASH_PROTECTION_ENABLE:
		{
			
			break;
		}
		
		
		case RUBICON_FLASH_PROTECTION_DISABLE:
		{
			
			break;
		}
		
		
		case RUBICON_START_BOOTLOADER:
		{
			
			break;
		}
		
		
		case RUBICON_EXECUTE_APPLICATION:
		{
			
			break;
		}
		
		
		case RUBICON_GET_SYS_STATUS:
		{
			
			break;
		}
		
		
		case RUBICON_GET_SYS_INFO:
		{
			
			break;
		}
		
		
		case RUBICON_GET_DISPLAY_BRIGHTNESS:
		{
			
			break;
		}
		
		
		case RUBICON_SET_DISPLAY_BRIGHTNESS:
		{
			
			break;
		}
		
		
		case RUBICON_GET_RTC_DATE_TIME:
		{
			
			break;
		}
		
		
		case RUBICON_SET_RTC_DATE_TIME:
		{
			
			break;
		}
		
		
		case RUBICON_GET_LOG_LIST:
		{
			
			break;
		}
		
		
		case RUBICON_DELETE_LOG_LIST:
		{
			
			break;
		}
		
		
		case RUBICON_GET_RS485_CONFIG:
		{
			
			break;
		}
		
		
		case RUBICON_SET_RS485_CONFIG:
		{
			
			break;
		}
		
		
		case RUBICON_GET_DIN_STATE:
		{
			
			break;
		}
		
		
		case RUBICON_SET_DOUT_STATE:
		{
			
			break;
		}
		
		
		case RUBICON_GET_DOUT_STATE:
		{
			
			break;
		}
		
		
		case RUBICON_GET_PCB_TEMPERATURE:
		{
			
			break;
		}
		
		
		case RUBICON_GET_TEMP_CARD_BUFFER:
		{
			
			break;
		}
		
		
		case RUBICON_GET_MIFARE_AUTHENTICATION_KEY_A:
		{
			
			break;
		}
		
		
		case RUBICON_SET_MIFARE_AUTHENTICATION_KEY_A:
		{
			
			break;
		}
		
		
		case RUBICON_GET_MIFARE_AUTHENTICATION_KEY_B:
		{
			
			break;
		}
		
		
		case RUBICON_SET_MIFARE_AUTHENTICATION_KEY_B:
		{
			
			break;
		}
		
		
		case RUBICON_GET_MIFARE_PERMITED_GROUP:
		{
			
			break;
		}
		
		
		case RUBICON_SET_MIFARE_PERMITED_GROUP:
		{
			
			break;
		}
		
		
		case RUBICON_GET_MIFARE_PERMITED_CARD_1:
		{
			
			break;
		}
		
		
		case RUBICON_SET_MIFARE_PERMITED_CARD_1:
		{
			
			break;
		}
		
		
		case RUBICON_GET_MIFARE_PERMITED_CARD_2:
		{
			
			break;
		}
		
		
		case RUBICON_SET_MIFARE_PERMITED_CARD_2:
		{
			
			break;
		}
		
		
		case RUBICON_GET_MIFARE_PERMITED_CARD_3:
		{
			
			break;
		}
		
		
		case RUBICON_SET_MIFARE_PERMITED_CARD_3:
		{
			
			break;
		}
		
		
		case RUBICON_GET_MIFARE_PERMITED_CARD_4:
		{
			
			break;
		}
		
		
		case RUBICON_SET_MIFARE_PERMITED_CARD_4:
		{
			
			break;
		}
		
		
		case RUBICON_GET_MIFARE_PERMITED_CARD_5:
		{
			
			break;
		}
		
		
		case RUBICON_SET_MIFARE_PERMITED_CARD_5:
		{
			
			break;
		}
		
		
		case RUBICON_GET_MIFARE_PERMITED_CARD_6:
		{
			
			break;
		}
		
		
		case RUBICON_SET_MIFARE_PERMITED_CARD_6:
		{
			
			break;
		}
		
		
		case RUBICON_GET_MIFARE_PERMITED_CARD_7:
		{
			
			break;
		}
		
		
		case RUBICON_SET_MIFARE_PERMITED_CARD_7:
		{
			
			break;
		}
		
		
		case RUBICON_GET_MIFARE_PERMITED_CARD_8:
		{
			
			break;
		}
		
		
		case RUBICON_SET_MIFARE_PERMITED_CARD_8:
		{
			
			break;
		}
		
		
		case RUBICON_GET_ROOM_STATUS:
		{
			
			break;
		}
		
		
		case RUBICON_SET_ROOM_STATUS:
		{
			
			break;
		}
		
		
		case RUBICON_RESET_SOS_ALARM:
		{
			
			break;
		}
		
		
		case RUBICON_GET_ROOM_TEMPERATURE:
		{
			
			break;
		}
		
		
		case RUBICON_SET_ROOM_TEMPERATURE:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_11:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_12:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_13:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_14:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_15:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_16:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_17:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_18:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_19:
		{
			
			break;
		}
		
		
		case RUBICON_DOWNLOAD_DISPLAY_IMAGE_20:
		{
			
			break;
		}

		
		case RUBICON_SET_MIFARE_PERMITED_CARD:
		{
			
			break;
		}
		
		
		case RUBICON_UPLOAD_ALL_IMAGE_TO_SELECTED:
		{
			
			break;
		}
		
		
		case RUBICON_UPLOAD_ALL_IMAGE_TO_ALL_IN_RANGE:
		{
			
			break;
		}
		
		
		case RUBICON_UPLOAD_USING_CONFIG_FILE:
		{
			
			break;
		}
		
		
		default:
		{
			break;
		}
		
	}
	********************************************
	*	End of rubicon_control_request switch
	********************************************
	*
	**/
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

void RUBICON_CmdParse(void)
{   
	uint32_t i;
	static uint32_t index = 0;
	char f_buff[256], *cp;
	char c_buff[16];
	unsigned long card_id;
	
	if(rs485_pcb_p->cmd_state != CMD_REQUEST)
	{
		rs485_pcb_p->cmd_rtn = CMD_INVALID_ERROR;
		return;
	}
	
	rs485_pcb_p->out_buff_p[0] = RUBICON_SOH;
	rs485_pcb_p->out_buff_p[1] = rs485_pcb_p->rec_addr >> 8;
	rs485_pcb_p->out_buff_p[2] = rs485_pcb_p->rec_addr & 0x00ff;
	rs485_pcb_p->out_buff_p[3] = rs485_interface_address >> 8;
	rs485_pcb_p->out_buff_p[4] = rs485_interface_address & 0x00ff;
	rs485_pcb_p->out_buff_p[5] = 0x01;
	rs485_pcb_p->out_buff_p[6] = rs485_pcb_p->cmd;

	
	switch(rs485_pcb_p->cmd)
	{
		case RUBICON_DOWNLOAD_FIRMWARE:
		{		
			if(rs485_pcb_p->cmd_rtn == CMD_IDLE)
			{
				if (f_mount(&filesystem, "", 1) == FR_OK)
				{
					find_file_by_type(f_buff, "RC*.BIN");
					
					if(strlen(f_buff) != 0) 
					{
						if (f_open(&file_SD, f_buff, FA_READ) == FR_OK) 
						{
							rs485_pcb_p->pck_total = file_SD.obj.objsize / RUBICON_PACKET_BUFFER_SIZE;							
							if ((rs485_pcb_p->pck_total * RUBICON_PACKET_BUFFER_SIZE) < file_SD.obj.objsize)
							{
								++rs485_pcb_p->pck_total;
							}
							rs485_pcb_p->out_buff_p[6] = RUBICON_START_BOOTLOADER;
							rs485_pcb_p->tmr_preset = RUBICON_RESPONSE_TIMEOUT;
							rs485_pcb_p->pck_mode = CMD_P2P;
							rs485_pcb_p->flag = 1;
						}
						else
						{
							f_mount(NULL,"",0);
							rs485_pcb_p->cmd = CMD_IDLE;
							rs485_pcb_p->cmd_state = CMD_IDLE;
							rs485_pcb_p->cmd_rtn = CMD_INVALID_ERROR;
							rs485_pcb_p->cmd_rtn = FILE_ERROR;
							return;
						}
					}
					else
					{
						f_mount(NULL,"",0);
						rs485_pcb_p->cmd = CMD_IDLE;
						rs485_pcb_p->cmd_state = CMD_IDLE;
						rs485_pcb_p->cmd_rtn = FILE_ERROR;
						break;
					}	
				}
				else
				{
					rs485_pcb_p->cmd = CMD_IDLE;
					rs485_pcb_p->cmd_state = CMD_IDLE;
					rs485_pcb_p->cmd_rtn = FILE_SYS_ERROR;
					return;
				}							
			}
			else if(rs485_pcb_p->cmd_rtn == CMD_OK)
			{
				if(rs485_pcb_p->flag == 1)
				{
					index = 0;
					rs485_pcb_p->out_buff_p[5] = 0x03;
					rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_FIRMWARE;
					rs485_pcb_p->out_buff_p[7] = rs485_pcb_p->pck_total >> 8;
					rs485_pcb_p->out_buff_p[8] = rs485_pcb_p->pck_total & 0xff;
					rs485_pcb_p->flag = 2;
					rs485_pcb_p->pck_sent = 0;
					rs485_pcb_p->cmd_rtn = CMD_IDLE;
					rs485_pcb_p->pck_mode = CMD_P2P;										
					rs485_pcb_p->cmd_attempt = RUBICON_MAX_ERRORS;
					rs485_pcb_p->tmr_preset = RUBICON_FW_EXE_BOOT_TIME;
					RUBICON_StartRxTxTimeoutTimer(RUBICON_BOOTLOADER_START_TIME);					
				}
				if(rs485_pcb_p->flag == 2)
				{
					rs485_pcb_p->out_buff_p[0] = RUBICON_STX;
					rs485_pcb_p->out_buff_p[5] = 0x42;
					rs485_pcb_p->out_buff_p[6] = rs485_pcb_p->pck_sent >> 8;
					rs485_pcb_p->out_buff_p[7] = rs485_pcb_p->pck_sent & 0xff;
					f_read(&file_SD, (uint8_t*) &rs485_pcb_p->out_buff_p[8], RUBICON_PACKET_BUFFER_SIZE, (UINT*) (&index));
					rs485_pcb_p->pck_mode = CMD_P2P;
					rs485_pcb_p->cmd_rtn = CMD_IDLE;
					rs485_pcb_p->cmd_attempt = RUBICON_MAX_ERRORS;
					rs485_pcb_p->tmr_preset = RUBICON_RESPONSE_TIMEOUT;					
				}				
			}				
			break;
		}
				
		case RUBICON_SET_DISPLAY_BRIGHTNESS:
		{
			rs485_pcb_p->out_buff_p[5] = 0x03;
			
			while((*rs485_pcb_p->cmd_args_p) != NULL) ++rs485_pcb_p->cmd_args_p;
			++rs485_pcb_p->cmd_args_p;
			
			i = atoi(rs485_pcb_p->cmd_args_p);
			rs485_pcb_p->out_buff_p[7] = i >> 8;	// display brightness MSB
			rs485_pcb_p->out_buff_p[8] = i & 0xff;	// display brightness LSB
			rs485_pcb_p->pck_mode = CMD_P2P;
			break;
		}
		
		case RUBICON_SET_RTC_DATE_TIME:
		{
			RTC_GetDate(RTC_Format_BCD, &RTC_Date);
			RTC_GetTime(RTC_Format_BCD, &RTC_Time);	
			
			rs485_pcb_p->out_buff_p[5] = 0x0d;
			rs485_pcb_p->out_buff_p[6] = RUBICON_SET_RTC_DATE_TIME;
			rs485_pcb_p->out_buff_p[7] = (RTC_Date.RTC_Date >> 4) + 48;
			rs485_pcb_p->out_buff_p[8] = (RTC_Date.RTC_Date & 0x0f) + 48;
			rs485_pcb_p->out_buff_p[9] = (RTC_Date.RTC_Month >> 4) + 48;
			rs485_pcb_p->out_buff_p[10] = (RTC_Date.RTC_Month & 0x0f) + 48;
			rs485_pcb_p->out_buff_p[11] = (RTC_Date.RTC_Year >> 4) + 48;
			rs485_pcb_p->out_buff_p[12] = (RTC_Date.RTC_Year & 0x0f) + 48;
			rs485_pcb_p->out_buff_p[13] = (RTC_Time.RTC_Hours >> 4) + 48;
			rs485_pcb_p->out_buff_p[14] = (RTC_Time.RTC_Hours & 0x0f) + 48;
			rs485_pcb_p->out_buff_p[15] = (RTC_Time.RTC_Minutes >> 4) + 48;
			rs485_pcb_p->out_buff_p[16] = (RTC_Time.RTC_Minutes & 0x0f) + 48;
			rs485_pcb_p->out_buff_p[17] = (RTC_Time.RTC_Seconds >> 4) + 48;
			rs485_pcb_p->out_buff_p[18] = (RTC_Time.RTC_Seconds & 0x0f) + 48;
			rs485_pcb_p->pck_mode = CMD_BROADCAST;
			break;
		}
		
		
		case RUBICON_SET_RS485_CONFIG:
		{
			rs485_pcb_p->out_buff_p[5] = 13;
			
			while((*rs485_pcb_p->cmd_args_p) != NULL) ++rs485_pcb_p->cmd_args_p;
			++rs485_pcb_p->cmd_args_p;
			
			i = atoi(rs485_pcb_p->cmd_args_p);
			rs485_pcb_p->in_buff_p[0] = i >> 8;
			rs485_pcb_p->in_buff_p[1] = i & 0xff;
			rs485_pcb_p->in_buff_p[2] = rs485_group_address >> 8;
			rs485_pcb_p->in_buff_p[3] = rs485_group_address & 0xff;
			rs485_pcb_p->in_buff_p[4] = rs485_broadcast_address >> 8;
			rs485_pcb_p->in_buff_p[5] = rs485_broadcast_address & 0xff;
			Hex2Str(&rs485_pcb_p->in_buff_p[0], 6, &rs485_pcb_p->out_buff_p[7]);
			rs485_pcb_p->pck_mode = CMD_P2P;			
			break;
		}
	
		
		case RUBICON_SET_DOUT_STATE:
		{
			rs485_pcb_p->out_buff_p[5] = 17;
			
			while((*rs485_pcb_p->cmd_args_p) != NULL) ++rs485_pcb_p->cmd_args_p;
			++rs485_pcb_p->cmd_args_p;
			
			i = 7;
			while (i < 23) rs485_pcb_p->out_buff_p[i++] = *rs485_pcb_p->cmd_args_p++;
			rs485_pcb_p->pck_mode = CMD_P2P;
			break;
		}
	
		
		case RUBICON_SET_ROOM_STATUS:
		{
			while((*rs485_pcb_p->cmd_args_p) != NULL) ++rs485_pcb_p->cmd_args_p;
			++rs485_pcb_p->cmd_args_p;
			
			if(IS_09(*rs485_pcb_p->cmd_args_p))
			{
				rs485_pcb_p->pck_mode = CMD_P2P;
				rs485_pcb_p->out_buff_p[5] = 0x02;
				rs485_pcb_p->out_buff_p[7] = *rs485_pcb_p->cmd_args_p++;				
				
				if(IS_09(*rs485_pcb_p->cmd_args_p))
				{
					rs485_pcb_p->out_buff_p[5] = 0x03;
					rs485_pcb_p->out_buff_p[8] = *rs485_pcb_p->cmd_args_p;
				}
			}
			else
			{
				rs485_pcb_p->cmd = CMD_IDLE;
				rs485_pcb_p->cmd_state = CMD_IDLE;
				rs485_pcb_p->cmd_rtn = CMD_PARAMETAR_INVALID_ERROR;
				return;
			}
			break;
		}
		
		
		case RUBICON_RESET_SOS_ALARM:
		{
			rs485_pcb_p->pck_mode = CMD_P2P;
			rs485_pcb_p->out_buff_p[5] = 2;
			rs485_pcb_p->out_buff_p[7] = '1';
			break;
		}
		
	
		case RUBICON_SET_ROOM_TEMPERATURE:
		{
			rs485_pcb_p->out_buff_p[5] = 8;
			
			while((*rs485_pcb_p->cmd_args_p) != NULL) ++rs485_pcb_p->cmd_args_p;
			++rs485_pcb_p->cmd_args_p;
			
			i = atoi(rs485_pcb_p->cmd_args_p);
			
			if (i & (1 << 7)) rs485_pcb_p->out_buff_p[7] = 'E';
			else rs485_pcb_p->out_buff_p[7] = 'D';
			
			if (i  & (1 << 6)) rs485_pcb_p->out_buff_p[8] = 'H';
			else rs485_pcb_p->out_buff_p[8] = 'C';
			
			i &= 0xc0;
			rs485_pcb_p->out_buff_p[9] = 48;			
			
			while(i > 9)
			{
				++rs485_pcb_p->out_buff_p[9];
				i -= 10;
			}
			
			rs485_pcb_p->out_buff_p[10] = i + 48;
			
			while((*rs485_pcb_p->cmd_args_p) != NULL) ++rs485_pcb_p->cmd_args_p;
			++rs485_pcb_p->cmd_args_p;
			
			i = atoi(rs485_pcb_p->cmd_args_p);
			
			if (i & (1 << 7)) rs485_pcb_p->out_buff_p[7] = 'O';
			i &= 0x80;
			
			rs485_pcb_p->out_buff_p[11] = 48;
			rs485_pcb_p->out_buff_p[12] = 48;
			
			while(i > 99) 
			{
				i -= 100;
				++rs485_pcb_p->out_buff_p[11];
			}
			
			while(i > 9)
			{
				++rs485_pcb_p->out_buff_p[12];
				i -= 10;
			}
			
			rs485_pcb_p->out_buff_p[13] = i + 48;	
			rs485_pcb_p->pck_mode = CMD_P2P;			
			break;
		}
		
		
		case RUBICON_SET_MIFARE_PERMITED_CARD:
		{
			rs485_pcb_p->out_buff_p[5] = 21;
			
			while((*rs485_pcb_p->cmd_args_p) != NULL) ++rs485_pcb_p->cmd_args_p;
			++rs485_pcb_p->cmd_args_p;
			
			if((*rs485_pcb_p->cmd_args_p) == '1') rs485_pcb_p->out_buff_p[6] = RUBICON_SET_MIFARE_PERMITED_CARD_1;
			else if((*rs485_pcb_p->cmd_args_p) == '2') rs485_pcb_p->out_buff_p[6] = RUBICON_SET_MIFARE_PERMITED_CARD_2;
			else if((*rs485_pcb_p->cmd_args_p) == '3') rs485_pcb_p->out_buff_p[6] = RUBICON_SET_MIFARE_PERMITED_CARD_3;
			else if((*rs485_pcb_p->cmd_args_p) == '4') rs485_pcb_p->out_buff_p[6] = RUBICON_SET_MIFARE_PERMITED_CARD_4;
			else if((*rs485_pcb_p->cmd_args_p) == '5') rs485_pcb_p->out_buff_p[6] = RUBICON_SET_MIFARE_PERMITED_CARD_5;
			else if((*rs485_pcb_p->cmd_args_p) == '6') rs485_pcb_p->out_buff_p[6] = RUBICON_SET_MIFARE_PERMITED_CARD_6;
			else if((*rs485_pcb_p->cmd_args_p) == '7') rs485_pcb_p->out_buff_p[6] = RUBICON_SET_MIFARE_PERMITED_CARD_7;
			else if((*rs485_pcb_p->cmd_args_p) == '8') rs485_pcb_p->out_buff_p[6] = RUBICON_SET_MIFARE_PERMITED_CARD_8;
			else 
			{
				rs485_pcb_p->cmd_rtn = CMD_PARAMETAR_INVALID_ERROR;
				return;
			}
			
			while((*rs485_pcb_p->cmd_args_p) != NULL) ++rs485_pcb_p->cmd_args_p;
			++rs485_pcb_p->cmd_args_p;
			while((*rs485_pcb_p->cmd_args_p) == '0') ++rs485_pcb_p->cmd_args_p;	
			
			card_id = strtoul(rs485_pcb_p->cmd_args_p, NULL, 0);
			rs485_pcb_p->in_buff_p[0] = card_id  & 0xff;
			rs485_pcb_p->in_buff_p[1] = (card_id >> 8) & 0xff;
			rs485_pcb_p->in_buff_p[2] = (card_id >> 16) & 0xff;
			rs485_pcb_p->in_buff_p[3] = (card_id >> 24) & 0xff;				
			Hex2Str(&rs485_pcb_p->in_buff_p[0], 4, &rs485_pcb_p->out_buff_p[7]);
			rs485_pcb_p->out_buff_p[15] = NULL;
			rs485_pcb_p->out_buff_p[16] = NULL;			
			
			while((*rs485_pcb_p->cmd_args_p) != NULL) ++rs485_pcb_p->cmd_args_p;
			++rs485_pcb_p->cmd_args_p;
			
			i = 17;
			while (i < 27) rs485_pcb_p->out_buff_p[i++] = *rs485_pcb_p->cmd_args_p++;
			rs485_pcb_p->pck_mode = CMD_P2P;
			break;
		}
		
		
		case FILE_UPDATE_ALL_IMAGE_TO_SELECTED:
		{
			if(rs485_pcb_p->cmd_rtn == CMD_IDLE)
			{
				if (f_mount(&filesystem, "", 1) == FR_OK)
				{
					i = 0;
					while(i < 16) c_buff[i++] = NULL;					// clear buffer		
					Int2Str((uint8_t *)c_buff, rs485_pcb_p->rec_addr);	// make image prefix
					i = strlen(c_buff);									// find end of string
					strncat(&c_buff[i], "*.RAW", 5);					// append text to form search string
					find_file_by_type(f_buff, c_buff);					// search disk for all files matching
						
					if(strlen(f_buff) != 0) 							// is any file find
					{	
						cp = strstr(f_buff, ".RAW");					// count number of files found
						rs485_pcb_p->cmd_cycle = 1;
						
						while(cp != NULL)
						{
							cp = strstr(cp + 1, ".RAW");
							++rs485_pcb_p->cmd_cycle;
						}
						
						i = 0;
						cp = strstr(f_buff, ".RAW");
						
						while(i < rs485_pcb_p->cmd_cycle)
						{
							cp = strstr(cp + 1, ".RAW");
							i++;
						}
							
						if (f_open(&file_SD, cp, FA_READ) == FR_OK) // open first file
						{	
							rs485_pcb_p->out_buff_p[5] = 0x03;
							cp = strchr(f_buff, '_');					// search for file sufix
							i = atoi(cp +1);							// convert to image number
							
							if(i == 1) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_1;
							else if(i == 2) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_2;
							else if(i == 3) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_3;
							else if(i == 4) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_4;
							else if(i == 5) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_5;
							else if(i == 6) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_6;
							else if(i == 7) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_7;
							else if(i == 8) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_8;
							else if(i == 9) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_9;
							else if(i == 10) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_10;
							else if(i == 11) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_11;
							else if(i == 12) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_12;
							else if(i == 13) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_13;
							else if(i == 14) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_14;
							else if(i == 15) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_15;
							else if(i == 16) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_16;
							else if(i == 17) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_17;
							else if(i == 18) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_18;
							else if(i == 19) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_19;
							
							rs485_pcb_p->pck_total = file_SD.obj.objsize / RUBICON_PACKET_BUFFER_SIZE;
							
							if ((rs485_pcb_p->pck_total * RUBICON_PACKET_BUFFER_SIZE) < file_SD.obj.objsize)
							{
								++rs485_pcb_p->pck_total;
							}
							
							rs485_pcb_p->pck_mode = CMD_P2P;
							rs485_pcb_p->out_buff_p[7] = rs485_pcb_p->pck_total >> 8;
							rs485_pcb_p->out_buff_p[8] = rs485_pcb_p->pck_total & 0xff;
							rs485_pcb_p->flag = rs485_pcb_p->cmd_cycle;
							rs485_pcb_p->tmr_preset = RUBICON_RESPONSE_TIMEOUT;
						}
						else
						{
							f_mount(NULL,"",0);
							rs485_pcb_p->cmd = CMD_IDLE;
							rs485_pcb_p->cmd_state = CMD_IDLE;
							rs485_pcb_p->cmd_rtn = FILE_ERROR;
							return;
						}
					}
					else
					{
						rs485_pcb_p->cmd = CMD_IDLE;
						rs485_pcb_p->cmd_state = CMD_IDLE;
						rs485_pcb_p->cmd_rtn = FILE_SYS_ERROR;
						return;
					}
				}
				else
				{
					rs485_pcb_p->cmd = CMD_IDLE;
					rs485_pcb_p->cmd_state = CMD_IDLE;
					rs485_pcb_p->cmd_rtn = FILE_SYS_ERROR;
					return;
				}							
			}
			else if(rs485_pcb_p->cmd_rtn == CMD_OK)
			{
				if(rs485_pcb_p->cmd_cycle != rs485_pcb_p->flag)
				{
					if (f_mount(&filesystem, "", 1) == FR_OK)
					{
						i = 0;
						while(i < 16) c_buff[i++] = NULL;					// clear buffer		
						Int2Str((uint8_t *)c_buff, rs485_pcb_p->rec_addr);	// make image prefix
						i = strlen(c_buff);									// find end of string
						strncat(&c_buff[i], "*.RAW", 5);					// append text to form search string
						find_file_by_type(f_buff, c_buff);					// search disk for all files matching
							
						if(strlen(f_buff) != 0) 							// is any file find
						{	
							i = 0;											// find next file
							cp = strstr(f_buff, ".RAW");
							
							while(i < rs485_pcb_p->cmd_cycle)
							{
								cp = strstr(cp + 1, ".RAW");
								i++;
							}
								
							if (f_open(&file_SD, cp, FA_READ) == FR_OK) // open first file
							{	
								rs485_pcb_p->out_buff_p[5] = 0x03;
								cp = strchr(f_buff, '_');					// search for file sufix
								i = atoi(cp +1);							// convert to image number
								
								if(i == 1) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_1;
								else if(i == 2) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_2;
								else if(i == 3) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_3;
								else if(i == 4) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_4;
								else if(i == 5) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_5;
								else if(i == 6) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_6;
								else if(i == 7) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_7;
								else if(i == 8) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_8;
								else if(i == 9) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_9;
								else if(i == 10) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_10;
								else if(i == 11) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_11;
								else if(i == 12) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_12;
								else if(i == 13) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_13;
								else if(i == 14) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_14;
								else if(i == 15) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_15;
								else if(i == 16) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_16;
								else if(i == 17) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_17;
								else if(i == 18) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_18;
								else if(i == 19) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_19;
								
								rs485_pcb_p->pck_total = file_SD.obj.objsize / RUBICON_PACKET_BUFFER_SIZE;
								
								if ((rs485_pcb_p->pck_total * RUBICON_PACKET_BUFFER_SIZE) < file_SD.obj.objsize)
								{
									++rs485_pcb_p->pck_total;
								}
								
								rs485_pcb_p->pck_mode = CMD_P2P;
								rs485_pcb_p->out_buff_p[7] = rs485_pcb_p->pck_total >> 8;
								rs485_pcb_p->out_buff_p[8] = rs485_pcb_p->pck_total & 0xff;
								rs485_pcb_p->flag = rs485_pcb_p->cmd_cycle;
								rs485_pcb_p->tmr_preset = RUBICON_RESPONSE_TIMEOUT;
							}
							else
							{
								f_mount(NULL,"",0);
								rs485_pcb_p->cmd = CMD_IDLE;
								rs485_pcb_p->cmd_state = CMD_IDLE;
								rs485_pcb_p->cmd_rtn = FILE_ERROR;
								return;
							}
						}
						else
						{
							rs485_pcb_p->cmd = CMD_IDLE;
							rs485_pcb_p->cmd_state = CMD_IDLE;
							rs485_pcb_p->cmd_rtn = FILE_SYS_ERROR;
							return;
						}
					}
					else
					{
						rs485_pcb_p->cmd = CMD_IDLE;
						rs485_pcb_p->cmd_state = CMD_IDLE;
						rs485_pcb_p->cmd_rtn = FILE_SYS_ERROR;
						return;
					}	
				}
				else
				{
					rs485_pcb_p->out_buff_p[0] = RUBICON_STX;
					rs485_pcb_p->out_buff_p[5] = 0x42;
					rs485_pcb_p->out_buff_p[6] = rs485_pcb_p->pck_sent >> 8;
					rs485_pcb_p->out_buff_p[7] = rs485_pcb_p->pck_sent & 0xff;						
					f_read(&file_SD, (uint8_t*) &rs485_pcb_p->out_buff_p[8], RUBICON_PACKET_BUFFER_SIZE, (UINT*) (&index));		
					rs485_pcb_p->pck_sent = 0;
					rs485_pcb_p->cmd_rtn = CMD_IDLE;
					rs485_pcb_p->pck_mode = CMD_P2P;	
					rs485_pcb_p->cmd_attempt = RUBICON_MAX_ERRORS;
					rs485_pcb_p->tmr_preset = RUBICON_RESPONSE_TIMEOUT;
					RUBICON_StartRxTxTimeoutTimer(RUBICON_READY_FOR_FILE_TIMEOUT);						
				}	
			}
			break;
		}
		
		
		case FILE_UPDATE_ALL_IMAGE_TO_ALL_IN_RANGE:
		{
			
			break;
		}
		
		
		case FILE_UPDATE_FROM_CONFIG_FILE:
		{
			
			break;
		}
		
		
		default:
		{
			if((rs485_pcb_p->cmd >= FILE_UPDATE_IMAGE_1) && (rs485_pcb_p->cmd <= FILE_UPDATE_IMAGE_19))
			{
				if(rs485_pcb_p->cmd_rtn == CMD_IDLE)
				{
					if (f_mount(&filesystem, "", 1) == FR_OK)
					{
						i = 0;
						index = 0;
						while(i < 16) c_buff[i++] = NULL;					
						Int2Str((uint8_t *)c_buff, rs485_pcb_p->rec_addr);
						i = strlen(c_buff);
						c_buff[i++] = '_';
						c_buff[i] = NULL;
						Int2Str((uint8_t *)&c_buff[i], ((rs485_pcb_p->cmd + 1) - FILE_UPDATE_IMAGE_1));
						i = strlen(c_buff);
						strncat(&c_buff[i], ".RAW", 4);
						
						if (f_open(&file_SD, c_buff, FA_READ) == FR_OK) 
						{
							rs485_pcb_p->pck_total = file_SD.obj.objsize / RUBICON_PACKET_BUFFER_SIZE;	
							
							if ((rs485_pcb_p->pck_total * RUBICON_PACKET_BUFFER_SIZE) < file_SD.obj.objsize)
							{
								++rs485_pcb_p->pck_total;
							}							
							
							rs485_pcb_p->out_buff_p[5] = 0x03;
							
							if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_1) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_1;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_2) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_2;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_3) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_3;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_4 )rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_4;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_5) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_5;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_6) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_6;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_7) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_7;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_8) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_8;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_9) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_9;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_10) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_10;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_11) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_11;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_12) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_12;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_13) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_13;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_14) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_14;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_15) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_15;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_16) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_16;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_17) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_17;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_18) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_18;
							else if(rs485_pcb_p->cmd == FILE_UPDATE_IMAGE_19) rs485_pcb_p->out_buff_p[6] = RUBICON_DOWNLOAD_DISPLAY_IMAGE_19;
							
							rs485_pcb_p->out_buff_p[7] = rs485_pcb_p->pck_total >> 8;
							rs485_pcb_p->out_buff_p[8] = rs485_pcb_p->pck_total & 0xff;
							rs485_pcb_p->tmr_preset = RUBICON_READY_FOR_FILE_TIMEOUT;
							rs485_pcb_p->pck_mode = CMD_P2P;
						}
						else
						{
							f_mount(NULL,"",0);
							rs485_pcb_p->cmd = CMD_IDLE;
							rs485_pcb_p->cmd_state = CMD_IDLE;
							rs485_pcb_p->cmd_rtn = FILE_ERROR;
							return;
						}
					}
					else
					{
						rs485_pcb_p->cmd = CMD_IDLE;
						rs485_pcb_p->cmd_state = CMD_IDLE;
						rs485_pcb_p->cmd_rtn = FILE_SYS_ERROR;
						return;
					}							
				}
				else if(rs485_pcb_p->cmd_rtn == CMD_OK)
				{
					rs485_pcb_p->out_buff_p[0] = RUBICON_STX;
					rs485_pcb_p->out_buff_p[5] = 0x42;
					rs485_pcb_p->out_buff_p[6] = rs485_pcb_p->pck_sent >> 8;
					rs485_pcb_p->out_buff_p[7] = rs485_pcb_p->pck_sent & 0xff;						
					f_read(&file_SD, (uint8_t*) &rs485_pcb_p->out_buff_p[8], RUBICON_PACKET_BUFFER_SIZE, (UINT*) (&index));		
					rs485_pcb_p->pck_sent = 0;
					rs485_pcb_p->pck_mode = CMD_P2P;
					rs485_pcb_p->cmd_rtn = CMD_IDLE;					
					rs485_pcb_p->cmd_attempt = RUBICON_MAX_ERRORS;
					rs485_pcb_p->tmr_preset = RUBICON_RESPONSE_TIMEOUT;	
				}				
			}
			break;
		}
	}

	rs485_packet_checksum = 0;

    for (i = 6; i < (rs485_pcb_p->out_buff_p[5] + 6); i++)
    {
        rs485_packet_checksum += rs485_pcb_p->out_buff_p[i];
    }

    rs485_pcb_p->out_buff_p[rs485_pcb_p->out_buff_p[5] + 6] = rs485_packet_checksum >> 8;
    rs485_pcb_p->out_buff_p[rs485_pcb_p->out_buff_p[5] + 7] = rs485_packet_checksum & 0xff;
    rs485_pcb_p->out_buff_p[rs485_pcb_p->out_buff_p[5] + 8] = RUBICON_EOT;
	rs485_pcb_p->cmd_state = CMD_SEND;	
}


void RUBICON_SystemService(void)
{
	uint8_t ba;
	
	if((rs485_pcb_p->cmd_state == CMD_IDLE) && (rubicon_cmd_request == NULL) && (rubicon_http_cmd_state != NULL))	
	{
		rubicon_cmd_request = rubicon_http_cmd_state;
		rubicon_http_cmd_state = NULL;
	}
	/*
	**********************************************************************************
	*
	*	on external request initialize packet control block structure calling
	*	command parser function. after valid command request data transfer
	*	is started in one of three transfer mode: "peer-to-peer" with acknowledge 
	*	for every valid paket transfer, "group" without response from receiver and 
	*	broadcast also without response from receivers. when parsed commad request
	*	value of packet total is initialized to 1 and command cyce also to 1 for 
	*	command completed in single pass through state machine. when value packet 
	*	total is initialized to more than 1 and command cycle is with 1, command
	*	is executed in multi loop
	*
	***********************************************************************************
	*/
	if((rs485_pcb_p->cmd_state == CMD_IDLE) && (rubicon_cmd_request != NULL))	
	{
		rs485_pcb_p->cmd = rubicon_cmd_request;
		rs485_pcb_p->cmd_rtn = CMD_IDLE;
		rs485_pcb_p->cmd_state = CMD_REQUEST;
		rs485_pcb_p->cmd_args_p = (char *)rubicon_ctrl_buffer;
		rs485_pcb_p->cmd_attempt = RUBICON_MAX_ERRORS;
		rs485_pcb_p->cmd_cycle = 1;
		rs485_pcb_p->pck_sent = 0;
		rs485_pcb_p->pck_total = 1;
		rs485_pcb_p->in_buff_p = rx_buffer;
		rs485_pcb_p->flag = 0;
		rs485_pcb_p->out_buff_p = tx_buffer;
		rs485_pcb_p->byte_cnt = 0;
		rs485_pcb_p->addr_index = 0;
		
		if(IS_09(*rs485_pcb_p->cmd_args_p))
		{
			rs485_pcb_p->rec_addr = atoi(rs485_pcb_p->cmd_args_p);
		}
		else
		{
			rs485_pcb_p->rec_addr = rs485_rubicon_address;
		}
				
		rubicon_cmd_request = NULL;		
		RUBICON_CmdParse();			
//		p_rubicon_buffer = rubicon_ctrl_buffer;
//		while (p_rubicon_buffer< rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_rubicon_buffer++ = NULL;
		
		if(rs485_pcb_p->cmd_rtn == CMD_INVALID_ERROR)
		{
			rs485_pcb_p->cmd = CMD_IDLE;
			rs485_pcb_p->cmd_state = CMD_IDLE;
		}
	}
	else if((rs485_pcb_p->cmd_state == CMD_REQUEST) && (rs485_pcb_p->cmd != CMD_IDLE))
	{
		rs485_pcb_p->cmd_args_p = (const char *)rubicon_ctrl_buffer;
		rs485_pcb_p->in_buff_p = rx_buffer;
		rs485_pcb_p->out_buff_p = tx_buffer;
		
		RUBICON_CmdParse();
//		p_rubicon_buffer = rubicon_ctrl_buffer;
//		while (p_rubicon_buffer< rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_rubicon_buffer++ = NULL;
	
		if(rs485_pcb_p->cmd_rtn == CMD_INVALID_ERROR)
		{
			rs485_pcb_p->cmd = CMD_IDLE;
			rs485_pcb_p->cmd_state = CMD_IDLE;
		}
	}
	else if((rs485_pcb_p->cmd_state == CMD_SEND) && IsRUBICON_RxTxTimeoutTimerExpired())
	{
		rs485_rx_cnt = 0;
		p_comm_buffer = rx_buffer;
		while(p_comm_buffer < rx_buffer + sizeof(rx_buffer)) *p_comm_buffer++ = NULL;				
		RS485_Send_Data(tx_buffer, (tx_buffer[5] + 9));
		rs485_pcb_p->cmd_state = CMD_RECEIVE;
		RUBICON_StartRxTxTimeoutTimer(rs485_pcb_p->tmr_preset);
	}
	else if(rs485_pcb_p->cmd_state == CMD_RECEIVE)
	{
		if(((rx_buffer[1] == (rs485_interface_address >> 8)) && \
			(rx_buffer[2] == (rs485_interface_address & 0xff))) && \
			((rx_buffer[3] == (rs485_pcb_p->rec_addr >> 8)) && \
			(rx_buffer[4] == (rs485_pcb_p->rec_addr & 0xff))) && \
			(rx_buffer[rx_buffer[5] + 8] == RUBICON_EOT))
		{
			rs485_packet_checksum = 0;
			for (ba = 6; ba < (rx_buffer[5] + 6); ba++) rs485_packet_checksum += rx_buffer[ba];

			if ((rx_buffer[rx_buffer[5] + 6] == (rs485_packet_checksum >> 8)) && \
				(rx_buffer[rx_buffer[5] + 7] == (rs485_packet_checksum & 0xff)))
			{
				RUBICON_StopRxTxTimeoutTimer();
				
				if(++rs485_pcb_p->pck_sent == rs485_pcb_p->pck_total)
				{
					rs485_pcb_p->cmd_rtn = CMD_OK;
					rs485_pcb_p->cmd_state = CMD_COMPLETED;
				}
				else
				{
					rs485_pcb_p->cmd_rtn = CMD_OK;
					rs485_pcb_p->cmd_state = CMD_REQUEST;
				}
			}
		}
		else if(IsRUBICON_RxTxTimeoutTimerExpired())
		{
			if(--rs485_pcb_p->cmd_attempt == 0)
			{
				if(--rs485_pcb_p->cmd_cycle == 0)
				{
					f_close(&file_SD);
					f_mount(NULL,"",0);
					rs485_pcb_p->cmd = CMD_IDLE;
					rs485_pcb_p->cmd_rtn = CMD_MAX_ATTEMPT_ERROR;
					rs485_pcb_p->cmd_state = CMD_IDLE;
				}
				else
				{
					rs485_pcb_p->cmd_rtn = CMD_MAX_ATTEMPT_ERROR;
					rs485_pcb_p->cmd_state = CMD_REQUEST;
				}
			}
			else
			{
				rs485_pcb_p->cmd_rtn = CMD_TIMEOUT_ERROR;
				rs485_pcb_p->cmd_state = CMD_REQUEST;
			}
		}
	}
	else if(rs485_pcb_p->cmd_state == CMD_COMPLETED)
	{
		//RUBICON_StartRxTxTimeoutTimer(RUBICON_RX_TO_TX_DELAY);
		if(--rs485_pcb_p->cmd_cycle == 0)
		{
			f_close(&file_SD);
			f_mount(NULL,"",0);
			rs485_pcb_p->cmd = CMD_IDLE;
			rs485_pcb_p->cmd_rtn = CMD_COMPLETED;
			rs485_pcb_p->cmd_state = CMD_IDLE;
		}
		else
		{
			rs485_pcb_p->cmd_rtn = CMD_OK;
			rs485_pcb_p->cmd_state = CMD_REQUEST;
		}
	}
	else if (IsRUBICON_TimerExpired())
	{
		rubicon_cmd_request = RUBICON_SET_RTC_DATE_TIME;
		rs485_rubicon_address = RUBICON_GetNextAddress();
		rs485_pcb_p->tmr_preset = RUBICON_RX_TO_TX_DELAY;		
		RUBICON_StartTimer(RUBICON_TIME_UPDATE_PERIOD);
	}
	else if(IsRUBICON_RxTxTimeoutTimerExpired())
	{
		rubicon_cmd_request = RUBICON_GET_SYS_STATUS;
		rs485_rubicon_address = RUBICON_GetNextAddress();
		rs485_pcb_p->tmr_preset = RUBICON_RESPONSE_TIMEOUT;
	}
}




