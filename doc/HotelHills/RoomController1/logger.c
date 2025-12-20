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
#include "time.h"

/* Defines    ----------------------------------------------------------------*/

/* Types  --------------------------------------------------------------------*/

/* Variables  ----------------------------------------------------------------*/
uint8_t aLoggerBuffer[LOGGER_BUFFER_SIZE];
uint8_t logger_pcnt;

uint16_t logger_list_count;
uint16_t logger_next_log_id;
uint16_t logger_next_log_address;
uint16_t logger_tmp_log_address;

uint32_t logger_timer;
uint32_t logger_flags;

LOGGER_EventTypeDef LogEvent;

extern I2C_HandleTypeDef hi2c1;
extern RTC_HandleTypeDef hrtc;
extern RTC_DateTypeDef date;
extern RTC_TimeTypeDef time;

/* Macros     ----------------------------------------------------------------*/

/* Private prototypes    -----------------------------------------------------*/
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

	HAL_I2C_Mem_Read(&hi2c1, EE_READ_CMD, logger_next_log_address, I2C_MEMADD_SIZE_16BIT, aLoggerBuffer, LOG_SIZE, EE_I2C_TIMEOUT);
	
	while(logger_next_log_address <= (EE_LOG_LIST_END_ADDRESS - LOG_SIZE))
	{	
		if((aLoggerBuffer[0] == NULL) && (aLoggerBuffer[1] == NULL)) break;

		logger_next_log_address += LOG_SIZE;
		++logger_next_log_id;
		++logger_list_count;
		HAL_I2C_Mem_Read(&hi2c1, EE_READ_CMD, logger_next_log_address, I2C_MEMADD_SIZE_16BIT, aLoggerBuffer, LOG_SIZE, EE_I2C_TIMEOUT);
	}
}

LOGGER_StatusTypeDef LOGGER_Write(void)
{
	HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BCD);
	HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BCD);

	aLoggerBuffer[0] = (logger_next_log_id >> 8);
	aLoggerBuffer[1] = (logger_next_log_id & 0xff);
//	aLoggerBuffer[2] = LogEvent.log_event;
//	aLoggerBuffer[3] = LogEvent.log_type;
//	aLoggerBuffer[4] = LogEvent.log_group;
//	aLoggerBuffer[6] = LogEvent.log_card_id[0];
//	aLoggerBuffer[7] = LogEvent.log_card_id[1];
//	aLoggerBuffer[8] = LogEvent.log_card_id[2];
//	aLoggerBuffer[9] = LogEvent.log_card_id[3];
//	aLoggerBuffer[9] = LogEvent.log_card_id[4];
	aLoggerBuffer[10] = date.Date;
	aLoggerBuffer[11] = date.Month;
	aLoggerBuffer[12] = date.Year;
	aLoggerBuffer[13] = time.Hours;
	aLoggerBuffer[14] = time.Minutes;
	aLoggerBuffer[15] = time.Seconds;
	
	if(logger_next_log_address > (EE_LOG_LIST_END_ADDRESS - LOG_SIZE))
	{
		return (LOGGER_FULL);
	}
	else if(HAL_I2C_IsDeviceReady(&hi2c1, EE_READ_CMD, 1000, EE_I2C_TIMEOUT) == HAL_OK)
	{
		I2C_EE_WriteEnable();
		
		if(HAL_I2C_Mem_Write(&hi2c1, EE_WRITE_CMD, logger_next_log_address, I2C_MEMADD_SIZE_16BIT, \
							 aLoggerBuffer, LOG_SIZE, EE_I2C_TIMEOUT) != HAL_OK)
		{
			I2C_EE_WriteDisable();
			return(LOGGER_ERROR);
		}
		
		I2C_EE_WriteDisable();
	}
	else
	{
		return (LOGGER_ERROR);
	}
	
	++logger_list_count;
	++logger_next_log_id;
	logger_next_log_address += LOG_SIZE;

	logger_pcnt = 0;
	while(logger_pcnt < LOG_SIZE) aLoggerBuffer[logger_pcnt++] == NULL;
		
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
	
	if(HAL_I2C_Mem_Read(&hi2c1, EE_READ_CMD, logger_tmp_log_address, I2C_MEMADD_SIZE_16BIT, \
						aLoggerBuffer, LOG_SIZE, EE_I2C_TIMEOUT) != HAL_OK)
	{
		return(LOGGER_ERROR);
	}
	else
	{
		return(LOGGER_OK);
	}
}

LOGGER_StatusTypeDef LOGGER_Delete(uint16_t log_id)
{
	if(logger_list_count == 0)
	{
		return(LOGGER_EMPTY);
	}
	else if(log_id == 0x0000)
	{
		logger_pcnt = 0;
		while(logger_pcnt < LOG_SIZE) aLoggerBuffer[logger_pcnt++] == NULL;
		logger_tmp_log_address = logger_next_log_address - LOG_SIZE;
		
		if(HAL_I2C_IsDeviceReady(&hi2c1, EE_READ_CMD, 1000, EE_I2C_TIMEOUT) == HAL_OK)
		{
			I2C_EE_WriteEnable();
			
			if(HAL_I2C_Mem_Write(&hi2c1, EE_WRITE_CMD, logger_tmp_log_address, I2C_MEMADD_SIZE_16BIT, \
								 aLoggerBuffer, LOG_SIZE, EE_I2C_TIMEOUT) != HAL_OK)
			{
				I2C_EE_WriteDisable();
				return(LOGGER_ERROR);
			}
			
			I2C_EE_WriteDisable();
			logger_next_log_address -= LOG_SIZE;
			--logger_list_count;
		}
		else
		{
			return (LOGGER_ERROR);
		}
	}
	else if(log_id == 0xffff)
	{
		I2C_EE_WriteEnable();
		
		logger_pcnt = 0;
		while(logger_pcnt < LOG_SIZE) aLoggerBuffer[logger_pcnt++] == NULL;
		logger_tmp_log_address = EE_LOG_LIST_START_ADDRESS;
		
		while(logger_tmp_log_address <= (EE_LOG_LIST_END_ADDRESS - LOG_SIZE))
		{
			if(HAL_I2C_IsDeviceReady(&hi2c1, EE_READ_CMD, 1000, EE_I2C_TIMEOUT) == HAL_OK)
			{
				if(HAL_I2C_Mem_Write(&hi2c1, EE_WRITE_CMD, logger_tmp_log_address, I2C_MEMADD_SIZE_16BIT, \
									 aLoggerBuffer, LOG_SIZE, EE_I2C_TIMEOUT) != HAL_OK)
				{
					I2C_EE_WriteDisable();
					return(LOGGER_ERROR);
				}
			}
			else
			{
				I2C_EE_WriteDisable();
				return (LOGGER_ERROR);
			}
			
			logger_tmp_log_address += LOG_SIZE;			
		}
		
		I2C_EE_WriteDisable();
		
		logger_list_count = 0;
		logger_next_log_id = 1;
		logger_next_log_address = EE_LOG_LIST_START_ADDRESS;
		
		return(LOGGER_OK);
	}
	else if(log_id >= logger_next_log_id)
	{
		return(LOGGER_WRONG_ID);
	}
	else
	{
		return(LOGGER_WRONG_ID);
	}
	
	return (LOGGER_ERROR);
}

/******************************   END OF FILE  **********************************/
