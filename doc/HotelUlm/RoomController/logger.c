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
#include "dio.h"
#include "room.h"
#include "rs485.h"
#include "logger.h"
#include "display.h"
#include "onewire.h"
#include "stm32746g.h"
#include "stm32746g_ts.h"
#include "stm32746g_qspi.h"
#include "stm32746g_sdram.h"
#include "stm32746g_eeprom.h"


#if (__LOGGER_H__ != FW_BUILD)
    #error "logger header version mismatch"
#endif


/* Imported Types  -----------------------------------------------------------*/
LOGGER_EventTypeDef LogEvent;
/* Imported Variables --------------------------------------------------------*/
/* Imported Functions    -----------------------------------------------------*/
/* Private Variables  --------------------------------------------------------*/
uint16_t logger_next_log_address;
uint16_t logger_next_log_id;
uint16_t logger_list_count;
/* Private Macros    ---------------------------------------------------------*/
/* Private Prototypes    -----------------------------------------------------*/
/* Program code   ------------------------------------------------------------*/
/**
  * @brief
  * @param
  * @retval
  */
void LOGGER_Init(void)
{
    uint8_t log_buff[LOG_DSIZE];
    logger_list_count = 0U;
    logger_next_log_id = 1U;
    memset(&LogEvent, 0U, sizeof(LogEvent));
    logger_next_log_address = EE_LOG_LIST_START_ADDR;
    if (EE_ReadBuffer(log_buff, logger_next_log_address, LOG_DSIZE) != HAL_OK)      ErrorHandler(LOGGER_FUNC, I2C_DRV);
    while(logger_next_log_address <= (EE_LOG_LIST_END_ADDR - LOG_DSIZE))
    {	
#ifdef	USE_WATCHDOG
        HAL_IWDG_Refresh(&hiwdg);
#endif
        if((log_buff[0] == 0U) && (log_buff[1] == 0U)) break;
        ++logger_next_log_id;
        ++logger_list_count;
        logger_next_log_address += LOG_DSIZE;                                                
        if (EE_ReadBuffer(log_buff, logger_next_log_address, LOG_DSIZE) != HAL_OK)  ErrorHandler(LOGGER_FUNC, I2C_DRV);
    }
    /**
    *	set log list not empty 	-> system status flag
    *	set log list full  		-> system status flag
    */
    if(logger_list_count != 0U) SYS_NewLogSet();
    if(logger_next_log_address > (EE_LOG_LIST_END_ADDR - LOG_DSIZE)) SYS_LogListFullSet();
}
/**
  * @brief
  * @param
  * @retval
  */
LOGGER_StatusTypeDef LOGGER_Write(void)
{
	RTC_TimeTypeDef time_log;
    RTC_DateTypeDef date_log;
    uint8_t log_buff[LOG_DSIZE];
    
	if(logger_next_log_address > (EE_LOG_LIST_END_ADDR - LOG_DSIZE))
	{
		SYS_LogListFullSet();
		return (LOGGER_FULL);
	}
    
    HAL_RTC_GetTime(&hrtc, &time_log, RTC_FORMAT_BCD);
    HAL_RTC_GetDate(&hrtc, &date_log, RTC_FORMAT_BCD);
    log_buff[0] = (logger_next_log_id >> 8);
    log_buff[1] = (logger_next_log_id & 0xFFU);
    log_buff[2] = LogEvent.log_event;
    log_buff[3] = LogEvent.log_type;
    log_buff[4] = LogEvent.log_group;
    log_buff[5] = LogEvent.log_card_id[0];
    log_buff[6] = LogEvent.log_card_id[1];
    log_buff[7] = LogEvent.log_card_id[2];
    log_buff[8] = LogEvent.log_card_id[3];
    log_buff[9] = LogEvent.log_card_id[4];
    log_buff[10] = date_log.Date;
    log_buff[11] = date_log.Month;
    log_buff[12] = date_log.Year;
    log_buff[13] = time_log.Hours;
    log_buff[14] = time_log.Minutes;
    log_buff[15] = time_log.Seconds;
    
    memset(&LogEvent, 0U, sizeof(LogEvent));
    if (EE_WriteBuffer (log_buff, logger_next_log_address, LOG_DSIZE) != HAL_OK)    ErrorHandler(LOGGER_FUNC, I2C_DRV);
    if (EE_IsDeviceReady(EE_ADDR, DRV_TRIAL) != HAL_OK)                             ErrorHandler(LOGGER_FUNC, I2C_DRV);
    logger_next_log_address += LOG_DSIZE;
    ++logger_list_count;
    ++logger_next_log_id;
    if(logger_next_log_address > (EE_LOG_LIST_END_ADDR - LOG_DSIZE)) SYS_LogListFullSet();
    SYS_NewLogSet();
    return (LOGGER_OK);
}
/**
  * @brief
  * @param
  * @retval
  */
LOGGER_StatusTypeDef LOGGER_Read(uint8_t *buff)
{
	if (logger_list_count == 0U) return(LOGGER_EMPTY);
    if (EE_ReadBuffer (buff, logger_next_log_address - LOG_DSIZE,  LOG_DSIZE) != HAL_OK)    ErrorHandler(LOGGER_FUNC, I2C_DRV);
    return(LOGGER_OK);
}
/**
  * @brief
  * @param
  * @retval
  */
LOGGER_StatusTypeDef LOGGER_Delete(void)
{
    uint8_t log_buff[LOG_DSIZE];
    
	if (logger_list_count == 0U) return(LOGGER_EMPTY);
    ZEROFILL(log_buff, LOG_DSIZE);
    if (EE_WriteBuffer (log_buff, logger_next_log_address - LOG_DSIZE, LOG_DSIZE) != HAL_OK)    ErrorHandler(LOGGER_FUNC, I2C_DRV);
    if (EE_IsDeviceReady(EE_ADDR, DRV_TRIAL) != HAL_OK)                                         ErrorHandler(LOGGER_FUNC, I2C_DRV);
    logger_next_log_address -= LOG_DSIZE;
    --logger_list_count;
    if (logger_list_count == 0U) SYS_NewLogReset();
    SYS_LogListFullReset();
    return (LOGGER_OK);
}
/************************ (C) COPYRIGHT JUBERA D.O.O Sarajevo ************************/
