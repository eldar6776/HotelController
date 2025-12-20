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
#include "rc522.h"
#include "eeprom.h"
#include "common.h"
#include "logger.h"
#include "dio_interface.h"
#include "tft_lcd.h"
#include "tft_lcd_font.h"
#include "display.h"
#include "one_wire.h"
#include "signal.h"

/* Imported variables ---------------------------------------------------------*/
extern const char sys_info[];
extern uint8_t aMifareAuthenticationKeyA[]; 
extern uint8_t aMifareAuthenticationKeyB[];
extern uint8_t aRoomPowerExpiryDateTime[];
extern UART_HandleTypeDef huart1;
extern RTC_HandleTypeDef hrtc;
extern RTC_TimeTypeDef time;
extern RTC_DateTypeDef date;
extern TIM_HandleTypeDef htim4;
extern pFunction JumpToBootloader;
extern uint8_t fw_update_status;
extern uint8_t sys_status;
extern uint16_t TemperatureC;
extern uint16_t AD_value;
extern uint16_t V25;// when V25=1.41V at ref 3.3V
extern float Avg_Slope; //when avg_slope=4.3mV/C at ref 3.3V
extern uint16_t lcd_brightness;
extern uint16_t temperature_measured;
extern uint8_t temperature_setpoint;
extern uint8_t temperature_difference;

/* Typedef -------------------------------------------------------------------*/
eTransferModeTypeDef eTransferMode;
eComStateTypeDef eComState = COM_INIT;

/* Define --------------------------------------------------------------------*/
/* Macro ---------------------------------------------------------------------*/
/* Variables -----------------------------------------------------------------*/
volatile uint16_t received_byte_cnt;
volatile uint8_t packet_type;
volatile uint16_t receive_pcnt;
volatile uint32_t rs485_timer;
volatile uint32_t rs485_flags;
volatile uint32_t rs485_update_timeout_timer;
volatile uint16_t receiving_errors;
volatile uint16_t rs485_sender_address;
volatile uint16_t rs485_packet_data_lenght;
volatile uint16_t rs485_packet_data_checksum;
volatile uint8_t activ_command;

uint8_t aCommBuffer[COM_BUFFER_SIZE];
uint8_t *p_comm_buffer;
uint8_t aString[10];
uint8_t rs485_interface_address[2];
uint8_t rs485_group_address[2];
uint8_t rs485_broadcast_address[2];
uint8_t aRoomPowerExpiryDateTime[6];

uint16_t next_packet_number;
uint16_t total_packet_number;
uint32_t flash_destination;
uint32_t flash_protection;
uint32_t display_brightnes;

/* Private macros   ----------------------------------------------------------*/
#define RS485_ResponsePacketReady()			(rs485_flags |= 0x00000002)
#define RS485_NoResponse()					(rs485_flags &= 0xfffffffd)
#define IsRS485_ResponsePacketPending()		(rs485_flags & 0x00000002)

/* Private function prototypes -----------------------------------------------*/
extern void BootloaderExe(void);
extern void MX_TIM4_Init(void);

uint16_t CalcChecksum(const uint8_t *p_data, uint32_t size);
void PrepareForDowloadImage(uint32_t start_address, uint8_t number_of_blocks);

/* Code  ---------------------------------------------------------------------*/
void RS485_Init(void)
{
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
	eTransferMode = TRANSFER_IDLE;
	
	if (huart1.State == HAL_UART_STATE_BUSY_RX)
	{
		__HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
		huart1.State = HAL_UART_STATE_READY;
	}

	if(HAL_UART_Receive_IT(&huart1, aCommBuffer, sizeof(aCommBuffer)) == HAL_OK)
	{
		eComState = COM_PACKET_PENDING;
	}
	else
	{
		RS485_StartTimer(RECEIVER_REINIT_TIMEOUT);
	}
}

void RS485_Service(void)
{
	uint16_t temperature;
	
	if(eComState == COM_PACKET_RECEIVED)
	{
		if(packet_type == SOH)
		{ 
			switch(aCommBuffer[0])
			{
				case DOWNLOAD_DISPLAY_IMAGE_1:
				{
					PrepareForDowloadImage(EE_IMAGE_1_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_2:
				{
					PrepareForDowloadImage(EE_IMAGE_2_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_3:
				{
					PrepareForDowloadImage(EE_IMAGE_3_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_4:
				{
					PrepareForDowloadImage(EE_IMAGE_4_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_5:
				{
					PrepareForDowloadImage(EE_IMAGE_5_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_6:
				{
					PrepareForDowloadImage(EE_IMAGE_6_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_7:
				{
					PrepareForDowloadImage(EE_IMAGE_7_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_8:
				{
					PrepareForDowloadImage(EE_IMAGE_8_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_9:
				{
					PrepareForDowloadImage(EE_IMAGE_9_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_10:
				{
					PrepareForDowloadImage(EE_IMAGE_10_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_11:
				{
					PrepareForDowloadImage(EE_IMAGE_11_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_12:
				{
					PrepareForDowloadImage(EE_IMAGE_12_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_13:
				{
					PrepareForDowloadImage(EE_IMAGE_13_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_14:
				{
					PrepareForDowloadImage(EE_IMAGE_14_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_15:
				{
					PrepareForDowloadImage(EE_IMAGE_15_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_16:
				{
					PrepareForDowloadImage(EE_IMAGE_16_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_17:
				{
					PrepareForDowloadImage(EE_IMAGE_17_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_18:
				{
					PrepareForDowloadImage(EE_IMAGE_18_START_ADDRESS, 3);
					break;
				}
				
				case DOWNLOAD_DISPLAY_IMAGE_19:
				{
					PrepareForDowloadImage(EE_IMAGE_19_START_ADDRESS, 3);
					break;
				}
//				case DOWNLOAD_SMALL_FONTS:
//				{
//					SPI_FLASH_WriteStatusRegister(0x00);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_UnprotectSector(EE_FONT_SMALL_START_ADDRESS);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_Erase(EE_FONT_SMALL_START_ADDRESS, SPI_EE_4K_BLOCK_ERASE);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;	
//					flash_destination = EE_FONT_SMALL_START_ADDRESS;
//					next_packet_number = 1;
//					total_packet_number = (aCommBuffer[1] << 8) + aCommBuffer[2];
//					if(eTransferMode == TRANSFER_P2P) Serial_PutByte(ACK);
//					activ_command = aCommBuffer[0];
//					break;
//				}
				
//				case DOWNLOAD_MIDDLE_FONTS:
//				{
//					SPI_FLASH_WriteStatusRegister(0x00);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_UnprotectSector(EE_FONT_MIDDLE_START_ADDRESS);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_Erase(EE_FONT_MIDDLE_START_ADDRESS, SPI_EE_4K_BLOCK_ERASE);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_Erase(EE_FONT_MIDDLE_START_ADDRESS + 0x1000, SPI_EE_4K_BLOCK_ERASE);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					flash_destination = EE_FONT_MIDDLE_START_ADDRESS;
//					next_packet_number = 1;
//					total_packet_number = (aCommBuffer[1] << 8) + aCommBuffer[2];
//					if(eTransferMode == TRANSFER_P2P) Serial_PutByte(ACK);
//					activ_command = aCommBuffer[0];
//					break;
//				}
				
//				case DOWNLOAD_BIG_FONTS:
//				{
//					SPI_FLASH_WriteStatusRegister(0x00);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_UnprotectSector(EE_FONT_BIG_START_ADDRESS);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_Erase(EE_FONT_BIG_START_ADDRESS, SPI_EE_4K_BLOCK_ERASE);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_Erase(EE_FONT_BIG_START_ADDRESS + 0x1000, SPI_EE_4K_BLOCK_ERASE);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_Erase(EE_FONT_BIG_START_ADDRESS + 0x2000, SPI_EE_4K_BLOCK_ERASE);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_Erase(EE_FONT_BIG_START_ADDRESS + 0x3000, SPI_EE_4K_BLOCK_ERASE);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_Erase(EE_FONT_BIG_START_ADDRESS + 0x4000, SPI_EE_4K_BLOCK_ERASE);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					flash_destination = EE_FONT_BIG_START_ADDRESS;
//					next_packet_number = 1;
//					total_packet_number = (aCommBuffer[1] << 8) + aCommBuffer[2];
//					if(eTransferMode == TRANSFER_P2P) Serial_PutByte(ACK);
//					activ_command = aCommBuffer[0];
//					break;
//				}
				
//				case DOWNLOAD_TEXT_DATE_TIME:
//				{
//					SPI_FLASH_WriteStatusRegister(0x00);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_UnprotectSector(EE_TEXT_DATE_TIME_START_ADDRESS);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_Erase(EE_TEXT_DATE_TIME_START_ADDRESS, SPI_EE_4K_BLOCK_ERASE);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;	
//					flash_destination = EE_TEXT_DATE_TIME_START_ADDRESS;
//					next_packet_number = 1;
//					total_packet_number = (aCommBuffer[1] << 8) + aCommBuffer[2];
//					if(eTransferMode == TRANSFER_P2P) Serial_PutByte(ACK);
//					activ_command = aCommBuffer[0];
//					break;
//				}
				
//				case DOWNLOAD_TEXT_EVENTS:
//				{
//					SPI_FLASH_WriteStatusRegister(0x00);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_UnprotectSector(EE_TEXT_EVENTS_START_ADDRESS);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
//					SPI_FLASH_Erase(EE_TEXT_EVENTS_START_ADDRESS, SPI_EE_4K_BLOCK_ERASE);
//					while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;	
//					flash_destination = EE_TEXT_EVENTS_START_ADDRESS;
//					next_packet_number = 1;
//					total_packet_number = (aCommBuffer[1] << 8) + aCommBuffer[2];
//					if(eTransferMode == TRANSFER_P2P) Serial_PutByte(ACK);
//					activ_command = aCommBuffer[0];
//					break;
//				}
				
				case START_BOOTLOADER:
				{
					if (eTransferMode == TRANSFER_P2P) Serial_PutByte(ACK);
					HAL_Delay(5);
					BootloaderExe();
					break;
				}
				
				case GET_SYS_STATUS:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 9;
						aCommBuffer[6] = GET_SYS_STATUS;
						if(sys_status & 0x01) aCommBuffer[7] = '1';
						else aCommBuffer[7] = '0';
						if(sys_status & 0x02) aCommBuffer[8] = '1';
						else aCommBuffer[8] = '0';
						if(sys_status & 0x04) aCommBuffer[9] = '1';
						else aCommBuffer[9] = '0';
						if(sys_status & 0x08) aCommBuffer[10] = '1';
						else aCommBuffer[10] = '0';
						if(sys_status & 0x10) aCommBuffer[11] = '1';
						else aCommBuffer[11] = '0';
						if(sys_status & 0x20) aCommBuffer[12] = '1';
						else aCommBuffer[12] = '0';
						if(sys_status & 0x40) aCommBuffer[13] = '1';
						else aCommBuffer[13] = '0';
						if(sys_status & 0x80) aCommBuffer[14] = '1';
						else aCommBuffer[14] = '0';
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case GET_SYS_INFO:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 0;
						aCommBuffer[6] = GET_SYS_INFO;
						
						while (sys_info[aCommBuffer[5]] != '\0')
						{
							aCommBuffer[aCommBuffer[5] + 7] = sys_info[aCommBuffer[5]++];
						}
						
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case GET_RTC_DATE_TIME:
				{
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
					break;
				}
					
				case SET_RTC_DATE_TIME:
				{
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
					break;
				}
				
				case GET_LOG_LIST:
				{
					if(LOGGER_Read(0x0000) == LOGGER_OK)
					{
						aCommBuffer[0] = 0;
						while(aCommBuffer[0] < LOG_SIZE) aCommBuffer[aCommBuffer[0] + 7] = aEepromBuffer[aCommBuffer[0]++];
						aCommBuffer[5] = aCommBuffer[0] + 2;
						aCommBuffer[6] = GET_LOG_LIST;
						RS485_ResponsePacketReady();
					}
					else if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = GET_LOG_LIST;
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case DELETE_LOG_LIST:
				{
					LOGGER_Delete(0x0000);
				
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = DELETE_LOG_LIST;
						RS485_ResponsePacketReady();
					}				
					break;
				}
				
				case GET_RS485_CONFIG:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 13;
						aCommBuffer[6] = GET_RS485_CONFIG;
						Hex2Str(rs485_interface_address, 2, &aCommBuffer[7]);
						Hex2Str(rs485_group_address, 2, &aCommBuffer[11]);
						Hex2Str(rs485_broadcast_address, 2, &aCommBuffer[15]);
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case SET_RS485_CONFIG:
				{
					Str2Hex(&aCommBuffer[1], 2, rs485_interface_address);
					Str2Hex(&aCommBuffer[5], 2, rs485_group_address);
					Str2Hex(&aCommBuffer[9], 2, rs485_broadcast_address);
				
					if(HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, 1000, I2C_EE_TIMEOUT) == HAL_OK)
					{
						I2C_EE_WriteEnable();
						aEepromBuffer[0] = EE_RS485_INTERFACE_ADDRESS >> 8;
						aEepromBuffer[1] = EE_RS485_INTERFACE_ADDRESS;
						aEepromBuffer[2] = rs485_interface_address[0];
						aEepromBuffer[3] = rs485_interface_address[1];
						HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 4, I2C_EE_TIMEOUT);
						HAL_Delay(I2C_EE_WRITE_DELAY);
						I2C_EE_WriteDisable();
					}
					
					if(HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, 1000, I2C_EE_TIMEOUT) == HAL_OK)
					{
						I2C_EE_WriteEnable();
						aEepromBuffer[0] = EE_RS485_GROUP_ADDRESS >> 8;
						aEepromBuffer[1] = EE_RS485_GROUP_ADDRESS;
						aEepromBuffer[2] = rs485_group_address[0];
						aEepromBuffer[3] = rs485_group_address[1];
						HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 4, I2C_EE_TIMEOUT);
						HAL_Delay(I2C_EE_WRITE_DELAY);
						I2C_EE_WriteDisable();
					}
					
					if(HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, 1000, I2C_EE_TIMEOUT) == HAL_OK)
					{
						I2C_EE_WriteEnable();
						aEepromBuffer[0] = EE_RS485_BROADCAST_ADDRESS >> 8;
						aEepromBuffer[1] = EE_RS485_BROADCAST_ADDRESS;
						aEepromBuffer[2] = rs485_broadcast_address[0];
						aEepromBuffer[3] = rs485_broadcast_address[1];
						HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 4, I2C_EE_TIMEOUT);
						HAL_Delay(I2C_EE_WRITE_DELAY);
						I2C_EE_WriteDisable();
					}
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_RS485_CONFIG;
						RS485_ResponsePacketReady();
					}
					break;
				}

				case GET_MIFARE_AUTHENTICATION_KEY_A:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 13;
						aCommBuffer[6] = GET_MIFARE_AUTHENTICATION_KEY_A;						
						Hex2Str(aMifareAuthenticationKeyA, 12, &aCommBuffer[7]);
						RS485_ResponsePacketReady();
					}				
					break;
				}
				
				case SET_MIFARE_AUTHENTICATION_KEY_A:
				{
					Str2Hex(&aCommBuffer[1], 6, aMifareAuthenticationKeyA);					
					aEepromBuffer[0] = EE_MIFARE_AUTHENTICATION_KEY_A >> 8;
					aEepromBuffer[1] = EE_MIFARE_AUTHENTICATION_KEY_A;
					aEepromBuffer[2] = aMifareAuthenticationKeyA[0];
					aEepromBuffer[3] = aMifareAuthenticationKeyA[1];
					aEepromBuffer[4] = aMifareAuthenticationKeyA[2];
					aEepromBuffer[5] = aMifareAuthenticationKeyA[3];
					aEepromBuffer[6] = aMifareAuthenticationKeyA[4];
					aEepromBuffer[7] = aMifareAuthenticationKeyA[5];					
					I2C_EE_WriteEnable();
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 8, I2C_EE_TIMEOUT);
					//HAL_Delay(I2C_EE_WRITE_DELAY);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_MIFARE_AUTHENTICATION_KEY_A;
						RS485_ResponsePacketReady();
					}				
					break;
				}
				
				case GET_MIFARE_AUTHENTICATION_KEY_B:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 13;
						aCommBuffer[6] = GET_MIFARE_AUTHENTICATION_KEY_B;
						Hex2Str(aMifareAuthenticationKeyB, 12, &aCommBuffer[7]);
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case SET_MIFARE_AUTHENTICATION_KEY_B:
				{
					Str2Hex(&aCommBuffer[1], 6, aMifareAuthenticationKeyB);					
					aEepromBuffer[0] = EE_MIFARE_AUTHENTICATION_KEY_B >> 8;
					aEepromBuffer[1] = EE_MIFARE_AUTHENTICATION_KEY_B;
					aEepromBuffer[2] = aMifareAuthenticationKeyB[0];
					aEepromBuffer[3] = aMifareAuthenticationKeyB[1];
					aEepromBuffer[4] = aMifareAuthenticationKeyB[2];
					aEepromBuffer[5] = aMifareAuthenticationKeyB[3];
					aEepromBuffer[6] = aMifareAuthenticationKeyB[4];
					aEepromBuffer[7] = aMifareAuthenticationKeyB[5];					
					I2C_EE_WriteEnable();
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 8, I2C_EE_TIMEOUT);
					//HAL_Delay(I2C_EE_WRITE_DELAY);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_MIFARE_AUTHENTICATION_KEY_B;
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case GET_MIFARE_PERMITED_GROUP:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 17;
						aCommBuffer[6] = GET_MIFARE_PERMITED_GROUP;	
						
						aEepromBuffer[0] = EE_MIFARE_PERMITED_GROUP_START_ADDRESS >> 8;
						aEepromBuffer[1] = EE_MIFARE_PERMITED_GROUP_START_ADDRESS;
						HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
						HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, &aCommBuffer[7], 16, I2C_EE_TIMEOUT);
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case SET_MIFARE_PERMITED_GROUP:
				{
					aEepromBuffer[0] = EE_MIFARE_PERMITED_GROUP_START_ADDRESS >> 8;
					aEepromBuffer[1] = EE_MIFARE_PERMITED_GROUP_START_ADDRESS;
					aEepromBuffer[2] = aCommBuffer[1];
					aEepromBuffer[3] = aCommBuffer[2];
					aEepromBuffer[4] = aCommBuffer[3];
					aEepromBuffer[5] = aCommBuffer[4];
					aEepromBuffer[6] = aCommBuffer[5];
					aEepromBuffer[7] = aCommBuffer[6];
					aEepromBuffer[8] = aCommBuffer[7];
					aEepromBuffer[9] = aCommBuffer[8];
					aEepromBuffer[10] = aCommBuffer[9];
					aEepromBuffer[11] = aCommBuffer[10];
					aEepromBuffer[12] = aCommBuffer[11];
					aEepromBuffer[13] = aCommBuffer[12];
					aEepromBuffer[14] = aCommBuffer[13];
					aEepromBuffer[15] = aCommBuffer[14];
					aEepromBuffer[16] = aCommBuffer[15];
					aEepromBuffer[17] = aCommBuffer[16];
					I2C_EE_WriteEnable();
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 18, I2C_EE_TIMEOUT);
					//HAL_Delay(I2C_EE_WRITE_DELAY);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_MIFARE_PERMITED_GROUP;
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case GET_MIFARE_PERMITED_CARD_1:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 21;
						aCommBuffer[6] = GET_MIFARE_PERMITED_CARD_1;
						aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_1_ID >> 8;
						aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_1_ID;
						HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
						HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, 10, I2C_EE_TIMEOUT);
						Hex2Str(aEepromBuffer, 10, &aCommBuffer[7]);
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case SET_MIFARE_PERMITED_CARD_1:
				{
					aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_1_ID >> 8;
					aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_1_ID;
					Str2Hex(&aCommBuffer[1], 10, &aEepromBuffer[2]);
					I2C_EE_WriteEnable();
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 12, I2C_EE_TIMEOUT);
					//HAL_Delay(I2C_EE_WRITE_DELAY);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
					aCommBuffer[5] = 0;
					
					while(aCommBuffer[5] < 6)
					{
						aRoomPowerExpiryDateTime[aCommBuffer[5]] = aEepromBuffer[aCommBuffer[5] + 7];
						++aCommBuffer[5];
					}
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_MIFARE_PERMITED_CARD_1;
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case GET_MIFARE_PERMITED_CARD_2:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 21;
						aCommBuffer[6] = GET_MIFARE_PERMITED_CARD_2;	
						aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_2_ID >> 8;
						aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_2_ID;
						HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
						HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, 10, I2C_EE_TIMEOUT);
						Hex2Str(aEepromBuffer, 10, &aCommBuffer[7]);
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case SET_MIFARE_PERMITED_CARD_2:
				{
					aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_2_ID >> 8;
					aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_2_ID;
					Str2Hex(&aCommBuffer[1], 10, &aEepromBuffer[2]);
					I2C_EE_WriteEnable();
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 12, I2C_EE_TIMEOUT);
					//HAL_Delay(I2C_EE_WRITE_DELAY);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
					aCommBuffer[5] = 0;
				
					while(aCommBuffer[5] < 6)
					{
						aRoomPowerExpiryDateTime[aCommBuffer[5]] = aEepromBuffer[aCommBuffer[5] + 7];
						++aCommBuffer[5];
					}
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_MIFARE_PERMITED_CARD_2;
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case GET_MIFARE_PERMITED_CARD_3:
				{			
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 21;
						aCommBuffer[6] = GET_MIFARE_PERMITED_CARD_3;	
						aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_3_ID >> 8;
						aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_3_ID;
						HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
						HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, 10, I2C_EE_TIMEOUT);
						Hex2Str(aEepromBuffer, 10, &aCommBuffer[7]);
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case SET_MIFARE_PERMITED_CARD_3:
				{
					aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_3_ID >> 8;
					aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_3_ID;
					Str2Hex(&aCommBuffer[1], 10, &aEepromBuffer[2]);
					I2C_EE_WriteEnable();
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 12, I2C_EE_TIMEOUT);
					//HAL_Delay(I2C_EE_WRITE_DELAY);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
					aCommBuffer[5] = 0;
				
					while(aCommBuffer[5] < 6)
					{
						aRoomPowerExpiryDateTime[aCommBuffer[5]] = aEepromBuffer[aCommBuffer[5] + 7];
						++aCommBuffer[5];
					}
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_MIFARE_PERMITED_CARD_3;
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case GET_MIFARE_PERMITED_CARD_4:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 21;
						aCommBuffer[6] = GET_MIFARE_PERMITED_CARD_4;	
						aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_4_ID >> 8;
						aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_4_ID;
						HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
						HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, 10, I2C_EE_TIMEOUT);
						Hex2Str(aEepromBuffer, 10, &aCommBuffer[7]);
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case SET_MIFARE_PERMITED_CARD_4:
				{
					aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_4_ID >> 8;
					aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_4_ID;
					Str2Hex(&aCommBuffer[1], 10, &aEepromBuffer[2]);
					I2C_EE_WriteEnable();
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 12, I2C_EE_TIMEOUT);
					//HAL_Delay(I2C_EE_WRITE_DELAY);
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
					I2C_EE_WriteDisable();
					aCommBuffer[5] = 0;
				
					while(aCommBuffer[5] < 6)
					{
						aRoomPowerExpiryDateTime[aCommBuffer[5]] = aEepromBuffer[aCommBuffer[5] + 7];
						++aCommBuffer[5];
					}
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_MIFARE_PERMITED_CARD_4;
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case GET_MIFARE_PERMITED_CARD_5:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 21;
						aCommBuffer[6] = GET_MIFARE_PERMITED_CARD_5;	
						aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_5_ID >> 8;
						aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_5_ID;
						HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
						HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, 10, I2C_EE_TIMEOUT);
						Hex2Str(aEepromBuffer, 10, &aCommBuffer[7]);
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case SET_MIFARE_PERMITED_CARD_5:
				{
					aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_5_ID >> 8;
					aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_5_ID;
					Str2Hex(&aCommBuffer[1], 10, &aEepromBuffer[2]);
					I2C_EE_WriteEnable();
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 12, I2C_EE_TIMEOUT);
					//HAL_Delay(I2C_EE_WRITE_DELAY);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
					aCommBuffer[5] = 0;
				
					while(aCommBuffer[5] < 6)
					{
						aRoomPowerExpiryDateTime[aCommBuffer[5]] = aEepromBuffer[aCommBuffer[5] + 7];
						++aCommBuffer[5];
					}
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_MIFARE_PERMITED_CARD_5;
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case GET_MIFARE_PERMITED_CARD_6:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 21;
						aCommBuffer[6] = GET_MIFARE_PERMITED_CARD_6;	
						aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_6_ID >> 8;
						aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_6_ID;
						HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
						HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, 10, I2C_EE_TIMEOUT);
						Hex2Str(aEepromBuffer, 10, &aCommBuffer[7]);
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case SET_MIFARE_PERMITED_CARD_6:
				{
					aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_6_ID >> 8;
					aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_6_ID;
					Str2Hex(&aCommBuffer[1], 10, &aEepromBuffer[2]);
					I2C_EE_WriteEnable();
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 12, I2C_EE_TIMEOUT);
					//HAL_Delay(I2C_EE_WRITE_DELAY);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
					aCommBuffer[5] = 0;
				
					while(aCommBuffer[5] < 6)
					{
						aRoomPowerExpiryDateTime[aCommBuffer[5]] = aEepromBuffer[aCommBuffer[5] + 7];
						++aCommBuffer[5];
					}
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_MIFARE_PERMITED_CARD_6;
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case GET_MIFARE_PERMITED_CARD_7:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 21;
						aCommBuffer[6] = GET_MIFARE_PERMITED_CARD_7;
						aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_7_ID >> 8;
						aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_7_ID;
						HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
						HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, 10, I2C_EE_TIMEOUT);
						Hex2Str(aEepromBuffer, 10, &aCommBuffer[7]);
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case SET_MIFARE_PERMITED_CARD_7:
				{
					aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_7_ID >> 8;
					aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_7_ID;
					Str2Hex(&aCommBuffer[1], 10, &aEepromBuffer[2]);
					I2C_EE_WriteEnable();
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 12, I2C_EE_TIMEOUT);
					//HAL_Delay(I2C_EE_WRITE_DELAY);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
					aCommBuffer[5] = 0;
				
					while(aCommBuffer[5] < 6)
					{
						aRoomPowerExpiryDateTime[aCommBuffer[5]] = aEepromBuffer[aCommBuffer[5] + 7];
						++aCommBuffer[5];
					}
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_MIFARE_PERMITED_CARD_7;
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case GET_MIFARE_PERMITED_CARD_8:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 21;
						aCommBuffer[6] = GET_MIFARE_PERMITED_CARD_8;
						aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_8_ID >> 8;
						aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_8_ID;
						HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
						HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, 10, I2C_EE_TIMEOUT);
						Hex2Str(aEepromBuffer, 10, &aCommBuffer[7]);
						RS485_ResponsePacketReady();
					}
					break;
				}
					
				case SET_MIFARE_PERMITED_CARD_8:
				{
					aEepromBuffer[0] = EE_MIFARE_PERMITED_CARD_8_ID >> 8;
					aEepromBuffer[1] = EE_MIFARE_PERMITED_CARD_8_ID;
					Str2Hex(&aCommBuffer[1], 10, &aEepromBuffer[2]);
					I2C_EE_WriteEnable();
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 12, I2C_EE_TIMEOUT);
					HAL_Delay(I2C_EE_WRITE_DELAY);;
					aEepromBuffer[5] = EE_ROOM_POWER_TIMEOUT >> 8;
					aEepromBuffer[6] = EE_ROOM_POWER_TIMEOUT;
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, &aEepromBuffer[5], 7, I2C_EE_TIMEOUT);
					//HAL_Delay(I2C_EE_WRITE_DELAY);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
					aCommBuffer[5] = 0;
				
					while(aCommBuffer[5] < 6)
					{
						aRoomPowerExpiryDateTime[aCommBuffer[5]] = aEepromBuffer[aCommBuffer[5] + 7];
						++aCommBuffer[5];
					}
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_MIFARE_PERMITED_CARD_8;
						RS485_ResponsePacketReady();
					}
					break;
				}
				
				case GET_DIN_STATE:
				{
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
					break;
				}
					
				case SET_DOUT_STATE:
				{
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
					if(aCommBuffer[7] == '1') dout_0_7_remote |= 0x4040;
					else if (aCommBuffer[7] == '0') dout_0_7_remote &= 0xbfff;
					if(aCommBuffer[8] == '1') dout_0_7_remote |= 0x8000;
					else if (aCommBuffer[8] == '0') dout_0_7_remote &= 0x7fff;
					
//					if(aCommBuffer[9] == '1') dout_0_7_remote |= 0x0001;
//					else if (aCommBuffer[9] == '0') dout_0_7_remote &= 0xfffe;
//					if(aCommBuffer[10] == '1') dout_0_7_remote |= 0x0002;
//					else if (aCommBuffer[10] == '0') dout_0_7_remote &= 0xfffd;
//					if(aCommBuffer[11] == '1') dout_0_7_remote |= 0x0004;
//					else if (aCommBuffer[11] == '0') dout_0_7_remote &= 0xfffb;
//					if(aCommBuffer[12] == '1') dout_0_7_remote |= 0x0008;
//					else if (aCommBuffer[12] == '0') dout_0_7_remote &= 0xfff7;
//					if(aCommBuffer[13] == '1') dout_0_7_remote |= 0x0010;
//					else if (aCommBuffer[13] == '0') dout_0_7_remote &= 0xffef;
//					if(aCommBuffer[14] == '1') dout_0_7_remote |= 0x0020;
//					else if (aCommBuffer[14] == '0') dout_0_7_remote &= 0xffdf;
//					if(aCommBuffer[15] == '1') dout_0_7_remote |= 0x0040;
//					else if (aCommBuffer[15] == '0') dout_0_7_remote &= 0xffbf;
//					if(aCommBuffer[16] == '1') dout_0_7_remote |= 0x0080;
//					else if (aCommBuffer[16] == '0') dout_0_7_remote &= 0xff7f;
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_DOUT_STATE;
						RS485_ResponsePacketReady();						
					}
					break;
				}
					
				case GET_DOUT_STATE:
				{	
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
					break;
				}	
					
				case GET_PCB_TEMPERATURE:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 4;
						aCommBuffer[6] = GET_PCB_TEMPERATURE;
						TemperatureC = (int16_t)(((V25-AD_value)/Avg_Slope) + 25);
						Int2Str(&aCommBuffer[7], TemperatureC);
						aCommBuffer[9] = 'C';					
						RS485_ResponsePacketReady();						
					}
					break;
				}
					
				case GET_TEMP_CARD_BUFFER:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 16;
						aCommBuffer[6] = GET_TEMP_CARD_BUFFER;
						aCommBuffer[7] = sCardData.card_status;
						aCommBuffer[8] = sCardData.aUserCardID[0];
						aCommBuffer[9] = sCardData.aUserCardID[1];
						aCommBuffer[10] = sCardData.aUserCardID[2];
						aCommBuffer[11] = sCardData.aUserCardID[3];
						aCommBuffer[12] = sCardData.aUserCardID[4];
						aCommBuffer[13] = sCardData.card_user_group;
						aCommBuffer[14] = sCardData.aCardExpiryTime[0];
						aCommBuffer[15] = sCardData.aCardExpiryTime[1];
						aCommBuffer[16] = sCardData.aCardExpiryTime[2];
						aCommBuffer[17] = sCardData.aCardExpiryTime[3];
						aCommBuffer[18] = sCardData.aCardExpiryTime[4];
						aCommBuffer[19] = sCardData.aCardExpiryTime[5];
						aCommBuffer[20] = sCardData.controller_id >> 8;
						aCommBuffer[21] = sCardData.controller_id;
						RS485_ResponsePacketReady();						
					}
					break;
				}	
					
				case GET_DISPLAY_BRIGHTNESS:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 3;
						aCommBuffer[6] = GET_DISPLAY_BRIGHTNESS;
						aCommBuffer[7] = lcd_brightness >> 8;
						aCommBuffer[8] = lcd_brightness;
						RS485_ResponsePacketReady();						
					}
					break;
				}
				
				case SET_DISPLAY_BRIGHTNESS:
				{
					lcd_brightness = (aCommBuffer[1] << 8) + aCommBuffer[2];
					__HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_2, lcd_brightness);
					I2C_EE_WriteEnable();
					aEepromBuffer[0] = EE_LCD_BRIGHTNESS >> 8;
					aEepromBuffer[1] = EE_LCD_BRIGHTNESS;
					aEepromBuffer[2] = lcd_brightness >> 8;
					aEepromBuffer[3] = lcd_brightness;
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 4, I2C_EE_TIMEOUT);
					//HAL_Delay(I2C_EE_WRITE_DELAY);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
				
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_DISPLAY_BRIGHTNESS;
						RS485_ResponsePacketReady();						
					}
					break;
				}	
					
				case GET_ROOM_STATUS:
				{
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 36;
						aCommBuffer[6] = GET_ROOM_STATUS;
						
						if(ROOM_Status == ROOM_IDLE) 					aCommBuffer[7] = '0';
						else if(ROOM_Status == ROOM_READY) 				aCommBuffer[7] = '1';	
						else if(ROOM_Status == ROOM_BUSY) 				aCommBuffer[7] = '2';	
						else if(ROOM_Status == ROOM_CLEANING) 			aCommBuffer[7] = '3';
						else if(ROOM_Status == ROOM_GENERAL_CLEANING) 	aCommBuffer[7] = '4';
						else if(ROOM_Status == ROOM_OUT_OF_ORDER)		aCommBuffer[7] = '5';
						else if(ROOM_Status == ROOM_LATE_CHECKOUT)		aCommBuffer[7] = '6';
						else if(ROOM_Status == ROOM_HANDMAID_IN)		aCommBuffer[7] = '7';
						else if(ROOM_Status == ROOM_FIRE_EXIT)			aCommBuffer[7] = '9';
						else if(ROOM_Status == ROOM_FIRE_ALARM)			aCommBuffer[7] = 'A';
						
						if(!IsONE_WIRE_SensorConnected()) aCommBuffer[8] = 'X';	// sensor error
						else
						{
							if(temperature_measured & 0x8000) aCommBuffer[8] = '-';
							else aCommBuffer[8] = '+';
							
							temperature = (temperature_measured & 0x0fff);	// delete minus sign from temperature
							aCommBuffer[9] = 48;
							
							while(temperature > 99)
							{
								temperature -= 100;
								++aCommBuffer[9];
							}
							
							aCommBuffer[10] = 48;
							
							while(temperature > 9)
							{
								temperature -= 10;
								++aCommBuffer[10];
							}
							
							aCommBuffer[11] = temperature + 48;							
						}
						
						if(temperature_setpoint & 0x80) aCommBuffer[12] = 'E';
						else if (temperature_difference & 0x80) aCommBuffer[12] = 'O';
						else aCommBuffer[12] = 'D';
						
						if(temperature_setpoint & 0x40) aCommBuffer[13] = 'H';
						else aCommBuffer[13] = 'C';
						
						temperature = temperature_setpoint;
						temperature &= 0x3f;
						aCommBuffer[14] = 0;
						
						while(temperature > 9)
						{
							temperature -= 10;
							++aCommBuffer[14];
						}
						
						aCommBuffer[14] += 48;
						aCommBuffer[15] = temperature + 48;	
						temperature = temperature_difference;							
						temperature &= 0x3f;
						aCommBuffer[16] = 0;
						
						while(temperature > 9)
						{
							temperature -= 10;
							++aCommBuffer[16];
						}
						
						aCommBuffer[16] += 48;
						aCommBuffer[17] = temperature + 48;
						
						if(din_0_7 & 0x01) aCommBuffer[18] = '1';
						else aCommBuffer[18] = '0';
						if(din_0_7 & 0x02) aCommBuffer[19] = '1';
						else aCommBuffer[19] = '0';
						if(din_0_7 & 0x04) aCommBuffer[20] = '1';
						else aCommBuffer[20] = '0';
						if(din_0_7 & 0x08) aCommBuffer[21] = '1';
						else aCommBuffer[21] = '0';
						if(din_0_7 & 0x10) aCommBuffer[22] = '1';
						else aCommBuffer[22] = '0';
						if(din_0_7 & 0x20) aCommBuffer[23] = '1';
						else aCommBuffer[23] = '0';
						if(din_0_7 & 0x40) aCommBuffer[24] = '1';
						else aCommBuffer[24] = '0';
						if(din_0_7 & 0x80) aCommBuffer[25] = '1';
						else aCommBuffer[25] = '0';
						
						if(dout_0_7 & 0x01) aCommBuffer[26] = '1';
						else aCommBuffer[26] = '0';
						if(dout_0_7 & 0x02) aCommBuffer[27] = '1';
						else aCommBuffer[27] = '0';
						if(dout_0_7 & 0x04) aCommBuffer[28] = '1';
						else aCommBuffer[28] = '0';
						if(dout_0_7 & 0x08) aCommBuffer[29] = '1';
						else aCommBuffer[29] = '0';
						if(dout_0_7 & 0x10) aCommBuffer[30] = '1';
						else aCommBuffer[30] = '0';
						if(dout_0_7 & 0x20) aCommBuffer[31] = '1';
						else aCommBuffer[31] = '0';
						if(dout_0_7 & 0x40) aCommBuffer[32] = '1';
						else aCommBuffer[32] = '0';
						if(dout_0_7 & 0x80) aCommBuffer[33] = '1';
						else aCommBuffer[33] = '0';	
						
						aCommBuffer[34] = sys_info[44];
						aCommBuffer[35] = sys_info[45];
						aCommBuffer[36] = sys_info[46];
						aCommBuffer[37] = sys_info[47];
						aCommBuffer[38] = sys_info[48];
						aCommBuffer[39] = sys_info[49];
						aCommBuffer[40] = sys_info[50];
						aCommBuffer[41] = sys_info[51];
						
						RS485_ResponsePacketReady();						
					}
					break;						
				}
				
				
				case SET_ROOM_STATUS:
				{	
					if(aCommBuffer[1] == '0') ROOM_Status = ROOM_IDLE;
					else if(aCommBuffer[1] == '1') 
					{
						if(aCommBuffer[2] == '0') ROOM_Status = ROOM_FIRE_ALARM;
						else ROOM_Status = ROOM_READY;
					}
					else if(aCommBuffer[1] == '2') ROOM_Status = ROOM_BUSY;	
					else if(aCommBuffer[1] == '3') ROOM_Status = ROOM_CLEANING;	
					else if(aCommBuffer[1] == '4') ROOM_Status = ROOM_GENERAL_CLEANING;
					else if(aCommBuffer[1] == '5') ROOM_Status = ROOM_OUT_OF_ORDER;	
					else if(aCommBuffer[1] == '6') ROOM_Status = ROOM_LATE_CHECKOUT;
					else if(aCommBuffer[1] == '7') ROOM_Status = ROOM_HANDMAID_IN;
					else if(aCommBuffer[1] == '8') ROOM_Status = ROOM_FORCING_DND;
					else if(aCommBuffer[1] == '9') ROOM_Status = ROOM_FIRE_EXIT;
				
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_ROOM_STATUS;
						RS485_ResponsePacketReady();						
					}
					break;						
				}
				
				
				case RESET_SOS_ALARM:
				{
					if((aCommBuffer[1] == '1') && IsSosAlarmActiv())
					{
						SosAlarm_Off();
						LogEvent.log_event = SOS_ALARM_RESET;
						LogEvent.log_group = NULL;
						LogEvent.log_type = NULL;
						LogEvent.log_card_id[0] = NULL;
						LogEvent.log_card_id[1] = NULL;
						LogEvent.log_card_id[2] = NULL;
						LogEvent.log_card_id[3] = NULL;
						LogEvent.log_card_id[4] = NULL;
						LOGGER_Write();
						DISPLAY_SosAlarmImageDelete();
						SignalBuzzer = BUZZ_OFF;
						aCommBuffer[7] = '1';
					}
					else
					{
						aCommBuffer[7] = '0';
					}
				
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 2;
						aCommBuffer[6] = RESET_SOS_ALARM;
						RS485_ResponsePacketReady();						
					}
					break;
				}	
				
				
				case GET_ROOM_TEMPERATURE:
				{
					if(eTransferMode == TRANSFER_P2P)
					{						
						aCommBuffer[6] = GET_ROOM_TEMPERATURE;
						aCommBuffer[5] = 11;
						
						if(!IsONE_WIRE_SensorConnected())
						{
							aCommBuffer[7] = 'S';							// sensor error
							aCommBuffer[8] = 'E';
							aCommBuffer[9] = 'R';
							aCommBuffer[10] = 'R';
						}
						else
						{
							if(temperature_measured & 0x8000) aCommBuffer[7] = '-';
							else aCommBuffer[7] = '+';
							
							temperature = (temperature_measured & 0x0fff);	// delete minus sign from temperature
							aCommBuffer[8] = 0;
							
							while(temperature > 99)
							{
								temperature -= 100;
								++aCommBuffer[8];
							}
							
							aCommBuffer[8] +=  48;
							aCommBuffer[9] = 0;
							
							while(temperature > 9)
							{
								temperature -= 10;
								++aCommBuffer[9];
							}
							
							aCommBuffer[9] +=  48;
							aCommBuffer[10] = temperature + 48;							
						}
						
						if(temperature_setpoint & 0x80) aCommBuffer[11] = 'E';
						else if (temperature_difference & 0x80) aCommBuffer[11] = 'O';
						else aCommBuffer[11] = 'D';
						
						if(temperature_setpoint & 0x40) aCommBuffer[12] = 'H';
						else aCommBuffer[12] = 'C';
						
						temperature = temperature_setpoint;
						temperature &= 0x3f;
						aCommBuffer[13] = 0;
						
						while(temperature > 9)
						{
							temperature -= 10;
							++aCommBuffer[13];
						}
						
						aCommBuffer[13] += 48;
						aCommBuffer[14] = temperature + 48;	
						temperature = temperature_difference;							
						temperature &= 0x3f;
						aCommBuffer[15] = 0;
						
						while(temperature > 9)
						{
							temperature -= 10;
							++aCommBuffer[15];
						}
						
						aCommBuffer[15] += 48;
						aCommBuffer[16] = temperature + 48;					
						RS485_ResponsePacketReady();						
					}
					break;
				}
				
				
				case SET_ROOM_TEMPERATURE:
				{
					temperature = 0;
				
					if(aCommBuffer[1] == 'E') temperature |= 0x80;				
					if(aCommBuffer[2] == 'H') temperature |= 0x40;
				
					temperature += (aCommBuffer[3] - 48) * 10;
					temperature += (aCommBuffer[4] - 48);
					temperature_setpoint = temperature;
				
					I2C_EE_WriteEnable();
					aEepromBuffer[0] = EE_ROOM_TEMPERATURE_SETPOINT >> 8;
					aEepromBuffer[1] = EE_ROOM_TEMPERATURE_SETPOINT;
					aEepromBuffer[2] = temperature_setpoint;
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 3, I2C_EE_TIMEOUT);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
				
					temperature = 0;
				
					if(aCommBuffer[1] == 'O') temperature |= 0x80;
					
					temperature += (aCommBuffer[5] - 48) * 100;					
					temperature += (aCommBuffer[6] - 48) * 10;
					temperature += (aCommBuffer[7] - 48);
					temperature_difference = temperature;
				
					I2C_EE_WriteEnable();
					aEepromBuffer[0] = EE_ROOM_TEMPERATURE_DIFFERENCE >> 8;
					aEepromBuffer[1] = EE_ROOM_TEMPERATURE_DIFFERENCE;
					aEepromBuffer[2] = temperature_difference;
					HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 3, I2C_EE_TIMEOUT);
					I2C_EE_WriteDisable();
					HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
					
					if(eTransferMode == TRANSFER_P2P)
					{
						aCommBuffer[5] = 1;
						aCommBuffer[6] = SET_ROOM_TEMPERATURE;						
						RS485_ResponsePacketReady();						
					}
					break;
				}
				
				
				default:
				{
					RS485_NoResponse();
					break;
				}
			}// End of switch aComBuffer[0] - command function	
		}
		else if(packet_type == STX)
		{
			if((next_packet_number == ((aCommBuffer[0] << 8) + aCommBuffer[1])) && (flash_destination != NULL))
			{
									
				SPI_FLASH_WritePage(flash_destination, &aCommBuffer[2], (rs485_packet_data_lenght - 2));
				while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;					
				flash_destination += (rs485_packet_data_lenght - 2);
				if (eTransferMode == TRANSFER_P2P) Serial_PutByte(ACK);
				
				if(next_packet_number == total_packet_number)
				{
					activ_command = NULL;
					flash_destination = NULL;
					next_packet_number = NULL;
					RS485_StopUpdate();
					sys_status |= 0x08;
				}
				else ++next_packet_number;
			}
		}
		
		if(IsRS485_ResponsePacketPending())
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
			HAL_UART_Transmit(&huart1, aCommBuffer, (aCommBuffer[5] + 9), PACKET_TRANSFER_TIMEOUT);
			while(HAL_UART_GetState(&huart1) != HAL_UART_STATE_READY) continue;
			RS485_NoResponse();
			RS485_DirRx();
		}
		
		RS485_Init();
	}
}// End of command parser function

uint16_t CalcChecksum(const uint8_t *p_data, uint32_t size)
{  
    uint16_t sum = 0;
    const uint8_t *p_data_end = p_data + size;

    while (p_data < p_data_end ) sum += *p_data++;

    return (sum);
    
}// End of calculating chechsum function


void PrepareForDowloadImage(uint32_t start_address, uint8_t number_of_blocks)
{
	uint8_t bcnt;
	uint32_t address;
	
	RS485_StartUpdateTimeoutTimer(UPDATE_TIMEOUT);
	RS485_StartUpdate();
	sys_status &= 0xf7;
	address = start_address;
	bcnt = 0;
	
	while(bcnt < number_of_blocks)
	{
		SPI_FLASH_WriteStatusRegister(0x00);
		while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
		SPI_FLASH_UnprotectSector(address);
		while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
		SPI_FLASH_Erase(address, SPI_EE_64K_BLOCK_ERASE);
		while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;		
		address += 0x00010000;
		bcnt++;
	}
	
	flash_destination = start_address;
	next_packet_number = 1;
	total_packet_number = (aCommBuffer[1] << 8) + aCommBuffer[2];
	if(eTransferMode == TRANSFER_P2P) Serial_PutByte(ACK);
	activ_command = aCommBuffer[0];
}

/* Public functions ---------------------------------------------------------*/
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) 
{
    __HAL_UART_CLEAR_PEFLAG(&huart1);
    __HAL_UART_CLEAR_FEFLAG(&huart1);
    __HAL_UART_CLEAR_NEFLAG(&huart1);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    __HAL_UART_CLEAR_OREFLAG(&huart1);
	__HAL_UART_FLUSH_DRREGISTER(&huart1);
	huart->ErrorCode = HAL_UART_ERROR_NONE;
	receive_pcnt = 0;
	received_byte_cnt = 0;
	++receiving_errors;;
    
}// End of hal usart error calback function

/******************************   END OF FILE  **********************************/
