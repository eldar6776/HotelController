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
#include "eeprom.h"
#include "common.h"
#include "dio_interface.h"
#include "logger.h"
#include "rc522.h"
#include "one_wire.h"
#include "rs485.h"
#include "display.h"
#include "signal.h"


/* Imported Type  ------------------------------------------------------------*/
/* Imported Variable  --------------------------------------------------------*/
extern uint8_t rs485_interface_address[];


/* Imported Function  --------------------------------------------------------*/
/* Private Define ------------------------------------------------------------*/
#define DEFAULT_SYSTEM_ID					0xABCDU
#define RC522_POWER_ON_DELAY_TIME           8901U	// 8 s on power up reader is disbled 
#define RC522_PROCESS_TIME					234U	// 234 ms read rate
#define RC522_POWER_VALID_GROUP_PERMITED	2U		// 2 hours room power valid for menager, service, handmaid card
#define RC522_BUFF_LEN						16U     // buffer byte lenght
#define RC522_HANDMAID_CARD_DELAY			2345U	// reset handmaid card status
#define RC522_READ    						0x7FU	// RC522 i2c read address
#define RC522_WRITE   						0x7EU	// RC522 i2c write address
#define RC522_TIMEOUT						5U		// RC522 i2c 5 ms transfer time_mout limit
#define RC522_TRIALS						50U		// RC522 i2c 100 time_ms operation retry
#define RC522_BLOCK_SIZE              		16U 
#define CARD_PENDING						0U	// card data status type
#define CARD_VALID							0x06U	// card data status type
#define CARD_INVALID						0x15U	// card data status type
#define CARD_DATA_FORMATED					0x7FU	// card data status type
/** ==========================================================================*/
/**			M I F A R E		C A R D		S E C T O R		A D D R E S S E		  */
/** ==========================================================================*/
#define SECTOR_0							0x00U
#define SECTOR_1							0x04U
#define SECTOR_2							0x08U
#define SECTOR_3							0x0CU
#define SECTOR_4							0x10U
#define SECTOR_5							0x14U
#define SECTOR_6							0x18U
#define SECTOR_7							0x1CU
#define SECTOR_8							0x20U
#define SECTOR_9							0x24U
#define SECTOR_10							0x28U
#define SECTOR_11							0x2CU
#define SECTOR_12							0x30U
#define SECTOR_13							0x34U
#define SECTOR_14							0x38U
#define SECTOR_15							0x3CU
#define SECTOR_16							0x40U
#define SECTOR_17							0x44U
#define SECTOR_18							0x48U
#define SECTOR_19							0x4CU
#define SECTOR_20							0x50U
#define SECTOR_21							0x54U
#define SECTOR_22							0x58U
#define SECTOR_23							0x5CU
#define SECTOR_24							0x60U
#define SECTOR_25							0x64U
#define SECTOR_26							0x6CU
#define SECTOR_27							0x70U
#define SECTOR_28							0x74U
#define SECTOR_29							0x78U
#define SECTOR_30							0x7CU
#define SECTOR_31             				0x80U
/** ==========================================================================*/
/**			R C 5 2 2			C O M M A N D			L I S T				  */
/** ==========================================================================*/
#define PCD_IDLE							0x00U   //NO action; Cancel the current command
#define PCD_AUTHENT							0x0EU   //Authentication Key
#define PCD_RECEIVE							0x08U   //Receive Data
#define PCD_TRANSMIT						0x04U   //Transmit data
#define PCD_TRANSCEIVE						0x0CU   //Transmit and receive data,
#define PCD_RESETPHASE						0x0FU   //Reset
#define PCD_CALCCRC							0x03U   //CRC Calculate
/* Mifare_One card command word */
#define PICC_REQIDL							0x26U   // find the antenna area does not enter hibernation
#define PICC_REQALL							0x52U   // find all the cards antenna area
#define PICC_ANTICOLL						0x93U   // anti-collision
#define PICC_SELECTTAG						0x93U   // election card
#define PICC_AUTHENT1A						0x60U   // authentication key A
#define PICC_AUTHENT1B						0x61U   // authentication key B
#define PICC_READ							0x30U   // Read Block
#define PICC_WRITE							0xA0U   // write block
#define PICC_DECREMENT						0xC0U   // debit
#define PICC_INCREMENT						0xC1U   // recharge
#define PICC_RESTORE						0xC2U   // transfer block data to the buffer
#define PICC_TRANSFER						0xB0U   // save the data in the buffer
#define PICC_HALT							0x50U   // Sleep
/** ==========================================================================*/
/**			R C 5 2 2			R E G I S T E R			L I S T				  */
/** ==========================================================================*/
//Page 0: Command and Status
#define RC522_REG_RESERVED00				0x00U    
#define RC522_REG_COMMAND					0x01U    
#define RC522_REG_COMM_IE_N					0x02U    
#define RC522_REG_DIV1_EN					0x03U    
#define RC522_REG_COMM_IRQ					0x04U    
#define RC522_REG_DIV_IRQ					0x05U
#define RC522_REG_ERROR						0x06U    
#define RC522_REG_STATUS1					0x07U    
#define RC522_REG_STATUS2					0x08U    
#define RC522_REG_FIFO_DATA					0x09U
#define RC522_REG_FIFO_LEVEL				0x0AU
#define RC522_REG_WATER_LEVEL				0x0BU
#define RC522_REG_CONTROL					0x0CU
#define RC522_REG_BIT_FRAMING				0x0DU
#define RC522_REG_COLL						0x0EU
#define RC522_REG_RESERVED01				0x0FU
//Page 1: Command 
#define RC522_REG_RESERVED10				0x10U
#define RC522_REG_MODE						0x11U
#define RC522_REG_TX_MODE					0x12U
#define RC522_REG_RX_MODE					0x13U
#define RC522_REG_TX_CONTROL				0x14U
#define RC522_REG_TX_AUTO					0x15U
#define RC522_REG_TX_SELL					0x16U
#define RC522_REG_RX_SELL					0x17U
#define RC522_REG_RX_THRESHOLD				0x18U
#define RC522_REG_DEMOD						0x19U
#define RC522_REG_RESERVED11				0x1AU
#define RC522_REG_RESERVED12				0x1BU
#define RC522_REG_MIFARE					0x1CU
#define RC522_REG_RESERVED13				0x1DU
#define RC522_REG_RESERVED14				0x1EU
#define RC522_REG_SERIALSPEED				0x1FU
//Page 2: CFG    
#define RC522_REG_RESERVED20				0x20U  
#define RC522_REG_CRC_RESULT_M				0x21U
#define RC522_REG_CRC_RESULT_L				0x22U
#define RC522_REG_RESERVED21				0x23U
#define RC522_REG_MOD_WIDTH					0x24U
#define RC522_REG_RESERVED22				0x25U
#define RC522_REG_RF_CFG					0x26U
#define RC522_REG_GS_N						0x27U
#define RC522_REG_CWGS_PREG					0x28U
#define RC522_REG_MODGS_PREG				0x29U
#define RC522_REG_T_MODE					0x2AU
#define RC522_REG_T_PRESCALER				0x2BU
#define RC522_REG_T_RELOAD_H				0x2CU
#define RC522_REG_T_RELOAD_L				0x2DU
#define RC522_REG_T_COUNTER_VALUE_H			0x2EU
#define RC522_REG_T_COUNTER_VALUE_L			0x2FU
//Page 3:TestRegister 
#define RC522_REG_RESERVED30				0x30U
#define RC522_REG_TEST_SEL1					0x31U
#define RC522_REG_TEST_SEL2					0x32U
#define RC522_REG_TEST_PIN_EN				0x33U
#define RC522_REG_TEST_PIN_VALUE			0x34U
#define RC522_REG_TEST_BUS					0x35U
#define RC522_REG_AUTO_TEST					0x36U
#define RC522_REG_VERSION					0x37U
#define RC522_REG_ANALOG_TEST				0x38U
#define RC522_REG_TEST_ADC1					0x39U 
#define RC522_REG_TEST_ADC2					0x3AU  
#define RC522_REG_TEST_ADC0					0x3BU  
#define RC522_REG_RESERVED31				0x3CU  
#define RC522_REG_RESERVED32				0x3DU
#define RC522_REG_RESERVED33				0x3EU  
#define RC522_REG_RESERVED34				0x3FU


/* Private Type --------------------------------------------------------------*/
typedef enum 
{
	MI_ERR			= 0x00U,
	MI_OK 			= 0x01U,
	MI_NOTAGERR		= 0x02U,
	MI_SKIP_OVER	= 0x03U

} RC522_StatusTypeDef;


typedef struct
{
    uint8_t block_0[RC522_BLOCK_SIZE];
    uint8_t block_1[RC522_BLOCK_SIZE];
    uint8_t block_2[RC522_BLOCK_SIZE];
    uint8_t block_3[RC522_BLOCK_SIZE];
	
} RC522_SectorTypeDef;


RC522_SectorTypeDef sector_0;
RC522_SectorTypeDef sector_1;
RC522_SectorTypeDef sector_2;
RC522_CardDataTypeDef sCard;


/* Private Variable ----------------------------------------------------------*/
__IO uint32_t mifare_process_flags;

uint8_t card_id[5];
uint8_t system_id[2];
uint8_t card_serial[5];
uint8_t mifare_key_a[6]; 
uint8_t mifare_key_b[6];
uint8_t permitted_add[8][2];
uint8_t reset_card_serial[5] = {0x34U, 0x75U, 0xA6U, 0xA7U, 0x40U};
uint8_t rc522_rx_buff[RC522_BUFF_LEN];
uint8_t rc522_tx_buff[RC522_BUFF_LEN];


/* Private Macro -------------------------------------------------------------*/
/* Private Function Prototype ------------------------------------------------*/
static RC522_StatusTypeDef RC522_Check(uint8_t* id);
static void RC522_WriteRegister(uint8_t addr, uint8_t val);
static uint8_t RC522_ReadRegister(uint8_t addr);
static void RC522_SetBitMask(uint8_t reg, uint8_t mask);
static void RC522_ClearBitMask(uint8_t reg, uint8_t mask);
static void RC522_AntennaOn(void);
static RC522_StatusTypeDef RC522_Reset(void);
static RC522_StatusTypeDef RC522_Request(uint8_t reqMode, uint8_t* TagType);
static RC522_StatusTypeDef RC522_ToCard(uint8_t command, uint8_t* sendData, uint8_t sendLen, uint8_t* backData, uint16_t* backLen);
static RC522_StatusTypeDef RC522_Anticoll(uint8_t* serNum);
static void RC522_CalculateCRC(uint8_t* pIndata, uint8_t len, uint8_t* pOutData);
static uint8_t RC522_SelectTag(uint8_t* serNum);
static RC522_StatusTypeDef RC522_Auth(uint8_t authMode, uint8_t BlockAddr, uint8_t* Sectorkey, uint8_t* serNum);
static RC522_StatusTypeDef RC522_Read(uint8_t blockAddr, uint8_t* recvData);
static void RC522_Halt(void);
static RC522_StatusTypeDef RC522_ReadCard(void);
static RC522_StatusTypeDef RC522_VerifyData(void);



/* Program Code  -------------------------------------------------------------*/
void RC522_Init(void) 
{
	RC522_RST_SetLow(); 
	DIO_SetOuput();	
	HAL_Delay(10);    
	RC522_RST_SetHigh();
	DIO_SetOuput();
	HAL_Delay(50);
	RC522_WriteRegister(RC522_REG_COMMAND, PCD_RESETPHASE);	
	RC522_WriteRegister(RC522_REG_T_MODE, 0x8DU);
	RC522_WriteRegister(RC522_REG_T_PRESCALER, 0x3EU);
	RC522_WriteRegister(RC522_REG_T_RELOAD_H, 0U);
	RC522_WriteRegister(RC522_REG_T_RELOAD_L, 30U);		
	RC522_WriteRegister(RC522_REG_TX_AUTO, 0x40U);
	RC522_WriteRegister(RC522_REG_MODE, 0x3DU);
	RC522_AntennaOn();
	RC522_HandmaidReentranceDisable();
}


void RC522_Service(void)
{	
    uint32_t i;
    uint8_t ee_buff[8];
    RTC_TimeTypeDef time_m;
    RTC_DateTypeDef date_m;
    static uint32_t mifare_time = 0U;    
	static uint32_t mifare_timer = 0U;
    static uint32_t handmaid_card_time = 0U;    
    static uint32_t handmaid_card_timer = 0;
	static uint8_t handmaid_card_cycles = 0U;
    
    if(eComState == COM_PACKET_RECEIVED) RS485_Service();
    
    if(IsRC522_ExtendDoorlockTimeActiv())
    {
        RC522_ExtendDoorlockTimeReset();
        mifare_timer = HAL_GetTick(); 
        mifare_time = RC522_CARD_VALID_EVENT_TIME;
    }
    
	if((HAL_GetTick() - handmaid_card_timer) >= handmaid_card_time)
	{
		if ((handmaid_card_cycles == 1U) && (ROOM_Status == ROOM_HANDMAID_IN)) RC522_HandmaidReentranceEnable();
		handmaid_card_cycles = 0U;
	}

    if(((HAL_GetTick() - mifare_timer) < mifare_time) || IsRS485_UpdateActiv()) return;
    else mifare_timer = HAL_GetTick();
    
	if (RC522_Check(card_serial) == MI_OK)
	{
		RC522_ClearData();
		RC522_ReadCard();
		RC522_VerifyData();
		
        HAL_RTC_GetTime(&hrtc, &time_m, RTC_FORMAT_BCD);
        HAL_RTC_GetDate(&hrtc, &date_m, RTC_FORMAT_BCD);
        
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
				
			LogEvent.log_card_id[0] = sCard.card_id[0];
			LogEvent.log_card_id[1] = sCard.card_id[1];
			LogEvent.log_card_id[2] = sCard.card_id[2];
			LogEvent.log_card_id[3] = sCard.card_id[3];
			LogEvent.log_card_id[4] = sCard.card_id[4];
			
			if(sCard.user_group == CARD_USER_GROUP_GUEST)
			{
				if(ROOM_Status == ROOM_OUT_OF_ORDER)
				{
					handmaid_card_cycles = 1U;
					DISPLAY_RoomOutOfServiceImage();
					SignalBuzzer = BUZZ_CARD_INVALID;
                    mifare_time = RC522_CARD_INVALID_EVENT_TIME;
				}
				else
				{
					ee_buff[0] = EE_ROOM_POWER_TIMEOUT >> 8U;
					ee_buff[1] = EE_ROOM_POWER_TIMEOUT;
					ee_buff[2] = sCard.expiry_time[0];
					ee_buff[3] = sCard.expiry_time[1];
					ee_buff[4] = sCard.expiry_time[2];
					ee_buff[5] = sCard.expiry_time[3];
					ee_buff[6] = sCard.expiry_time[4];
					SIGNAL_ExpiryTimeFromGuestCardSet();
					if((ROOM_Status == ROOM_IDLE) || (ROOM_Status == ROOM_READY)) ROOM_Status = ROOM_BUSY;
					if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, ee_buff, 7U, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
					if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);	
					
					i = 6;

					do
					{
                        --i;
						aRoomPowerExpiryDateTime[i] = sCard.expiry_time[i];
					}
                    while(i != 0);
										
					LOGGER_Write();
				}	
			}
			else if(((sCard.user_group == CARD_USER_GROUP_HANDMAID) && !IsIndorCardReaderActiv() && !IsDonNotDisturbActiv()) || \
					(sCard.user_group == CARD_USER_GROUP_MANAGER) || \
					(sCard.user_group == CARD_USER_GROUP_SERVICE))
			{
				if((sCard.user_group == CARD_USER_GROUP_MANAGER) || \
					(sCard.user_group == CARD_USER_GROUP_SERVICE))
				{
					aRoomPowerExpiryDateTime[0] = date_m.Date;
					aRoomPowerExpiryDateTime[1] = date_m.Month;
					aRoomPowerExpiryDateTime[2] = date_m.Year;
					aRoomPowerExpiryDateTime[4] = time_m.Minutes;
					SIGNAL_ExpiryTimeFromGuestCardReset();
					
					if((time_m.Hours + RC522_POWER_VALID_GROUP_PERMITED) < 0x24U)
					{
						aRoomPowerExpiryDateTime[3] = (time_m.Hours + RC522_POWER_VALID_GROUP_PERMITED);
					}
					else
					{
						aRoomPowerExpiryDateTime[3] = 0x24U;
					}			
					
					LOGGER_Write();
				}
				else if (sCard.user_group == CARD_USER_GROUP_HANDMAID)
				{
					if(ROOM_Status == ROOM_OUT_OF_ORDER)
					{
						handmaid_card_cycles = 1U;
						DISPLAY_RoomOutOfServiceImage();
						SignalBuzzer = BUZZ_CARD_INVALID;
                        mifare_time = RC522_CARD_INVALID_EVENT_TIME;
					}
					else if ((sCard.expiry_time[0] == EXPIRY_TIME_INVALID) ||	\
						(sCard.expiry_time[1] == EXPIRY_TIME_INVALID) ||	\
						(sCard.expiry_time[2] == EXPIRY_TIME_INVALID) || 	\
						(sCard.expiry_time[3] == EXPIRY_TIME_INVALID) || 	\
						(sCard.expiry_time[4] == EXPIRY_TIME_INVALID) ||	\
						(sCard.expiry_time[5] == EXPIRY_TIME_INVALID))
					{
						handmaid_card_cycles = 1U;
						DISPLAY_TimeExpiredImage();
						SignalBuzzer = BUZZ_CARD_INVALID;
                        mifare_time = RC522_CARD_INVALID_EVENT_TIME;					
						LogEvent.log_event = CARD_EXPIRED;
						LOGGER_Write();
					}
					else
					{
						++handmaid_card_cycles;
						SignalBuzzer = BUZZ_CARD_VALID;
                        mifare_time = RC522_CARD_INVALID_EVENT_TIME;
                        handmaid_card_time = RC522_HANDMAID_CARD_DELAY;
						handmaid_card_timer = HAL_GetTick();
                        
						if ((ROOM_Status == ROOM_CLEANING) || (ROOM_Status == ROOM_BEDDING_REPLACEMENT) ||(ROOM_Status == ROOM_GENERAL_CLEANING))
						{
							handmaid_card_cycles = 0U;
							ROOM_Status = ROOM_HANDMAID_IN;						
							aRoomPowerExpiryDateTime[0] = date_m.Date;
							aRoomPowerExpiryDateTime[1] = date_m.Month;
							aRoomPowerExpiryDateTime[2] = date_m.Year;
							aRoomPowerExpiryDateTime[4] = time_m.Minutes;
							SIGNAL_ExpiryTimeFromGuestCardReset();
							
							if((time_m.Hours + RC522_POWER_VALID_GROUP_PERMITED) < 0x24U)
							{
								aRoomPowerExpiryDateTime[3] = (time_m.Hours + RC522_POWER_VALID_GROUP_PERMITED);
							}
							else
							{
								aRoomPowerExpiryDateTime[3] = 0x24U;
							}
																					
							LOGGER_Write();
						}
						else if(handmaid_card_cycles == 3U)
						{
							ROOM_Status = ROOM_READY;
							SignalBuzzer = BUZZ_HANDMAID_FINISH;
                            mifare_time = RC522_CARD_VALID_EVENT_TIME;
							LogEvent.log_event = HANDMAID_SERVICE_END;
							LOGGER_Write();
                        }
						else if (ROOM_Status != ROOM_HANDMAID_IN)
						{
							handmaid_card_cycles = 0U;	
								
							if(IsHandmaidRequested())
							{
								ROOM_OldStatus = ROOM_BUSY;
								ROOM_Status = ROOM_FORCING_DND;
							}
							else
							{
								ROOM_Status = ROOM_READY;
								aRoomPowerExpiryDateTime[0] = date_m.Date;
								aRoomPowerExpiryDateTime[1] = date_m.Month;
								aRoomPowerExpiryDateTime[2] = date_m.Year;
								aRoomPowerExpiryDateTime[4] = time_m.Minutes;
								SIGNAL_ExpiryTimeFromGuestCardReset();
								
								if((time_m.Hours + RC522_POWER_VALID_GROUP_PERMITED) < 0x24U)
								{
									aRoomPowerExpiryDateTime[3] = (time_m.Hours + RC522_POWER_VALID_GROUP_PERMITED);
								}
								else
								{
									aRoomPowerExpiryDateTime[3] = 0x24U;
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
					handmaid_card_cycles = 1U;
					DISPLAY_RoomOutOfServiceImage();
					SignalBuzzer = BUZZ_CARD_INVALID;
                    mifare_time = RC522_CARD_INVALID_EVENT_TIME;
				}
				else if ((sCard.expiry_time[0] == EXPIRY_TIME_INVALID) ||	\
						(sCard.expiry_time[1] == EXPIRY_TIME_INVALID) ||	\
						(sCard.expiry_time[2] == EXPIRY_TIME_INVALID) || 	\
						(sCard.expiry_time[3] == EXPIRY_TIME_INVALID) || 	\
						(sCard.expiry_time[4] == EXPIRY_TIME_INVALID) ||	\
						(sCard.expiry_time[5] == EXPIRY_TIME_INVALID))					
				{						
					handmaid_card_cycles = 1U;						
					DISPLAY_TimeExpiredImage();						
					SignalBuzzer = BUZZ_CARD_INVALID;
                    mifare_time = RC522_CARD_INVALID_EVENT_TIME;	
					LogEvent.log_event = CARD_EXPIRED;
					LOGGER_Write();
				}					
				else					
				{						
                    handmaid_card_time = RC522_HANDMAID_CARD_DELAY;
                    handmaid_card_timer = HAL_GetTick();
                    
					if(++handmaid_card_cycles == 3U)
					{
						handmaid_card_cycles = 0U;
						SignalBuzzer = BUZZ_HANDMAID_FINISH;
                        mifare_time = RC522_CARD_VALID_EVENT_TIME;
						LogEvent.log_event = HANDMAID_SERVICE_END;
						LOGGER_Write();
                        
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
						SignalBuzzer = BUZZ_DOOR_BELL;
                        mifare_time = RC522_CARD_INVALID_EVENT_TIME;
					}
				}
			}
			else if(handmaid_card_cycles == 0U)
			{
                if(IsDISPLAY_UserCardInfoTextEnabled())
                {
                    /**
                    *	will use rc522 rx and tx buffer as temp for displaying user data
                    */
                    ClearBuffer(rc522_rx_buff, sizeof(rc522_rx_buff));
                    ClearBuffer(rc522_tx_buff, sizeof(rc522_tx_buff));
                    
                    i = 0U;

                    while((i < 16U) && (sector_0.block_1[i] != 0xFFU))	// copy user data from card to display buffer
                    {	
                        rc522_rx_buff[i] = sector_0.block_1[i];
                        ++i;
                    }

                    if((rc522_rx_buff[0] != 0U) && (sector_2.block_0[11] == 'M'))
                    {
                        rc522_rx_buff[0] |= (1U << 7);
                    }

                    i = 0U;

                    while((i < 16U) && (sector_0.block_2[i] != 0xFFU))	// copy user data from card to display buffer
                    {	
                        rc522_tx_buff[i] = sector_0.block_2[i];
                        ++i;
                    }
                }
				DoorLockCoil_On();
				PowerContactor_On();
				DISPLAY_CardValidImage();			
				SignalBuzzer = BUZZ_CARD_VALID;
                mifare_time = RC522_CARD_VALID_EVENT_TIME;
			}
			else if(handmaid_card_cycles == 3U)
			{
				handmaid_card_cycles = 0U;
			}
		}
		else
		{
			handmaid_card_cycles = 0U;
			SignalBuzzer = BUZZ_CARD_INVALID;
            mifare_time = RC522_CARD_INVALID_EVENT_TIME;
			
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
				else if((sCard.expiry_time[0] == EXPIRY_TIME_INVALID) || \
						(sCard.expiry_time[1] == EXPIRY_TIME_INVALID) || \
						(sCard.expiry_time[2] == EXPIRY_TIME_INVALID) || \
						(sCard.expiry_time[3] == EXPIRY_TIME_INVALID) || \
						(sCard.expiry_time[4] == EXPIRY_TIME_INVALID) || \
						(sCard.expiry_time[5] == EXPIRY_TIME_INVALID))
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
			
			LogEvent.log_card_id[0] = sCard.card_id[0];
			LogEvent.log_card_id[1] = sCard.card_id[1];
			LogEvent.log_card_id[2] = sCard.card_id[2];
			LogEvent.log_card_id[3] = sCard.card_id[3];
			LogEvent.log_card_id[4] = sCard.card_id[4];			
			LOGGER_Write();			
		}		
	}
	else if (IsRC522_HandmaidReentranceActiv())
	{
		HAL_RTC_GetTime(&hrtc, &time_m, RTC_FORMAT_BCD);
		HAL_RTC_GetDate(&hrtc, &date_m, RTC_FORMAT_BCD);
							
		aRoomPowerExpiryDateTime[0] = date_m.Date;
		aRoomPowerExpiryDateTime[1] = date_m.Month;
		aRoomPowerExpiryDateTime[2] = date_m.Year;
		aRoomPowerExpiryDateTime[4] = time_m.Minutes;
		SIGNAL_ExpiryTimeFromGuestCardReset();
		
		if((time_m.Hours + RC522_POWER_VALID_GROUP_PERMITED) < 0x24U)
		{
			aRoomPowerExpiryDateTime[3] = (time_m.Hours + RC522_POWER_VALID_GROUP_PERMITED);
		}
		else
		{
			aRoomPowerExpiryDateTime[3] = 0x24U;
		}
		
		LOGGER_Write();
		DoorLockCoil_On();
		PowerContactor_On();
		DISPLAY_CardValidImage();			
		SignalBuzzer = BUZZ_CARD_VALID;
        mifare_time = RC522_CARD_VALID_EVENT_TIME;
		RC522_HandmaidReentranceDisable();
	}
	else
	{
		if(!(dout_0_7_remote & (1U << 6))) DoorLockCoil_Off();
		RC522_RST_SetLow(); 
		DIO_SetOuput();
		
		for (i = 10U; i != 0U; i--)
		{
			HAL_Delay(1);
			if(eComState == COM_PACKET_RECEIVED) RS485_Service();
		}
		    
		RC522_RST_SetHigh(); 
		DIO_SetOuput();
		
		for (i = 10U; i != 0U; i--)
		{
			HAL_Delay(1);
			if(eComState == COM_PACKET_RECEIVED) RS485_Service();
		}
		
		RC522_Reset();
		RC522_WriteRegister(RC522_REG_T_MODE, 0x8DU);
		RC522_WriteRegister(RC522_REG_T_PRESCALER, 0x3EU);
		RC522_WriteRegister(RC522_REG_T_RELOAD_L, 30U);           
		RC522_WriteRegister(RC522_REG_T_RELOAD_H, 0U);
		RC522_WriteRegister(RC522_REG_TX_AUTO, 0x40U);
		RC522_WriteRegister(RC522_REG_MODE, 0x3DU);
		RC522_AntennaOn();
        mifare_time = RC522_PROCESS_TIME;
	}
}


void RC522_ClearData(void)
{
	uint32_t i;
		
    sCard.card_status = 0U;
    sCard.user_group = 0U;
    sCard.system_id = 0U;
    sCard.controller_id = 0U;
	ClearBuffer(sCard.card_id, 5U);
	ClearBuffer(sCard.expiry_time, 6U);
    
	for(i = 0U; i < 16U; i++)
	{
        rc522_rx_buff[i] = 0U;
        rc522_tx_buff[i] = 0U;
        
		sector_0.block_0[i] = 0U;
		sector_0.block_1[i] = 0U;
		sector_0.block_2[i] = 0U;
		sector_0.block_3[i] = 0U;
		
		sector_1.block_0[i] = 0U;
		sector_1.block_1[i] = 0U;
		sector_1.block_2[i] = 0U;
		sector_1.block_3[i] = 0U;
		
		sector_2.block_0[i] = 0U;
		sector_2.block_1[i] = 0U;
		sector_2.block_2[i] = 0U;
		sector_2.block_3[i] = 0U;
	}
}


static RC522_StatusTypeDef RC522_Check(uint8_t* id) 
{
	RC522_StatusTypeDef status;
	
	status = RC522_Request(PICC_REQIDL, id);            // Find cards, return card type
    
	if (status == MI_OK) 
    {                                                   // Card detected
		status = RC522_Anticoll(id);                    // Anti-collision, return card serial number 4 bytes
	}
    
	RC522_Halt();                                       // Command card into hibernation 

	return (status);
}


static void RC522_WriteRegister(uint8_t addr, uint8_t val) 
{
	rc522_tx_buff[0] = addr;
    rc522_tx_buff[1] = val;     // set value
	if(HAL_I2C_Master_Transmit(&hi2c1, RC522_WRITE, rc522_tx_buff, 2U, RC522_TIMEOUT) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
}


static uint8_t RC522_ReadRegister(uint8_t addr) 
{
	rc522_tx_buff[0] = addr;
	if(HAL_I2C_Master_Transmit(&hi2c1, RC522_WRITE, rc522_tx_buff, 1U, RC522_TIMEOUT) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
	if(HAL_I2C_Master_Receive(&hi2c1, RC522_READ, rc522_rx_buff, 1U, RC522_TIMEOUT) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
    return (rc522_rx_buff[0]);
}


static void RC522_SetBitMask(uint8_t reg, uint8_t mask) 
{   
	RC522_WriteRegister(reg, RC522_ReadRegister(reg) | mask);
}


static void RC522_ClearBitMask(uint8_t reg, uint8_t mask)
{  
	RC522_WriteRegister(reg, RC522_ReadRegister(reg) & (~mask));
}


static void RC522_AntennaOn(void) 
{ 
	uint8_t temp;

	temp = RC522_ReadRegister(RC522_REG_TX_CONTROL);
    
	if (!(temp & 0x03U))        
    {
		RC522_SetBitMask(RC522_REG_TX_CONTROL, 0x03U);
	}
}


static RC522_StatusTypeDef RC522_Reset(void) 
{  
    uint32_t delay;
    /**
    *   Issue the SoftReset command.
    */
	RC522_WriteRegister(RC522_REG_COMMAND, PCD_RESETPHASE);
    /**
    *   The datasheet does not mention how long the SoftRest command takes to complete.
    *   But the RC522 might have been in soft power-down mode (triggered by bit 4 of CommandReg)
    *   Section 8.8.2 in the datasheet says the oscillator start-up time_m is the start up time_m of the crystal + 37,74us. Let us be generous: 50ms.
    */
    HAL_Delay(5);

	delay = 500U;
    /**
    *   Wait for the PowerDown bit in CommandReg to be cleared
    */	
    while (RC522_ReadRegister(RC522_REG_COMMAND) & (1U << 4))
    {
		if(eComState != COM_PACKET_PENDING) RS485_Service();
        /**
        *   RC522 still restarting - unlikely after waiting 50ms and more
        *   mifare modul is unresponsive so return error status
        */
        --delay;
        
        if(delay == 0U)
		{			
            return (MI_ERR);
        }
		else
		{
			HAL_Delay(1);
		}
    }
    /**
    *   reset finished - return OK flag
    */
    return (MI_OK);
}


static RC522_StatusTypeDef RC522_Request(uint8_t reqMode, uint8_t* TagType) 
{
	RC522_StatusTypeDef status;  
	uint16_t backBits;                                  //The received data bits

	RC522_WriteRegister(RC522_REG_BIT_FRAMING, 0x07U);	// TxLastBits = BitFramingReg[2..0]	???

	TagType[0] = reqMode;
	status = RC522_ToCard(PCD_TRANSCEIVE, TagType, 1U, TagType, &backBits);

	if ((status != MI_OK) || (backBits != (1U << 4))) 
    {
		status = MI_ERR;
	}

	return (status);
}


static RC522_StatusTypeDef RC522_ToCard(uint8_t command, uint8_t* sendData, uint8_t sendLen, uint8_t* backData, uint16_t* backLen) 
{
    RC522_StatusTypeDef status = MI_ERR;
	uint8_t irqEn = 0U;
	uint8_t waitIRq = 0U;
	uint32_t n, i, lastBits; 

	switch (command) 
	{
		case PCD_AUTHENT:
			irqEn = 0x12U;
			waitIRq = 0x10U;            
			break;
        
		case PCD_TRANSCEIVE:            
			irqEn = 0x77U;
			waitIRq = 0x30U;            
			break;
        
		default:
			break;
	}
	
	RC522_WriteRegister(RC522_REG_COMM_IE_N, (irqEn | (1U << 7)));
	RC522_ClearBitMask(RC522_REG_COMM_IRQ, (1U << 7));
	RC522_SetBitMask(RC522_REG_FIFO_LEVEL, (1U << 7));
	RC522_WriteRegister(RC522_REG_COMMAND, PCD_IDLE);

	for (i = 0U; i < sendLen; i++) 
    {   
		RC522_WriteRegister(RC522_REG_FIFO_DATA, sendData[i]);   //Writing data to the FIFO
	}

	RC522_WriteRegister(RC522_REG_COMMAND, command);            //Execute the command
    
	if (command == PCD_TRANSCEIVE) 
    {    
		RC522_SetBitMask(RC522_REG_BIT_FRAMING, (1U << 7));     //StartSend=1,transmission of data starts  
    }
    /**
    *   Waiting to receive data to complete
    */
	i = 2000U;	//i according to the clock frequency adjustment, the operator M1 card maximum waiting time_m 25ms???
    
	do {
        /**
        *   CommIrqReg[7..0]
        *   Set1 TxIRq RxIRq IdleIRq HiAlerIRq LoAlertIRq ErrIRq TimerIRq
        */
		n = RC522_ReadRegister(RC522_REG_COMM_IRQ);
		--i;
	} 
    while ((i != 0U) && !(n & (1U << 0)) && !(n & waitIRq));          // End of do...while loop            
    /**
    *   StartSend=0
    */
	RC522_ClearBitMask(RC522_REG_BIT_FRAMING, (1U << 7));

	if (i != 0U)  
    {
		if (!(RC522_ReadRegister(RC522_REG_ERROR) & 0x1BU)) 
        {
			status = MI_OK;
            
			if (n & irqEn & (1U << 0)) 
            {
				status = MI_NOTAGERR;
			}

			if (command == PCD_TRANSCEIVE) 
            {
				n = RC522_ReadRegister(RC522_REG_FIFO_LEVEL);
				lastBits = (RC522_ReadRegister(RC522_REG_CONTROL) & 0x07U);
                
				if (lastBits != 0U) *backLen = ((n - 1U) * 8U + lastBits);  
                else *backLen = (n * 8U);  

				if (n == 0U) n = 1U;
                
				if (n > RC522_BUFF_LEN) n = RC522_BUFF_LEN;   
				/**
                *   Reading the received data in FIFO
                */
				for (i = 0U; i < n; i++) 
                {
					backData[i] = RC522_ReadRegister(RC522_REG_FIFO_DATA);
				}
			}
		} 
        else 
        {
			status = MI_ERR;
		}
	}

	return (status);
}


static RC522_StatusTypeDef RC522_Anticoll(uint8_t* serNum) 
{
	RC522_StatusTypeDef status;
	uint32_t i;
	uint8_t serNumCheck = 0U;
	uint16_t unLen;

	RC522_WriteRegister(RC522_REG_BIT_FRAMING, 0U);   // TxLastBists = BitFramingReg[2..0]
	serNum[0] = PICC_ANTICOLL;
	serNum[1] = 0x20U;
	status = RC522_ToCard(PCD_TRANSCEIVE, serNum, 2U, serNum, &unLen);

	if (status == MI_OK) 
    {
		/**
        *   Check card serial number
        */
		for (i = 0U; i < 4U; i++) 
        {
			serNumCheck ^= serNum[i];
		}
        
		if (serNumCheck != serNum[i])
        {
			status = MI_ERR;
		}
	}
    
	return (status);
}


static void RC522_CalculateCRC(uint8_t*  pIndata, uint8_t len, uint8_t* pOutData)
{ 
	uint32_t i, n;

	RC522_WriteRegister(RC522_REG_COMMAND, PCD_IDLE);
	RC522_ClearBitMask(RC522_REG_DIV_IRQ, 0x04U);       // CRCIrq = 0
	RC522_SetBitMask(RC522_REG_FIFO_LEVEL, 0x80U);      // Clear the FIFO pointer
	/**
    *   Write_RC522(CommandReg, PCD_IDLE);
    *   Writing data to the FIFO
    */
	for (i = 0U; i < len; i++) 
    {
		RC522_WriteRegister(RC522_REG_FIFO_DATA, *(pIndata + i)); 
	}
    /**
	*   Start CRC calculation
    */
	RC522_WriteRegister(RC522_REG_COMMAND, PCD_CALCCRC);
    /**
	*   Wait for CRC calculation to complete
    */
	i = 0xFFU;
    
	do 
    {
        n = RC522_ReadRegister(RC522_REG_DIV_IRQ);
		--i;
	} 
    while ((i != 0U) && !(n & (1U << 2)));              // wait for CRCIrq = 1
	
	RC522_WriteRegister(RC522_REG_COMMAND, PCD_IDLE);
	/**
    *   Read CRC calculation result
    */
	pOutData[0] = RC522_ReadRegister(RC522_REG_CRC_RESULT_L);
	pOutData[1] = RC522_ReadRegister(RC522_REG_CRC_RESULT_M);
}


static uint8_t RC522_SelectTag(uint8_t* serNum) 
{
    RC522_StatusTypeDef status;
	uint32_t i;
	uint16_t recvBits;
	uint8_t buffer[9], size; 

	buffer[0] = PICC_SELECTTAG;
	buffer[1] = 0x70U;
    
	for (i = 0U; i < 5U; i++) 
    {
		buffer[i + 2U] = *(serNum + i);
	}
    
	RC522_CalculateCRC(buffer, 7U, &buffer[7]);		//??
	status = RC522_ToCard(PCD_TRANSCEIVE, buffer, 9U, buffer, &recvBits);

	if ((status == MI_OK) && (recvBits == 0x18U)) 
    {
		size = buffer[0]; 
	} 
    else 
    {
		size = 0U;
	}

	return (size);
}


static RC522_StatusTypeDef RC522_Auth(uint8_t authMode, uint8_t BlockAddr, uint8_t* Sectorkey, uint8_t* serNum) 
{
    RC522_StatusTypeDef status;
	uint16_t recvBits;
	uint8_t i;
	uint8_t buff[12]; 

	//Verify the command block address + sector + password + card serial number
	buff[0] = authMode;
	buff[1] = BlockAddr;
    
	for (i = 0U; i < 6U; i++) 
    { 
		buff[i + 2U] = *(Sectorkey + i); 
	}
    
	for (i = 0U; i < 4U; i++) 
    {
		buff[i + 8U] = *(serNum + i);
	}
    
	status = RC522_ToCard(PCD_AUTHENT, buff, 12U, buff, &recvBits);

	if ((status != MI_OK) || (!(RC522_ReadRegister(RC522_REG_STATUS2) & (1U << 3)))) 
    {
		status = MI_ERR;
	}
    
	return (status);
}


static RC522_StatusTypeDef RC522_Read(uint8_t blockAddr, uint8_t* recvData) 
{
	RC522_StatusTypeDef status;
	uint16_t unLen;

	recvData[0] = PICC_READ;
	recvData[1] = blockAddr;
	RC522_CalculateCRC(recvData, 2U, &recvData[2]);
	status = RC522_ToCard(PCD_TRANSCEIVE, recvData, 4U, recvData, &unLen);

	if ((status != MI_OK) || (unLen != 0x90U)) 
    {
		status = MI_ERR;
	}

	return (status);
}


static void RC522_Halt(void) 
{
	uint16_t unLen;
	uint8_t buff[4]; 

	buff[0] = PICC_HALT;
	buff[1] = 0U;
	RC522_CalculateCRC(buff, 2U, &buff[2]);
	RC522_ToCard(PCD_TRANSCEIVE, buff, 4U, buff, &unLen);
}


static RC522_StatusTypeDef RC522_ReadCard(void)
{
    uint32_t i;
    uint8_t str[RC522_BUFF_LEN];
    RC522_StatusTypeDef status;
	
    RC522_Request(PICC_REQIDL, str);	
    status = RC522_Anticoll(str);

    for(i = 0U; i < 5U; i++)
    {
        card_id[i] = str[i];
    }

    RC522_SelectTag(card_id);	
	
    status = RC522_Auth(PICC_AUTHENT1A, SECTOR_0, mifare_key_a, card_id);
	
    if(status != MI_OK)
	{
		RC522_Halt();
		return (status);
	}
	else
	{    
        RC522_Read(SECTOR_0, &sector_0.block_0[0]);
        RC522_Read((SECTOR_0 + 1U), &sector_0.block_1[0]);
        RC522_Read((SECTOR_0 + 2U), &sector_0.block_2[0]);
        RC522_Read((SECTOR_0 + 3U), &sector_0.block_3[0]);
	}
	
	status = RC522_Auth(PICC_AUTHENT1A, SECTOR_1, mifare_key_a, card_id);
	
    if(status != MI_OK)
	{	
		RC522_Halt();
		return (status);
	}
	else
	{      
        RC522_Read(SECTOR_1, &sector_1.block_0[0]);
        RC522_Read((SECTOR_1 + 1U), &sector_1.block_1[0]);
        RC522_Read((SECTOR_1 + 2U), &sector_1.block_2[0]);
        RC522_Read((SECTOR_1 + 3U), &sector_1.block_3[0]);    
	}
	
	status = RC522_Auth(PICC_AUTHENT1A, SECTOR_2, mifare_key_a, card_id);
	
    if(status != MI_OK)
	{		
		RC522_Halt();
		return (status);
	}
	else
	{      
        RC522_Read(SECTOR_2, &sector_2.block_0[0]);
        RC522_Read((SECTOR_2 + 1U), &sector_2.block_1[0]);
        RC522_Read((SECTOR_2 + 2U), &sector_2.block_2[0]);
        RC522_Read((SECTOR_2 + 3U), &sector_2.block_3[0]); 	
	}

    RC522_Halt();
	return (status);
}


static RC522_StatusTypeDef RC522_VerifyData(void)
{
    RTC_TimeTypeDef time_m;
    RTC_DateTypeDef date_m;
	uint8_t b_cnt, m_cnt;
	uint32_t temp_address;
	uint8_t ee_buf[16];
    
	for(b_cnt = 0U; b_cnt < 5U; b_cnt++)
	{
		sCard.card_id[b_cnt] = card_serial[b_cnt];
	}
    /**
	*			U S E R S  G R O U P   C H E C K
	**/	
	ee_buf[0] = (EE_PERMITED_GROUP_ADDRESS >> 8);
	ee_buf[1] = (EE_PERMITED_GROUP_ADDRESS & 0xFFU);
	if(HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, ee_buf, 2U, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
	if(HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, ee_buf, 16U, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(RC522_FUNC,  I2C_DRIVER);
	
	for(b_cnt = 0U; b_cnt < 16U; b_cnt++)
	{
		for(m_cnt = 0U; m_cnt < 16U; m_cnt++)
		{
			if((sector_1.block_0[b_cnt] == ee_buf[m_cnt]) && (ee_buf[m_cnt] != 0U) \
				&& (ee_buf[m_cnt] != CARD_DATA_FORMATED))
			{				
				sCard.user_group = ee_buf[m_cnt];
				m_cnt = 16U;
				b_cnt = 16U;
			}
			else if((sector_1.block_0[b_cnt] == 0U) || (sector_1.block_0[b_cnt] == CARD_DATA_FORMATED))
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
	temp_address = 0U;
	
	if(IS_09(sector_1.block_1[0]))  temp_address += (sector_1.block_1[0] - 48U) * 10000U;
	if(IS_09(sector_1.block_1[1]))  temp_address += (sector_1.block_1[1] - 48U) * 1000U;
	if(IS_09(sector_1.block_1[2]))  temp_address += (sector_1.block_1[2] - 48U) * 100U;
	if(IS_09(sector_1.block_1[3]))  temp_address += (sector_1.block_1[3] - 48U) * 10U;
	if(IS_09(sector_1.block_1[4]))  temp_address += (sector_1.block_1[4] - 48U);

	if((((temp_address >> 8U) & 0xFFU) == system_id[0]) && ((temp_address & 0xFFU) == system_id[1]))
	{
		sCard.system_id = temp_address & 0xFFFFU;
	}
	else if((sector_1.block_1[0] == 0U) || \
			(sector_1.block_1[1] == 0U) || \
			(sector_1.block_1[2] == 0U) || \
			(sector_1.block_1[3] == 0U) || \
			(sector_1.block_1[4] == 0U) || \
			(sector_1.block_1[0] == CARD_DATA_FORMATED) || \
			(sector_1.block_1[1] == CARD_DATA_FORMATED) || \
			(sector_1.block_1[2] == CARD_DATA_FORMATED) || \
			(sector_1.block_1[3] == CARD_DATA_FORMATED) || \
			(sector_1.block_1[4] == CARD_DATA_FORMATED))
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
	if ((sector_2.block_0[6] == rs485_interface_address[0]) && (sector_2.block_0[7] == rs485_interface_address[1]) ) 
	{
		sCard.controller_id = (rs485_interface_address[0] << 8U) + rs485_interface_address[1];
	}
	else
	{
        for(b_cnt = 0; b_cnt < 8; b_cnt++)
        {
            if ((sector_2.block_0[6] == permitted_add[b_cnt][0]) && (sector_2.block_0[7] == permitted_add[b_cnt][1])) 
            {
                sCard.controller_id = ((permitted_add[b_cnt][0] << 8U) + permitted_add[b_cnt][1]);
                b_cnt = 8;
            }
        }
	}
	
	
	if(((sector_2.block_0[6] == 0U) && (sector_2.block_0[7] == 0U)) || \
       ((sector_2.block_0[6] == CARD_DATA_FORMATED) && (sector_2.block_0[7] == CARD_DATA_FORMATED)))
	{
		sCard.controller_id = CONTROLLER_ID_DATA_INVALID;
	}
	else if((sCard.controller_id == 0U) || (sCard.controller_id == 0xFFFFU))
	{
		sCard.controller_id = CONTROLLER_ID_INVALID;
	}
    /**
	*			C A R D   E X P I R Y   T I M E    C H E C K
	**/
	HAL_RTC_GetTime(&hrtc, &time_m, RTC_FORMAT_BCD);
	HAL_RTC_GetDate(&hrtc, &date_m, RTC_FORMAT_BCD);
			
	if (((sector_2.block_0[0] >> 4) > 3U) || (((sector_2.block_0[0] >> 4) ==  0U) && \
		((sector_2.block_0[0] & 0x0FU) == 0U)) || ((sector_2.block_0[0] & 0x0FU) > 9U))
	{
		sCard.expiry_time[0] = EXPIRY_TIME_DATA_INVALID;
	}
	else if (((sector_2.block_0[1] >> 4) > 1U) || (((sector_2.block_0[1] >> 4) == 0U) && \
			((sector_2.block_0[1] & 0x0FU) == 0U)) || ((sector_2.block_0[1] & 0x0FU) > 9U))
	{
		sCard.expiry_time[1] = EXPIRY_TIME_DATA_INVALID;
	}
	else if (((sector_2.block_0[2] >> 4) > 9U) || ((sector_2.block_0[2] & 0x0FU) > 9U))
	{
		sCard.expiry_time[2] = EXPIRY_TIME_DATA_INVALID;
	}
	else if (((sector_2.block_0[3] >> 4) > 2U) || ((sector_2.block_0[3] & 0x0FU) > 9U))
	{
		sCard.expiry_time[3] = EXPIRY_TIME_DATA_INVALID;
	}
	else if (((sector_2.block_0[4] >> 4) > 5U) || ((sector_2.block_0[4] & 0x0FU) > 9U))
	{
		sCard.expiry_time[4] = EXPIRY_TIME_DATA_INVALID;
	}
	else
	{
		if((sector_2.block_0[2] > date_m.Year) \
			|| ((sector_2.block_0[2] == date_m.Year) && (sector_2.block_0[1] > date_m.Month))	\
			|| ((sector_2.block_0[2] == date_m.Year) && (sector_2.block_0[1] == date_m.Month) && (sector_2.block_0[0] > date_m.Date))	\
			|| ((sector_2.block_0[2] == date_m.Year) && (sector_2.block_0[1] == date_m.Month) && (sector_2.block_0[0] == date_m.Date) && (sector_2.block_0[3] > time_m.Hours))	\
			|| ((sector_2.block_0[2] == date_m.Year) && (sector_2.block_0[1] == date_m.Month) && (sector_2.block_0[0] == date_m.Date) && (sector_2.block_0[3] == time_m.Hours) 	\
			&& (sector_2.block_0[4] >= time_m.Minutes)))
		{
			for(b_cnt = 0U; b_cnt < 6U; b_cnt++)
			{
				sCard.expiry_time[b_cnt] = sector_2.block_0[b_cnt];	
			}	
		}
		else
		{
			for(b_cnt = 0U; b_cnt < 6U; b_cnt++)
			{
				sCard.expiry_time[b_cnt] = EXPIRY_TIME_INVALID;		
			}
		}
	}		
	/**
	*			S E T   C A R D     S T A T U S
	**/
	if ((sCard.user_group == USER_GROUP_INVALID) || (sCard.user_group == USER_GROUP_DATA_INVALID)			\
		|| (sCard.expiry_time[0] == EXPIRY_TIME_DATA_INVALID) || (sCard.expiry_time[1] == EXPIRY_TIME_DATA_INVALID) 	\
		|| (sCard.expiry_time[2] == EXPIRY_TIME_DATA_INVALID) || (sCard.expiry_time[3] == EXPIRY_TIME_DATA_INVALID) 	\
		|| (sCard.expiry_time[4] == EXPIRY_TIME_DATA_INVALID) || (sCard.expiry_time[5] == EXPIRY_TIME_DATA_INVALID) 	\
		|| (sCard.controller_id == CONTROLLER_ID_INVALID) || (sCard.controller_id == CONTROLLER_ID_DATA_INVALID)	\
		|| (sCard.system_id == SYSTEM_ID_INVALID) || (sCard.system_id == SYSTEM_ID_DATA_INVALID))
	 
    {
		 sCard.card_status = CARD_INVALID;
    }
    else if ((sCard.expiry_time[0] == EXPIRY_TIME_INVALID) || (sCard.expiry_time[1] == EXPIRY_TIME_INVALID) ||	\
             (sCard.expiry_time[2] == EXPIRY_TIME_INVALID) || (sCard.expiry_time[3] == EXPIRY_TIME_INVALID) || \
             (sCard.expiry_time[4] == EXPIRY_TIME_INVALID) || (sCard.expiry_time[5] == EXPIRY_TIME_INVALID))
    {
        sCard.card_status = EXPIRY_TIME_INVALID;
    }
    else
    {
        sCard.card_status = CARD_VALID;
    }
	
	if(sCard.card_status == CARD_VALID) return (MI_OK);
	m_cnt = 0U;
	/**
	*			C H E C K   I S   R E S E T   C A R D   S E R I A L    
	**/
	for(b_cnt = 0U; b_cnt < 5U; b_cnt++)
	{
		if(card_serial[b_cnt] == reset_card_serial[b_cnt]) ++m_cnt;
	}

	if(m_cnt > 4U)
    {
        LogEvent.log_event = SOFTWARE_RESET;
        LOGGER_Write();
        BootloaderExe();
    }
    
	return (MI_OK);
}


