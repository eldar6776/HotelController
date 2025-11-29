/**
 ******************************************************************************
 * File Name          : ic2_eeprom.h
 * Date               : 21/08/2016 20:59:16
 * Description        : 24c1024 i2c eeprom control modul header
 ******************************************************************************
 */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __I2CEE_H__
#define __I2CEE_H__					        HC_BETA191014	// version

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define I2C2_CLOCK_FRQ                      100000          // 400 kHz i2c clock
#define I2C2_TIMEOUT                        0x3000U	        // Timeout Zeit
#define I2CEE_TRIALS                        ((uint8_t)0xFFU)// 255 function trials 
#define I2CEE_PAGE_SIZE		                0x10000U        // one i2c eeprom page 65535 / 8 = 8 kByte
#define I2CEE_WRITE_DELAY		            15U             // wait 15ms after write to eeprom
#define	I2CEE_BSIZE		                    0x200U          // i2c buffer size
#define I2CEE_BLOCK                         0x100U          // max. single write block size
#define I2CEE_PAGE_0 	                    ((uint8_t)0xA0U)// eeprom i2c address page 0 selected
#define I2CEE_PAGE_1 	                    ((uint8_t)0xA2U)// eeprom i2c address page 1 selected
/** ==========================================================================*/
/**     	I 2 C    E E P R O M    M E M O R Y    A D D R E S S E      	  */
/** ==========================================================================*/
#define EE_ETH_IP_ADD                       ((uint16_t)0x0000U)
#define EE_ETH_SUB_ADD                      ((uint16_t)0x0004U)
#define EE_ETH_GW_ADD                       ((uint16_t)0x0008U)
#define EE_RS485_IFADD			            ((uint16_t)0x000CU)
#define EE_RS485_GRADD				        ((uint16_t)0x000EU)
#define EE_RS485_BRADD			            ((uint16_t)0x0010U)
#define EE_RS485_BAUD_ADD                   ((uint16_t)0x0012U)
#define EE_SYS_CFG_ADD                      ((uint16_t)0x0013U)
#define EE_SYS_ID_ADD				        ((uint16_t)0x0017U)
#define EE_PASSWORD_ADD					    ((uint16_t)0x0019U)

#define EE_UPD_RP_ADDR                      ((uint16_t)0x0020U) // update service repeat
#define EE_UPD_YR_ADDR                      ((uint16_t)0x0021U) // update service start year
#define EE_UPD_MO_ADDR                      ((uint16_t)0x0022U) // update service start month
#define EE_UPD_DY_ADDR                      ((uint16_t)0x0023U) // update service start day
#define EE_UPD_HR_ADDR                      ((uint16_t)0x0024U) // update service start hour
#define EE_UPD_MN_ADDR                      ((uint16_t)0x0025U) // update service start minute

#define EE_FORECAST_ADD                     ((uint16_t)0x0080U)
#define EE_LOG_LIST_START_ADD  		        ((uint16_t)0x0100U) // beginning of log list
#define EE_LOG_LIST_END_ADD   		        ((uint32_t)0x20000U)// end of log list
/* Exported types    ---------------------------------------------------------*/
/* Exported variables  -------------------------------------------------------*/
extern uint8_t eebuff[I2CEE_BSIZE];
/* Exported macros     -------------------------------------------------------*/
/* Exported functions  -------------------------------------------------------*/
void I2CEE_Init(void);
void I2C2_Init(void);
uint8_t I2C_EEPROM_timeout  (uint8_t retval);
uint8_t I2CEE_WriteCMD      (uint8_t slave_address, uint8_t cmd);
uint8_t I2CEE_ReadByte      (uint8_t slave_address, uint8_t  data_address, uint8_t *data);
uint8_t I2CEE_WriteByte     (uint8_t slave_address, uint8_t  data_address, uint8_t *data);
uint8_t I2CEE_ReadByte16    (uint8_t slave_address, uint16_t data_address, uint8_t *data);
uint8_t I2CEE_WriteByte16   (uint8_t slave_address, uint16_t data_address, uint8_t *data);
uint8_t I2CEE_ReadBytes     (uint8_t slave_address, uint8_t  data_address, uint8_t *data, uint16_t cnt);
uint8_t I2CEE_WriteBytes    (uint8_t slave_address, uint8_t  data_address, uint8_t *data, uint16_t cnt);
uint8_t I2CEE_ReadBytes16   (uint8_t slave_address, uint16_t data_address, uint8_t *data, uint16_t cnt);
uint8_t I2CEE_WriteBytes16  (uint8_t slave_address, uint16_t data_address, uint8_t *data, uint16_t cnt);

#endif
/***************** (C) COPYRIGHT JUBERA D.O.O. SARAJEVO ***********************/
