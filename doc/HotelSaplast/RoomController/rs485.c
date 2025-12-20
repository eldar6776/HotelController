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
#include "main.h"
#include "common.h"
#include "dio_interface.h"
#include "eeprom.h"
#include "logger.h"
#include "signal.h"
#include "tft_lcd.h"
#include "rs485.h"
#include "rc522.h"
#include "one_wire.h"
#include "display.h"


/* Imported Types  -----------------------------------------------------------*/
/* Imported Variables --------------------------------------------------------*/
extern uint8_t aMifareKeyA[]; 
extern uint8_t aMifareKeyB[];
extern uint8_t aRoomPowerExpiryDateTime[];
extern uint8_t fw_update_status;
extern uint8_t sys_status;
extern uint16_t lcd_brightness;


/* Imported Functions    -----------------------------------------------------*/
/* Private Typedef -----------------------------------------------------------*/
enum 	/* Error code */
{
    FLASHIF_OK = 0,
    FLASHIF_ERASEKO,
    FLASHIF_WRITINGCTRL_ERROR,
    FLASHIF_WRITING_ERROR,
    FLASHIF_PROTECTION_ERRROR
};

enum	/* protection type */  
{
    FLASHIF_PROTECTION_NONE         = 0,
    FLASHIF_PROTECTION_PCROPENABLED = 0x1,
    FLASHIF_PROTECTION_WRPENABLED   = 0x2,
    FLASHIF_PROTECTION_RDPENABLED   = 0x4,
};

enum 	/* protection update */
{
    FLASHIF_WRP_ENABLE,
    FLASHIF_WRP_DISABLE
};

eTransferModeTypeDef eTransferMode;
eComStateTypeDef eComState = COM_INIT;


/* Private Define  -----------------------------------------------------------*/
/* Private Variables  --------------------------------------------------------*/
__IO uint16_t received_byte_cnt;
__IO uint8_t packet_type;
__IO uint16_t receive_pcnt;
__IO uint32_t rs485_timer;
__IO uint32_t rs485_flags;
__IO uint32_t rs485_update_timeout_timer;
__IO uint16_t receiving_errors;
__IO uint16_t rs485_sender_address;
__IO uint16_t rs485_packet_data_lenght;
__IO uint16_t rs485_packet_data_checksum;

uint8_t sys_status;
uint8_t fw_update_status;
uint8_t activ_command;
uint8_t post_process;
uint8_t aCommBuffer[COM_BUFFER_SIZE];
uint8_t aJournal_1[JOURNAL_SIZE];
uint8_t aJournal_2[JOURNAL_SIZE];
uint8_t aString[10];
uint8_t rs485_interface_address[2];
uint8_t rs485_group_address[2];
uint8_t rs485_broadcast_address[2];
uint8_t rs485_interface_baudrate;
uint8_t aRoomPowerExpiryDateTime[6];

uint16_t next_packet_number;
uint16_t total_packet_number;
uint32_t total_bytes_in_file;
uint32_t crc_32_calculated;
uint32_t crc_32_file;

uint32_t flash_destination;
uint32_t flash_protection;
uint32_t file_copy_source_address;
uint32_t file_copy_destination_address;
uint32_t file_copy_size;


/* Private macros   ----------------------------------------------------------*/
#define RS485_DirRx()   					(HAL_GPIO_WritePin(RS485_DIR_Port, RS485_DIR_Pin, GPIO_PIN_RESET))
#define RS485_DirTx()   					(HAL_GPIO_WritePin(RS485_DIR_Port, RS485_DIR_Pin, GPIO_PIN_SET))


/* Private Function Prototypes -----------------------------------------------*/
void FileTransferSetup(uint32_t storage_address);
void BackupOldFirmware(void);
uint32_t UpdateBootloader(void);
uint16_t CalcChecksum(const uint8_t *p_data, uint32_t size);
uint32_t FLASH_If_WriteProtectionConfig(uint32_t protectionstate);
void FormatFileStorage(uint32_t start_address, uint8_t number_of_blocks);
uint32_t FLASH_If_Write(uint32_t destination, uint32_t *p_source, uint32_t length);
void CopyFile(uint32_t file_source_address, uint32_t file_destination_address, uint32_t file_size);
void Serial_PutString(uint8_t *p_string);
void Serial_PutByte(uint8_t param);


/* Program Code  -------------------------------------------------------------*/
void RS485_Init(void)
{
	ClearBuffer(aCommBuffer, COM_BUFFER_SIZE);
    /**
    *   start usart receiving in interrupt mode
    *   to get packet header for address checking
    */
    RS485_DirRx();
	packet_type = NULL;
	receive_pcnt = 0;
	received_byte_cnt = 0;
	eTransferMode = TRANSFER_IDLE;
	
	if (huart1.RxState == HAL_UART_STATE_BUSY_RX)
	{
		__HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
		huart1.RxState = HAL_UART_STATE_READY;
		huart1.gState = HAL_UART_STATE_READY;
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
	uint8_t tmp_b;
	uint16_t temperature;
	
	if(eComState == COM_PACKET_RECEIVED)
	{
		if(packet_type == SOH)
		{ 
			if((aCommBuffer[0] >= DOWNLOAD_DISPLAY_IMAGE_1) && (aCommBuffer[0] <= DOWNLOAD_BIG_FONT))
			{
				FileTransferSetup(((aCommBuffer[0] - DOWNLOAD_DISPLAY_IMAGE_1) * 0x00030000));
				if(aCommBuffer[0] == DOWNLOAD_FIRMWARE_IMAGE) FormatFileStorage(EE_NEW_FIRMWARE_ADDRESS, 3);					
				else if(aCommBuffer[0] == DOWNLOAD_BOOTLOADER_IMAGE) FormatFileStorage(EE_NEW_BOOTLOADER_ADDRESS, 3);						
				else FormatFileStorage(EE_NEW_IMAGE_ADDRESS, 3);				
			}
			else
			{
				switch(aCommBuffer[0])
				{
					case UPDATE_BOOTLOADER:
					{
						RS485_StartUpdateTimeoutTimer(UPDATE_TIMEOUT);
						RS485_StartUpdate();
						
						if(UpdateBootloader() == HAL_OK)
						{
							DISPLAY_BootloaderUpdated();
							SYS_UpdateSuccessSet();
							//FLASH_If_WriteProtectionConfig(FLASHIF_WRP_ENABLE);
						}
						else 
						{
							DISPLAY_BootloaderUpdateFail();
							SYS_UpdateFailSet();
						}
						
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 1;
							aCommBuffer[6] = UPDATE_BOOTLOADER;				
							RS485_ResponsePacketReady();
						}
						break;
					}
					
					
					case RESTART_CONTROLLER:
					{
						if (eTransferMode == TRANSFER_P2P) Serial_PutByte(ACK);
						HAL_Delay(10);
						BootloaderExe();
						break;
					}

					
					case START_BOOTLOADER:
					{
						if (eTransferMode == TRANSFER_P2P) Serial_PutByte(ACK);
						aEepromBuffer[0] = EE_FW_UPDATE_STATUS >> 8;
						aEepromBuffer[1] = EE_FW_UPDATE_STATUS;
						aEepromBuffer[2] = BOOTLOADER_CMD_RUN;
						if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 3, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						BackupOldFirmware();
						BootloaderExe();
						break;
					}	
					
					
					case DOWNLOAD_JOURNAL_1:
					{
						ClearBuffer(aJournal_1, JOURNAL_SIZE);
						
						for(tmp_b = 0; tmp_b < (rs485_packet_data_lenght - 2); tmp_b++)
						{
							aJournal_1[tmp_b] = aCommBuffer[tmp_b + 1];
						}
						
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 1;
							aCommBuffer[6] = DOWNLOAD_JOURNAL_1;				
							RS485_ResponsePacketReady();
						}
						break;
					}
					
					
					case DOWNLOAD_JOURNAL_2:
					{
						ClearBuffer(aJournal_2, JOURNAL_SIZE);
						
						for(tmp_b = 0; tmp_b < (rs485_packet_data_lenght - 2); tmp_b++)
						{
							aJournal_2[tmp_b] = aCommBuffer[tmp_b + 1];
						}
						
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 1;
							aCommBuffer[6] = DOWNLOAD_JOURNAL_2;				
							RS485_ResponsePacketReady();
						}
						break;
					}
					
					
					case SET_BEDDING_REPLACEMENT_PERIOD:
					{
						aEepromBuffer[0] = EE_BEDDING_REPL_PERIOD_ADDRESS >> 8;
						aEepromBuffer[1] = EE_BEDDING_REPL_PERIOD_ADDRESS;
						aEepromBuffer[2] = (aCommBuffer[1] - 48) * 10;
						aEepromBuffer[2] += (aCommBuffer[2] - 48);
                        
						if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 3, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
                        
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 1;
							aCommBuffer[6] = SET_BEDDING_REPLACEMENT_PERIOD;				
							RS485_ResponsePacketReady();
						}
						break;
					}
					
					
					case GET_SYS_STATUS:
					{
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 9;
							aCommBuffer[6] = GET_SYS_STATUS;
							 
							for(tmp_b = 0; tmp_b < 8; tmp_b++) 
							{	
								if(sys_status & (1 << tmp_b)) aCommBuffer[tmp_b + 7] = '1';
								else aCommBuffer[tmp_b + 7] = '0';
							}
							RS485_ResponsePacketReady();
						}
						break;
					}
                    
						
					case SET_RTC_DATE_TIME:
					{
                        ONEWIRE_TimeUpdateSet();
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
							for(tmp_b = 0; tmp_b < LOG_SIZE; tmp_b++)
							{
								aCommBuffer[tmp_b + 7] = aEepromBuffer[tmp_b];
							}
							
							aCommBuffer[5] = tmp_b + 2;
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
					

					case SET_RS485_CONFIG:
					{
						Str2Hex(&aCommBuffer[1], 2, rs485_interface_address);
						Str2Hex(&aCommBuffer[5], 2, rs485_group_address);
						Str2Hex(&aCommBuffer[9], 2, rs485_broadcast_address);
						rs485_interface_baudrate = aCommBuffer[13];
                        
						aEepromBuffer[0] = (EE_RS485_INTERFACE_ADDRESS >> 8);
						aEepromBuffer[1] = EE_RS485_INTERFACE_ADDRESS;
						aEepromBuffer[2] = rs485_interface_address[0];
						aEepromBuffer[3] = rs485_interface_address[1];
						aEepromBuffer[4] = rs485_group_address[0];
						aEepromBuffer[5] = rs485_group_address[1];
						aEepromBuffer[6] = rs485_broadcast_address[0];
						aEepromBuffer[7] = rs485_broadcast_address[1];
						aEepromBuffer[8] = rs485_interface_baudrate;
                        
						if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 9, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 1;
							aCommBuffer[6] = SET_RS485_CONFIG;				
							RS485_ResponsePacketReady();
						}					
						break;
					}
				
					
					case SET_MIFARE_PERMITED_GROUP:
					{
						aEepromBuffer[0] = EE_PERMITED_GROUP_ADDRESS >> 8;
						aEepromBuffer[1] = EE_PERMITED_GROUP_ADDRESS;
						
						for(tmp_b = 1; tmp_b < 17; tmp_b++)
						{
							aEepromBuffer[tmp_b + 1] = aCommBuffer[tmp_b];
						}
						
						if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 18, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 1;
							aCommBuffer[6] = SET_MIFARE_PERMITED_GROUP;
							RS485_ResponsePacketReady();
						}
						break;
					}
					

					case SET_DOUT_STATE:
					{
						for(tmp_b = 0; tmp_b < 9; tmp_b++)
						{
							if(aCommBuffer[tmp_b + 1] == '1') dout_0_7_remote |= (uint16_t) (1 << tmp_b);
							else if(aCommBuffer[tmp_b + 1] == '0') dout_0_7_remote &= (uint16_t) (~(1 << tmp_b));
						}
						
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 1;
							aCommBuffer[6] = SET_DOUT_STATE;
							RS485_ResponsePacketReady();						
						}
						break;
					}

					
					case SET_DISPLAY_BRIGHTNESS:
					{
						lcd_brightness = (aCommBuffer[1] << 8) + aCommBuffer[2];
						__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, lcd_brightness);
                        
						aEepromBuffer[0] = EE_LCD_BRIGHTNESS >> 8;
						aEepromBuffer[1] = EE_LCD_BRIGHTNESS;
						aEepromBuffer[2] = lcd_brightness >> 8;
						aEepromBuffer[3] = lcd_brightness;
                        
						if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 4, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
					
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
							aCommBuffer[5] = 40;
							aCommBuffer[6] = GET_ROOM_STATUS;
							
							aCommBuffer[7] = ROOM_Status + 0x30;
							if(aCommBuffer[7] > 0x39) aCommBuffer[7] += 7;
                            
							aCommBuffer[8] = 'X';       // sensor error
                            
							if(IsONEWIRE_DalasSensorConnected() || IsONEWIRE_ThermostatConnected()) 	
							{
								if(temperature_measured & 0x8000) aCommBuffer[8] = '-';
								else aCommBuffer[8] = '+';								
								temperature = (temperature_measured & 0x0fff);	// delete minus sign from temperature								
								Int2StrSized(&aCommBuffer[9], temperature, 3);	
                                
                                if(temperature_setpoint & 0x80) aCommBuffer[12] = 'E';
                                else if (temperature_difference & 0x80) aCommBuffer[12] = 'O';
                                else aCommBuffer[12] = 'D';

                                if(temperature_setpoint & 0x40) aCommBuffer[13] = 'H';
                                else aCommBuffer[13] = 'C';		

                                temperature = temperature_setpoint & 0x3f;
                                Int2StrSized(&aCommBuffer[14], temperature, 2);
                                temperature = temperature_difference & 0x3f;
                                Int2StrSized(&aCommBuffer[16], temperature, 2);
							}
                            
							for(tmp_b = 0; tmp_b < 8; tmp_b++)
							{
								if(din_0_7 & (1 << tmp_b)) aCommBuffer[tmp_b + 18] = '1';
								else aCommBuffer[tmp_b + 18] = '0';
								if(dout_0_7 & (1 << tmp_b)) aCommBuffer[tmp_b + 26] = '1';
								else aCommBuffer[tmp_b + 26] = '0';
							}
							
							for(tmp_b = 34; tmp_b < 42; tmp_b++)
							{
								 aCommBuffer[tmp_b] = sys_info[tmp_b + 10];
							}
							
							aEepromBuffer[0] = EE_BEDDING_REPL_PERIOD_ADDRESS >> 8;
							aEepromBuffer[1] = EE_BEDDING_REPL_PERIOD_ADDRESS;
							if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
							if (HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, &aCommBuffer[43], 1, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
							
							if(aCommBuffer[43] > 99) aCommBuffer[43] = 99;
							aCommBuffer[42] = ((aCommBuffer[43] / 10) + 48);
							while(aCommBuffer[43] > 9) aCommBuffer[43] -=10;
							aCommBuffer[43] += 48;
                            
                            Int2StrSized(&aCommBuffer[44], thermostat_display_image_id, 2);
							RS485_ResponsePacketReady();						
						}
						break;						
					}
					
					
					case SET_ROOM_STATUS:
					{							
						ROOM_Status = (ROOM_StatusTypeDef) (aCommBuffer[1] - 0x30);
						if(IS_09(aCommBuffer[2])) ROOM_Status += ((aCommBuffer[2] - 0x30) + 9);
							
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 1;
							aCommBuffer[6] = SET_ROOM_STATUS;
							RS485_ResponsePacketReady();						
						}
						break;						
					}
                    
                    
                    case SET_ROOM_THERMOSTAT_IMAGE:
					{
                        thermostat_display_image_id = aCommBuffer[1];
                        thermostat_display_image_time = aCommBuffer[2];
                        thermostat_buzzer_mode = aCommBuffer[3];
                        thermostat_buzzer_repeat_time = aCommBuffer[4];
                        ONEWIRE_UpdateDisplayImageSet();
                        
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 1;
							aCommBuffer[6] = SET_ROOM_THERMOSTAT_IMAGE;
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
							LOGGER_Write();
							DISPLAY_SosAlarmImageDelete();
                            ONEWIRE_UpdateButtonStateSet();
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
					
					
					case SET_ROOM_TEMPERATURE:
					{
						temperature = 0;
						
                        if(aCommBuffer[1] == 'E') temperature |= 0x80;				
						if(aCommBuffer[2] == 'H') temperature |= 0x40;					
						
                        temperature += (aCommBuffer[3] - 48) * 10;
						temperature += (aCommBuffer[4] - 48);
						temperature_setpoint = temperature;
						aEepromBuffer[0] = EE_ROOM_TEMPERATURE_SETPOINT >> 8;
						aEepromBuffer[1] = EE_ROOM_TEMPERATURE_SETPOINT;
						aEepromBuffer[2] = temperature_setpoint;
						
                        if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 3, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);						
						
                        temperature = 0;
						if(aCommBuffer[1] == 'O') temperature |= 0x80;
						temperature += (aCommBuffer[5] - 48) * 100;					
						temperature += (aCommBuffer[6] - 48) * 10;
						temperature += (aCommBuffer[7] - 48);
						temperature_difference = temperature;
                        ONEWIRE_UpdateThermostatParameterSet();
                        
						aEepromBuffer[0] = EE_ROOM_TEMPERATURE_DIFFERENCE >> 8;
						aEepromBuffer[1] = EE_ROOM_TEMPERATURE_DIFFERENCE;
						aEepromBuffer[2] = temperature_difference;
                        
						if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 3, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						
                        if(IsTemperatureRegulatorOn() || IsTemperatureRegulatorOneCycleOn()) HVAC_Contactor_On();
                        
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 1;
							aCommBuffer[6] = SET_ROOM_TEMPERATURE;						
							RS485_ResponsePacketReady();						
						}
						break;
					}
					
					
					case SET_SYSTEM_ID:
					{
						aSystemID[0] = aCommBuffer[1];
						aSystemID[1] = aCommBuffer[2];
						aEepromBuffer[0] = EE_SYSTEM_ID_ADDRESS >> 8;
						aEepromBuffer[1] = EE_SYSTEM_ID_ADDRESS;
						aEepromBuffer[2] = aSystemID[0];
						aEepromBuffer[3] = aSystemID[1];
						if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 4, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 1;
							aCommBuffer[6] = SET_SYSTEM_ID;						
							RS485_ResponsePacketReady();						
						}
						break;
					}
					
					
					case PREVIEW_DISPLAY_IMAGE:
					{
						DISPLAY_PreviewImage();
						
						if(eTransferMode == TRANSFER_P2P)
						{
							aCommBuffer[5] = 1;
							aCommBuffer[6] = PREVIEW_DISPLAY_IMAGE;						
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

		}
		else if(packet_type == STX)
		{
			if((next_packet_number == ((aCommBuffer[0] << 8) + aCommBuffer[1])) && (flash_destination != NULL))
			{
				if(next_packet_number == 1) CRC_ResetDR();						
				SPI_FLASH_WritePage(flash_destination, &aCommBuffer[2], (rs485_packet_data_lenght - 2));
				while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;					
				flash_destination += (rs485_packet_data_lenght - 2);
				if (eTransferMode == TRANSFER_P2P) Serial_PutByte(ACK);
				crc_32_calculated = CRC_Calculate8 (&aCommBuffer[2], (rs485_packet_data_lenght - 2));
				RS485_StartUpdateTimeoutTimer(UPDATE_TIMEOUT);
				
				if(next_packet_number == total_packet_number)
				{
					if(crc_32_calculated == crc_32_file)
					{	
						if(activ_command == DOWNLOAD_FIRMWARE_IMAGE)
						{
							aEepromBuffer[0] = EE_FW_UPDATE_BYTE_CNT >> 8;
							aEepromBuffer[1] = EE_FW_UPDATE_BYTE_CNT;
							aEepromBuffer[2] = (total_bytes_in_file >> 24);
							aEepromBuffer[3] = (total_bytes_in_file >> 16);
							aEepromBuffer[4] = (total_bytes_in_file >> 8);
							aEepromBuffer[5] = (total_bytes_in_file & 0xff);
							if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 6, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
							if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RS485_FUNC, I2C_DRIVER);
						}
						else if(post_process == COPY_NEW_DISPLAY_IMAGE)
						{
							CopyFile(file_copy_source_address, file_copy_destination_address, file_copy_size);
							SYS_UpdateSuccessSet();
						}
						
						SYS_FileTransferSuccessSet();
					}
					else
					{
						SYS_FileTransferFailSet();
					}
					
					post_process = NULL;
					activ_command = NULL;
					flash_destination = NULL;
					RS485_StopUpdate();					
				}
				else ++next_packet_number;
			}
			else if(eTransferMode == TRANSFER_P2P) Serial_PutByte(NAK);
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
			while(huart1.gState != HAL_UART_STATE_READY) continue;
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


void FileTransferSetup(uint32_t storage_address)
{
	RS485_StartUpdate();
	SYS_FileTransferFailReset();
	
	RS485_StartUpdateTimeoutTimer(UPDATE_TIMEOUT);
	receiving_errors = 0;
	next_packet_number = 1;
	activ_command = aCommBuffer[0];
	total_packet_number = (aCommBuffer[1] << 8) + aCommBuffer[2];
	total_bytes_in_file = (aCommBuffer[3] << 24) + (aCommBuffer[4] << 16) + (aCommBuffer[5] << 8) + aCommBuffer[6];
	crc_32_file = (aCommBuffer[7] << 24) + (aCommBuffer[8] << 16) + (aCommBuffer[9] << 8) + aCommBuffer[10];
	
	if((activ_command != DOWNLOAD_FIRMWARE_IMAGE) && (activ_command != DOWNLOAD_BOOTLOADER_IMAGE))
	{
		SYS_ImageUpdateRequestReset();
		flash_destination = EE_NEW_IMAGE_ADDRESS;
		post_process = COPY_NEW_DISPLAY_IMAGE;
		file_copy_source_address = EE_NEW_IMAGE_ADDRESS;
		file_copy_destination_address = storage_address;
		file_copy_size = total_bytes_in_file;
	}
	else
	{
		SYS_FirmwareUpdateRequestReset();
		flash_destination = storage_address;
	}
	
	if(eTransferMode == TRANSFER_P2P)
	{
		aCommBuffer[5] = 1;
		aCommBuffer[6] = activ_command;						
		RS485_ResponsePacketReady();						
	}
}

void FormatFileStorage(uint32_t start_address, uint8_t number_of_blocks)
{
	uint8_t bcnt;
	uint32_t address;
	
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
}

void CopyFile(uint32_t file_source_address, uint32_t file_destination_address, uint32_t file_size)
{
	uint8_t n_page;
	uint32_t byte_copied;
	
	n_page = file_size / 0x00010000;
	if(file_size > (n_page * 0x00010000)) n_page++;
	else n_page = 1;
	FormatFileStorage(file_destination_address, n_page);
	
	byte_copied = 0;
	
	while(byte_copied < file_size)
	{
		if((byte_copied + EE_BUFFER_SIZE) > file_size)
		{
			SPI_FLASH_ReadPage(file_source_address, aEepromBuffer, (file_size - byte_copied));
			SPI_FLASH_WritePage(file_destination_address, aEepromBuffer, (file_size - byte_copied));
			while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;
			byte_copied = file_size;
		}
		else
		{
			SPI_FLASH_ReadPage(file_source_address, aEepromBuffer, EE_BUFFER_SIZE);
			SPI_FLASH_WritePage(file_destination_address, aEepromBuffer, EE_BUFFER_SIZE);
			while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;	
			file_destination_address += EE_BUFFER_SIZE;
			file_source_address += EE_BUFFER_SIZE;			
			byte_copied += EE_BUFFER_SIZE;
		}		
	}
}

void BackupOldFirmware(void)
{
	uint32_t fl_destination;
	uint8_t *fl_address;
	uint32_t b_total;
	uint32_t b_cnt; 
	
	b_cnt = 0;
	b_total = 0;
	fl_address = (uint8_t *)APPLICATION_ADDRESS;
	fl_destination = EE_OLD_FIRMWARE_ADDRESS;
	
	FormatFileStorage(fl_destination, 1);
	
	while(b_total < USER_FLASH_SIZE)
	{
		while(b_cnt < 256)
		{
			aCommBuffer[b_cnt] = *fl_address++;
			b_cnt++;
			b_total++;
		}
		
		SPI_FLASH_WritePage(fl_destination, aCommBuffer, 256);
		while(SPI_FLASH_ReadStatusRegister() & STATUS_REG_BUSY_MASK) continue;					
		fl_destination += 256;
		b_cnt = 0;
	}
}

uint32_t UpdateBootloader(void)
{
	FLASH_EraseInitTypeDef FLASH_EraseInit;
	HAL_StatusTypeDef status = HAL_OK;
	uint32_t page_erase_error;
	uint32_t fl_destination;
	uint32_t fl_address;
	uint32_t ram_source;
	uint32_t b_cnt;
	
	HAL_FLASH_Unlock();
	FLASH_EraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
	FLASH_EraseInit.PageAddress = BOOTLOADER_ADDRESS;
	FLASH_EraseInit.Banks = FLASH_BANK_1;
	FLASH_EraseInit.NbPages = 12;	
	status = HAL_FLASHEx_Erase(&FLASH_EraseInit, &page_erase_error);
	HAL_FLASH_Lock();
	
	if(status != HAL_OK) return HAL_ERROR;
	
	fl_destination = BOOTLOADER_ADDRESS;
	fl_address = EE_NEW_BOOTLOADER_ADDRESS;
	b_cnt = 0;
	
	while(b_cnt <  BOOTLOADER_FLASH_SIZE)
	{		
		SPI_FLASH_ReadPage(fl_address, aCommBuffer,  COM_BUFFER_SIZE);		
		ram_source = (uint32_t) aCommBuffer;
		
		if (FLASH_If_Write(fl_destination, (uint32_t*) ram_source, (COM_BUFFER_SIZE / 4)) == FLASHIF_OK)                   
		{
			fl_destination += COM_BUFFER_SIZE;
		}
		else return HAL_ERROR;
		
		fl_address += COM_BUFFER_SIZE;
		b_cnt += COM_BUFFER_SIZE;
		
		if((b_cnt + COM_BUFFER_SIZE) > BOOTLOADER_FLASH_SIZE)
		{
			SPI_FLASH_ReadPage(fl_address, aCommBuffer, (BOOTLOADER_FLASH_SIZE - b_cnt));			
			ram_source = (uint32_t) aCommBuffer;
			
			if (FLASH_If_Write(fl_destination, (uint32_t*) ram_source, ((BOOTLOADER_FLASH_SIZE - b_cnt) / 4)) == FLASHIF_OK)                   
			{
				fl_destination += COM_BUFFER_SIZE;
			}
			else return HAL_ERROR;
			
			b_cnt = BOOTLOADER_FLASH_SIZE;
		}
	}
	
	return HAL_OK;
}


uint32_t FLASH_If_Write(uint32_t destination, uint32_t *p_source, uint32_t length)
{
  uint32_t i = 0;

  /* Unlock the Flash to enable the flash control register access *************/
  HAL_FLASH_Unlock();

  for (i = 0; (i < length) && (destination <= (USER_FLASH_END_ADDRESS-4)); i++)
  {
    /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
       be done by word */ 
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, destination, *(uint32_t*)(p_source+i)) == HAL_OK)      
    {
     /* Check the written value */
      if (*(uint32_t*)destination != *(uint32_t*)(p_source+i))
      {
        /* Flash content doesn't match SRAM content */
        return(FLASHIF_WRITINGCTRL_ERROR);
      }
      /* Increment FLASH destination address */
      destination += 4;
    }
    else
    {
      /* Error occurred while writing data in Flash memory */
      return (FLASHIF_WRITING_ERROR);
    }
  }

  /* Lock the Flash to disable the flash control register access (recommended
     to protect the FLASH memory against possible unwanted operation) *********/
  HAL_FLASH_Lock();

  return (FLASHIF_OK);
}


uint32_t FLASH_If_WriteProtectionConfig(uint32_t protectionstate)
{
  uint32_t ProtectedPAGE = 0x0;
  FLASH_OBProgramInitTypeDef config_new, config_old;
  HAL_StatusTypeDef result = HAL_OK;
  

  /* Get pages write protection status ****************************************/
  HAL_FLASHEx_OBGetConfig(&config_old);

  /* The parameter says whether we turn the protection on or off */
  config_new.WRPState = (protectionstate == FLASHIF_WRP_ENABLE ? OB_WRPSTATE_ENABLE : OB_WRPSTATE_DISABLE);

  /* We want to modify only the Write protection */
  config_new.OptionType = OPTIONBYTE_WRP;
  
  /* No read protection, keep BOR and reset settings */
  config_new.RDPLevel = OB_RDP_LEVEL_0;
  config_new.USERConfig = config_old.USERConfig;  
  /* Get pages already write protected ****************************************/
  ProtectedPAGE = config_old.WRPPage | FLASH_PAGE_TO_BE_PROTECTED;

  /* Unlock the Flash to enable the flash control register access *************/ 
  HAL_FLASH_Unlock();

  /* Unlock the Options Bytes *************************************************/
  HAL_FLASH_OB_Unlock();
  
  /* Erase all the option Bytes ***********************************************/
  result = HAL_FLASHEx_OBErase();
    
  if (result == HAL_OK)
  {
    config_new.WRPPage    = ProtectedPAGE;
    result = HAL_FLASHEx_OBProgram(&config_new);
  }
  
  return (result == HAL_OK ? FLASHIF_OK: FLASHIF_PROTECTION_ERRROR);
}


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
}


void Serial_PutString(uint8_t *p_string)
{   
    uint16_t length = 0;

    while (p_string[length] != '\0') length++;
	RS485_DirTx();
	RS485_StartTimer(PACKET_TRANSFER_TIMEOUT);
	if (HAL_UART_Transmit(&huart1, p_string, length, PACKET_TRANSFER_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, USART_DRIVER);
	while(huart1.gState != HAL_UART_STATE_READY) continue;
	RS485_StopTimer(); 
    RS485_DirRx();
}


void Serial_PutByte(uint8_t param)
{ 
	RS485_DirTx();
	RS485_StartTimer(PACKET_TRANSFER_TIMEOUT);
    if (HAL_UART_Transmit(&huart1, &param, 1, PACKET_TRANSFER_TIMEOUT) != HAL_OK) Error_Handler(RS485_FUNC, USART_DRIVER);
	while(huart1.gState != HAL_UART_STATE_READY) continue;
	RS485_StopTimer();
	RS485_DirRx();
	
}

/******************************   END OF FILE  **********************************/
