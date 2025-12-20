/**
 ******************************************************************************
 * File Name          : logger.c
 * Date               : 28/02/2016 23:16:19
 * Description        : data logger software modul
 ******************************************************************************
 *
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "logger.h"
#include "eeprom.h"
#include "common.h"

/* Defines    ----------------------------------------------------------------*/

/* Types  --------------------------------------------------------------------*/

/* Variables  ----------------------------------------------------------------*/
uint8_t logger_pcnt;
uint16_t logger_list_count;
uint16_t logger_next_log_id;
uint16_t logger_next_log_address;
uint16_t logger_tmp_log_address;

uint32_t logger_timer;
uint32_t logger_flags;

LOGGER_EventTypeDef LogEvent;
LOGGER_StatusTypeDef LOGGER_Status;

extern I2C_HandleTypeDef hi2c1;
extern RTC_HandleTypeDef hrtc;
extern RTC_DateTypeDef date;
extern RTC_TimeTypeDef time;
extern uint8_t sys_status;

/* Macros     ----------------------------------------------------------------*/

/* Private prototypes    -----------------------------------------------------*/
extern void MX_I2C1_Init(void);

void LOGGER_Init(void);
LOGGER_StatusTypeDef LOGGER_Write(void);
LOGGER_StatusTypeDef LOGGER_Read(uint16_t log_id);
LOGGER_StatusTypeDef LOGGER_Delete(uint16_t log_id);

/* Program code   ------------------------------------------------------------*/
void LOGGER_Init(void)
{
	LogEvent.log_event = NULL;
	LogEvent.log_type = NULL;
	LogEvent.log_group = NULL;
	LogEvent.log_card_id[0] = NULL;
	LogEvent.log_card_id[1] = NULL;
	LogEvent.log_card_id[2] = NULL;
	LogEvent.log_card_id[3] = NULL;
	LogEvent.log_card_id[4] = NULL;
	LogEvent.log_time_stamp[0] = NULL;
	LogEvent.log_time_stamp[1] = NULL;
	LogEvent.log_time_stamp[2] = NULL;
	LogEvent.log_time_stamp[3] = NULL;
	LogEvent.log_time_stamp[4] = NULL;
	LogEvent.log_time_stamp[5] = NULL;
	
	logger_pcnt = 0;
	logger_list_count = 0;
	logger_next_log_id = 1;
	logger_next_log_address = EE_LOG_LIST_START_ADDRESS;
	logger_tmp_log_address = 0;
	LOGGER_Status = LOGGER_OK;

	aEepromBuffer[0] = logger_next_log_address >> 8;
	aEepromBuffer[1] = logger_next_log_address;
	HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
	HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, LOG_SIZE, I2C_EE_TIMEOUT);
	
	while(logger_next_log_address <= (EE_LOG_LIST_END_ADDRESS - LOG_SIZE))
	{	
		if((aEepromBuffer[0] == NULL) && (aEepromBuffer[1] == NULL)) break;

		logger_next_log_address += LOG_SIZE;
		++logger_next_log_id;
		++logger_list_count;
		aEepromBuffer[0] = logger_next_log_address >> 8;
		aEepromBuffer[1] = logger_next_log_address;
		HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
		HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, LOG_SIZE, I2C_EE_TIMEOUT);
	}
	
	/**
	*	set log list not empty 	-> system status flag
	*	set log list full  		-> system status flag
	*/
	if(logger_list_count != 0) sys_status |= 0x01;
	if(logger_next_log_address >= (EE_LOG_LIST_END_ADDRESS - LOG_SIZE)) sys_status |= 0x02;
	
#ifdef DEBUG_LOGGEER
	Serial_PutString("logger init\n\r");
	Serial_PutString("log list count: ");
	logger_pcnt = 0;
	while(logger_pcnt < LOG_SIZE) aEepromBuffer[logger_pcnt++] = NULL;
	Int2Str(aEepromBuffer, logger_list_count);
	Serial_PutString(aEepromBuffer);
	Serial_PutString("\n\r");
	Serial_PutString("next log address: ");
	logger_pcnt = 0;
	while(logger_pcnt < LOG_SIZE) aEepromBuffer[logger_pcnt++] = NULL;
	Int2Str(aEepromBuffer, logger_next_log_address);
	Serial_PutString(aEepromBuffer);
	Serial_PutString("\n\r");
#endif
}

LOGGER_StatusTypeDef LOGGER_Write(void)
{
	HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BCD);
	HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BCD);
	
	logger_pcnt = 0;
	while(logger_pcnt < LOG_SIZE) aEepromBuffer[logger_pcnt++] = NULL;
	
	aEepromBuffer[2] = (logger_next_log_id >> 8);
	aEepromBuffer[3] = (logger_next_log_id & 0xff);
	aEepromBuffer[4] = LogEvent.log_event;
	aEepromBuffer[5] = LogEvent.log_type;
	aEepromBuffer[6] = LogEvent.log_group;
	aEepromBuffer[7] = LogEvent.log_card_id[0];
	aEepromBuffer[8] = LogEvent.log_card_id[1];
	aEepromBuffer[9] = LogEvent.log_card_id[2];
	aEepromBuffer[10] = LogEvent.log_card_id[3];
	aEepromBuffer[11] = LogEvent.log_card_id[4];
	aEepromBuffer[12] = date.Date;
	aEepromBuffer[13] = date.Month;
	aEepromBuffer[14] = date.Year;
	aEepromBuffer[15] = time.Hours;
	aEepromBuffer[16] = time.Minutes;
	aEepromBuffer[17] = time.Seconds;
	
	if(logger_next_log_address > (EE_LOG_LIST_END_ADDRESS - LOG_SIZE))
	{
#ifdef DEBUG_LOGGEER
	Serial_PutString("log list full\n\r");
#endif
		sys_status |= 0x02;
		return (LOGGER_FULL);
	}
	
	I2C_EE_WriteEnable();
	aEepromBuffer[0] = logger_next_log_address >> 8;
	aEepromBuffer[1] = logger_next_log_address;
	HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 18, I2C_EE_TIMEOUT);
	//HAL_Delay(I2C_EE_WRITE_DELAY);
	HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
	I2C_EE_WriteDisable();
	++logger_list_count;
	++logger_next_log_id;
	logger_next_log_address += LOG_SIZE;
		
#ifdef DEBUG_LOGGEER
	Serial_PutString("log written\n\r");
	Serial_PutString("log list count: ");
	logger_pcnt = 0;
	while(logger_pcnt < LOG_SIZE) aEepromBuffer[logger_pcnt++] = NULL;
	Int2Str(aEepromBuffer, logger_list_count);
	Serial_PutString(aEepromBuffer);
	Serial_PutString("\n\r");
	Serial_PutString("next log address: ");
	logger_pcnt = 0;
	while(logger_pcnt < LOG_SIZE) aEepromBuffer[logger_pcnt++] = NULL;
	Int2Str(aEepromBuffer, logger_next_log_address);
	Serial_PutString(aEepromBuffer);
	Serial_PutString("\n\r");
#endif

	logger_pcnt = 0;
	while(logger_pcnt < LOG_SIZE) aEepromBuffer[logger_pcnt++] == NULL;
	sys_status |= 0x01;	
	return (LOGGER_OK);		
}

LOGGER_StatusTypeDef LOGGER_Read(uint16_t log_id)
{
	if(logger_list_count == 0)
	{
		return(LOGGER_EMPTY);
	}
	else if(log_id == 0x0000)
	{
		logger_tmp_log_address = logger_next_log_address - LOG_SIZE;
	}
	else if(log_id >= logger_next_log_id)
	{
		return(LOGGER_WRONG_ID);
	}
	else
	{
		logger_tmp_log_address = (logger_next_log_address - ((logger_next_log_id - log_id) * LOG_SIZE));
	}
	
	aEepromBuffer[0] = logger_tmp_log_address >> 8;
	aEepromBuffer[1] = logger_tmp_log_address;
	HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT);
	HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, LOG_SIZE, I2C_EE_TIMEOUT);
	
	return(LOGGER_OK);
}

LOGGER_StatusTypeDef LOGGER_Delete(uint16_t log_id)
{
	if(logger_list_count == 0)
	{
#ifdef DEBUG_LOGGEER
	Serial_PutString("log list empty\n\r");
#endif
		return(LOGGER_EMPTY);
	}
	else if(log_id == 0x0000)
	{
		logger_pcnt = 0;
		while(logger_pcnt < LOG_SIZE) aEepromBuffer[logger_pcnt++] = NULL;
		logger_tmp_log_address = logger_next_log_address - LOG_SIZE;
		
		if(HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, 1000, I2C_EE_TIMEOUT) == HAL_OK)
		{
			I2C_EE_WriteEnable();
			aEepromBuffer[0] = logger_tmp_log_address >> 8;
			aEepromBuffer[1] = logger_tmp_log_address;
			HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 18, I2C_EE_TIMEOUT);
			//HAL_Delay(I2C_EE_WRITE_DELAY);
			HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
			I2C_EE_WriteDisable();
			
#ifdef DEBUG_LOGGEER
	Serial_PutString("log id: ");
	logger_pcnt = 0;
	while(logger_pcnt < LOG_SIZE) aEepromBuffer[logger_pcnt++] = NULL;
	Int2Str(aEepromBuffer, logger_list_count);
	Serial_PutString(aEepromBuffer);
	Serial_PutString(" deleted\n\r");
#endif			
			logger_next_log_address -= LOG_SIZE;
			--logger_list_count;
			sys_status &= 0xfd;
			if(logger_list_count == 0) sys_status &= 0xfe;
			return (LOGGER_OK);
		}
		else
		{
#ifdef DEBUG_LOGGEER
	Serial_PutString("eeprom busy error\n\r");
#endif
			return (LOGGER_ERROR);
		}
	}
	else if(log_id == 0xffff)
	{
		I2C_EE_WriteEnable();
		logger_pcnt = 0;
		while(logger_pcnt < LOG_SIZE) aEepromBuffer[logger_pcnt++] = NULL;
		logger_tmp_log_address = EE_LOG_LIST_START_ADDRESS;
		
		while(logger_tmp_log_address <= (EE_LOG_LIST_END_ADDRESS - LOG_SIZE))
		{
			if(HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, 1000, I2C_EE_TIMEOUT) == HAL_OK)
			{
				aEepromBuffer[0] = logger_tmp_log_address >> 8;
				aEepromBuffer[1] = logger_tmp_log_address;
				HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 18, I2C_EE_TIMEOUT);
				//HAL_Delay(I2C_EE_WRITE_DELAY);
				HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY);
			}
			else
			{
#ifdef DEBUG_LOGGEER
				Serial_PutString("eeprom busy error\n\r");
#endif
				I2C_EE_WriteDisable();
				return (LOGGER_ERROR);
			}	
			
			logger_tmp_log_address += LOG_SIZE;			
		}
		
		I2C_EE_WriteDisable();
		logger_list_count = 0;
		logger_next_log_id = 1;
		logger_next_log_address = EE_LOG_LIST_START_ADDRESS;
		
#ifdef DEBUG_LOGGEER
	Serial_PutString("log list deleted\n\r");
	Serial_PutString("log list count: ");
	logger_pcnt = 0;
	while(logger_pcnt < LOG_SIZE) aEepromBuffer[logger_pcnt++] = NULL;
	Int2Str(aEepromBuffer, logger_list_count);
	Serial_PutString(aEepromBuffer);
	Serial_PutString("\n\r");
	Serial_PutString("next log address: ");
	logger_pcnt = 0;
	while(logger_pcnt < LOG_SIZE) aEepromBuffer[logger_pcnt++] = NULL;
	Int2Str(aEepromBuffer, logger_next_log_address);
	Serial_PutString(aEepromBuffer);
	Serial_PutString("\n\r");
#endif
		return(LOGGER_OK);
	}
	else if(log_id >= logger_next_log_id)
	{
#ifdef DEBUG_LOGGEER
	Serial_PutString("wrong log id\n\r");
#endif
		return(LOGGER_WRONG_ID);
	}
	else
	{
#ifdef DEBUG_LOGGEER
	Serial_PutString("wrong log id\n\r");
#endif
		return(LOGGER_WRONG_ID);
	}
}

/******************************   END OF FILE  **********************************/
