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
#include "main.h"
#include "common.h"
#include "eeprom.h"
#include "logger.h"
#include "rs485.h"


/* Imported Types  -----------------------------------------------------------*/
/* Imported Variables --------------------------------------------------------*/
/* Imported Functions    -----------------------------------------------------*/
/* Private Variables  --------------------------------------------------------*/
LOGGER_EventTypeDef LogEvent;
LOGGER_StatusTypeDef LOGGER_Status;
uint32_t logger_timer;
uint32_t logger_flags;
uint16_t logger_list_count;
uint16_t logger_next_log_id;
uint16_t logger_next_log_address;
uint16_t logger_tmp_log_address;
uint8_t logger_pcnt;


/* Private Macros    ---------------------------------------------------------*/
/* Private Prototypes    -----------------------------------------------------*/
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
	if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
	if (HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, LOG_SIZE, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
	
	while(logger_next_log_address <= (EE_LOG_LIST_END_ADDRESS - LOG_SIZE))
	{	
		if((aEepromBuffer[0] == NULL) && (aEepromBuffer[1] == NULL)) break;
		logger_next_log_address += LOG_SIZE;
		++logger_next_log_id;
		++logger_list_count;
		aEepromBuffer[0] = logger_next_log_address >> 8;
		aEepromBuffer[1] = logger_next_log_address;
		if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
		if (HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, LOG_SIZE, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
	}
	
	/**
	*	set log list not empty 	-> system status flag
	*	set log list full  		-> system status flag
	*/
	if(logger_list_count != 0) sys_status |= 0x01;
	if(logger_next_log_address >= (EE_LOG_LIST_END_ADDRESS - LOG_SIZE)) sys_status |= 0x02;
}


LOGGER_StatusTypeDef LOGGER_Write(void)
{
    ClearBuffer(aEepromBuffer, LOG_SIZE);
	
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
    
	if(logger_next_log_address > (EE_LOG_LIST_END_ADDRESS - LOG_SIZE))
	{
		sys_status |= 0x02;
		return (LOGGER_FULL);
	}	
	
	aEepromBuffer[0] = logger_next_log_address >> 8;
	aEepromBuffer[1] = logger_next_log_address;
	if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 18, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
	if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
	++logger_list_count;
	++logger_next_log_id;
	logger_next_log_address += LOG_SIZE;
	ClearBuffer(aEepromBuffer, LOG_SIZE);
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
	if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 2, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
	if (HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, aEepromBuffer, LOG_SIZE, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);	
	return(LOGGER_OK);
}

LOGGER_StatusTypeDef LOGGER_Delete(uint16_t log_id)
{
	if(logger_list_count == 0)
	{
		return(LOGGER_EMPTY);
	}
	else if(log_id == 0x0000)
	{
		ClearBuffer(aEepromBuffer, LOG_SIZE);
		logger_tmp_log_address = logger_next_log_address - LOG_SIZE;
		
		if(HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, 1000, I2C_EE_TIMEOUT) == HAL_OK)
		{
			aEepromBuffer[0] = logger_tmp_log_address >> 8;
			aEepromBuffer[1] = logger_tmp_log_address;
			if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 18, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
			if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);			
			logger_next_log_address -= LOG_SIZE;
			--logger_list_count;
			sys_status &= 0xfd;
			if(logger_list_count == 0) sys_status &= 0xfe;
			return (LOGGER_OK);
		}
		else
		{
			return (LOGGER_ERROR);
		}
	}
	else if(log_id == 0xffff)
	{
		ClearBuffer(aEepromBuffer, LOG_SIZE);
		logger_tmp_log_address = EE_LOG_LIST_START_ADDRESS;
		
		while(logger_tmp_log_address <= (EE_LOG_LIST_END_ADDRESS - LOG_SIZE))
		{
			if(HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) == HAL_OK)
			{
				aEepromBuffer[0] = logger_tmp_log_address >> 8;
				aEepromBuffer[1] = logger_tmp_log_address;
				if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, aEepromBuffer, 18, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
				if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
			}
			else
			{
				return (LOGGER_ERROR);
			}	
			
			logger_tmp_log_address += LOG_SIZE;			
		}
		
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
}

/******************************   END OF FILE  **********************************/
