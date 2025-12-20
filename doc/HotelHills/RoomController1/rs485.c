/**
 ******************************************************************************
 * File Name          : rs485.c
 * Date               : 28/02/2016 23:16:19
 * Description        : rs485 communication modul
 ******************************************************************************
 *
 *
 ******************************************************************************
 */
 
/* Includes ------------------------------------------------------------------*/
#include "rs485.h"
#include "eeprom.h"
#include "dio_interface.h"
#include "tft_lcd.h"
#include "tft_lcd_font.h"

/* Imported variables ---------------------------------------------------------*/
extern UART_HandleTypeDef huart1;
extern RTC_HandleTypeDef hrtc;
extern RTC_TimeTypeDef time;
extern RTC_DateTypeDef date;

/* Typedef -------------------------------------------------------------------*/
eTransferModeTypeDef eTransferMode;
eComStateTypeDef eComState;

/* Define --------------------------------------------------------------------*/
/* Macro ---------------------------------------------------------------------*/
/* Variables -----------------------------------------------------------------*/
volatile uint16_t received_byte_cnt;
volatile uint8_t packet_type;
volatile uint8_t receive_pcnt;
volatile uint32_t rs485_timer;
volatile uint32_t rs485_flags;
volatile uint8_t receiving_errors;
volatile uint16_t rs485_sender_address;
volatile uint8_t rs485_packet_data_lenght;
volatile uint16_t rs485_packet_data_checksum;
volatile uint8_t rec_byte[2];

uint8_t rs485_interface_address[2];
uint8_t rs485_group_address[2];
uint8_t rs485_broadcast_address[2];

uint8_t aString[10];
uint8_t aCommBuffer[256];
uint8_t *p_comm_buffer;
uint16_t next_packet_number;
uint16_t total_packet_number;
uint32_t flash_destination;
uint32_t flash_protection;
uint8_t activ_command;

/* Private macros   ----------------------------------------------------------*/
#define RS485_ResponsePacketReady()			(rs485_flags |= 0x00000002)
#define RS485_NoResponse()					(rs485_flags &= 0xfffffffd)
#define RS485_IsResponsePacketPending()		(rs485_flags & 0x00000002)

/* Private function prototypes -----------------------------------------------*/
uint16_t CalcChecksum(const uint8_t *p_data, uint32_t size);

/* Code  ---------------------------------------------------------------------*/
void RS485_Init(void)
{ 
	eTransferMode = TRANSFER_IDLE;
	eComState = COM_INIT;
    /**
    *   load rs485 addresse
    */
#ifndef LOAD_DEFAULT		
    if(HAL_I2C_Mem_Read(&hi2c1, EE_READ_CMD, EE_RS485_INTERFACE_ADDRESS, I2C_MEMADD_SIZE_16BIT, rs485_interface_address, 2, 10) != HAL_OK) eComState = COM_ERROR;
    if(HAL_I2C_Mem_Read(&hi2c1, EE_READ_CMD, EE_RS485_GROUP_ADDRESS, I2C_MEMADD_SIZE_16BIT, rs485_group_address, 2, 10) != HAL_OK) eComState = COM_ERROR;;
	if(HAL_I2C_Mem_Read(&hi2c1, EE_READ_CMD, EE_RS485_BROADCAST_ADDRESS, I2C_MEMADD_SIZE_16BIT, rs485_broadcast_address, 2, 10) != HAL_OK) eComState = COM_ERROR;;
#endif
	
#ifdef LOAD_DEFAULT	
	rs485_interface_address[0] = (RS485_DEFAULT_INTERFACE_ADDRESS >> 8);
    rs485_interface_address[1] = (RS485_DEFAULT_INTERFACE_ADDRESS & 0xff);
	rs485_group_address[0] = (RS485_DEFFAULT_GROUP_ADDRESS >> 8);
    rs485_group_address[1] = (RS485_DEFFAULT_GROUP_ADDRESS & 0xff);
	rs485_broadcast_address[0] = (RS485_DEFFAULT_BROADCAST_ADDRESS >> 8);
    rs485_broadcast_address[1] = (RS485_DEFFAULT_BROADCAST_ADDRESS & 0xff);
#endif
	/**
    *   clear data buffer
    */
    p_comm_buffer = aCommBuffer;
	while(p_comm_buffer < aCommBuffer + sizeof(aCommBuffer)) *p_comm_buffer++ = NULL;
    /**
    *   start usart receiving in interrupt mode
    *   to get packet header for address checking
    */
    RS485_DirRx();
	packet_type = NULL;
	receive_pcnt = 0;
	received_byte_cnt = 0;
	activ_command = NULL;
	
	if((HAL_UART_Receive_IT(&huart1, aCommBuffer, sizeof(aCommBuffer)) == HAL_OK) \
		&& (eTransferMode == TRANSFER_IDLE)) 
	{
		eComState = COM_PACKET_PENDING;
	}
}

void RS485_Service(void)
{
	switch(eComState)
	{
		case COM_INIT:
			
			RS485_Init();
			break;
		
		
		case COM_PACKET_PENDING:
			break;
				
			
		case COM_PACKET_RECEIVED:
			
			if(packet_type == SOH)
			{
				if(activ_command == NULL) activ_command = aCommBuffer[0];
			}
			else if(packet_type == STX)
			{
				
			}
			
			switch(activ_command)
			{
				case DOWNLOAD_DISPLAY_IMAGE_1:
					break;
				
				
				case DOWNLOAD_DISPLAY_IMAGE_2:
					break;
				
				
				case DOWNLOAD_DISPLAY_IMAGE_3:
					break;
				
				
				case DOWNLOAD_DISPLAY_IMAGE_4:
					break;
				
				
				case DOWNLOAD_DISPLAY_IMAGE_5:
					break;
				
				
				case DOWNLOAD_DISPLAY_IMAGE_6:
					break;
				
				
				case DOWNLOAD_DISPLAY_IMAGE_7:
					break;
				
				
				case DOWNLOAD_DISPLAY_IMAGE_8:
					break;
				
				
				case DOWNLOAD_DISPLAY_IMAGE_9:
					break;
				
				
				case DOWNLOAD_DISPLAY_IMAGE_10:
					break;
				
				
				case DOWNLOAD_SMALL_FONTS:
					break;
				
				
				case DOWNLOAD_MIDDLE_FONTS:
					break;
				
				
				case DOWNLOAD_BIG_FONTS:
					break;
				
				
				case DOWNLOAD_TEXT_DATE_TIME:
					break;
				
				
				case DOWNLOAD_TEXT_EVENTS:
					break;
				
				
				case START_BOOTLOADER:
					HAL_FLASH_OB_Launch();
					break;
				
				
				case GET_SYS_STATUS:
					break;
				
				
				case GET_SYS_INFO:
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 16;
						HAL_I2C_Mem_Read(&hi2c1, EE_READ_CMD, EE_SYS_INFO, I2C_MEMADD_SIZE_16BIT, &aCommBuffer[6], 16, EE_I2C_TIMEOUT);
						RS485_ResponsePacketReady();
					}	
					activ_command = NULL;
					break;
				
				
				case GET_SYS_CONFIG:
					break;
				
				
				case SET_SYS_CONFIG:
					break;
				
				
				case GET_RTC_DATE_TIME:
					
					if(eTransferMode == TRANSFER_P2P)
					{
						HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BCD);
						HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BCD);
						aCommBuffer[5] = 13;
						aCommBuffer[6] = GET_RTC_DATE_TIME;
						aCommBuffer[7] = (date.Date >> 4) + 48;
						aCommBuffer[8] = (date.Date & 0x0f) + 48;
						aCommBuffer[9] = (date.Month >> 4) + 48;
						aCommBuffer[10] = (date.Month & 0x0f) + 48;
						aCommBuffer[11] = (date.Year >> 4) + 48;
						aCommBuffer[12] = (date.Year & 0x0f) + 48;
						aCommBuffer[13] = (time.Hours >> 4) + 48;
						aCommBuffer[14] = (time.Hours & 0x0f) + 48;
						aCommBuffer[15] = (time.Minutes >> 4) + 48;
						aCommBuffer[16] = (time.Minutes & 0x0f) + 48;
						aCommBuffer[17] = (time.Seconds >> 4) + 48;
						aCommBuffer[18] = (time.Seconds & 0x0f) + 48;
						RS485_ResponsePacketReady();
					}	
					activ_command = NULL;				
					break;
				
					
				case SET_RTC_DATE_TIME:
					
					date.Date = (((aCommBuffer[1] - 48) << 4) + ((aCommBuffer[2] - 48) & 0x0f));
					date.Month = (((aCommBuffer[3] - 48) << 4) + ((aCommBuffer[4] - 48) & 0x0f));
					date.Year = (((aCommBuffer[5] - 48) << 4) + ((aCommBuffer[6] - 48) & 0x0f));
		
					time.Hours = (((aCommBuffer[7] - 48) << 4) + ((aCommBuffer[8] - 48) & 0x0f));
					time.Minutes = (((aCommBuffer[9] - 48) << 4) + ((aCommBuffer[10] - 48) & 0x0f));
					time.Seconds = (((aCommBuffer[11] - 48) << 4) + ((aCommBuffer[12] - 48) & 0x0f));
		
					HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BCD);
					HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BCD);
				
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_RTC_DATE_TIME;
						RS485_ResponsePacketReady();
					}
					activ_command = NULL;
					break;
				
				
				case GET_LOG_LIST:
					break;
				
				
				case DELETE_LOG_LIST:
					break;
				
				
				case GET_CLOCK_ALARM:
					break;
				
				
				case SET_CLOCK_ALARM:
					break;
				
				
				case GET_RESTART_COUNTER:
					break;
				
				
				case CLEAR_RESTART_COUNTER:
					break;
				
				
				case GET_RS485_CONFIG:
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 13;
						aCommBuffer[6] = GET_RS485_CONFIG;
						aCommBuffer[7] = (rs485_interface_address[0] >> 4) + 48;
						aCommBuffer[8] = (rs485_interface_address[0] & 0x0f) + 48;
						aCommBuffer[9] = (rs485_interface_address[1] >> 4) + 48;
						aCommBuffer[10] = (rs485_interface_address[1]& 0x0f) + 48;
						aCommBuffer[11] = (rs485_group_address[0] >> 4) + 48;
						aCommBuffer[12] = (rs485_group_address[0] & 0x0f) + 48;
						aCommBuffer[13] = (rs485_group_address[1] >> 4) + 48;
						aCommBuffer[14] = (rs485_group_address[1] & 0x0f) + 48;
						aCommBuffer[15] = (rs485_broadcast_address[0] >> 4) + 48;
						aCommBuffer[16] = (rs485_broadcast_address[0] & 0x0f) + 48;
						aCommBuffer[17] = (rs485_broadcast_address[1] >> 4) + 48;
						aCommBuffer[18] = (rs485_broadcast_address[1]& 0x0f) + 48;
						RS485_ResponsePacketReady();
					}
					activ_command = NULL;
					break;
				
				
				case SET_RS485_CONFIG:
					
					rs485_interface_address[0] = (((aCommBuffer[1] - 48) << 4) + ((aCommBuffer[2] - 48) & 0x0f));
					rs485_interface_address[1] = (((aCommBuffer[3] - 48) << 4) + ((aCommBuffer[4] - 48) & 0x0f));
					
					rs485_group_address[0] = (((aCommBuffer[5] - 48) << 4) + ((aCommBuffer[6] - 48) & 0x0f));
					rs485_group_address[1] = (((aCommBuffer[7] - 48) << 4) + ((aCommBuffer[8] - 48) & 0x0f));
				
					rs485_broadcast_address[0] = (((aCommBuffer[9] - 48) << 4) + ((aCommBuffer[10] - 48) & 0x0f));
					rs485_broadcast_address[1] = (((aCommBuffer[11] - 48) << 4) + ((aCommBuffer[12] - 48) & 0x0f));
				
					HAL_I2C_Mem_Write(&hi2c1, EE_WRITE_CMD, EE_RS485_INTERFACE_ADDRESS, I2C_MEMADD_SIZE_16BIT, rs485_interface_address, 2, EE_I2C_TIMEOUT);
					HAL_Delay(10);
				
					HAL_I2C_Mem_Write(&hi2c1, EE_WRITE_CMD, EE_RS485_GROUP_ADDRESS, I2C_MEMADD_SIZE_16BIT, rs485_interface_address, 2, EE_I2C_TIMEOUT);
					HAL_Delay(10);
				
					HAL_I2C_Mem_Write(&hi2c1, EE_WRITE_CMD, EE_RS485_BROADCAST_ADDRESS, I2C_MEMADD_SIZE_16BIT, rs485_interface_address, 2, EE_I2C_TIMEOUT);
					HAL_Delay(10);
				
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_RS485_CONFIG;
						RS485_ResponsePacketReady();
					}
					activ_command = NULL;
					break;
				
				
				case GET_MIFARE_CONFIG:
					break;
				
				
				case SET_MIFARE_CONFIG:
					break;
				
				
				case GET_MIFARE_PERMITED_GROUP:
					break;
				
				
				case SET_MIFARE_PERMITED_GROUP:
					break;	
				
				
				case GET_MIFARE_PERMITED_CARD:
					break;
				
				
				case SET_MIFARE_PERMITED_CARD:
					break;
				
				case GET_MIFARE_FORBIDEN_CARD:
					break;
				
				
				case SET_MIFARE_FORBIDEN_CARD:
					break;	
				
				
				case GET_MIFARE_COMMAND:
					break;
				
				
				case SET_MIFARE_COMMAND:
					break;
				
				
				case GET_MIFARE_CARD_DATA:
					break;
				
				
				case SET_MIFARE_CARD_DATA:
					break;
				
				
				case GET_DIN_STATE:
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 9;
						aCommBuffer[6] = GET_DIN_STATE;
						if(din_0_7 & 0x01) aCommBuffer[7] = '1';
						else aCommBuffer[7] = '0';
						if(din_0_7 & 0x02) aCommBuffer[8] = '1';
						else aCommBuffer[8] = '0';
						if(din_0_7 & 0x04) aCommBuffer[9] = '1';
						else aCommBuffer[9] = '0';
						if(din_0_7 & 0x08) aCommBuffer[10] = '1';
						else aCommBuffer[10] = '0';
						if(din_0_7 & 0x10) aCommBuffer[11] = '1';
						else aCommBuffer[11] = '0';
						if(din_0_7 & 0x20) aCommBuffer[12] = '1';
						else aCommBuffer[12] = '0';
						if(din_0_7 & 0x40) aCommBuffer[13] = '1';
						else aCommBuffer[13] = '0';
						if(din_0_7 & 0x80) aCommBuffer[14] = '1';
						else aCommBuffer[14] = '0';						
						RS485_ResponsePacketReady();					
					}	
					activ_command = NULL;
					break;

				case SET_DOUT_STATE:
					
					if(aCommBuffer[1] == '1') dout_0_7_remote |= 0x0100;
					else if (aCommBuffer[1] == '0') dout_0_7_remote &= 0xfeff;	
					if(aCommBuffer[2] == '1') dout_0_7_remote |= 0x0200;
					else if (aCommBuffer[2] == '0') dout_0_7_remote &= 0xfdff;
					if(aCommBuffer[3] == '1') dout_0_7_remote |= 0x0400;
					else if (aCommBuffer[3] == '0') dout_0_7_remote &= 0xfbff;
					if(aCommBuffer[4] == '1') dout_0_7_remote |= 0x0800;
					else if (aCommBuffer[4] == '0') dout_0_7_remote &= 0xf7ff;
					if(aCommBuffer[5] == '1') dout_0_7_remote |= 0x1000;
					else if (aCommBuffer[5] == '0') dout_0_7_remote &= 0xefff;
					if(aCommBuffer[6] == '1') dout_0_7_remote |= 0x2000;
					else if (aCommBuffer[6] == '0') dout_0_7_remote &= 0xdfff;
					if(aCommBuffer[7] == '1') dout_0_7_remote |= 0x4000;
					else if (aCommBuffer[7] == '0') dout_0_7_remote &= 0xbfff;
					if(aCommBuffer[8] == '1') dout_0_7_remote |= 0x8000;
					else if (aCommBuffer[8] == '0') dout_0_7_remote &= 0x7fff;
					
					if(aCommBuffer[9] == '1') dout_0_7_remote |= 0x0001;
					else if (aCommBuffer[9] == '0') dout_0_7_remote &= 0xfffe;
					if(aCommBuffer[10] == '1') dout_0_7_remote |= 0x0002;
					else if (aCommBuffer[10] == '0') dout_0_7_remote &= 0xfffd;
					if(aCommBuffer[11] == '1') dout_0_7_remote |= 0x0004;
					else if (aCommBuffer[11] == '0') dout_0_7_remote &= 0xfffb;
					if(aCommBuffer[12] == '1') dout_0_7_remote |= 0x0008;
					else if (aCommBuffer[12] == '0') dout_0_7_remote &= 0xfff7;
					if(aCommBuffer[13] == '1') dout_0_7_remote |= 0x0010;
					else if (aCommBuffer[13] == '0') dout_0_7_remote &= 0xffef;
					if(aCommBuffer[14] == '1') dout_0_7_remote |= 0x0020;
					else if (aCommBuffer[14] == '0') dout_0_7_remote &= 0xffdf;
					if(aCommBuffer[15] == '1') dout_0_7_remote |= 0x0040;
					else if (aCommBuffer[15] == '0') dout_0_7_remote &= 0xffbf;
					if(aCommBuffer[16] == '1') dout_0_7_remote |= 0x0080;
					else if (aCommBuffer[16] == '0') dout_0_7_remote &= 0xff7f;
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_DOUT_STATE;
						RS485_ResponsePacketReady();						
					}
					activ_command = NULL;
					break;

				case GET_DOUT_STATE:
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 9;
						aCommBuffer[6] = GET_DOUT_STATE;
						if(dout_0_7 & 0x01) aCommBuffer[7] = '1';
						else aCommBuffer[7] = '0';
						if(dout_0_7 & 0x02) aCommBuffer[8] = '1';
						else aCommBuffer[8] = '0';
						if(dout_0_7 & 0x04) aCommBuffer[9] = '1';
						else aCommBuffer[9] = '0';
						if(dout_0_7 & 0x08) aCommBuffer[10] = '1';
						else aCommBuffer[10] = '0';
						if(dout_0_7 & 0x10) aCommBuffer[11] = '1';
						else aCommBuffer[11] = '0';
						if(dout_0_7 & 0x20) aCommBuffer[12] = '1';
						else aCommBuffer[12] = '0';
						if(dout_0_7 & 0x40) aCommBuffer[13] = '1';
						else aCommBuffer[13] = '0';
						if(dout_0_7 & 0x80) aCommBuffer[14] = '1';
						else aCommBuffer[14] = '0';						
						RS485_ResponsePacketReady();						
					}
					activ_command = NULL;
					break;
				
				default:
					break;
				
			}// End of switch aComBuffer[0] - command function	

			if(RS485_IsResponsePacketPending())
			{
				aCommBuffer[0] = ACK;
				aCommBuffer[1] = rs485_sender_address >> 8;
				aCommBuffer[2] = rs485_sender_address;
				aCommBuffer[3] = rs485_interface_address[0];
				aCommBuffer[4] = rs485_interface_address[1];
				
				rs485_packet_data_checksum = 0;
				
				for(uint8_t i = 6; i < (aCommBuffer[5] + 6); i++)
				{
					rs485_packet_data_checksum += aCommBuffer[i];
				}
				aCommBuffer[aCommBuffer[5] + 6] = rs485_packet_data_checksum >> 8;
				aCommBuffer[aCommBuffer[5] + 7] = rs485_packet_data_checksum;
				aCommBuffer[aCommBuffer[5] + 8] = EOT;
				
				RS485_DirTx();
				if(HAL_UART_Transmit(&huart1, aCommBuffer, (aCommBuffer[5] + 9), PACKET_TRANSFER_TIMEOUT) != HAL_OK)
				{
					
				}
				while(HAL_UART_GetState(&huart1) != HAL_UART_STATE_READY) continue;
				RS485_NoResponse();
				RS485_DirRx();
				
				p_comm_buffer = aCommBuffer;
				while(p_comm_buffer < aCommBuffer + sizeof(aCommBuffer)) *p_comm_buffer++ = NULL;
				/**
				*   start usart receiving in interrupt mode
				*   to get packet header for address checking
				*/
				packet_type = NULL;
				receive_pcnt = 0;
				received_byte_cnt = 0;
				activ_command = NULL;
				eTransferMode = TRANSFER_IDLE;
				
				if(HAL_UART_Receive_IT(&huart1, aCommBuffer, sizeof(aCommBuffer)) == HAL_OK) eComState = COM_PACKET_PENDING;
				else eComState = COM_INIT;
			}
			break;
		
		case COM_RECEIVE_SUSPEND:
			
			if(IsRS485_TimerExpired())
			{
				RS485_DirRx();
				packet_type = NULL;
				receive_pcnt = 0;
				received_byte_cnt = 0;
				
				if(HAL_UART_Receive_IT(&huart1, aCommBuffer, sizeof(aCommBuffer)) == HAL_OK) eComState = COM_PACKET_PENDING;
				else  eComState = COM_INIT;
			}
			break;
		
		case COM_ERROR:
			break;
		
		default:
			break;
	}// End of eComState switch
}// End of command parser function

uint16_t CalcChecksum(const uint8_t *p_data, uint32_t size)
{  
    uint16_t sum = 0;
    const uint8_t *p_data_end = p_data + size;

    while (p_data < p_data_end ) sum += *p_data++;

    return (sum);
    
}// End of calculating chechsum function

/* Public functions ---------------------------------------------------------*/
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) 
{
    __HAL_UART_CLEAR_PEFLAG(&huart1);
    __HAL_UART_CLEAR_FEFLAG(&huart1);
    __HAL_UART_CLEAR_NEFLAG(&huart1);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    __HAL_UART_CLEAR_OREFLAG(&huart1);
	__HAL_UART_FLUSH_DRREGISTER(&huart1);
	__HAL_UNLOCK(&huart1);
	huart1.State = HAL_UART_STATE_READY;
    ++receiving_errors;
	eComState = COM_INIT;
    
}// End of hal usart error calback function
/******************************   END OF FILE  **********************************/
