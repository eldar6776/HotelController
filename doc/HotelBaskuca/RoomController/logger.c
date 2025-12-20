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
#include "eeprom.h"
#include "common.h"
#include "dio_interface.h"
#include "logger.h"
#include "rc522.h"
#include "one_wire.h"
#include "rs485.h"
#include "display.h"
#include "signal.h"


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
/* Program code   ------------------------------------------------------------*/
void LOGGER_Init(void)
{
    uint8_t log_buff[LOG_SIZE];
    
	LogEvent.log_event = 0U;
	LogEvent.log_type = 0U;
	LogEvent.log_group = 0U;
	LogEvent.log_card_id[0] = 0U;
	LogEvent.log_card_id[1] = 0U;
	LogEvent.log_card_id[2] = 0U;
	LogEvent.log_card_id[3] = 0U;
	LogEvent.log_card_id[4] = 0U;
	LogEvent.log_time_stamp[0] = 0U;
	LogEvent.log_time_stamp[1] = 0U;
	LogEvent.log_time_stamp[2] = 0U;
	LogEvent.log_time_stamp[3] = 0U;
	LogEvent.log_time_stamp[4] = 0U;
	LogEvent.log_time_stamp[5] = 0U;
	
	logger_pcnt = 0U;
	logger_list_count = 0U;
	logger_next_log_id = 1U;
	logger_next_log_address = EE_LOG_LIST_START_ADDRESS;
	logger_tmp_log_address = 0U;
	LOGGER_Status = LOGGER_OK;

	log_buff[0] = logger_next_log_address >> 8U;
	log_buff[1] = logger_next_log_address;
	if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, log_buff, 2U, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
	if (HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, log_buff, LOG_SIZE, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
	
	while(logger_next_log_address <= (EE_LOG_LIST_END_ADDRESS - LOG_SIZE))
	{	
		if((log_buff[0] == 0U) && (log_buff[1] == 0U)) break;
		logger_next_log_address += LOG_SIZE;
		++logger_next_log_id;
		++logger_list_count;
		log_buff[0] = logger_next_log_address >> 8U;
		log_buff[1] = logger_next_log_address;
		if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, log_buff, 2U, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
		if (HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, log_buff, LOG_SIZE, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
	}
	
	/**
	*	set log list not empty 	-> system status flag
	*	set log list full  		-> system status flag
	*/
	if(logger_list_count != 0U) sys_status |= (1U << 0U);
	if(logger_next_log_address >= (EE_LOG_LIST_END_ADDRESS - LOG_SIZE)) sys_status |= (1U << 1U);
}


LOGGER_StatusTypeDef LOGGER_Write(void)
{
	RTC_TimeTypeDef time_log;
    RTC_DateTypeDef date_log;
    uint8_t log_buff[24];
    
    HAL_RTC_GetTime(&hrtc, &time_log, RTC_FORMAT_BCD);
	HAL_RTC_GetDate(&hrtc, &date_log, RTC_FORMAT_BCD);
    
	log_buff[2] = (logger_next_log_id >> 8U);
	log_buff[3] = (logger_next_log_id & 0xFFU);
	log_buff[4] = LogEvent.log_event;
	log_buff[5] = LogEvent.log_type;
	log_buff[6] = LogEvent.log_group;
	log_buff[7] = LogEvent.log_card_id[0];
	log_buff[8] = LogEvent.log_card_id[1];
	log_buff[9] = LogEvent.log_card_id[2];
	log_buff[10] = LogEvent.log_card_id[3];
	log_buff[11] = LogEvent.log_card_id[4];
	log_buff[12] = date_log.Date;
	log_buff[13] = date_log.Month;
	log_buff[14] = date_log.Year;
	log_buff[15] = time_log.Hours;
	log_buff[16] = time_log.Minutes;
	log_buff[17] = time_log.Seconds;
	
    LogEvent.log_event = 0U;
	LogEvent.log_type = 0U;
	LogEvent.log_group = 0U;
	LogEvent.log_card_id[0] = 0U;
	LogEvent.log_card_id[1] = 0U;
	LogEvent.log_card_id[2] = 0U;
	LogEvent.log_card_id[3] = 0U;
	LogEvent.log_card_id[4] = 0U;
	LogEvent.log_time_stamp[0] = 0U;
	LogEvent.log_time_stamp[1] = 0U;
	LogEvent.log_time_stamp[2] = 0U;
	LogEvent.log_time_stamp[3] = 0U;
	LogEvent.log_time_stamp[4] = 0U;
	LogEvent.log_time_stamp[5] = 0U;
    
	if(logger_next_log_address > (EE_LOG_LIST_END_ADDRESS - LOG_SIZE))
	{
		sys_status |= (1U << 1U);
		return (LOGGER_FULL);
	}	
	
	log_buff[0] = logger_next_log_address >> 8U;
	log_buff[1] = logger_next_log_address;
	if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, log_buff, 18U, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
	if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
	++logger_list_count;
	++logger_next_log_id;
	logger_next_log_address += LOG_SIZE;
	sys_status |= (1U << 0U);	
    
	return (LOGGER_OK);		
}


LOGGER_StatusTypeDef LOGGER_Read(uint16_t log_id, uint8_t *buff)
{
    uint8_t log_buff[4];
    
	if(logger_list_count == 0U)
	{
		return(LOGGER_EMPTY);
	}
	else if(log_id == 0U)
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
	
	log_buff[0] = logger_tmp_log_address >> 8U;
	log_buff[1] = logger_tmp_log_address;
	if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, log_buff, 2U, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
	if (HAL_I2C_Master_Receive(&hi2c1, I2C_EE_READ, buff, LOG_SIZE, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);	
	return(LOGGER_OK);
}


LOGGER_StatusTypeDef LOGGER_Delete(uint16_t log_id)
{
    uint8_t log_buff[24];
    
	if(logger_list_count == 0U)
	{
		return(LOGGER_EMPTY);
	}
	else if(log_id == 0U)
	{
		ClearBuffer(log_buff, 24U);
		logger_tmp_log_address = logger_next_log_address - LOG_SIZE;
		
		if(HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, 1000U, I2C_EE_TIMEOUT) == HAL_OK)
		{
			log_buff[0] = logger_tmp_log_address >> 8U;
			log_buff[1] = logger_tmp_log_address;
			if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, log_buff, 18U, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
			if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);			
			logger_next_log_address -= LOG_SIZE;
			--logger_list_count;
			sys_status &= 0xFDU;
			if(logger_list_count == 0U) sys_status &= 0xFEU;
			return (LOGGER_OK);
		}
		else
		{
			return (LOGGER_ERROR);
		}
	}
	else if(log_id == 0xFFFFU)
	{
		ClearBuffer(log_buff, 24U);
		logger_tmp_log_address = EE_LOG_LIST_START_ADDRESS;
		
		while(logger_tmp_log_address <= (EE_LOG_LIST_END_ADDRESS - LOG_SIZE))
		{
			if(HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) == HAL_OK)
			{
				log_buff[0] = logger_tmp_log_address >> 8U;
				log_buff[1] = logger_tmp_log_address;
				if (HAL_I2C_Master_Transmit(&hi2c1, I2C_EE_WRITE, log_buff, 18U, I2C_EE_TIMEOUT) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
				if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_EE_READ, I2C_EE_TRIALS, I2C_EE_WRITE_DELAY) != HAL_OK) Error_Handler(LOGGER_FUNC, I2C_DRIVER);
			}
			else
			{
				return (LOGGER_ERROR);
			}	
			
			logger_tmp_log_address += LOG_SIZE;			
		}
		
		logger_list_count = 0U;
		logger_next_log_id = 1U;
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
