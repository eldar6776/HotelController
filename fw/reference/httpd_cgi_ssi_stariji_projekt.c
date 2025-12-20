/**
 ******************************************************************************
 * @file    httpd_cg_ssi.c
 * @author  MCD Application Team
 * @version V1.0.0
 * @date    31-October-2011
 * @brief   Webserver SSI and CGI handlers
 ******************************************************************************
 * @attention
 *
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * <h2><center>&copy; Portions COPYRIGHT 2011 STMicroelectronics</center></h2>
 ******************************************************************************
 */
/**
 ******************************************************************************
 * <h2><center>&copy; Portions COPYRIGHT 2012 Embest Tech. Co., Ltd.</center></h2>
 * @file    httpd_cg_ssi.c
 * @author  CMP Team
 * @version V1.0.0
 * @date    28-December-2012
 * @brief   Webserver SSI and CGI handlers
 *          Modified to support the STM32F4DISCOVERY, STM32F4DIS-BB and
 *          STM32F4DIS-LCD modules. 
 ******************************************************************************
 * @attention
 *
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, Embest SHALL NOT BE HELD LIABLE FOR ANY DIRECT, INDIRECT
 * OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE CONTENT
 * OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING INFORMATION
 * CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 ******************************************************************************
 */
/* Includes ------------------------------------------------------------------*/


#include "lwip/debug.h"
#include "httpd.h"
#include "lwip/tcp.h"
#include "fs.h"
#include "ff.h"
#include "main.h"
#include "rtc.h"
#include "gpio.h"
#include "hotel_ctrl.h"
#include "buzzer.h"
#include "w25q16.h"
#include "stm32f429i_lcd.h"
#include "uart.h"
#include "i2c_eeprom.h"
#include "common.h"
#include "display.h"


extern struct netif netif;
extern uint8_t ip_address[4];
extern uint8_t subnet_mask[4];
extern uint8_t gateway_address[4];

tSSIHandler ADC_Page_SSI_Handler;
uint32_t ADC_not_configured = 1;
uint32_t LED_not_configured = 1;
uint8_t *p_buffer;

#define LOG_DELETED_STRING_LENGHT 	22
const char log_deleted_answer[] = "log list block deleted";


/* we will use character "t" as tag for CGI */
char const* TAGCHAR = "t";
char const** TAGS = &TAGCHAR;

/* CGI handler for incoming http request */
const char * HTTP_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);

/* Html request for "/sysctrl.cgi" will start HTTP_CGI_Handler */
const tCGI HTTP_CGI = {"/sysctrl.cgi", HTTP_CGI_Handler};

/* Cgi call table, only one CGI used */
tCGI CGI_TAB[12];


u16_t HTTP_ResponseHandler(int iIndex, char *pcInsert, int iInsertLen)
{
	uint16_t v;
    /* We have only one SSI handler iIndex = 0 */
    if (iIndex == 0)
    {
		if (HTTP_LogListTransfer.log_transfer_state == HTTP_LOG_LIST_READY)
		{
			v = 0;
			while(v < 512) 
			{
				*(pcInsert + v) = (char) rubicon_ctrl_buffer[v];
				v++;
			}
			rubicon_ctrl_request = NULL;
			HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_TRANSFER_IDLE;
		}
		else if (HTTP_LogListTransfer.log_transfer_state == HTTP_LOG_LIST_DELETED)
		{
			v = 0;
			while(v < LOG_DELETED_STRING_LENGHT) 
			{
				*(pcInsert + v) = (char) log_deleted_answer[v];
				v++;
			}
			rubicon_ctrl_request = NULL;
			HTTP_LogListTransfer.log_transfer_state = HTTP_LOG_TRANSFER_IDLE;
		}
		else if (rubicon_http_cmd_state == HTTP_RUBICON_ROOM_STATUS_READY)
		{
			v = 0;
			
			while(v < 50)
			{
				*(pcInsert + v) = (char) rx_buffer[v + 7];
				v++;
			}
			
			rubicon_ctrl_request = NULL;
			rubicon_http_cmd_state = NULL;
		}
		else if (rubicon_http_cmd_state == HTTP_RUBICON_SYS_INFO_READY)
		{
			v = 0;
			*(pcInsert + v++) = (char) rx_buffer[51];	
			*(pcInsert + v++) = (char) rx_buffer[52];
			*(pcInsert + v++) = (char) rx_buffer[53];
			*(pcInsert + v++) = (char) rx_buffer[54];	
			*(pcInsert + v++) = (char) rx_buffer[55];
			*(pcInsert + v++) = (char) rx_buffer[56];
			*(pcInsert + v++) = (char) rx_buffer[57];
			*(pcInsert + v++) = (char) rx_buffer[58];
			*(pcInsert + v++) = (char) rx_buffer[59];				
			rubicon_ctrl_request = NULL;
			rubicon_http_cmd_state = NULL;
		}
        else if ((rubicon_http_cmd_state == HTTP_STATUS_RESPONSE_TIMEOUT)   ||
                 (rubicon_http_cmd_state == HTTP_STATUS_RESPONSE_ERROR)     ||
                 (rubicon_http_cmd_state == HTTP_STATUS_RESPONSE_BUSY)      ||
                 (rubicon_http_cmd_state == HTTP_STATUS_RESPONSE_OK))
        {
                if (rubicon_http_cmd_state == HTTP_STATUS_RESPONSE_TIMEOUT) 
                {
                    memcpy(pcInsert, "TIMEOUT\0", 8U);
                }
                else if (rubicon_http_cmd_state == HTTP_STATUS_RESPONSE_ERROR) 
                {
                    memcpy(pcInsert, "ERROR\0", 6U);
                }
                else if (rubicon_http_cmd_state == HTTP_STATUS_RESPONSE_BUSY) 
                {
                    memcpy(pcInsert, "BUSY\0", 5U);
                }
                else if (rubicon_http_cmd_state == HTTP_STATUS_RESPONSE_OK) 
                {
                    memcpy(pcInsert, "OK\0", 3U);
                }
                rubicon_http_cmd_state = NULL;
        }
        return 512;
    }
    return 0;
}

/**
 * @brief  uljucuje LED
 * @param  1 - LED1.
 * @param  2 - LED2
 * @retval None
 */
void led_set(uint8_t led)
{
    if (led == 1)
    {
        LED_GPIO_PORT->BSRRH = LED1_GPIO_PIN;
    }
    else if (led == 2)
    {
        LED_GPIO_PORT->BSRRH = LED2_GPIO_PIN;
    }
}

/**
 * @brief  iskljucuje LED
 * @param  1 - LED1.
 * @param  2 - LED2
 * @retval None
 */
void led_clr(uint8_t led)
{
    if (led == 1)
    {
        LED_GPIO_PORT->BSRRL = LED1_GPIO_PIN;
    }
    else if (led == 2)
    {
        LED_GPIO_PORT->BSRRL = LED2_GPIO_PIN;
    }
}

/**
 * @brief  CGI handler for HTTP request 
 */
const char * HTTP_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
	uint16_t first_address, last_address, address_cnt;
	uint8_t first_file, last_file;
    uint32_t i = 0;
    uint32_t t = 0;
    uint32_t x = 0;
	struct ip_addr new_ipaddr;
	struct ip_addr new_netmask;
	struct ip_addr new_gateway;
	
    if (LED_not_configured == 1)
    {
        LED_Init();
        LED_not_configured = 0;
    }

    /* We have only one SSI handler iIndex = 0 */
    if (iIndex == 0)
    {
        /* All leds off */
        led_clr(1);
        led_clr(2);

        /* Check cgi parameter for LED control: 						SET /sysctrl.cgi?led 	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "led") == 0)
            {
                if (strcmp(pcValue[i], "1") == 0)
                {
                    led_set(1);
                }
                else if (strcmp(pcValue[i], "2") == 0)
                {
                    led_set(2);
                }
            }
        }
        /* Check cgi parameter for date & time: 						SET /sysctrl.cgi?tdu  	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "tdu") == 0)
            {
                if ((RUBICON_FirmwareUpdatePacket.update_state != FW_UPDATE_IDLE) ||
				    (RUBICON_FileUpdatePacket.update_state != FILE_UPDATE_IDLE))
                {
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_BUSY;
                    return "/log.html";
                }
                
                p_buffer = rubicon_ctrl_buffer;
                while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                x = 0;

                while ((x < sizeof (rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
                {
                    rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
                    x++;
                }

                if (x == 15)
                {
                    RTC_HttpCgiDateTimeUpdate(rubicon_ctrl_buffer);
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_OK;
                    return "/log.html";
                }
                else
                {
                    p_buffer = rubicon_ctrl_buffer;
                    while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_ERROR;
                    return "/log.html";
                }
            }
        }
        /* Check cgi parameter for log list request: 					GET /sysctrl.cgi?log;3-4-5	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "log") == 0)
            {
                if ((RUBICON_FirmwareUpdatePacket.update_state != FW_UPDATE_IDLE) ||
				    (RUBICON_FileUpdatePacket.update_state != FILE_UPDATE_IDLE))
                {
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_BUSY;
                    return "/log.html";
                }
                else if (strcmp(pcValue[i], "3") == 0)
                {
					HTTP_LogListTransfer.log_transfer_state = HTTP_GET_LOG_LIST;
                    rubicon_ctrl_request = RUBICON_GET_LOG_LIST;
					RUBICON_ReadBlockFromLogList();
                    return "/log.html";
                }
				else if (strcmp(pcValue[i], "4") == 0)
                {
					HTTP_LogListTransfer.log_transfer_state = HTTP_DELETE_LOG_LIST;
                    rubicon_ctrl_request = RUBICON_DELETE_LOG_LIST;
					RUBICON_DeleteBlockFromLogList();
                    return "/log.html";
                }
				else if (strcmp(pcValue[i], "5") == 0)
                {
					HTTP_LogListTransfer.log_transfer_state = HTTP_FORMAT_LOG_LIST;
                    rubicon_ctrl_request = RUBICON_DELETE_LOG_LIST;
					RUBICON_FormatLogList();
                    return "/log.html";
                }
                
                rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_ERROR;
                return "/log.html";
            }
        }
        /* Check cgi parameter for RUBICON room status request:			GET /sysctrl.cgi?cst  	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "cst") == 0)
            {	
                if ((RUBICON_FirmwareUpdatePacket.update_state != FW_UPDATE_IDLE) ||
				    (RUBICON_FileUpdatePacket.update_state != FILE_UPDATE_IDLE))
                {
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_BUSY;
                    return "/log.html";
                }
                p_buffer = rubicon_ctrl_buffer;
                while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
							
                x = 0;

                /* copy all string till next '&' parameter marker */
                while ((x < sizeof (rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
                {
                    rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
                    x++;
                }
                
				rubicon_http_cmd_state = HTTP_GET_RUBICON_ROOM_STATUS;
				rubicon_ctrl_request = RUBICON_GET_ROOM_STATUS;
				RUBICON_StartResponseTimer(RUBICON_HTTP_RESPONSE_TIMEOUT);
				
				while(!IsRUBICON_ResponseTimerExpired()) 
				{
					RUBICON_ProcessService();
					
					if(rubicon_http_cmd_state != HTTP_GET_RUBICON_ROOM_STATUS)
					{
						RUBICON_StopResponseTimer();
						return "/log.html";
					}
				}
				
				rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_TIMEOUT;
				rubicon_ctrl_request = NULL;
				return "/log.html";
            }
        }
        /* Check cgi parameter for RUBICON set room status:				SET /sysctrl.cgi?stg;val	*/
        for (i = 0; i < iNumParams; i++) 
        {
			if (strcmp(pcParam[i] , "stg") == 0) 
			{       
				p_buffer = rubicon_ctrl_buffer;
				while(p_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_buffer++ = NULL;
				x = 0;
				
				while((x < sizeof(rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
				{
					rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
					x++;
				}
				rubicon_ctrl_buffer[x] = NULL;
				x++;
			}
			else if (strcmp(pcParam[i] , "val") == 0) 
			{       
				t = 0;
				/* copy all string till next '&' parameter marker */
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
                if ((RUBICON_FirmwareUpdatePacket.update_state != FW_UPDATE_IDLE) ||
				    (RUBICON_FileUpdatePacket.update_state != FILE_UPDATE_IDLE))
                {
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_BUSY;
                    return "/log.html";
                }
                else
                {
                    rubicon_ctrl_request = RUBICON_SET_ROOM_STATUS;
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_OK;
                    return "/log.html";
                }
				
			}
		}
        /* Check cgi parameter for RUBICON set digital output:			SET /sysctrl.cgi?cdo 	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "cdo") == 0)
            {
                p_buffer = rubicon_ctrl_buffer;
                while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                x = 0;
				p_buffer = rubicon_ctrl_buffer;
                /* copy all string till next '&' parameter marker */
                while ((p_buffer < (rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer))) && (pcValue[i][x] != NULL))
                {
                    *p_buffer++ = (uint8_t) pcValue[i][x++];
                }
				
				*p_buffer++ = NULL;
            }
            else if (strcmp(pcParam[i], "do0") == 0)
            {
               *p_buffer++ = (uint8_t) pcValue[i][0];
            }
            else if (strcmp(pcParam[i], "do1") == 0)
            {
				*p_buffer++ = (uint8_t) pcValue[i][0];
            }
            else if (strcmp(pcParam[i], "do2") == 0)
            {
                *p_buffer++ = (uint8_t) pcValue[i][0];
            }
            else if (strcmp(pcParam[i], "do3") == 0)
            {
                *p_buffer++ = (uint8_t) pcValue[i][0];
            }
            else if (strcmp(pcParam[i], "do4") == 0)
            {
                *p_buffer++ = (uint8_t) pcValue[i][0];
            }
            else if (strcmp(pcParam[i], "do5") == 0)
            {
               *p_buffer++ = (uint8_t) pcValue[i][0];
            }
            else if (strcmp(pcParam[i], "do6") == 0)
            {
                *p_buffer++ = (uint8_t) pcValue[i][0];
            }
			else if (strcmp(pcParam[i], "do7") == 0)
            {
                *p_buffer++ = (uint8_t) pcValue[i][0];
            }
            else if (strcmp(pcParam[i], "ctrl") == 0)
            {
                *p_buffer = (uint8_t) pcValue[i][0];
                
                if ((RUBICON_FirmwareUpdatePacket.update_state != FW_UPDATE_IDLE) ||
				    (RUBICON_FileUpdatePacket.update_state != FILE_UPDATE_IDLE))
                {
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_BUSY;
                    return "/log.html";
                }
                else
                {
                    rubicon_ctrl_request = RUBICON_SET_DOUT_STATE;
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_OK;
                    return "/log.html";
                }
            }
        }
        /* Check cgi parameter for CONTROLLER update firmware: 			SET /sysctrl.cgi?cuf 	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "cuf") == 0)
            {
				if (strcmp(pcValue[i], "1") == 0)
                {
                    BootloaderActivate();
					I2C_EERPOM_WriteByte16(I2C_EE_WRITE_PAGE_0, EE_SYSTEM_CONFIG_ADDRESS, (uint8_t) (system_config >> 24));
					delay(I2C_EE_WRITE_DELAY);
					NVIC_SystemReset();
                }
            }
        }
        /* Check cgi parameter for RUBICON update old firmware: 		GET /sysctrl.cgi?cud 	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "cud") == 0)
            {
                p_buffer = rubicon_ctrl_buffer;
                while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                x = 0;

                /* copy all string till next '&' parameter marker */
                while ((x < sizeof (rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
                {
                    rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
                    x++;
                }

				RUBICON_FirmwareUpdatePacket.update_command = NULL;
				RUBICON_FileUpdatePacket.update_command = NULL;
                rubicon_ctrl_request = RUBICON_UPDATE_FIRMWARE;
            }
        }
        /* Check cgi parameter for RUBICON update firmware: 			SET /sysctrl.cgi?fuf;ful 	*/
        for (i = 0; i < iNumParams; i++)
        {
			if (strcmp(pcParam[i] , "fuf") == 0) 
			{       
				p_buffer = rubicon_ctrl_buffer;
				while(p_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_buffer++ = NULL;
				x = 0;
				
				while((x < sizeof(rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
				{
					rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
					x++;
				}
				rubicon_ctrl_buffer[x] = NULL;
				x++;
			}
			else if (strcmp(pcParam[i] , "ful") == 0) 
			{       
				t = 0;
				/* copy all string till next '&' parameter marker */
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
				first_address = atoi((char *)rubicon_ctrl_buffer);
				x = 0;
				while(rubicon_ctrl_buffer[x] != NULL) x++;
				x++;
				last_address = atoi((char *)&rubicon_ctrl_buffer[x]);
				
				address_cnt = 0;
				while(address_cnt < rubicon_address_list_size) rubicon_firmware_update_address_list[address_cnt++] = NULL;
				address_cnt = 0;
				while(address_cnt < RUBICON_FILE_UPDATE_LIST_SIZE) rubicon_file_update_list[address_cnt++] = NULL;
				address_cnt = 0;
				rubicon_firmware_update_list_cnt = 0;
				
				while(last_address >= first_address)
				{
					if(rubicon_address_list[address_cnt] == first_address)
					{
						rubicon_firmware_update_address_list[rubicon_firmware_update_list_cnt] = first_address;
						first_address++;
						rubicon_firmware_update_list_cnt++;
						address_cnt = 0;
					}
					else if(rubicon_address_list[address_cnt] == 0x0000)
					{
						address_cnt = 0;
						first_address++;
					}
					else address_cnt++;
				}
			
				RUBICON_FirmwareUpdatePacket.update_command = NULL;
				RUBICON_FileUpdatePacket.update_command = NULL;
				
				if(rubicon_firmware_update_list_cnt != 0)
				{
					RUBICON_FirmwareUpdatePacket.update_command = RUBICON_UPDATE_FIRMWARE;
					rubicon_file_update_list[0] = (FILE_UPDATE_IMAGE_20 - 40);
					rubicon_file_update_list_cnt = 0;
					rubicon_firmware_update_list_cnt = 0;
					rubicon_ctrl_request = RUBICON_DOWNLOAD_DIPLAY_IMAGE;						
				}
			}
        }
        /* Check cgi parameter for RUBICON update bootloader: 			SET /sysctrl.cgi?buf;bul 	*/
        for (i = 0; i < iNumParams; i++)
        {
			if (strcmp(pcParam[i] , "buf") == 0) 
			{       
				p_buffer = rubicon_ctrl_buffer;
				while(p_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_buffer++ = NULL;
				x = 0;
				
				while((x < sizeof(rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
				{
					rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
					x++;
				}
				rubicon_ctrl_buffer[x] = NULL;
				x++;
			}
			else if (strcmp(pcParam[i] , "bul") == 0) 
			{       
				t = 0;
				/* copy all string till next '&' parameter marker */
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
								first_address = atoi((char *)rubicon_ctrl_buffer);
				x = 0;
				while(rubicon_ctrl_buffer[x] != NULL) x++;
				x++;
				last_address = atoi((char *)&rubicon_ctrl_buffer[x]);
				
				address_cnt = 0;
				while(address_cnt < rubicon_address_list_size) rubicon_firmware_update_address_list[address_cnt++] = NULL;
				address_cnt = 0;
				while(address_cnt < RUBICON_FILE_UPDATE_LIST_SIZE) rubicon_file_update_list[address_cnt++] = NULL;
				address_cnt = 0;
				rubicon_firmware_update_list_cnt = 0;
				
				while(last_address >= first_address)
				{
					if(rubicon_address_list[address_cnt] == first_address)
					{
						rubicon_firmware_update_address_list[rubicon_firmware_update_list_cnt] = first_address;
						first_address++;
						rubicon_firmware_update_list_cnt++;
						address_cnt = 0;
					}
					else if(rubicon_address_list[address_cnt] == 0x0000)
					{
						address_cnt = 0;
						first_address++;
					}
					else address_cnt++;
				}
			
				RUBICON_FirmwareUpdatePacket.update_command = NULL;
				RUBICON_FileUpdatePacket.update_command = NULL;
				
				if(rubicon_firmware_update_list_cnt != 0)
				{
					RUBICON_FirmwareUpdatePacket.update_command = RUBICON_UPDATE_BOOTLOADER;
					rubicon_file_update_list[0] = (FILE_UPDATE_IMAGE_21 - 40);
					rubicon_file_update_list_cnt = 0;
					rubicon_firmware_update_list_cnt = 0;
					rubicon_ctrl_request = RUBICON_DOWNLOAD_DIPLAY_IMAGE;						
				}
			}
        }
        /* Check cgi parameter for RUBICON update display image:        SET /sysctrl.cgi?iuf;iul;ifa;ila	*/
        for (i = 0; i < iNumParams; i++) 
        {
                if ((RUBICON_FirmwareUpdatePacket.update_state != FW_UPDATE_IDLE) ||
                    (RUBICON_FileUpdatePacket.update_state != FILE_UPDATE_IDLE))
                {
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_BUSY;
                    return "/log.html";
                }
                
          if (strncmp (pcParam[i] , "iuf", 3) == 0) 
          {  
            p_buffer = rubicon_ctrl_buffer;
            while(p_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_buffer++ = NULL;
            x = 0;
            
            while((x < sizeof(rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
            {
              rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
              x++;
            }
            
            rubicon_ctrl_buffer[x] = NULL;
            x++;
          }
          else if (strncmp (pcParam[i] , "iul", 3) == 0) 
          {  
            t = 0;
            
            while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
            {
              rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
            }
            
            rubicon_ctrl_buffer[x] = NULL;
            x++;
          }
          else if (strncmp (pcParam[i] , "ifa", 3) == 0) 
          {  
            t = 0;
            
            while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
            {
              rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
            }
            
            rubicon_ctrl_buffer[x] = NULL;
            x++;
          }
          else if (strncmp (pcParam[i] , "ila", 3) == 0) 
          {  
            t = 0;
            
            while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
            {
              rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
            }
            
            address_cnt = 0;
            while(address_cnt < rubicon_image_update_address_list_size) rubicon_image_update_address_list[address_cnt++] = NULL;
            address_cnt = 0;
            while(address_cnt < RUBICON_FILE_UPDATE_LIST_SIZE) rubicon_file_update_list[address_cnt++] = NULL;
            address_cnt = 0;
            
            
            first_file = atoi((char *)rubicon_ctrl_buffer);
            x = 0;
            while(rubicon_ctrl_buffer[x] != NULL) x++;
            x++;
            last_file = atoi((char *)&rubicon_ctrl_buffer[x]);
            while(rubicon_ctrl_buffer[x] != NULL) x++;
            x++;
            first_address = atoi((char *)&rubicon_ctrl_buffer[x]);
            while(rubicon_ctrl_buffer[x] != NULL) x++;
            x++;
            last_address = atoi((char *)&rubicon_ctrl_buffer[x]);
            
            x = 0;
            while(first_file <= last_file)
            {
              rubicon_file_update_list[x++] = first_file++;
            }
            
            rubicon_image_update_list_cnt = 0;
            
            while(last_address >= first_address)
            {
              if(rubicon_address_list[address_cnt] == first_address)
              {
                rubicon_image_update_address_list[rubicon_image_update_list_cnt] = first_address;
                first_address++;
                rubicon_image_update_list_cnt++;
                address_cnt = 0;
              }
              else if(rubicon_address_list[address_cnt] == 0x0000)
              {
                address_cnt = 0;
                first_address++;
              }
              else address_cnt++;
            }
          
            RUBICON_FirmwareUpdatePacket.update_command = NULL;
            RUBICON_FileUpdatePacket.update_command = NULL;
            
            if(rubicon_image_update_list_cnt != 0)
            {
              RUBICON_FileUpdatePacket.update_command = RUBICON_DOWNLOAD_DIPLAY_IMAGE;
              rubicon_file_update_list_cnt = 0;
              rubicon_image_update_list_cnt = 0;
              rubicon_ctrl_request = RUBICON_DOWNLOAD_DIPLAY_IMAGE;
                        rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_OK;
                        return "/log.html";                    
            }
                    else
                    {
                        rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_ERROR;
                        return "/log.html";
                    }
          }			
        }
        /* Check cgi parameter for RUBICON set display brightness:		SET /sysctrl.cgi?cbr 	*/
        for (i = 0; i < iNumParams; i++) 
        {
          if (strcmp(pcParam[i] , "cbr") == 0) 
          {       
            p_buffer = rubicon_ctrl_buffer;
            while(p_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_buffer++ = NULL;
            x = 0;
            
            while((x < sizeof(rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
            {
              rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
              x++;
            }
            rubicon_ctrl_buffer[x] = NULL;
            x++;
          }
          else if (strcmp(pcParam[i] , "br") == 0) 
          {       
            t = 0;
            /* copy all string till next '&' parameter marker */
            while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
            {
              rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
            }
            
            rubicon_ctrl_request = RUBICON_SET_DISPLAY_BRIGHTNESS;
            rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_OK;
                    return "/log.html";
          }
        }
        /* Check cgi parameter for RUBICON SOS alarm reset request:		SET /sysctrl.cgi?rud	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "rud") == 0)
            {
				x = 0;
				p_buffer = rubicon_ctrl_buffer;	
				
                while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
				
				p_buffer = rubicon_ctrl_buffer;
				
                while ((p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
                {
                    *p_buffer++ = (uint8_t) pcValue[i][x++];
                }
				
                if ((x > 0) || (x < 5))
                {
					if(*(p_buffer - x) == '0') BUZZER_Off();
					else rubicon_ctrl_request = RUBICON_RESET_SOS_ALARM;
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_OK;
                    return "/log.html";
                }
                else
                {
                    p_buffer = rubicon_ctrl_buffer;
                    while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_ERROR;
                    return "/log.html";
                }
            }
        }
        /* Check cgi parameter for RUBICON reset controller:			SET /sysctrl.cgi?rst 	*/
        for (i = 0; i < iNumParams; i++) 
        {
          if (strcmp(pcParam[i], "rst") == 0)
                {	
            if (strcmp(pcValue[i], "0") == 0)
                    {
                        NVIC_SystemReset();
                    }
            
                    p_buffer = rubicon_ctrl_buffer;
                    while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                  
                    x = 0;

                    /* copy all string till next '&' parameter marker */
                    while ((x < sizeof (rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
                    {
                        rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
                        x++;
                    }

                    rubicon_ctrl_request = RUBICON_RESTART_CONTROLLER;                
                }
        }
        /* Check cgi parameter for RUBICON set room temperature:		SET /sysctrl.cgi?tca;tsp;tdf    */
        for (i = 0; i < iNumParams; i++) 
        {
          if (strcmp(pcParam[i] , "tca") == 0) 
          {       
            p_buffer = rubicon_ctrl_buffer;
            while(p_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_buffer++ = NULL;
            x = 0;
            
            while((x < sizeof(rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
            {
              rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
              x++;
            }
            
            rubicon_ctrl_buffer[x] = NULL;
            x++;
          }
          else if (strcmp(pcParam[i] , "tsp") == 0) 
          {       
            t = 0;
            
            while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
            {
              rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
            }
            
            rubicon_ctrl_buffer[x] = NULL;
            x++;
            
          }
          else if (strcmp(pcParam[i] , "tdf") == 0) 
          {       
            t = 0;
            
            while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
            {
              rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
            }
            
                    if ((RUBICON_FirmwareUpdatePacket.update_state != FW_UPDATE_IDLE) ||
                        (RUBICON_FileUpdatePacket.update_state != FILE_UPDATE_IDLE))
                    {
                        rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_BUSY;
                        return "/log.html";
                    }
                    else
                    {
                        rubicon_ctrl_request = RUBICON_SET_ROOM_TEMPERATURE;
                        rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_OK;
                        return "/log.html";
                    }			
          }
        }
        /* Check cgi parameter for THERMOSTAT update display image:     SET /sysctrl.cgi?tda;tdn;tdi;tdt;tbm;tbt    */
        for (i = 0; i < iNumParams; i++) 
        {
            if      (strcmp(pcParam[i] , "tda") == 0) 
			{       
				p_buffer = rubicon_ctrl_buffer;
				while(p_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_buffer++ = NULL;
				x = 0;
				
				while((x < sizeof(rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
				{
					rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
					x++;
				}
				
				rubicon_ctrl_buffer[x] = NULL;
				x++;
			}
			else if (strcmp(pcParam[i] , "tdn") == 0) 
			{       
				t = 0;
				
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
				rubicon_ctrl_buffer[x] = NULL;
				x++;
				
			}
            else if (strcmp(pcParam[i] , "tdi") == 0) 
			{       
				t = 0;
				
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
				rubicon_ctrl_buffer[x] = NULL;
				x++;
				
			}
            else if (strcmp(pcParam[i] , "tdt") == 0) 
			{       
				t = 0;
				
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
				rubicon_ctrl_buffer[x] = NULL;
				x++;
				
			}
            else if (strcmp(pcParam[i] , "tbm") == 0) 
			{       
				t = 0;
				
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
				rubicon_ctrl_buffer[x] = NULL;
				x++;
				
			}
			else if (strcmp(pcParam[i] , "tbt") == 0) 
			{       
				t = 0;
				
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
                if ((RUBICON_FirmwareUpdatePacket.update_state != FW_UPDATE_IDLE) ||
                    (RUBICON_FileUpdatePacket.update_state != FILE_UPDATE_IDLE))
                {
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_BUSY;
                    return "/log.html";
                }
                else
                {
                    rubicon_ctrl_request = RUBICON_SET_ROOM_THERMOSTAT_IMAGE;
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_OK;
                    return "/log.html";
                }	
								
			}			
		}
        /* Check cgi parameter for RUBICON update journal: 				SET /sysctrl.cgi?jrn1 	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "jrn1") == 0)
            {
                p_buffer = rubicon_ctrl_buffer;
                while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                x = 0;
				/** insert broadcast address as recipient for journal*/
				Int2Str((char *)rubicon_ctrl_buffer, rs485_broadcast_address, 0U);
				x = 0;				
				while((x < sizeof(rubicon_ctrl_buffer)) && (rubicon_ctrl_buffer[x]!= NULL)) ++x;
				++x;
				t = 0;
                /* copy all string till next '&' parameter marker */
                while ((x < sizeof (rubicon_ctrl_buffer)) && (pcValue[i][t] != NULL))
                {
                    rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
                }

                rubicon_ctrl_request = RUBICON_DOWNLOAD_JOURNAL_1;
            }
        }
        /* Check cgi parameter for RUBICON set bedding period: 			SET /sysctrl.cgi?sbr;per 	*/
        for (i = 0; i < iNumParams; i++)
        {           			
			if (strcmp(pcParam[i] , "sbr") == 0) 
			{       
				p_buffer = rubicon_ctrl_buffer;
				while(p_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_buffer++ = NULL;
				x = 0;
				
				while((x < sizeof(rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
				{
					rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
					x++;
				}
				rubicon_ctrl_buffer[x] = NULL;
				x++;
			}
			else if (strcmp(pcParam[i] , "per") == 0) 
			{       
				t = 0;
				/* copy all string till next '&' parameter marker */
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
				rubicon_ctrl_request = RUBICON_SET_BEDDING_REPLACEMENT_PERIOD;
			}
        }
        /* Check cgi parameter for RUBICON set rs485 address: 			SET /sysctrl.cgi?rsc;rsa;rga;rba;rib */
        for (i = 0; i < iNumParams; i++) 
        {
            if (strncmp (pcParam[i] , "rsc", 3) == 0) 
            {  
                p_buffer = rubicon_ctrl_buffer;
                while(p_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                x = 0;
                
                while((x < sizeof(rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
                {
                    rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
                    x++;
                }
                
                rubicon_ctrl_buffer[x] = NULL;
                x++;
            }
            else if (strncmp (pcParam[i] , "rsa", 3) == 0) 
            {  
                t = 0;
                
                while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
                {
                    rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
                }
                
                rubicon_ctrl_buffer[x] = NULL;
                x++;
            }
            else if (strncmp (pcParam[i] , "rga", 3) == 0) 
            {  
                t = 0;
                
                while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
                {
                    rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
                }
                
                rubicon_ctrl_buffer[x] = NULL;
                x++;
            }
            else if (strncmp (pcParam[i] , "rba", 3) == 0) 
            {  
                t = 0;
                
                while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
                {
                    rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
                }
                
                rubicon_ctrl_buffer[x] = NULL;
                x++;
            }
            else if (strncmp (pcParam[i] , "rib", 3) == 0) 
            {  
                t = 0;
                
                while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
                {
                    rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
                }
                
                rubicon_ctrl_buffer[x] = NULL;
                /**
                *
                *	check if new rs485 interface settings are for main controller
                */
                t = atoi((char *)rubicon_ctrl_buffer);
                
                if(rs485_interface_address == (t & 0xffff))
                {
                    x = 0;
                    while((x < sizeof(rubicon_ctrl_buffer)) && (rubicon_ctrl_buffer[x] != NULL)) x++;
                    x++;
                    t = atoi((char *)&rubicon_ctrl_buffer[x]);
                    rs485_interface_address = (t & 0xffff);					
                    while((x < sizeof(rubicon_ctrl_buffer)) && (rubicon_ctrl_buffer[x] != NULL)) x++;
                    x++;
                    t = atoi((char *)&rubicon_ctrl_buffer[x]);
                    rs485_group_address = (t & 0xffff);
                    while((x < sizeof(rubicon_ctrl_buffer)) && (rubicon_ctrl_buffer[x] != NULL)) x++;
                    x++;
                    t = atoi((char *)&rubicon_ctrl_buffer[x]);
                    rs485_broadcast_address = (t & 0xffff);
                    while((x < sizeof(rubicon_ctrl_buffer)) && (rubicon_ctrl_buffer[x] != NULL)) x++;
                    x++;
                    rs485_interface_baudrate = rubicon_ctrl_buffer[x];
                    if((rs485_interface_baudrate < RS485_BAUDRATE_2400) || (rs485_interface_baudrate > RS485_BAUDRATE_921600))
                    {
                        rs485_interface_baudrate = RS485_BAUDRATE_115200;
                    }
                    USART_DeInit(USARTx);					
                    
                    i2c_ee_buffer[0] = (rs485_interface_address >> 8);
                    i2c_ee_buffer[1] = (rs485_interface_address & 0xff);
                    I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, EE_RS485_INTERFACE_ADDRESS, 2);
                    delay(I2C_EE_WRITE_DELAY);
                    i2c_ee_buffer[0] = (rs485_group_address >> 8);
                    i2c_ee_buffer[1] = (rs485_group_address & 0xff);
                    I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, EE_RS485_GROUP_ADDRESS, 2);
                    delay(I2C_EE_WRITE_DELAY);
                    i2c_ee_buffer[0] = (rs485_broadcast_address >> 8);
                    i2c_ee_buffer[1] = (rs485_broadcast_address & 0xff);
                    I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, EE_RS485_BROADCAST_ADDRESS, 2);
                    delay(I2C_EE_WRITE_DELAY);
                    I2C_EERPOM_WriteByte16(I2C_EE_WRITE_PAGE_0, EE_RS485_INTERFACE_BAUDRATE_ADDRESS, rs485_interface_baudrate);
                    delay(I2C_EE_WRITE_DELAY);

                    uart_init(rs485_interface_baudrate);
                    
                }
                else
                {
                    rubicon_ctrl_request = RUBICON_SET_RS485_CONFIG;
                }				
            }
        }
        /* Check cgi parameter for CONTROLLER set system ID: 			SET /sysctrl.cgi?sid;nid 		*/
        for (i = 0; i < iNumParams; i++) 
        {
            if (strcmp(pcParam[i] , "sid") == 0) 
            {       
                p_buffer = rubicon_ctrl_buffer;
                while(p_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                x = 0;
                
                while((x < sizeof(rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
                {
                    rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
                    x++;
                }
                
                rubicon_ctrl_buffer[x] = NULL;
                x++;
            }
            else if (strcmp(pcParam[i] , "nid") == 0) 
            {       
                
                if ((RUBICON_FirmwareUpdatePacket.update_state != FW_UPDATE_IDLE) ||
                        (RUBICON_FileUpdatePacket.update_state != FILE_UPDATE_IDLE))
                                {
                                        rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_BUSY;
                                        return "/log.html";
                                }
                                
                                t = 0;
                while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
                {
                    rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
                }
                
                rubicon_ctrl_buffer[x] = NULL;
                /**
                *
                *	if addressed main controller rs485 interface send new system ID to all devices
                */
                t = atoi((char *)rubicon_ctrl_buffer);
                
                if(rs485_interface_address == (t & 0xffff))
                {
                    x = 0;
                    while((x < sizeof(rubicon_ctrl_buffer)) && (rubicon_ctrl_buffer[x] != NULL)) x++;
                    x++;
                    t = atoi((char *)&rubicon_ctrl_buffer[x]);
                    system_id = (t & 0xffff);
                    i2c_ee_buffer[0] = (system_id >> 8);
                    i2c_ee_buffer[1] = (system_id & 0xff);
                    I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, EE_SYSTEM_ID_ADDRESS, 2);
                    delay(I2C_EE_WRITE_DELAY);
                    
                    p_buffer = rubicon_ctrl_buffer;
                    while(p_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_buffer++ = NULL;	
                    
                    Int2Str((char *)rubicon_ctrl_buffer, rs485_broadcast_address, 0U);
                    x = 0;				
                    while((x < sizeof(rubicon_ctrl_buffer)) && (rubicon_ctrl_buffer[x]!= NULL)) x++;
                    x++;
                    Int2Str((char *)&rubicon_ctrl_buffer[x], system_id, 0U);
                }	
            
                rubicon_ctrl_request = RUBICON_SET_SYSTEM_ID;
                                rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_OK;
                                return "/log.html";
            }	
        }
        /* Check cgi parameter for CONTROLLER set ethernet addresse: 	SET /sysctrl.cgi?ipa;snm;gwa	*/
        for (i = 0; i < iNumParams; i++) 
        {
            if (strncmp (pcParam[i] , "ipa", 3) == 0) 
            {  
                t = 0;
                                while (t < 8) rubicon_ctrl_buffer[t++] = NULL;                
                x = 0;	
                                while ((x < 8) && (pcValue[i][x] != NULL) && (IS09(&pcValue[i][x])))
                                {
                                        rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
                    x++;
                                }				
                rubicon_ctrl_buffer[x] = NULL;
                t = atoi((char *)rubicon_ctrl_buffer);
                ip_address[0] = (t & 0xff);				
                t = 0;
                                while (t < 8) rubicon_ctrl_buffer[t++] = NULL;				
                t = 0;
                x++;
                while ((x < 15) && (pcValue[i][x] != NULL) && (IS09(&pcValue[i][x])))
                                {
                                        rubicon_ctrl_buffer[t] = (uint8_t) pcValue[i][x];
                    x++;
                    t++;
                                }				
                rubicon_ctrl_buffer[x] = NULL;
                t = atoi((char *)rubicon_ctrl_buffer);
                ip_address[1] = (t & 0xff);				
                t = 0;
                                while (t < 8) rubicon_ctrl_buffer[t++] = NULL;				
                t = 0;
                x++;
                while ((x < 15) && (pcValue[i][x] != NULL) && (IS09(&pcValue[i][x])))
                                {
                                        rubicon_ctrl_buffer[t] = (uint8_t) pcValue[i][x];
                    x++;
                    t++;
                                }				
                rubicon_ctrl_buffer[x] = NULL;
                t = atoi((char *)rubicon_ctrl_buffer);
                ip_address[2] = (t & 0xff);				
                t = 0;
                                while (t < 8) rubicon_ctrl_buffer[t++] = NULL;				
                t = 0;
                x++;
                while ((x < 15) && (pcValue[i][x] != NULL) && (IS09(&pcValue[i][x])))
                                {
                                        rubicon_ctrl_buffer[t] = (uint8_t) pcValue[i][x];
                    x++;
                    t++;
                                }				
                rubicon_ctrl_buffer[x] = NULL;
                t = atoi((char *)rubicon_ctrl_buffer);
                ip_address[3] = (t & 0xff);				
            }
            else if (strncmp (pcParam[i] , "snm", 3) == 0) 
            {  
                t = 0;
                                while (t < 8) rubicon_ctrl_buffer[t++] = NULL;                
                x = 0;	
                                while ((x < 15) && (pcValue[i][x] != NULL) && (IS09(&pcValue[i][x])))
                                {
                                        rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
                    x++;
                                }				
                rubicon_ctrl_buffer[x] = NULL;
                t = atoi((char *)rubicon_ctrl_buffer);
                subnet_mask[0] = (t & 0xff);				
                t = 0;
                                while (t < 8) rubicon_ctrl_buffer[t++] = NULL;				
                t = 0;
                x++;
                while ((x < 15) && (pcValue[i][x] != NULL) && (IS09(&pcValue[i][x])))
                                {
                                        rubicon_ctrl_buffer[t] = (uint8_t) pcValue[i][x];
                    x++;
                    t++;
                                }				
                rubicon_ctrl_buffer[x] = NULL;
                t = atoi((char *)rubicon_ctrl_buffer);
                subnet_mask[1] = (t & 0xff);				
                t = 0;
                                while (t < 8) rubicon_ctrl_buffer[t++] = NULL;				
                t = 0;
                x++;
                while ((x < 15) && (pcValue[i][x] != NULL) && (IS09(&pcValue[i][x])))
                                {
                                        rubicon_ctrl_buffer[t] = (uint8_t) pcValue[i][x];
                    x++;
                    t++;
                                }				
                rubicon_ctrl_buffer[x] = NULL;
                t = atoi((char *)rubicon_ctrl_buffer);
                subnet_mask[2] = (t & 0xff);				
                t = 0;
                                while (t < 8) rubicon_ctrl_buffer[t++] = NULL;				
                t = 0;
                x++;
                while ((x < 15) && (pcValue[i][x] != NULL) && (IS09(&pcValue[i][x])))
                                {
                                        rubicon_ctrl_buffer[t] = (uint8_t) pcValue[i][x];
                    x++;
                    t++;
                                }				
                rubicon_ctrl_buffer[x] = NULL;
                t = atoi((char *)rubicon_ctrl_buffer);
                subnet_mask[3] = (t & 0xff);
            }
            else if (strncmp (pcParam[i] , "gwa", 3) == 0) 
            {  
                t = 0;
                                while (t < 8) rubicon_ctrl_buffer[t++] = NULL;                
                x = 0;	
                                while ((x < 15) && (pcValue[i][x] != NULL) && (IS09(&pcValue[i][x])))
                                {
                                        rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
                    x++;
                                }				
                rubicon_ctrl_buffer[x] = NULL;
                t = atoi((char *)rubicon_ctrl_buffer);
                gateway_address[0] = (t & 0xff);				
                t = 0;
                                while (t < 8) rubicon_ctrl_buffer[t++] = NULL;				
                t = 0;
                x++;
                while ((x < 15) && (pcValue[i][x] != NULL) && (IS09(&pcValue[i][x])))
                                {
                                        rubicon_ctrl_buffer[t] = (uint8_t) pcValue[i][x];
                    x++;
                    t++;
                                }				
                rubicon_ctrl_buffer[x] = NULL;
                t = atoi((char *)rubicon_ctrl_buffer);
                gateway_address[1] = (t & 0xff);				
                t = 0;
                                while (t < 8) rubicon_ctrl_buffer[t++] = NULL;				
                t = 0;
                x++;
                while ((x < 15) && (pcValue[i][x] != NULL) && (IS09(&pcValue[i][x])))
                                {
                                        rubicon_ctrl_buffer[t] = (uint8_t) pcValue[i][x];
                    x++;
                    t++;
                                }				
                rubicon_ctrl_buffer[x] = NULL;
                t = atoi((char *)rubicon_ctrl_buffer);
                gateway_address[2] = (t & 0xff);				
                t = 0;
                                while (t < 8) rubicon_ctrl_buffer[t++] = NULL;				
                t = 0;
                x++;
                while ((x < 15) && (pcValue[i][x] != NULL) && (IS09(&pcValue[i][x])))
                                {
                                        rubicon_ctrl_buffer[t] = (uint8_t) pcValue[i][x];
                    x++;
                    t++;
                                }				
                rubicon_ctrl_buffer[x] = NULL;
                t = atoi((char *)rubicon_ctrl_buffer);
                gateway_address[3] = (t & 0xff);
                        
                IP4_ADDR(&new_ipaddr, ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
                IP4_ADDR(&new_netmask, subnet_mask[0], subnet_mask[1], subnet_mask[2], subnet_mask[3]);
                IP4_ADDR(&new_gateway, gateway_address[0], gateway_address[1], gateway_address[2], gateway_address[3]);
                
                netif_set_addr(&netif, &new_ipaddr , &new_netmask, &new_gateway);
                i2c_ee_buffer[0] = ip_address[0];
                i2c_ee_buffer[1] = ip_address[1];
                i2c_ee_buffer[2] = ip_address[2];
                i2c_ee_buffer[3] = ip_address[3];
                I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, EE_ETHERNET_IP_ADDRESS, 4);
                delay(I2C_EE_WRITE_DELAY);
                i2c_ee_buffer[0] = subnet_mask[0];
                i2c_ee_buffer[1] = subnet_mask[1];
                i2c_ee_buffer[2] = subnet_mask[2];
                i2c_ee_buffer[3] = subnet_mask[3];
                I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, EE_ETHERNET_SUBNET_ADDRESS, 4);
                delay(I2C_EE_WRITE_DELAY);
                i2c_ee_buffer[0] = gateway_address[0];
                i2c_ee_buffer[1] = gateway_address[1];
                i2c_ee_buffer[2] = gateway_address[2];
                i2c_ee_buffer[3] = gateway_address[3];
                I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, EE_ETHERNET_GATEWAY_ADDRESS, 4);
                delay(I2C_EE_WRITE_DELAY);		
            }
        }
        /* Check cgi parameter for RUBICON preview display image: 		SET /sysctrl.cgi?ipr 	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "ipr") == 0)
            {
                p_buffer = rubicon_ctrl_buffer;
                while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                x = 0;

                /* copy all string till next '&' parameter marker */
                while ((x < sizeof (rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
                {
                    rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
                    x++;
                }

                rubicon_ctrl_request = RUBICON_PREVIEW_DISPLAY_IMAGE;
                rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_OK;
                return "/log.html";
            }
        }
        /* Check cgi parameter for CONTROLLER load address list: 		SET /sysctrl.cgi?cad;load 	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i] , "cad") == 0) 
			{       
				p_buffer = rubicon_ctrl_buffer;
				while(p_buffer < rubicon_ctrl_buffer + sizeof(rubicon_ctrl_buffer)) *p_buffer++ = NULL;
				x = 0;
				
				while((x < sizeof(rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
				{
					rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
					x++;
				}
				rubicon_ctrl_buffer[x] = NULL;
				x++;
				
                if ((RUBICON_FirmwareUpdatePacket.update_state != FW_UPDATE_IDLE) ||
                   (RUBICON_FileUpdatePacket.update_state != FILE_UPDATE_IDLE))
                {
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_BUSY;
                    return "/log.html";
                }
				else if(RUBICON_LoadAddressList() != FILE_OK)
                {
                    DISPLAY_uSD_CardErrorSet();
                    RUBICON_CreateAddressList();
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_ERROR;
                    return "/log.html";
                }
                else if(RUBICON_CreateAddressList() != FILE_OK)
                {
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_ERROR;
                    return "/log.html";
                }
                
                rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_OK;
                return "/log.html";
				
			}
        }
        /* Check cgi parameter for HC update weather forecast:          wfc */
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "wfc") == 0U)
            {
                if ((RUBICON_FirmwareUpdatePacket.update_state != FW_UPDATE_IDLE) ||
				    (RUBICON_FileUpdatePacket.update_state != FILE_UPDATE_IDLE))
                {
                    rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_BUSY;
                    return "/log.html";
                }
                
                int8_t sign = 1;
                i = 4U;
                x = 0U;
                t = strlen(&pcValue[0][0]);
                
                while(x < t)
                {
                    if(pcValue[0][x] == '-')
                    {
                        ++x;
                        sign = -1;
                    } else sign = 1;
                    
                    i2c_ee_buffer[i] = (int8_t) atoi(&pcValue[0][x]) * sign;
                    char *pos = strchr(&pcValue[0][x], ',');
                    
                    if(pos != NULL)
                    {
                        x = (pos - &pcValue[0][0] + 1U);
                        ++i;     
                    }
                    else x = t;
                }
                
                RTC_t dt;
                RTC_GetDateTime(&dt, RTC_Format_BIN);
                i2c_ee_buffer[0] = ((dt.unix >> 24U) & 0xFFU);
                i2c_ee_buffer[1] = ((dt.unix  >> 16U) & 0xFFU);
                i2c_ee_buffer[2] = ((dt.unix  >> 8U)  & 0xFFU);
                i2c_ee_buffer[3] =  (dt.unix  & 0xFFU);
                I2C_EERPOM_WriteBytes16(I2C_EE_WRITE_PAGE_0, EE_FORECAST_ADDRESS, WEATHER_FORECAST_DATA_SIZE);
                delay(I2C_EE_WRITE_DELAY);
                rubicon_http_cmd_state = HTTP_STATUS_RESPONSE_OK;
                return "/log.html";
            }
        }
    }
    /* uri to send after cgi call*/
    return "/sysctrl.html";
}

/**
 * Initialize SSI handlers
 */
void httpd_ssi_init(void)
{
    /* configure SSI handlers (ADC page SSI) */
    http_set_ssi_handler(HTTP_ResponseHandler, (char const **) TAGS, 1);
}

/**
 * Initialize CGI handlers
 */
void httpd_cgi_init(void)
{
    /* configure CGI handlers */
    CGI_TAB[0] = HTTP_CGI;
    http_set_cgi_handlers(CGI_TAB, 1);
}


