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
#include "main.h"
#include "rtc.h"
#include <string.h>
#include <stdlib.h>
#include "Gpio.h"
#include "hotel_room_controller.h"
#include "buzzer.h"
#include "W25Q16.h"
#include "stm32f429i_lcd.h"
#include "uart.h"


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


u16_t ADC_Handler(int iIndex, char *pcInsert, int iInsertLen)
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
		else if (rubicon_http_cmd_state == HTTP_RUBICON_TEMPERATURE_READY)
		{
			v = 0;
			*(pcInsert + v++) = 'T';
			*(pcInsert + v++) = 'M';
			*(pcInsert + v++) = '=';
			*(pcInsert + v++) = (char) rx_buffer[7];	
			*(pcInsert + v++) = (char) rx_buffer[8];
			*(pcInsert + v++) = (char) rx_buffer[9];
			*(pcInsert + v++) = (char) '.';
			*(pcInsert + v++) = (char) rx_buffer[10];
			*(pcInsert + v++) = ' ';
			*(pcInsert + v++) = 'T';
			*(pcInsert + v++) = 'S';
			*(pcInsert + v++) = '=';
			*(pcInsert + v++) = (char) rx_buffer[11];
			*(pcInsert + v++) = ' ';
			*(pcInsert + v++) = 'T';
			*(pcInsert + v++) = 'M';
			*(pcInsert + v++) = '=';
			*(pcInsert + v++) = (char) rx_buffer[12];
			*(pcInsert + v++) = ' ';
			*(pcInsert + v++) = 'S';
			*(pcInsert + v++) = 'P';
			*(pcInsert + v++) = '=';
			*(pcInsert + v++) = (char) rx_buffer[13];
			*(pcInsert + v++) = (char) rx_buffer[14];
			*(pcInsert + v++) = ' ';
			*(pcInsert + v++) = 'D';
			*(pcInsert + v++) = 'F';
			*(pcInsert + v++) = '=';
			*(pcInsert + v++) = (char) rx_buffer[15];
			*(pcInsert + v++) = (char) '.';
			*(pcInsert + v++) = (char) rx_buffer[16];			
			rubicon_ctrl_request = NULL;
			rubicon_http_cmd_state = NULL;
		}
		else if (rubicon_http_cmd_state == HTTP_RUBICON_ROOM_STATUS_READY)
		{
			v = 0;
			*(pcInsert + v++) = 'S';
			*(pcInsert + v++) = 'T';
			*(pcInsert + v++) = '=';
			*(pcInsert + v++) = (char) rx_buffer[7];		
			rubicon_ctrl_request = NULL;
			rubicon_http_cmd_state = NULL;
		}
        /* 4 characters need to be inserted in html*/
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
    uint32_t i = 0;
    uint32_t t = 0;
    uint32_t x = 0;
	
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

        /* Check cgi parameter for LED control: 					GET /sysctrl.cgi?led 	*/
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

        /* Check cgi parameter for date & time: 					GET /sysctrl.cgi?v  	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "v") == 0)
            {
                p_buffer = rubicon_ctrl_buffer;
                while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                x = 0;

                while ((x < sizeof (rubicon_ctrl_buffer)) && (pcValue[i][x] != NULL))
                {
                    rubicon_ctrl_buffer[x] = (uint8_t) pcValue[i][x];
                    x++;
                }

                if (x == 14)
                {
                    RTC_HttpCgiDateTimeUpdate(rubicon_ctrl_buffer);
                }
                else
                {
                    p_buffer = rubicon_ctrl_buffer;
                    while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                }
            }
        }

        /* Check cgi parameter for log list request: 				GET /sysctrl.cgi?log 	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "log") == 0)
            {
                if (strcmp(pcValue[i], "3") == 0)
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
            }
        }

        /* Check cgi parameter for RUBICON room status request:		GET /sysctrl.cgi?cst  	*/
        for (i = 0; i < iNumParams; i++)
        {
            if (strcmp(pcParam[i], "cst") == 0)
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

                /* check is number 3 or 4 digit and if not delete all*/
                if ((x == 3) || (x == 4))
                {
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
					
					rubicon_http_cmd_state = NULL;
					rubicon_ctrl_request = NULL;
					return "/log.html";
					
                }
                else
                {
                    p_buffer = rubicon_ctrl_buffer;
                    while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                }
            }
        }
        /* Check cgi parameter for RUBICON set room status:			SET /sysctrl.cgi?stg 	*/
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
				
				rubicon_ctrl_request = RUBICON_SET_ROOM_STATUS;
				
			}
		}
        /* Check cgi parameter for RUBICON set digital output:		GET /sysctrl.cgi?cdo 	*/
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
				
                /* check is number 3 or 4 digit and if not delete all*/
                if ((x == 3) || (x == 4))
                {
                    rubicon_ctrl_buffer[13] = '1';
                    rubicon_ctrl_buffer[14] = '1';
                    rubicon_ctrl_buffer[15] = '1';
                    rubicon_ctrl_buffer[16] = '1';
                    rubicon_ctrl_buffer[17] = '1';
                    rubicon_ctrl_buffer[18] = '1';
                    rubicon_ctrl_buffer[19] = '1';
                    rubicon_ctrl_buffer[20] = '1';
                    rubicon_ctrl_request = RUBICON_SET_DOUT_STATE;
                }
                else
                {
                    p_buffer = rubicon_ctrl_buffer;
                    while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                }
            }
        }

        /* Check cgi parameter for RUBICON update firmware: 		GET /sysctrl.cgi?cud 	*/
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

                /* check is number 3 or 4 digit and if not delete all*/
                if ((x == 3) || (x == 4))
                {
                    rubicon_ctrl_request = RUBICON_DOWNLOAD_FIRMWARE;
                }
                else
                {
                    p_buffer = rubicon_ctrl_buffer;
                    while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                }
            }
        }

        /* Check cgi parameter for RUBICON set display brightness:	GET /sysctrl.cgi?cbr 	*/
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
				
			}
		}
		/* Check cgi parameter for RUBICON presed card id:			GET /sysctrl.cgi?sud 	*/
		for (i = 0; i < iNumParams; i++) 
		{
			if (strcmp(pcParam[i] , "sud") == 0) 
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
			else if (strcmp(pcParam[i] , "slo") == 0) 
			{       
				t = 0;
				
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
				rubicon_ctrl_buffer[x] = NULL;
				x++;
				
			}
			else if (strcmp(pcParam[i] , "ska") == 0) 
			{       
				t = 0;
				
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
				rubicon_ctrl_buffer[x] = NULL;
				x++;
				
			}
			else if (strcmp(pcParam[i] , "sda") == 0) 
			{       
				t = 0;
				
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
				rubicon_ctrl_request = RUBICON_SET_MIFARE_PERMITED_CARD;
				
			}
		}
		/* Check cgi parameter for RUBICON SOS alarm reset request:	GET /sysctrl.cgi?rud	*/
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
                }
                else
                {
                    p_buffer = rubicon_ctrl_buffer;
                    while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                }
            }
        }
		/* Check cgi parameter for RUBICON reset controller:		GET /sysctrl.cgi?rst 	*/
		for (i = 0; i < iNumParams; i++) 
		{
			if (strcmp(pcParam[i], "rst") == 0)
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

                /* check is number 3 or 4 digit and if not delete all*/
                if ((x == 3) || (x == 4))
                {
                    rubicon_ctrl_request = RUBICON_START_BOOTLOADER;
                }
                else
                {
                    p_buffer = rubicon_ctrl_buffer;
                    while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                }
            }
		}
		/* Check cgi parameter for RUBICON image upload:			GET /sysctrl.cgi?sld 	*/
		for (i = 0; i < iNumParams; i++) 
		{
			if (strcmp(pcParam[i] , "sld") == 0) 
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
			else if (strcmp(pcParam[i] , "sli") == 0) 
			{       
				t = 0;
				/* copy all string till next '&' parameter marker */
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
				rubicon_ctrl_request = RUBICON_DOWNLOAD_DIPLAY_IMAGE;
				
			}
		}
		/* Check cgi parameter for RUBICON address set:				GET /sysctrl.cgi?iud 	*/
		for (i = 0; i < iNumParams; i++) 
		{
			if (strcmp(pcParam[i] , "iud") == 0) 
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
			else if (strcmp(pcParam[i] , "iun") == 0) 
			{       
				t = 0;
				/* copy all string till next '&' parameter marker */
				while((t < sizeof(rubicon_ctrl_buffer) + x) && (pcValue[i][t] != NULL))
				{
					rubicon_ctrl_buffer[x++] = (uint8_t) pcValue[i][t++];
				}
				
				rubicon_ctrl_request = RUBICON_SET_RS485_CONFIG;
				
			}
		}
		/* Check cgi parameter for RUBICON get room temperature:	GET /sysctrl.cgi?tct 	*/
        for (i = 0; i < iNumParams; i++)
        {
			if (strcmp(pcParam[i], "tct") == 0)
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

                /* check is number 3 or 4 digit and if not delete all*/
                if ((x == 3) || (x == 4))
                {
					rubicon_http_cmd_state = HTTP_GET_RUBICON_TEMPERATURE;
                    rubicon_ctrl_request = RUBICON_GET_ROOM_TEMPERATURE;
					RUBICON_StartResponseTimer(RUBICON_HTTP_RESPONSE_TIMEOUT);
					
					while(!IsRUBICON_ResponseTimerExpired()) 
					{
						RUBICON_ProcessService();
						
						if(rubicon_http_cmd_state != HTTP_GET_RUBICON_TEMPERATURE)
						{
							RUBICON_StopResponseTimer();
							return "/log.html";
						}
					}
					
					rubicon_http_cmd_state = NULL;
					rubicon_ctrl_request = NULL;
					return "/log.html";
					
                }
                else
                {
                    p_buffer = rubicon_ctrl_buffer;
                    while (p_buffer < rubicon_ctrl_buffer + sizeof (rubicon_ctrl_buffer)) *p_buffer++ = NULL;
                }
            }
        }
		/* Check cgi parameter for RUBICON set room temperature:	GET /sysctrl.cgi?tsp 	*/
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
				
				rubicon_ctrl_request = RUBICON_SET_ROOM_TEMPERATURE;				
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
    http_set_ssi_handler(ADC_Handler, (char const **) TAGS, 1);
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


