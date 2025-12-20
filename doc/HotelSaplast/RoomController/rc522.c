/**
 ******************************************************************************
 * File Name          : mfrc522.c
 * Date               : 28/02/2016 23:16:19
 * Description        : mifare RC522 software modul
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
#include "rs485.h"
#include "rc522.h"
#include "display.h"


/* Imported Type  ------------------------------------------------------------*/
/* Imported Variable  --------------------------------------------------------*/
extern uint8_t rs485_interface_address[];


/* Imported Function  --------------------------------------------------------*/
/* Private Define ------------------------------------------------------------*/
#define DEFAULT_SYSTEM_ID					0xabcd
#define RC522_SYSTEM_INIT_DISABLE_TIME		8901	// 8 s on power up reader is disbled 
#define RC522_PROCESS_TIME					234		// 234 ms read rate
#define RC522_POWER_VALID_GROUP_PERMITED	2		// 2 hours room power valid for menager, service, handmaid card
#define RC522_MAX_LEN						16      // buffer byte lenght
#define RC522_HANDMAID_CARD_DELAY			2345	// reset handmaid card status
#define RC522_READ    						0x7f	// RC522 i2c read address
#define RC522_WRITE   						0x7e	// RC522 i2c write address
#define RC522_TIMEOUT						5		// RC522 i2c 5 ms transfer timeout limit
#define RC522_TRIALS						50		// RC522 i2c 100 times operation retry
#define RC522_BLOCK_SIZE              		16 
#define CARD_PENDING						NULL	// card data status type
#define CARD_VALID							0x06	// card data status type
#define CARD_INVALID						0x15	// card data status type
#define CARD_DATA_FORMATED					0x7f	// card data status type
/** ==========================================================================*/
/**			M I F A R E		C A R D		S E C T O R		A D D R E S S E		  */
/** ==========================================================================*/
#define SECTOR_0							0x00 
#define SECTOR_1							0x04 
#define SECTOR_2							0x08 
#define SECTOR_3							0x0c 
#define SECTOR_4							0x10 
#define SECTOR_5							0x14
#define SECTOR_6							0x18
#define SECTOR_7							0x1c
#define SECTOR_8							0x20
#define SECTOR_9							0x24
#define SECTOR_10							0x28
#define SECTOR_11							0x2c
#define SECTOR_12							0x30
#define SECTOR_13							0x34
#define SECTOR_14							0x38
#define SECTOR_15							0x3c
#define SECTOR_16							0x40
#define SECTOR_17							0x44
#define SECTOR_18							0x48
#define SECTOR_19							0x4c
#define SECTOR_20							0x50
#define SECTOR_21							0x54
#define SECTOR_22							0x58
#define SECTOR_23							0x5c
#define SECTOR_24							0x60
#define SECTOR_25							0x64
#define SECTOR_26							0x6c
#define SECTOR_27							0x70
#define SECTOR_28							0x74
#define SECTOR_29							0x78
#define SECTOR_30							0x7c
#define SECTOR_31             				0x80
/** ==========================================================================*/
/**			R C 5 2 2			C O M M A N D			L I S T				  */
/** ==========================================================================*/
#define PCD_IDLE							0x00   //NO action; Cancel the current command
#define PCD_AUTHENT							0x0E   //Authentication Key
#define PCD_RECEIVE							0x08   //Receive Data
#define PCD_TRANSMIT						0x04   //Transmit data
#define PCD_TRANSCEIVE						0x0C   //Transmit and receive data,
#define PCD_RESETPHASE						0x0F   //Reset
#define PCD_CALCCRC							0x03   //CRC Calculate
/* Mifare_One card command word */
#define PICC_REQIDL							0x26   // find the antenna area does not enter hibernation
#define PICC_REQALL							0x52   // find all the cards antenna area
#define PICC_ANTICOLL						0x93   // anti-collision
#define PICC_SELECTTAG						0x93   // election card
#define PICC_AUTHENT1A						0x60   // authentication key A
#define PICC_AUTHENT1B						0x61   // authentication key B
#define PICC_READ							0x30   // Read Block
#define PICC_WRITE							0xA0   // write block
#define PICC_DECREMENT						0xC0   // debit
#define PICC_INCREMENT						0xC1   // recharge
#define PICC_RESTORE						0xC2   // transfer block data to the buffer
#define PICC_TRANSFER						0xB0   // save the data in the buffer
#define PICC_HALT							0x50   // Sleep
/** ==========================================================================*/
/**			R C 5 2 2			R E G I S T E R			L I S T				  */
/** ==========================================================================*/
//Page 0: Command and Status
#define RC522_REG_RESERVED00				0x00    
#define RC522_REG_COMMAND					0x01    
#define RC522_REG_COMM_IE_N					0x02    
#define RC522_REG_DIV1_EN					0x03    
#define RC522_REG_COMM_IRQ					0x04    
#define RC522_REG_DIV_IRQ					0x05
#define RC522_REG_ERROR						0x06    
#define RC522_REG_STATUS1					0x07    
#define RC522_REG_STATUS2					0x08    
#define RC522_REG_FIFO_DATA					0x09
#define RC522_REG_FIFO_LEVEL				0x0A
#define RC522_REG_WATER_LEVEL				0x0B
#define RC522_REG_CONTROL					0x0C
#define RC522_REG_BIT_FRAMING				0x0D
#define RC522_REG_COLL						0x0E
#define RC522_REG_RESERVED01				0x0F
//Page 1: Command 
#define RC522_REG_RESERVED10				0x10
#define RC522_REG_MODE						0x11
#define RC522_REG_TX_MODE					0x12
#define RC522_REG_RX_MODE					0x13
#define RC522_REG_TX_CONTROL				0x14
#define RC522_REG_TX_AUTO					0x15
#define RC522_REG_TX_SELL					0x16
#define RC522_REG_RX_SELL					0x17
#define RC522_REG_RX_THRESHOLD				0x18
#define RC522_REG_DEMOD						0x19
#define RC522_REG_RESERVED11				0x1A
#define RC522_REG_RESERVED12				0x1B
#define RC522_REG_MIFARE					0x1C
#define RC522_REG_RESERVED13				0x1D
#define RC522_REG_RESERVED14				0x1E
#define RC522_REG_SERIALSPEED				0x1F
//Page 2: CFG    
#define RC522_REG_RESERVED20				0x20  
#define RC522_REG_CRC_RESULT_M				0x21
#define RC522_REG_CRC_RESULT_L				0x22
#define RC522_REG_RESERVED21				0x23
#define RC522_REG_MOD_WIDTH					0x24
#define RC522_REG_RESERVED22				0x25
#define RC522_REG_RF_CFG					0x26
#define RC522_REG_GS_N						0x27
#define RC522_REG_CWGS_PREG					0x28
#define RC522_REG_MODGS_PREG				0x29
#define RC522_REG_T_MODE					0x2A
#define RC522_REG_T_PRESCALER				0x2B
#define RC522_REG_T_RELOAD_H				0x2C
#define RC522_REG_T_RELOAD_L				0x2D
#define RC522_REG_T_COUNTER_VALUE_H			0x2E
#define RC522_REG_T_COUNTER_VALUE_L			0x2F
//Page 3:TestRegister 
#define RC522_REG_RESERVED30				0x30
#define RC522_REG_TEST_SEL1					0x31
#define RC522_REG_TEST_SEL2					0x32
#define RC522_REG_TEST_PIN_EN				0x33
#define RC522_REG_TEST_PIN_VALUE			0x34
#define RC522_REG_TEST_BUS					0x35
#define RC522_REG_AUTO_TEST					0x36
#define RC522_REG_VERSION					0x37
#define RC522_REG_ANALOG_TEST				0x38
#define RC522_REG_TEST_ADC1					0x39  
#define RC522_REG_TEST_ADC2					0x3A   
#define RC522_REG_TEST_ADC0					0x3B   
#define RC522_REG_RESERVED31				0x3C   
#define RC522_REG_RESERVED32				0x3D
#define RC522_REG_RESERVED33				0x3E   
#define RC522_REG_RESERVED34				0x3F


/* Private Type --------------------------------------------------------------*/
typedef enum 
{
	MI_ERR			= 0x00,
	MI_OK 			= 0x01,
	MI_NOTAGERR		= 0x02,
	MI_SKIP_OVER	= 0x03

} RC522_StatusTypeDef;

typedef struct
{
    uint8_t Block_0[RC522_BLOCK_SIZE];
    uint8_t Block_1[RC522_BLOCK_SIZE];
    uint8_t Block_2[RC522_BLOCK_SIZE];
    uint8_t Block_3[RC522_BLOCK_SIZE];
	
} RC522_SectorTypeDef;

RC522_SectorTypeDef Sector_0;
RC522_SectorTypeDef Sector_1;
RC522_SectorTypeDef Sector_2;
RC522_SectorTypeDef Sector_3;
RC522_SectorTypeDef Sector_4;
RC522_SectorTypeDef Sector_5;
RC522_SectorTypeDef Sector_6;
RC522_SectorTypeDef Sector_7;
RC522_CardDataTypeDef sCard;


/* Private Variable ----------------------------------------------------------*/
__IO uint32_t mifare_timer;
__IO uint32_t handmaid_card_timer;
uint32_t mifare_process_flags;
uint8_t aSystemID[2];
uint8_t aPermitedAddresse[8][2];
uint8_t aRC522_RxBuffer[RC522_MAX_LEN];
uint8_t aRC522_TxBuffer[RC522_MAX_LEN];
uint8_t aMifareKeyA[6]; 
uint8_t aMifareKeyB[6]; 
uint8_t str[RC522_MAX_LEN];
uint8_t aCardID[5];
uint8_t aCardSerial[5];
uint8_t aResetCard[5] = {0x34, 0x75, 0xa6, 0xa7, 0x40};
uint8_t rc522_config;
uint8_t i;


/* Private Macro -------------------------------------------------------------*/
#define RC522_HandmaidReentranceEnable()		(mifare_process_flags |= 0x00000001)	
#define RC522_HandmaidReentranceDisable()		(mifare_process_flags &= 0xfffffffe)
#define IsRC522_HandmaidReentranceActiv()		(mifare_process_flags & 0x00000001)


/* Private Function Prototype ------------------------------------------------*/
RC522_StatusTypeDef RC522_Check(uint8_t* id);
RC522_StatusTypeDef RC522_Compare(uint8_t* aCardID, uint8_t* CompareID);
void RC522_WriteRegister(uint8_t addr, uint8_t val);
uint8_t RC522_ReadRegister(uint8_t addr);
void RC522_SetBitMask(uint8_t reg, uint8_t mask);
void RC522_ClearBitMask(uint8_t reg, uint8_t mask);
void RC522_AntennaOn(void);
void RC522_AntennaOff(void);
RC522_StatusTypeDef RC522_Reset(void);
RC522_StatusTypeDef RC522_Request(uint8_t reqMode, uint8_t* TagType);
RC522_StatusTypeDef RC522_ToCard(uint8_t command, uint8_t* sendData, uint8_t sendLen, uint8_t* backData, uint16_t* backLen);
RC522_StatusTypeDef RC522_Anticoll(uint8_t* serNum);
void RC522_CalculateCRC(uint8_t* pIndata, uint8_t len, uint8_t* pOutData);
uint8_t RC522_SelectTag(uint8_t* serNum);
RC522_StatusTypeDef RC522_Auth(uint8_t authMode, uint8_t BlockAddr, uint8_t* Sectorkey, uint8_t* serNum);
RC522_StatusTypeDef RC522_Read(uint8_t blockAddr, uint8_t* recvData);
RC522_StatusTypeDef RC522_Write(uint8_t blockAddr, uint8_t* writeData);
void RC522_Halt(void);
RC522_StatusTypeDef RC522_ReadCard(void);
void RC522_WriteCard(void);
RC522_StatusTypeDef RC522_VerifyData(void);
void RC522_ClearData(void);


/* Program Code  -------------------------------------------------------------*/
void RC522_Init(void) 
{
	//RC522_PinSelectHigh();
	RC522_RST_SetLow(); 
	DIO_SetOuput();	
	HAL_Delay(10);    
	RC522_RST_SetHigh();
	DIO_SetOuput();
	HAL_Delay(50);
	RC522_WriteRegister(RC522_REG_COMMAND, PCD_RESETPHASE);	
	RC522_WriteRegister(RC522_REG_T_MODE, 0x8d);
	RC522_WriteRegister(RC522_REG_T_PRESCALER, 0x3e);
	RC522_WriteRegister(RC522_REG_T_RELOAD_H, 0);
	RC522_WriteRegister(RC522_REG_T_RELOAD_L, 30);		
	RC522_WriteRegister(RC522_REG_TX_AUTO, 0x40);
	RC522_WriteRegister(RC522_REG_MODE, 0x3D);
	RC522_AntennaOn();
	RC522_StartTimer(RC522_SYSTEM_INIT_DISABLE_TIME);
	RC522_HandmaidReentranceDisable();

}// End of mifare modul init


void RC522_Service(void)
{	
	static uint8_t handmaid_card_cycles = 0;
	
	if(IsRC522_HandmaidCardTimerExpired()) 
	{
		if ((handmaid_card_cycles == 1) && (ROOM_Status == ROOM_HANDMAID_IN)) RC522_HandmaidReentranceEnable();
		handmaid_card_cycles = 0;
	}
		
	if(eComState == COM_PACKET_RECEIVED) RS485_Service();
    if(!IsRC522_TimerExpired() || IsRS485_UpdateActiv()) return;
	

	if (RC522_Check(aCardSerial) == MI_OK)
	{
		RC522_ClearData();
		RC522_ReadCard();
		RC522_VerifyData();
		
		if ((sCard.system_id != SYSTEM_ID_INVALID) && \
			(sCard.system_id != SYSTEM_ID_DATA_INVALID) && \
			((sCard.card_status == CARD_VALID) || \
			(sCard.user_group == CARD_USER_GROUP_MANAGER) || \
			(sCard.user_group == CARD_USER_GROUP_SERVICE) || \
			(sCard.user_group == CARD_USER_GROUP_HANDMAID)))
		{	
			if(sCard.user_group == CARD_USER_GROUP_GUEST) LogEvent.log_event = GUEST_CARD_VALID;
			else if(sCard.user_group == CARD_USER_GROUP_MANAGER) LogEvent.log_event = MANAGER_CARD;
			else if(sCard.user_group == CARD_USER_GROUP_SERVICE) LogEvent.log_event = SERVICE_CARD;
			else if(sCard.user_group == CARD_USER_GROUP_HANDMAID) LogEvent.log_event = HANDMAID_CARD_VALID;
				
			LogEvent.log_card_id[0] = sCard.aCardID[0];
			LogEvent.log_card_id[1] = sCard.aCardID[1];
			LogEvent.log_card_id[2] = sCard.aCardID[2];
			LogEvent.log_card_id[3] = sCard.aCardID[3];
			LogEvent.log_card_id[4] = sCard.aCardID[4];
			
			if(sCard.user_group == CARD_USER_GROUP_GUEST)
			{
				if(ROOM_Status == ROOM_OUT_OF_ORDER)
				{
					handmaid_card_cycles = 1;
					DISPLAY_RoomOutOfServiceImage();
					SignalBuzzer = BUZZ_CARD_INVALID;
					RC522_StartTimer(RC522_CARD_INVALID_EVENT_TIME);
				}
				else
				{
					aEepromBuffer[0] = EE_ROOM_POWER_TIMEOUT >> 8;
					aEepromBuffer[1] = EE_ROOM_POWER_TIMEOUT;
					aEepromBuffer[2] = sCard.aExpiryTime[0];
					aEepromBuffer[3] = sCard.aExpiryTime[1];
					aEepromBuffer[4] = sCard.aExpiryTime[2];
					aEepromBuffer[5] = sCard.aExpiryTime[3];
					aEepromBuffer[6] = sCard.aExpiryTime[4];
					SIGNAL_ExpiryTimeFromGuestCardSet();
					if((ROOM_Status == ROOM_IDLE) || (ROOM_Status == ROOM_READY)) ROOM_Status = ROOM_BUSY;
					if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 7, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
					if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);	
					
					i = 0;

					while(i < 6)
					{
						aRoomPowerExpiryDateTime[i] = sCard.aExpiryTime[i];
						i++;
					}
										
					LOGGER_Write();
				}	
			}
			else if(((sCard.user_group == CARD_USER_GROUP_HANDMAID) && !IsIndorCardReaderActiv() && !IsDonNotDisturbActiv()) || \
					(sCard.user_group == CARD_USER_GROUP_MANAGER) || \
					(sCard.user_group == CARD_USER_GROUP_SERVICE))
			{
//				HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BCD);
//				HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BCD);
				
				if((sCard.user_group == CARD_USER_GROUP_MANAGER) || \
					(sCard.user_group == CARD_USER_GROUP_SERVICE))
				{
					aRoomPowerExpiryDateTime[0] = date.Date;
					aRoomPowerExpiryDateTime[1] = date.Month;
					aRoomPowerExpiryDateTime[2] = date.Year;
					aRoomPowerExpiryDateTime[4] = time.Minutes;
					SIGNAL_ExpiryTimeFromGuestCardReset();
					
					if((time.Hours + RC522_POWER_VALID_GROUP_PERMITED) < 0x24)
					{
						aRoomPowerExpiryDateTime[3] = (time.Hours + RC522_POWER_VALID_GROUP_PERMITED);
					}
					else
					{
						aRoomPowerExpiryDateTime[3] = 0x24;
					}			
					
					LOGGER_Write();
				}
				else if (sCard.user_group == CARD_USER_GROUP_HANDMAID)
				{
					if(ROOM_Status == ROOM_OUT_OF_ORDER)
					{
						handmaid_card_cycles = 1;
						DISPLAY_RoomOutOfServiceImage();
						SignalBuzzer = BUZZ_CARD_INVALID;
						RC522_StartTimer(RC522_CARD_INVALID_EVENT_TIME);
					}
					else if ((sCard.aExpiryTime[0] == EXPIRY_TIME_INVALID) ||	\
						(sCard.aExpiryTime[1] == EXPIRY_TIME_INVALID) ||	\
						(sCard.aExpiryTime[2] == EXPIRY_TIME_INVALID) || 	\
						(sCard.aExpiryTime[3] == EXPIRY_TIME_INVALID) || 	\
						(sCard.aExpiryTime[4] == EXPIRY_TIME_INVALID) ||	\
						(sCard.aExpiryTime[5] == EXPIRY_TIME_INVALID))
					{
						handmaid_card_cycles = 1;
						DISPLAY_TimeExpiredImage();
						SignalBuzzer = BUZZ_CARD_INVALID;
						RC522_StartTimer(RC522_CARD_INVALID_EVENT_TIME);						
						LogEvent.log_event = CARD_EXPIRED;
						LOGGER_Write();
					}
					else
					{
						++handmaid_card_cycles;
						SignalBuzzer = BUZZ_HANDMAID_ENTRY;
						RC522_StartTimer(RC522_CARD_INVALID_EVENT_TIME);
						RC522_HandmaidCardStartTimer(RC522_HANDMAID_CARD_DELAY);
						
						if ((ROOM_Status == ROOM_CLEANING) || (ROOM_Status == ROOM_BEDDING_REPLACEMENT) ||(ROOM_Status == ROOM_GENERAL_CLEANING))
						{
							handmaid_card_cycles = 0;
							ROOM_Status = ROOM_HANDMAID_IN;						
							aRoomPowerExpiryDateTime[0] = date.Date;
							aRoomPowerExpiryDateTime[1] = date.Month;
							aRoomPowerExpiryDateTime[2] = date.Year;
							aRoomPowerExpiryDateTime[4] = time.Minutes;
							SIGNAL_ExpiryTimeFromGuestCardReset();
							
							if((time.Hours + RC522_POWER_VALID_GROUP_PERMITED) < 0x24)
							{
								aRoomPowerExpiryDateTime[3] = (time.Hours + RC522_POWER_VALID_GROUP_PERMITED);
							}
							else
							{
								aRoomPowerExpiryDateTime[3] = 0x24;
							}
																					
							LOGGER_Write();
						}
						else if(handmaid_card_cycles == 3)
						{
							ROOM_Status = ROOM_READY;
							SignalBuzzer = BUZZ_HANDMAID_FINISH;
							RC522_StartTimer(RC522_CARD_VALID_EVENT_TIME);														
							LogEvent.log_event = HANDMAID_SERVICE_END;
							LOGGER_Write();
							aEepromBuffer[0] = EE_ROOM_STATUS_ADDRESS >> 8;
							aEepromBuffer[1] = EE_ROOM_STATUS_ADDRESS;
							aEepromBuffer[2] = ROOM_Status;
							if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 3, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
							if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
						}
						else if (ROOM_Status != ROOM_HANDMAID_IN)
						{
							handmaid_card_cycles = 0;	
								
							if(IsHandmaidRequested())
							{
								ROOM_OldStatus = ROOM_BUSY;
								ROOM_Status = ROOM_FORCING_DND;
							}
							else
							{
								ROOM_Status = ROOM_READY;
								aEepromBuffer[0] = EE_ROOM_STATUS_ADDRESS >> 8;
								aEepromBuffer[1] = EE_ROOM_STATUS_ADDRESS;
								aEepromBuffer[2] = ROOM_Status;
								if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 3, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
								if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
								aRoomPowerExpiryDateTime[0] = date.Date;
								aRoomPowerExpiryDateTime[1] = date.Month;
								aRoomPowerExpiryDateTime[2] = date.Year;
								aRoomPowerExpiryDateTime[4] = time.Minutes;
								SIGNAL_ExpiryTimeFromGuestCardReset();
								
								if((time.Hours + RC522_POWER_VALID_GROUP_PERMITED) < 0x24)
								{
									aRoomPowerExpiryDateTime[3] = (time.Hours + RC522_POWER_VALID_GROUP_PERMITED);
								}
								else
								{
									aRoomPowerExpiryDateTime[3] = 0x24;
								}								
							}

							LOGGER_Write();
						}
					}	
				}
			}
			
			if((sCard.user_group == CARD_USER_GROUP_HANDMAID) && \
				(IsIndorCardReaderActiv() || IsDonNotDisturbActiv()))
			{
				if(ROOM_Status == ROOM_OUT_OF_ORDER)
				{
					handmaid_card_cycles = 1;
					DISPLAY_RoomOutOfServiceImage();
					SignalBuzzer = BUZZ_CARD_INVALID;
					RC522_StartTimer(RC522_CARD_INVALID_EVENT_TIME);
				}
				else if ((sCard.aExpiryTime[0] == EXPIRY_TIME_INVALID) ||	\
						(sCard.aExpiryTime[1] == EXPIRY_TIME_INVALID) ||	\
						(sCard.aExpiryTime[2] == EXPIRY_TIME_INVALID) || 	\
						(sCard.aExpiryTime[3] == EXPIRY_TIME_INVALID) || 	\
						(sCard.aExpiryTime[4] == EXPIRY_TIME_INVALID) ||	\
						(sCard.aExpiryTime[5] == EXPIRY_TIME_INVALID))					
				{						
					handmaid_card_cycles = 1;						
					DISPLAY_TimeExpiredImage();						
					SignalBuzzer = BUZZ_CARD_INVALID;						
					RC522_StartTimer(RC522_CARD_INVALID_EVENT_TIME);					
					LogEvent.log_event = CARD_EXPIRED;
					LOGGER_Write();
				}					
				else					
				{						
					RC522_HandmaidCardStartTimer(RC522_HANDMAID_CARD_DELAY);
				
					if(++handmaid_card_cycles == 3)
					{
						handmaid_card_cycles = 0;
						SignalBuzzer = BUZZ_HANDMAID_FINISH;
						RC522_StartTimer(RC522_CARD_VALID_EVENT_TIME);
						LogEvent.log_event = HANDMAID_SERVICE_END;
						LOGGER_Write();
						aEepromBuffer[0] = EE_ROOM_STATUS_ADDRESS >> 8;
						aEepromBuffer[1] = EE_ROOM_STATUS_ADDRESS;
						aEepromBuffer[2] = ROOM_Status;
						if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 3, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
						if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
						
						if(IsHandmaidRequested())
						{
							ROOM_OldStatus = ROOM_BUSY;
							ROOM_Status = ROOM_FORCING_DND;
						}
						else
						{
							ROOM_Status = ROOM_BUSY;
						}
					}
					else
					{
						SignalBuzzer = BUZZ_HANDMAID_ENTRY;
						RC522_StartTimer(RC522_CARD_INVALID_EVENT_TIME);
					}
				}
			}
			else if(handmaid_card_cycles == 0)
			{
				/**
				*	will use rc522 rx and tx buffer as temp for displaying user data
				*/
				i = 0;

				while(i < 16) 
				{
					aRC522_RxBuffer[i] = NULL;	// clear rx buffer for display
					aRC522_TxBuffer[i] = NULL;	// clear tx buffer for display
					i++;
				}

				i = 0;

				while((i < 16) && (Sector_0.Block_1[i] != 0xff))	// copy user data from card to display buffer
				{	
					aRC522_RxBuffer[i] = Sector_0.Block_1[i];
					i++;
				}

				if((aRC522_RxBuffer[0] != NULL) && (Sector_2.Block_0[11] == 'M'))
				{
					aRC522_RxBuffer[0] |= 0x80;
				}

				i = 0;

				while((i < 16) && (Sector_0.Block_2[i] != 0xff))	// copy user data from card to display buffer
				{	
					aRC522_TxBuffer[i] = Sector_0.Block_2[i];
					i++;
				}
				
				DoorLockCoil_On();
				PowerContactor_On();
				DISPLAY_CardValidImage();			
				SignalBuzzer = BUZZ_CARD_VALID;
				RC522_StartTimer(RC522_CARD_VALID_EVENT_TIME);
			}
			else if(handmaid_card_cycles == 3)
			{
				handmaid_card_cycles = 0;
			}
		}
		else
		{
			handmaid_card_cycles = 0;
			SignalBuzzer = BUZZ_CARD_INVALID;
			RC522_StartTimer(RC522_CARD_INVALID_EVENT_TIME);
			
			if (sCard.system_id == SYSTEM_ID_INVALID) 
			{
				DISPLAY_WrongRoomImage();
				LogEvent.log_event = WRONG_SYSTEM_ID;
			}
			else if(sCard.user_group == CARD_USER_GROUP_GUEST) 
			{
				if (sCard.controller_id == CONTROLLER_ID_INVALID) 
				{
					DISPLAY_WrongRoomImage();
					LogEvent.log_event = WRONG_ROOM;
				}
				else if((sCard.aExpiryTime[0] == EXPIRY_TIME_INVALID) || \
						(sCard.aExpiryTime[1] == EXPIRY_TIME_INVALID) || \
						(sCard.aExpiryTime[2] == EXPIRY_TIME_INVALID) || \
						(sCard.aExpiryTime[3] == EXPIRY_TIME_INVALID) || \
						(sCard.aExpiryTime[4] == EXPIRY_TIME_INVALID) || \
						(sCard.aExpiryTime[5] == EXPIRY_TIME_INVALID))
				{
					DISPLAY_TimeExpiredImage();
					LogEvent.log_event = CARD_EXPIRED;
				}
				else
				{
					DISPLAY_CardInvalidImage();
					LogEvent.log_event = GUEST_CARD_INVALID;
				}
			}
			else 
			{
				LogEvent.log_event = UNKNOWN_CARD;
				DISPLAY_CardInvalidImage();
			}
			
			LogEvent.log_card_id[0] = sCard.aCardID[0];
			LogEvent.log_card_id[1] = sCard.aCardID[1];
			LogEvent.log_card_id[2] = sCard.aCardID[2];
			LogEvent.log_card_id[3] = sCard.aCardID[3];
			LogEvent.log_card_id[4] = sCard.aCardID[4];			
			LOGGER_Write();			
		}		
	}
	else if (IsRC522_HandmaidReentranceActiv())
	{
//		HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BCD);
//		HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BCD);
							
		aRoomPowerExpiryDateTime[0] = date.Date;
		aRoomPowerExpiryDateTime[1] = date.Month;
		aRoomPowerExpiryDateTime[2] = date.Year;
		aRoomPowerExpiryDateTime[4] = time.Minutes;
		SIGNAL_ExpiryTimeFromGuestCardReset();
		
		if((time.Hours + RC522_POWER_VALID_GROUP_PERMITED) < 0x24)
		{
			aRoomPowerExpiryDateTime[3] = (time.Hours + RC522_POWER_VALID_GROUP_PERMITED);
		}
		else
		{
			aRoomPowerExpiryDateTime[3] = 0x24;
		}
		
		LOGGER_Write();
		DoorLockCoil_On();
		PowerContactor_On();
		DISPLAY_CardValidImage();			
		SignalBuzzer = BUZZ_CARD_VALID;
		RC522_StartTimer(RC522_CARD_VALID_EVENT_TIME);
		RC522_HandmaidReentranceDisable();
	}
	else
	{
		if(!(dout_0_7_remote & 0x40)) DoorLockCoil_Off();
		//RC522_PinSelectHigh();
		RC522_RST_SetLow(); 
		DIO_SetOuput();
		
		for (i = 0; i < 10; i++)
		{
			HAL_Delay(1);
			if(eComState == COM_PACKET_RECEIVED) RS485_Service();
		}
		    
		RC522_RST_SetHigh(); 
		DIO_SetOuput();
		
		for (i = 0; i < 10; i++)
		{
			HAL_Delay(1);
			if(eComState == COM_PACKET_RECEIVED) RS485_Service();
		}
		
		RC522_Reset();
		RC522_WriteRegister(RC522_REG_T_MODE, 0x8D);
		RC522_WriteRegister(RC522_REG_T_PRESCALER, 0x3E);
		RC522_WriteRegister(RC522_REG_T_RELOAD_L, 30);           
		RC522_WriteRegister(RC522_REG_T_RELOAD_H, 0);
		RC522_WriteRegister(RC522_REG_TX_AUTO, 0x40);
		RC522_WriteRegister(RC522_REG_MODE, 0x3D);
		RC522_AntennaOn();// Open the antenna
		RC522_StartTimer(RC522_PROCESS_TIME);
	}
}// End of mifare service function

RC522_StatusTypeDef RC522_Check(uint8_t* id) 
{
   
	RC522_StatusTypeDef status;
	
	status = RC522_Request(PICC_REQIDL, id);	            // Find cards, return card type
    
	if (status == MI_OK) {                                  // Card detected
		
		status = RC522_Anticoll(id);	                    // Anti-collision, return card serial number 4 bytes
        
	}// End of if...
    
	RC522_Halt();			                                // Command card into hibernation 

	return (status);
    
}// End of check command

RC522_StatusTypeDef RC522_Compare(uint8_t* aCardID, uint8_t* CompareID) 
{   
	uint8_t i;
    
	for (i = 0; i < 5; i++) {
        
		if (aCardID[i] != CompareID[i]) {
            
			return (MI_ERR);
            
		}// End of if...
	}// End of for loop
    
	return (MI_OK);
    
}// End of compare function

void RC522_WriteRegister(uint8_t addr, uint8_t val) 
{
    //RC522_PinSelectLow();
	aRC522_TxBuffer[0] = addr;
//    aRC522_TxBuffer[0] = (addr << 1) & 0x7E;               // Address offset
    aRC522_TxBuffer[1] = val;                              // set value
	HAL_I2C_Master_Transmit(&hi2c1, RC522_WRITE, aRC522_TxBuffer, 2, RC522_TIMEOUT);
//	if (HAL_SPI_Transmit(&hspi2, aRC522_TxBuffer , 2, 10) != HAL_OK) SPI2_Reinit();
    //RC522_PinSelectHigh();
	
}// End of write register function

uint8_t RC522_ReadRegister(uint8_t addr) 
{
	//RC522_PinSelectLow();	
//    aRC522_TxBuffer[0] = ((addr << 1) & 0x7E) | 0x80;      // Address offset
	aRC522_TxBuffer[0] = addr;
	HAL_I2C_Master_Transmit(&hi2c1, RC522_WRITE, aRC522_TxBuffer, 1, RC522_TIMEOUT);
	HAL_I2C_Master_Receive(&hi2c1, RC522_READ, aRC522_RxBuffer, 1, RC522_TIMEOUT);
//    if (HAL_SPI_TransmitReceive(&hspi2, aRC522_TxBuffer, aRC522_RxBuffer, 2, 10) != HAL_OK) SPI2_Reinit();
    //RC522_PinSelectHigh();
	
    return (aRC522_RxBuffer[0]);
        
}// End of read register function

void RC522_SetBitMask(uint8_t reg, uint8_t mask) 
{   
	RC522_WriteRegister(reg, RC522_ReadRegister(reg) | mask);
    
}// End of set bit mask function


void RC522_ClearBitMask(uint8_t reg, uint8_t mask)
{
    
	RC522_WriteRegister(reg, RC522_ReadRegister(reg) & (~mask));
    
}// End of clear bit mask function

void RC522_AntennaOn(void) 
{ 
	uint8_t temp;

	temp = RC522_ReadRegister(RC522_REG_TX_CONTROL);
    
	if (!(temp & 0x03)) {
        
		RC522_SetBitMask(RC522_REG_TX_CONTROL, 0x03);
        
	}// End of if...
    
}// End of antena on function

void RC522_AntennaOff(void) 
{   
	RC522_ClearBitMask(RC522_REG_TX_CONTROL, 0x03);
    
}// End of antena off function

RC522_StatusTypeDef RC522_Reset(void) 
{  
    uint16_t delay;
    /**
    *   Issue the SoftReset command.
    */
	RC522_WriteRegister(RC522_REG_COMMAND, PCD_RESETPHASE);
    /**
    *   The datasheet does not mention how long the SoftRest command takes to complete.
    *   But the RC522 might have been in soft power-down mode (triggered by bit 4 of CommandReg)
    *   Section 8.8.2 in the datasheet says the oscillator start-up time is the start up time of the crystal + 37,74us. Let us be generous: 50ms.
    */
    HAL_Delay(5);

	delay = 0;
    /**
    *   Wait for the PowerDown bit in CommandReg to be cleared
    */	
    while (RC522_ReadRegister(RC522_REG_COMMAND) & (1<<4)){
		
		if(eComState != COM_PACKET_PENDING) RS485_Service();
        /**
        *   RC522 still restarting - unlikely after waiting 50ms and more
        *   mifare modul is unresponsive so return error status
        */
        if(++delay == 0xffff)
		{			
            return (MI_ERR);
            
        }
		else
		{
			HAL_Delay(2);
		}
    }// End of while...
    /**
    *   reset finished - return OK flag
    */
    return (MI_OK);
    
}// End of software reset function

RC522_StatusTypeDef RC522_Request(uint8_t reqMode, uint8_t* TagType) 
{
    
	RC522_StatusTypeDef status;  
	uint16_t backBits;			                            //The received data bits

	RC522_WriteRegister(RC522_REG_BIT_FRAMING, 0x07);	// TxLastBits = BitFramingReg[2..0]	???

	TagType[0] = reqMode;
	status = RC522_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);

	if ((status != MI_OK) || (backBits != 0x10)) {
        
		status = MI_ERR;
        
	}// End of if...

	return (status);
    
}// End of request function

RC522_StatusTypeDef RC522_ToCard(uint8_t command, uint8_t* sendData, uint8_t sendLen, uint8_t* backData, uint16_t* backLen) 
{
	
    RC522_StatusTypeDef status = MI_ERR;
	uint8_t irqEn = 0x00;
	uint8_t waitIRq = 0x00;
	uint8_t lastBits;
	uint8_t n;
	uint16_t i;
  

	switch (command) 
	{
        
		case PCD_AUTHENT:
			irqEn = 0x12;
			waitIRq = 0x10;            
			break;
        
		case PCD_TRANSCEIVE:            
			irqEn = 0x77;
			waitIRq = 0x30;            
			break;
        
		default:
			break;
            
			
	}// End of switch command
	
	RC522_WriteRegister(RC522_REG_COMM_IE_N, irqEn|0x80);
	RC522_ClearBitMask(RC522_REG_COMM_IRQ, 0x80);
	RC522_SetBitMask(RC522_REG_FIFO_LEVEL, 0x80);
	RC522_WriteRegister(RC522_REG_COMMAND, PCD_IDLE);

	//Writing data to the FIFO
	for (i = 0; i < sendLen; i++) {   
        
		RC522_WriteRegister(RC522_REG_FIFO_DATA, sendData[i]);   
        
	}// End of for loop

	//Execute the command
	RC522_WriteRegister(RC522_REG_COMMAND, command);
    
	if (command == PCD_TRANSCEIVE) {    
        
		RC522_SetBitMask(RC522_REG_BIT_FRAMING, 0x80);  //StartSend=1,transmission of data starts  
	
    }// End of if... 
    /**
    *   Waiting to receive data to complete
    */
	i = 2000;	//i according to the clock frequency adjustment, the operator M1 card maximum waiting time 25ms???
	do {
        /**
        *   CommIrqReg[7..0]
        *   Set1 TxIRq RxIRq IdleIRq HiAlerIRq LoAlertIRq ErrIRq TimerIRq
        */
		n = RC522_ReadRegister(RC522_REG_COMM_IRQ);
		i--;
        
	} while ((i != 0) && !(n & 0x01) && !(n&waitIRq));          // End of do...while loop            
    /**
    *   StartSend=0
    */
	RC522_ClearBitMask(RC522_REG_BIT_FRAMING, 0x80);

	if (i != 0)  {
		
		if (!(RC522_ReadRegister(RC522_REG_ERROR) & 0x1B)) {
            
			status = MI_OK;
            
			if (n & irqEn & 0x01) {
                
				status = MI_NOTAGERR;
                
			}// End of if...

			if (command == PCD_TRANSCEIVE) {
                
				n = RC522_ReadRegister(RC522_REG_FIFO_LEVEL);
				lastBits = RC522_ReadRegister(RC522_REG_CONTROL) & 0x07;
                
				if (lastBits) *backLen = (n - 1)*8 + lastBits;  
                else *backLen = n * 8;  

				if (n == 0) n = 1;
                
				if (n > RC522_MAX_LEN) n = RC522_MAX_LEN;   
				/**
                *   Reading the received data in FIFO
                */
				for (i = 0; i < n; i++) {
                    
					backData[i] = RC522_ReadRegister(RC522_REG_FIFO_DATA);
                    
				}// End of for loop
			}// End of if (command == PCD_TRANSCEIVE)
            
		} else {
            
			status = MI_ERR;
            
		}// End of else
	}// End of if (i != 0)

	return (status);
    
}// End of to card function

RC522_StatusTypeDef RC522_Anticoll(uint8_t* serNum) 
{
    
	RC522_StatusTypeDef status;
	uint8_t i;
	uint8_t serNumCheck = 0;
	uint16_t unLen;

	RC522_WriteRegister(RC522_REG_BIT_FRAMING, 0x00);   // TxLastBists = BitFramingReg[2..0]

	serNum[0] = PICC_ANTICOLL;
	serNum[1] = 0x20;
	status = RC522_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);

	if (status == MI_OK) {
		/**
        *   Check card serial number
        */
		for (i = 0; i < 4; i++) {
            
			serNumCheck ^= serNum[i];
            
		}// End of for loop....
        
		if (serNumCheck != serNum[i]) {
            
			status = MI_ERR;
            
		}// End of if...
	}// End of if (status == MI_OK)
    
	return (status);
    
}// End of anticollision function


void RC522_CalculateCRC(uint8_t*  pIndata, uint8_t len, uint8_t* pOutData)
{
    
	uint8_t i, n;

	RC522_WriteRegister(RC522_REG_COMMAND, PCD_IDLE);
	RC522_ClearBitMask(RC522_REG_DIV_IRQ, 0x04);			//CRCIrq = 0
	RC522_SetBitMask(RC522_REG_FIFO_LEVEL, 0x80);			//Clear the FIFO pointer
	/**
    *   Write_RC522(CommandReg, PCD_IDLE);
    *   Writing data to the FIFO
    */
	for (i = 0; i < len; i++) {
        
		RC522_WriteRegister(RC522_REG_FIFO_DATA, *(pIndata+i)); 
        
	}// End of for loop...
    
	RC522_WriteRegister(RC522_REG_COMMAND, PCD_CALCCRC);

	//Wait CRC calculation is complete
	i = 0xFF;
	do {
		n = RC522_ReadRegister(RC522_REG_DIV_IRQ);
		i--;
	} while ((i!=0) && !(n&0x04));			//CRCIrq = 1
	
	RC522_WriteRegister(RC522_REG_COMMAND, PCD_IDLE);
	//Read CRC calculation result
	pOutData[0] = RC522_ReadRegister(RC522_REG_CRC_RESULT_L);
	pOutData[1] = RC522_ReadRegister(RC522_REG_CRC_RESULT_M);
    
}// End of calculate CRC function


uint8_t RC522_SelectTag(uint8_t* serNum) 
{
    
    RC522_StatusTypeDef status;
	uint8_t i;
	uint8_t size;
	uint16_t recvBits;
	uint8_t buffer[9]; 

	buffer[0] = PICC_SELECTTAG;
	buffer[1] = 0x70;
    
	for (i = 0; i < 5; i++) {
        
		buffer[i+2] = *(serNum+i);
        
	}// End of for loop...
    
	RC522_CalculateCRC(buffer, 7, &buffer[7]);		//??
	status = RC522_ToCard(PCD_TRANSCEIVE, buffer, 9, buffer, &recvBits);

	if ((status == MI_OK) && (recvBits == 0x18)) {
        
		size = buffer[0]; 
        
	} else {
        
		size = 0;
        
	}// End of else

	return (size);
    
}// End of select tag function


RC522_StatusTypeDef RC522_Auth(uint8_t authMode, uint8_t BlockAddr, uint8_t* Sectorkey, uint8_t* serNum) 
{
	
    RC522_StatusTypeDef status;
	uint16_t recvBits;
	uint8_t i;
	uint8_t buff[12]; 

	//Verify the command block address + sector + password + card serial number
	buff[0] = authMode;
	buff[1] = BlockAddr;
    
	for (i = 0; i < 6; i++) { 
        
		buff[i + 2] = *(Sectorkey + i); 
        
	}// End of for loop...
    
	for (i = 0; i < 4; i++) {
        
		buff[i + 8] = *(serNum + i);
        
	}// End of for loop...
    
	status = RC522_ToCard(PCD_AUTHENT, buff, 12, buff, &recvBits);

	if ((status != MI_OK) || (!(RC522_ReadRegister(RC522_REG_STATUS2) & 0x08))) {
        
		status = MI_ERR;
        
	}// End of if....

	return (status);
    
}// End of auth function


RC522_StatusTypeDef RC522_Read(uint8_t blockAddr, uint8_t* recvData) 
{
    
	RC522_StatusTypeDef status;
	uint16_t unLen;

	recvData[0] = PICC_READ;
	recvData[1] = blockAddr;
	RC522_CalculateCRC(recvData, 2, &recvData[2]);
	status = RC522_ToCard(PCD_TRANSCEIVE, recvData, 4, recvData, &unLen);

	if ((status != MI_OK) || (unLen != 0x90)) {
        
		status = MI_ERR;
        
	}// End of if...

	return (status);
    
}// End of read function


RC522_StatusTypeDef RC522_Write(uint8_t blockAddr, uint8_t* writeData) 
{
    
	RC522_StatusTypeDef status;
	uint16_t recvBits;
	uint8_t i;
	uint8_t buff[18]; 

	buff[0] = PICC_WRITE;
	buff[1] = blockAddr;
	RC522_CalculateCRC(buff, 2, &buff[2]);
	status = RC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff, &recvBits);

	if ((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0f) != 0x0a)) {   
        
		status = MI_ERR;   
        
	}// End of if...

	if (status == MI_OK) {
		/**
        *   Data to the FIFO write 16Byte
        */
		for (i = 0; i < 16; i++) {  
            
			buff[i] = *(writeData+i);
            
		}// End of for loop..
        
		RC522_CalculateCRC(buff, 16, &buff[16]);
		status = RC522_ToCard(PCD_TRANSCEIVE, buff, 18, buff, &recvBits);

		if ((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0f) != 0x0a)) { 
            
			status = MI_ERR;   
            
		}// End of if...
	}// End of if (status == MI_OK)

	return (status);
    
}// End of write function


void RC522_Halt(void) 
{
    
	uint16_t unLen;
	uint8_t buff[4]; 

	buff[0] = PICC_HALT;
	buff[1] = 0;
	RC522_CalculateCRC(buff, 2, &buff[2]);

	RC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff, &unLen);
    
}// End of halt function


RC522_StatusTypeDef RC522_ReadCard(void)
{
    RC522_StatusTypeDef status;
	
    RC522_Request(PICC_REQIDL, str);	
    status = RC522_Anticoll(str);

    for(i = 0; i < 5; i++){	 
        
        aCardID[i]=str[i];
        
    }// End of for loop...

    RC522_SelectTag(aCardID);	
	
    status = RC522_Auth(PICC_AUTHENT1A, SECTOR_0, aMifareKeyA, aCardID);
	
    if(status != MI_OK)
	{
		RC522_Halt();
		return (status);
	}
	else
	{    
        RC522_Read(SECTOR_0, &Sector_0.Block_0[0]);
        RC522_Read((SECTOR_0 + 1), &Sector_0.Block_1[0]);
        RC522_Read((SECTOR_0 + 2), &Sector_0.Block_2[0]);
        RC522_Read((SECTOR_0 + 3), &Sector_0.Block_3[0]);
	}
	
	status = RC522_Auth(PICC_AUTHENT1A, SECTOR_1, aMifareKeyA, aCardID);
	
    if(status != MI_OK)
	{	
		RC522_Halt();
		return (status);
	}
	else
	{      
        RC522_Read(SECTOR_1, &Sector_1.Block_0[0]);
        RC522_Read((SECTOR_1 + 1), &Sector_1.Block_1[0]);
        RC522_Read((SECTOR_1 + 2), &Sector_1.Block_2[0]);
        RC522_Read((SECTOR_1 + 3), &Sector_1.Block_3[0]);    
	}
	
	status = RC522_Auth(PICC_AUTHENT1A, SECTOR_2, aMifareKeyA, aCardID);
	
    if(status != MI_OK)
	{		
		RC522_Halt();
		return (status);
	}
	else
	{      
        RC522_Read(SECTOR_2, &Sector_2.Block_0[0]);
        RC522_Read((SECTOR_2 + 1), &Sector_2.Block_1[0]);
        RC522_Read((SECTOR_2 + 2), &Sector_2.Block_2[0]);
        RC522_Read((SECTOR_2 + 3), &Sector_2.Block_3[0]); 	
	}

    RC522_Halt();
	return (status);
            
}// End of read card function

void RC522_WriteCard(void)
{
	RC522_StatusTypeDef status;
	
	RC522_Request(PICC_REQIDL, str);	
	status = RC522_Anticoll(str);

	for(i = 0; i < 5; i++)
	{	 
		aCardID[i]=str[i]; 
	}

	RC522_SelectTag(aCardID);           
	status = RC522_Auth(PICC_AUTHENT1A, SECTOR_0, aMifareKeyA, aCardID);
	if(status == MI_OK)
	{
		//sector 0
		//status = RC522_Write(SECTOR_0, Sector_0.Block_0);
		status = RC522_Write((SECTOR_0 + 1), Sector_0.Block_1);
		status = RC522_Write((SECTOR_0 + 2), Sector_0.Block_2);
		//status = RC522_Write((SECTOR_0 + 3), Sector_0.Block_3); 
		status = RC522_Auth(PICC_AUTHENT1A, SECTOR_1, aMifareKeyA, aCardID);	
	}
	if(status == MI_OK)
	{
		//sector 1
		status = RC522_Write(SECTOR_1, Sector_1.Block_0);
		status = RC522_Write((SECTOR_1 + 1), Sector_1.Block_1);
		status = RC522_Write((SECTOR_1 + 2), Sector_1.Block_2);
		//status = RC522_Write((SECTOR_1 + 3), Sector_1.Block_3);
		status = RC522_Auth(PICC_AUTHENT1A, SECTOR_2, aMifareKeyA, aCardID);	
	}
	
	if(status == MI_OK)
	{
		status = RC522_Write(SECTOR_2, Sector_2.Block_0);
		status = RC522_Write((SECTOR_2 + 1), Sector_2.Block_1);
		status = RC522_Write((SECTOR_2 + 2), Sector_2.Block_2);
		//status = RC522_Write((SECTOR_2 + 3), Sector_2.Block_3);
		status = RC522_Auth(PICC_AUTHENT1A, SECTOR_3, aMifareKeyA, aCardID);	
	}

	if(status == MI_OK)
	{
		status = RC522_Write(SECTOR_3, Sector_3.Block_0);
		status = RC522_Write((SECTOR_3 + 1), Sector_3.Block_1);
		status = RC522_Write((SECTOR_3 + 2), Sector_3.Block_2);
		//status = RC522_Write((SECTOR_3 + 3), Sector_3.Block_3);
		status = RC522_Auth(PICC_AUTHENT1A, SECTOR_4, aMifareKeyA, aCardID);
	}
	
	if(status == MI_OK)
	{
		status = RC522_Write(SECTOR_4, Sector_4.Block_0);
		status = RC522_Write((SECTOR_4 + 1), Sector_4.Block_1);
		status = RC522_Write((SECTOR_4 + 2), Sector_4.Block_2);
		//status = RC522_Write((SECTOR_4 + 3), Sector_4.Block_3);
		status = RC522_Auth(PICC_AUTHENT1A, SECTOR_5, aMifareKeyA, aCardID);
	}
		
	if(status == MI_OK)
	{
		status = RC522_Write(SECTOR_5, Sector_5.Block_0);
		status = RC522_Write((SECTOR_5 + 1), Sector_5.Block_1);
		status = RC522_Write((SECTOR_5 + 2), Sector_5.Block_2);
		//status = RC522_Write((SECTOR_5 + 3), Sector_5.Block_3);
		status = RC522_Auth(PICC_AUTHENT1A, SECTOR_6, aMifareKeyA, aCardID);	
	}
		
	if(status == MI_OK)
	{
		status = RC522_Write(SECTOR_6, Sector_6.Block_0);
		status = RC522_Write((SECTOR_6 + 1), Sector_6.Block_1);
		status = RC522_Write((SECTOR_6 + 2), Sector_6.Block_2);
		//status = RC522_Write((SECTOR_6 + 3), Sector_6.Block_3);
		status = RC522_Auth(PICC_AUTHENT1A, SECTOR_7, aMifareKeyA, aCardID); 	
	}
	
	if(status == MI_OK)
	{
		status = RC522_Write(SECTOR_7, Sector_7.Block_0);
		status = RC522_Write((SECTOR_7 + 1), Sector_7.Block_1);
		status = RC522_Write((SECTOR_7 + 2), Sector_7.Block_2);
		//status = RC522_Write((SECTOR_7 + 3), Sector_7.Block_3);
	}
		

	RC522_Halt();
            
}// End of write card function

RC522_StatusTypeDef RC522_VerifyData(void)
{
	uint8_t b_cnt, m_cnt;
	uint32_t temp_address;
	
	sCard.card_status = NULL;
	ClearBuffer(sCard.aCardID, 5);
	sCard.controller_id = NULL;
	sCard.system_id = NULL;
	

	for(b_cnt = 0; b_cnt < 5; b_cnt++)
	{
		sCard.aCardID[b_cnt] = aCardSerial[b_cnt];
	}
	/**
	*			U S E R S  G R O U P   C H E C K
	**/	
	aEepromBuffer[0] = EE_PERMITED_GROUP_ADDRESS >> 8;
	aEepromBuffer[1] = EE_PERMITED_GROUP_ADDRESS;
	HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
	HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, 16, I2C_EE_TIMEOUT);
	
	for(b_cnt = 0; b_cnt < 16; b_cnt++)
	{
		for(m_cnt = 0; m_cnt < 16; m_cnt++)
		{
			if((Sector_1.Block_0[b_cnt] == aEepromBuffer[m_cnt]) && (aEepromBuffer[m_cnt] != NULL) \
				&& (aEepromBuffer[m_cnt] != CARD_DATA_FORMATED))
			{				
				sCard.user_group = aEepromBuffer[m_cnt];
				m_cnt = 16;
				b_cnt = 16;
			}
			else if((Sector_1.Block_0[b_cnt] == NULL) || (Sector_1.Block_0[b_cnt] == CARD_DATA_FORMATED))
			{
				sCard.user_group = USER_GROUP_INVALID;
			}
			else
			{
				sCard.user_group = USER_GROUP_DATA_INVALID;
			}
		}
	}
	/**
	*			S Y S T E M   I D   C H E C K
	**/
	temp_address = 0;
	
	if(IS_09(Sector_1.Block_1[0])) temp_address += (Sector_1.Block_1[0] - 48) * 10000;
	if(IS_09(Sector_1.Block_1[1])) temp_address += (Sector_1.Block_1[1] - 48) * 1000;
	if(IS_09(Sector_1.Block_1[2])) temp_address += (Sector_1.Block_1[2] - 48) * 100;
	if(IS_09(Sector_1.Block_1[3])) temp_address += (Sector_1.Block_1[3] - 48) * 10;
	if(IS_09(Sector_1.Block_1[4])) temp_address += (Sector_1.Block_1[4] - 48);
	

	if((((temp_address >> 8) & 0xff) == aSystemID[0]) && ((temp_address & 0xff) == aSystemID[1]))
	{
		sCard.system_id = temp_address & 0xffff;
	}
	else if((Sector_1.Block_1[0] == NULL) || \
			(Sector_1.Block_1[1] == NULL) || \
			(Sector_1.Block_1[2] == NULL) || \
			(Sector_1.Block_1[3] == NULL) || \
			(Sector_1.Block_1[4] == NULL) || \
			(Sector_1.Block_1[0] == CARD_DATA_FORMATED) || \
			(Sector_1.Block_1[1] == CARD_DATA_FORMATED) || \
			(Sector_1.Block_1[2] == CARD_DATA_FORMATED) || \
			(Sector_1.Block_1[3] == CARD_DATA_FORMATED) || \
			(Sector_1.Block_1[4] == CARD_DATA_FORMATED))
	{
		sCard.system_id = SYSTEM_ID_DATA_INVALID;
	}
	else
	{
		sCard.system_id = SYSTEM_ID_INVALID;
	}
	/**
	*			C O N T R O L L E R    A D D R E S S   C H E C K
	**/
	if ((Sector_2.Block_0[6] == rs485_interface_address[0]) && (Sector_2.Block_0[7] == rs485_interface_address[1]) ) 
	{
		sCard.controller_id = (rs485_interface_address[0] << 8) + rs485_interface_address[1];
	}
	else
	{
        for(b_cnt = 0; b_cnt < 8; b_cnt++)
        {
            if ((Sector_2.Block_0[6] == aPermitedAddresse[b_cnt][0]) && (Sector_2.Block_0[7] == aPermitedAddresse[b_cnt][1])) 
            {
                sCard.controller_id = ((aPermitedAddresse[b_cnt][0] << 8) + aPermitedAddresse[b_cnt][1]);
                b_cnt = 8;
            }
        }
	}
	
	
	if(((Sector_2.Block_0[6] == NULL) && (Sector_2.Block_0[7] == NULL)) \
			|| ((Sector_2.Block_0[6] == CARD_DATA_FORMATED) && (Sector_2.Block_0[7] == CARD_DATA_FORMATED)))
	{
		sCard.controller_id = CONTROLLER_ID_DATA_INVALID;
	}
	else if((sCard.controller_id == NULL) || (sCard.controller_id == 0xffff))
	{
		sCard.controller_id = CONTROLLER_ID_INVALID;
	}

//	HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BCD);
//	HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BCD);
			
	if (((Sector_2.Block_0[0] >> 4) > 0x03) || (((Sector_2.Block_0[0] >> 4) ==  0x00) && \
		((Sector_2.Block_0[0] & 0x0f) == 0x00)) || ((Sector_2.Block_0[0] & 0x0f) > 0x09))
	{
		sCard.aExpiryTime[0] = EXPIRY_TIME_DATA_INVALID;
	}
	else if (((Sector_2.Block_0[1] >> 4) > 0x01) || (((Sector_2.Block_0[1] >> 4) == 0x00) && \
			((Sector_2.Block_0[1] & 0x0f) == 0x00)) || ((Sector_2.Block_0[1] & 0x0f) > 0x09))
	{
		sCard.aExpiryTime[1] = EXPIRY_TIME_DATA_INVALID;
	}
	else if (((Sector_2.Block_0[2] >> 4) > 0x09) || ((Sector_2.Block_0[2] & 0x0f) > 0x09))
	{
		sCard.aExpiryTime[2] = EXPIRY_TIME_DATA_INVALID;
	}
	else if (((Sector_2.Block_0[3] >> 4) > 0x02) || ((Sector_2.Block_0[3] & 0x0f) > 0x09))
	{
		sCard.aExpiryTime[3] = EXPIRY_TIME_DATA_INVALID;
	}
	else if (((Sector_2.Block_0[4] >> 4) > 0x05) || ((Sector_2.Block_0[4] & 0x0f) > 0x09))
	{
		sCard.aExpiryTime[4] = EXPIRY_TIME_DATA_INVALID;
	}
	else
	{
		if((Sector_2.Block_0[2] > date.Year) \
			|| ((Sector_2.Block_0[2] == date.Year) && (Sector_2.Block_0[1] > date.Month))	\
			|| ((Sector_2.Block_0[2] == date.Year) && (Sector_2.Block_0[1] == date.Month) && (Sector_2.Block_0[0] > date.Date))	\
			|| ((Sector_2.Block_0[2] == date.Year) && (Sector_2.Block_0[1] == date.Month) && (Sector_2.Block_0[0] == date.Date) && (Sector_2.Block_0[3] > time.Hours))	\
			|| ((Sector_2.Block_0[2] == date.Year) && (Sector_2.Block_0[1] == date.Month) && (Sector_2.Block_0[0] == date.Date) && (Sector_2.Block_0[3] == time.Hours) 	\
			&& (Sector_2.Block_0[4] >= time.Minutes)))
		{
			for(b_cnt = 0; b_cnt < 6; b_cnt++)
			{
				sCard.aExpiryTime[b_cnt] = Sector_2.Block_0[b_cnt];	
			}	
		}
		else
		{
			for(b_cnt = 0; b_cnt < 6; b_cnt++)
			{
				sCard.aExpiryTime[b_cnt] = EXPIRY_TIME_INVALID;		
			}
		}
	}		
	
	if ((sCard.user_group == USER_GROUP_INVALID) || (sCard.user_group == USER_GROUP_DATA_INVALID)			\
		|| (sCard.aExpiryTime[0] == EXPIRY_TIME_DATA_INVALID) || (sCard.aExpiryTime[1] == EXPIRY_TIME_DATA_INVALID) 	\
		|| (sCard.aExpiryTime[2] == EXPIRY_TIME_DATA_INVALID) || (sCard.aExpiryTime[3] == EXPIRY_TIME_DATA_INVALID) 	\
		|| (sCard.aExpiryTime[4] == EXPIRY_TIME_DATA_INVALID) || (sCard.aExpiryTime[5] == EXPIRY_TIME_DATA_INVALID) 	\
		|| (sCard.controller_id == CONTROLLER_ID_INVALID) || (sCard.controller_id == CONTROLLER_ID_DATA_INVALID)	\
		|| (sCard.system_id == SYSTEM_ID_INVALID) || (sCard.system_id == SYSTEM_ID_DATA_INVALID))
	 {
		 sCard.card_status = CARD_INVALID;
	 }
	 else if ((sCard.aExpiryTime[0] == EXPIRY_TIME_INVALID) || (sCard.aExpiryTime[1] == EXPIRY_TIME_INVALID) ||	\
			  (sCard.aExpiryTime[2] == EXPIRY_TIME_INVALID) || (sCard.aExpiryTime[3] == EXPIRY_TIME_INVALID) || \
			  (sCard.aExpiryTime[4] == EXPIRY_TIME_INVALID) || (sCard.aExpiryTime[5] == EXPIRY_TIME_INVALID))
	 {
		 sCard.card_status = EXPIRY_TIME_INVALID;
	 }
	 else
	 {
		 sCard.card_status = CARD_VALID;
	 }
	
	if(sCard.card_status == CARD_VALID) return (MI_OK);
	m_cnt = 0;
	
	for(b_cnt = 0; b_cnt < 5; b_cnt++)
	{
		if(aCardSerial[b_cnt] == aResetCard[b_cnt]) ++m_cnt;
	}
	
	if(m_cnt > 4)
    {
        LogEvent.log_event = SOFTWARE_RESET;
        LOGGER_Write();
        BootloaderExe();
    }	
	return (MI_OK);
}

void RC522_ClearData(void)
{
	uint8_t i;
	
	for(i = 0; i < 16; i++)
	{
		Sector_0.Block_0[i] = NULL;
		Sector_0.Block_1[i] = NULL;
		Sector_0.Block_2[i] = NULL;
		Sector_0.Block_3[i] = NULL;
		
		Sector_1.Block_0[i] = NULL;
		Sector_1.Block_1[i] = NULL;
		Sector_1.Block_2[i] = NULL;
		Sector_1.Block_3[i] = NULL;
		
		Sector_2.Block_0[i] = NULL;
		Sector_2.Block_1[i] = NULL;
		Sector_2.Block_2[i] = NULL;
		Sector_2.Block_3[i] = NULL;
		
		Sector_3.Block_0[i] = NULL;
		Sector_3.Block_1[i] = NULL;
		Sector_3.Block_2[i] = NULL;
		Sector_3.Block_3[i] = NULL;
		
		Sector_4.Block_0[i] = NULL;
		Sector_4.Block_1[i] = NULL;
		Sector_4.Block_2[i] = NULL;
		Sector_4.Block_3[i] = NULL;
		
		Sector_5.Block_0[i] = NULL;
		Sector_5.Block_1[i] = NULL;
		Sector_5.Block_2[i] = NULL;
		Sector_5.Block_3[i] = NULL;
		
		Sector_6.Block_0[i] = NULL;
		Sector_6.Block_1[i] = NULL;
		Sector_6.Block_2[i] = NULL;
		Sector_6.Block_3[i] = NULL;
		
		Sector_7.Block_0[i] = NULL;
		Sector_7.Block_1[i] = NULL;
		Sector_7.Block_2[i] = NULL;
		Sector_7.Block_3[i] = NULL;
		
//		Sector_8.Block_0[i] = NULL;
//		Sector_8.Block_1[i] = NULL;
//		Sector_8.Block_2[i] = NULL;
//		Sector_8.Block_3[i] = NULL;
//		
//		Sector_9.Block_0[i] = NULL;
//		Sector_9.Block_1[i] = NULL;
//		Sector_9.Block_2[i] = NULL;
//		Sector_9.Block_3[i] = NULL;
//		
//		Sector_10.Block_0[i] = NULL;
//		Sector_10.Block_1[i] = NULL;
//		Sector_10.Block_2[i] = NULL;
//		Sector_10.Block_3[i] = NULL;
//		
//		Sector_11.Block_0[i] = NULL;
//		Sector_11.Block_1[i] = NULL;
//		Sector_11.Block_2[i] = NULL;
//		Sector_11.Block_3[i] = NULL;
//		
//		Sector_12.Block_0[i] = NULL;
//		Sector_12.Block_1[i] = NULL;
//		Sector_12.Block_2[i] = NULL;
//		Sector_12.Block_3[i] = NULL;
//		
//		Sector_13.Block_0[i] = NULL;
//		Sector_13.Block_1[i] = NULL;
//		Sector_13.Block_2[i] = NULL;
//		Sector_13.Block_3[i] = NULL;
//		
//		Sector_14.Block_0[i] = NULL;
//		Sector_14.Block_1[i] = NULL;
//		Sector_14.Block_2[i] = NULL;
//		Sector_14.Block_3[i] = NULL;
//		
//		Sector_15.Block_0[i] = NULL;
//		Sector_15.Block_1[i] = NULL;
//		Sector_15.Block_2[i] = NULL;
//		Sector_15.Block_3[i] = NULL;
	}
}
