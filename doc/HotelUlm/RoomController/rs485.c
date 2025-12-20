/**
 ******************************************************************************
 * File Name          : rs485.c
 * Date               : 28/02/2016 23:16:19
 * Description        : rs485 communication modul
 ******************************************************************************
 *
 *
 ******************************************************************************
 */
 
#if (__RS485_H__ != FW_BUILD)
    #error "rs485 header version mismatch"
#endif 
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
/* Imported Types  -----------------------------------------------------------*/
/* Imported Variables --------------------------------------------------------*/
/* Imported Functions    -----------------------------------------------------*/
/* Private Typedef -----------------------------------------------------------*/
eComStateTypeDef eComState = COM_INIT;
/* Private Define  -----------------------------------------------------------*/
#define BUFF_SIZE   256U
/* Private Variables  --------------------------------------------------------*/
__IO uint32_t rstmr;
__IO uint32_t rsflg;
uint8_t rsbps;
uint8_t rsifa[2];
uint8_t rsgra[2];
uint8_t rsbra[2];
uint8_t sysid[2]     = {0xAB,0xCD};
uint8_t rs_txaddr[2]; // sender address
//static uint32_t rs_rxtmr;
//static uint32_t rs_rxtout;
//static uint8_t  rfinit = 0U;
static uint8_t  rsbuf[RS485_BSIZE];

__IO uint32_t packet_type;
__IO uint32_t receive_pcnt;
__IO uint32_t rs485_timer;
__IO uint32_t rs485_flags;
__IO uint32_t rs485_sender_addr;
__IO uint32_t rs485_pak_dlen;
__IO uint32_t rec_bcnt;
__IO uint32_t rxtx_tmr;
static uint8_t rec;
FwInfoTypeDef newinf;
FwInfoTypeDef appinf;
uint8_t activ_cmd;
/* Private macros   ----------------------------------------------------------*/
/* Private Function Prototypes -----------------------------------------------*/
uint8_t UpdateBootloader(void);
void RS485_Response(uint8_t resp, uint8_t size);
void RS485_SetUsart(uint8_t brate, uint8_t bsize);
/* Program Code  -------------------------------------------------------------*/
/**
* @brief :  init usart interface to rs485 9 bit receiving 
* @param :  and init state to receive packet control block 
* @retval:  wait to receive:
*           packet start address marker SOH or STX  2 byte  (1 x 9 bit)
*           packet receiver address 4 bytes msb + lsb       (2 x 9 bit)
*           packet sender address msb + lsb 4 bytes         (2 x 9 bit)
*           packet lenght msb + lsb 4 bytes                 (2 x 9 bit)
*/
void RS485_Init(void) // init usart to known state
{
    rec = 0;
    rec_bcnt = 0;
    receive_pcnt = 0;
    packet_type = 0;
    COM_Link = NOLINK;
    RS485_StopTimer();
    ZEROFILL(rsbuf, COUNTOF(rsbuf));
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    
    if (huart1.RxState == HAL_UART_STATE_BUSY_RX)
    {
        __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
        huart1.RxState = HAL_UART_STATE_READY;
        huart1.gState = HAL_UART_STATE_READY;
    }

    if (HAL_UART_Receive_IT(&huart1, &rec, 1) != HAL_OK) ErrorHandler(RS485_FUNC, USART_DRV);
    eComState = COM_PACKET_PENDING;
}
/**
* @brief  : rs485 service function is  called from every
* @param  : main loop cycle to service rs485 communication
* @retval : receive and send on regulary base 
*/
void RS485_Service(void)
{
    uint8_t ebuf[32];
    uint8_t respb = ACK;    // default response byte
    uint8_t resps = 1;   // default response data size
    uint8_t restart = 0;
    uint32_t temp = 0U;
    int lpwd = 0U;
    char *par;
    RTC_TimeTypeDef tm;
    RTC_DateTypeDef dt;
	static uint32_t upd_tout_tmr;
    static uint32_t total_bytes_in_file;
    static uint32_t next_packet_number;
    static uint32_t total_packet_number;
    static uint32_t crc_32_calculated;
    static uint32_t crc_32_file;
    static uint32_t flash_destination;
    
    
    if (IsRS485_UpdateActiv())
    {
        if ((HAL_GetTick() - upd_tout_tmr) >= REC_TOUT)
        {
            RS485_OWStopUpdate();
			SYS_FileRxFailSet();
			SYS_FileRxOkReset();
        }
    }

	if (eComState == COM_PACKET_RECEIVED)
	{
		if      (packet_type == SOH)
		{ 
			if ((rsbuf[0] >= DWNLD_DISP_IMG_1) && (rsbuf[0] <= DWNLD_DISP_IMG_25))
			{
                next_packet_number  = 1U;
                activ_cmd           = rsbuf[0];
                total_packet_number = (rsbuf[1]<<8)| rsbuf[2];
                total_bytes_in_file = (rsbuf[3] <<24)|(rsbuf[4] <<16)|(rsbuf[5] <<8)|rsbuf[6];
                crc_32_file         = (rsbuf[7] <<24)|(rsbuf[8] <<16)|(rsbuf[9] <<8)|rsbuf[10];
                flash_destination   = RT_NEW_FILE_ADDR;
                MX_QSPI_Init(); // reinit qspi interface to execute sector erase command
                
                if (QSPI_Erase(flash_destination, flash_destination + total_bytes_in_file) == QSPI_OK) 
                {
                    RS485_OWStartUpdate();
                    rsbuf[0] = 1;  // next expected packet LSB
                    rsbuf[7] = 0;  // next expected packet MSB
                    upd_tout_tmr  = HAL_GetTick();
                    SYS_FileRxFailReset();
                    resps = 3;
                }
                else 
                {
                    if      (activ_cmd == DWNLD_DISP_IMG)  LogEvent.log_event = IMG_UPD_FAIL;
                    else if (activ_cmd == DWNLD_BLDR_IMG)  LogEvent.log_event = BLDR_UPD_FAIL;
                    else if (activ_cmd == DWNLD_FWR_IMG)   LogEvent.log_event = FW_UPD_FAIL;
                    LOGGER_Write();
                    activ_cmd = 0;
                    crc_32_file = 0;
                    flash_destination = 0;
                    total_packet_number = 0;
                    total_bytes_in_file = 0;
                    rsbuf[7] = LogEvent.log_event;
                    respb = NAK; 
                    resps = 2;
                }
                MX_QSPI_Init();         // reinit interface again to 
                QSPI_MemMapMode();      // reinit qspi interface to execute sector erase command  
                
                if      (activ_cmd == DWNLD_FWR_IMG)  SYS_FwRqReset();
                else if (activ_cmd == DWNLD_BLDR_IMG) SYS_FwRqReset();
                else if (activ_cmd == DWNLD_DISP_IMG) SYS_ImageRqReset();            
			}
			else
			{
				switch(rsbuf[0])
				{
					case UPDATE_BLDR:
					{
						RS485_OWStartUpdate();
						upd_tout_tmr = HAL_GetTick();
						if (UpdateBootloader())
						{
                            LogEvent.log_event = BLDR_UPD_FAIL;
							DISPBldrUpdFailSet();
							SYS_UpdFailSet();
                            respb = NAK;
						}
						else 
						{
                            LogEvent.log_event = BLDR_UPDATED;
							DISPBldrUpdSet();
							SYS_UpdOkSet();
						}
                        LOGGER_Write();
                        break;
					}
					
					case START_BLDR:
					{
						ebuf[0] = BLDR_CMD_RUN;
                        if (EE_WriteBuffer(ebuf, EE_FW_UPDATE_STATUS, 1) != EE_OK) respb = NAK;
						else restart = 1;
						break;
					}
                    
                    case SET_BR2OW:
					{
                        respb = OW_RS485_Bridge(rsbuf);
                        if  (respb == ESC) COM_Link = GROUP; // do not send response to group request
                        else resps = rsbuf[5];       // response packet size
						break;  // response, to broadcast or group addressed package
					}
                    
					case SET_BEDDING_REPL:
					{
                        if (EE_WriteBuffer(&rsbuf[1], EE_BEDNG_REPL_ADD, 1) != EE_OK) respb = NAK;
						break;
					}
					
					case GET_LOG_LIST:
					{   
                        if      (EE_IsDeviceReady(EE_ADDR, DRV_TRIAL) != HAL_OK) respb = NAK;
                        else if (LOGGER_Read(&rsbuf[7]) == LOGGER_OK) resps = LOG_DSIZE+2U; // try to read lod
                        else respb = NAK;
						break;
					}
					
					case DEL_LOG_LIST:
					{  
						if      (EE_IsDeviceReady(EE_ADDR, DRV_TRIAL) != HAL_OK) respb = NAK;	
                        else if (LOGGER_Delete() != LOGGER_OK) respb = NAK;             
						break;
					}
					
					case SET_RS485_CFG:
					{
                        rsifa[0] = rsbuf[1];
                        rsifa[1] = rsbuf[2];
                        rsgra[0] = rsbuf[3];
                        rsgra[1] = rsbuf[4];
                        rsbra[0] = rsbuf[5];
                        rsbra[1] = rsbuf[6];
						rsbps    = rsbuf[7];
                        if (EE_WriteBuffer(&rsbuf[1], EE_RSIFA, 7) != EE_OK) respb = NAK;
						break;
					}
				
					case SET_PERMITED_GROUP:
					{
                        if (EE_WriteBuffer(&rsbuf[1], EE_USRGR_ADD, 16) != EE_OK) respb = NAK;
						break;
					}
										
                    
					case SET_ROOM_TEMP:
					{                        
                        if (EE_IsDeviceReady(EE_ADDR, DRV_TRIAL) == HAL_OK)
                        {
                            if (rsbuf[1]) 
                            {   // update setpoint only if new value send
                                thst_sp   = rsbuf[1]; 
                                if      (thst_sp > thst_max_sp) thst_sp = thst_max_sp; // setpoint max. limit
                                else if (thst_sp < thst_min_sp) thst_sp = thst_min_sp; // setpoint min. limit
                                EE_WriteBuffer(&thst_sp, EE_ROOM_TEMP_SP, 1);
                            }
                            
                            if (rsbuf[2]) 
                            {   // update difference only if new value send
                                thst_dif  = rsbuf[2];
                                if (thst_dif > 9U) thst_dif = 1U;
                                EE_WriteBuffer(&thst_dif, EE_ROOM_TEMP_DIFF, 1);
                            }
                            
                            if (IsTempRegNewCfg(rsbuf[3])) 
                            {   // update config only if new value send
                                if (IsTempRegNewSta(rsbuf[3])) // is new state config
                                {
                                    if(IsTempRegSta(rsbuf[3]))  TempRegOn();
                                    else                        TempRegOff();
                                }
                                
                                if (IsTempRegNewMod(rsbuf[3])) // is new mode config
                                {
                                    if(IsTempRegMod(rsbuf[3]))  TempRegHeating();
                                    else                        TempRegCooling();
                                }
                                
                                if (IsTempRegNewCtr(rsbuf[3])) // is new controll state
                                {
                                    if(IsTempRegCtr(rsbuf[3]))  TempRegEnable();
                                    else                        TempRegDisable();
                                }
                                
                                if (IsTempRegNewOut(rsbuf[3])) // is new output state
                                {
                                    if(IsTempRegOut(rsbuf[3]))  TempRegOutputOn();
                                    else                        TempRegOutputOff();
                                }
                                ebuf[0]  = (sysfl >> 24);    
                                ebuf[1]  = (sysfl >> 16);
                                ebuf[2]  = (sysfl >>  8);
                                ebuf[3]  = (sysfl & 0xff);
                                EE_WriteBuffer(ebuf, EE_SYS_FLAGS2, 4);
                            }                            
                            ebuf[0] = STX;
                            ebuf[1] = owbra;
                            ebuf[2] = owifa;
                            ebuf[3] = 2U;
                            ebuf[4] = SET_ROOM_TEMP; // copy command
                            ebuf[5] = rsbuf[1];
                            ebuf[6] = rsbuf[2];
                            ebuf[7] = rsbuf[3];
                            OW_RS485_Bridge(ebuf);
                            SetpointUpdateSet();
                            BTNUpdSet();
                        }
                        else respb = NAK;
                        break;
					}
					
					case SET_SYSTEM_ID:
					{
						sysid[0] = rsbuf[1];
						sysid[1] = rsbuf[2];
                        if (EE_WriteBuffer(&sysid[0], EE_SYSID, 2) != EE_OK) respb = NAK;
						break;
					}
					
                    case SET_RTC_DATE_TIME:
                    {
                        if (!rsbuf[1]) rsbuf[1] = 7;
                        rtcdt.WeekDay = rsbuf[1];
                        rtcdt.Date    = rsbuf[2];
                        rtcdt.Month   = rsbuf[3];
                        rtcdt.Year    = rsbuf[4];
                        rtctm.Hours   = rsbuf[5];
                        rtctm.Minutes = rsbuf[6];
                        rtctm.Seconds = rsbuf[7];                       
                        HAL_RTC_SetTime(&hrtc, &rtctm, RTC_FORMAT_BCD);
                        HAL_RTC_SetDate(&hrtc, &rtcdt, RTC_FORMAT_BCD);
                        RtcTimeValidSet();
                        /**
                        *   propagate broadcast packet to onewire 
                        *   and radio connected interfaces  
                        */
                        if (IsRoomCtrlConfig())
                        {                            
                            rsbuf[0] = STX;
                            rsbuf[1] = owbra;
                            rsbuf[2] = owifa;
                            rsbuf[3] = 8;
                            rsbuf[4] = SET_RTC_DATE_TIME; // copy command
                            rsbuf[5] = rtcdt.WeekDay;
                            rsbuf[6] = rtcdt.Date;
                            rsbuf[7] = rtcdt.Month;
                            rsbuf[8] = rtcdt.Year;
                            rsbuf[9] = rtctm.Hours;
                            rsbuf[10]= rtctm.Minutes;
                            rsbuf[11]= rtctm.Seconds;
                            respb = OW_RS485_Bridge(rsbuf);                           
                        }
                        break;
                    }
                    
                    case SET_DOUT_STATE:
                    {
                        for (temp = 0U; temp < 8; temp++) // set high bits in loop
                        {
                            switch(rsbuf[temp+1U])
                            {  
                                case '0':   // digital output forced off, output config set to enabled
                                    dout_0_7_rem &= (uint16_t) (~(1U<<temp));
                                    dout_cfg[temp] = 2U;
                                    break;
                                case '1':   // digital output forced on, output config set to enabled
                                    dout_0_7_rem |= (uint16_t) (1U<<temp);
                                    dout_cfg[temp] = 2U;
                                    break;
                                case '2':   // digital output is enabled and processed in program
                                case '3':   // digital output set to constant disabled state and saved
                                case '4':   // digital output set to constant on state and saved
                                case '5':   // digital output set to constant off state and saved
                                    dout_cfg[temp] = TODEC(rsbuf[temp+1U]);
                                    break;
                                default:    // if not other defined, default is enabled state 
                                    dout_cfg[temp] = 2U;  
                                    break;
                            }
                        }
                        if      (rsbuf[9] == '1') dout_0_7_rem |= (1U<<8);   // digital output remote control enable
                        else if (rsbuf[9] == '0') dout_0_7_rem &= (~(1U<<8));  // digital output remote control disable
                        if      (EE_IsDeviceReady(EE_ADDR, DRV_TRIAL) != HAL_OK) respb = NAK;
                        else if (EE_WriteBuffer(&dout_cfg[0], EE_DOUT_CFG_ADD_1, 8) != EE_OK) respb = NAK;
                        break; 
                    }
                    
                    
                    case SET_DIN_CFG:
                    {
                        for (temp = 0U; temp < 8; temp++) // set high bits in loop
                        {
                            switch(rsbuf[temp+1U])
                            {  
                                case '0':   // digital output forced off, output config set to enabled
                                case '1':   // digital output forced on, output config set to enabled
                                case '2':   // digital output is enabled and processed in program
                                    din_cfg[temp] = 2U;
                                    break;
                                case '3':   // digital output set to constant disabled state and saved
                                case '4':   // digital output set to constant on state and saved
                                case '5':   // digital output set to constant off state and saved
                                    din_cfg[temp] = TODEC(rsbuf[temp+1U]);
                                    break;
                                default:    // if not other defined, default is enabled state 
                                    din_cfg[temp] = 3U;  
                                    break;
                            }
                        }
                        if      (EE_IsDeviceReady(EE_ADDR, DRV_TRIAL) != HAL_OK) respb = NAK;
                        else if (EE_WriteBuffer(&din_cfg[0], EE_DIN_CFG_ADD_1, 8) != EE_OK) respb = NAK;
                        break; 
                    }
                    
                    case RT_UPD_QRC:
                    {
                        if (rs485_pak_dlen < QRC_DSIZE) 
                        {
                            if (EE_WriteBuffer(&rsbuf[1], EE_QR_CODE, rs485_pak_dlen+1) != EE_OK) respb = NAK;
                        }
                        break;
                    }                    
                    
                    case RT_DISP_QRC:
                    {
                        DISPQRCode(((rsbuf[1]<<8)|rsbuf[2]), ((rsbuf[3]<<8)|rsbuf[4])); // set response from display function
                        break;
                    }
                    
                    case SET_PASSWORD:
                    {   
                        if (EE_IsDeviceReady(EE_ADDR, DRV_TRIAL) == HAL_OK)
                        {   
                            par = (char*)&rsbuf[1]; // set pointer to data
                            mem_zero (ebuf, sizeof(ebuf)); // clear temp buffer
                            EE_ReadBuffer(ebuf, EE_USRGR_ADD, 16U); // load all permited usergroup to local buffer
                            do
                            {   
                                if (strchr((char*)ebuf, *par)) // search usergroup in eeprom data
                                {
                                    if (*par == USERGRP_GUEST) // found guest tag 'G' inside received buffer
                                    {
                                        ++par;
                                        temp = TODEC(*par); // copy guest password ID and set buffer to next byte
                                        ++par;
                                        if (*par == 'X')    // delete this password
                                        {   
                                            mem_zero (&ebuf[16], 8); // clear temp buffer
                                            EE_WriteBuffer(&ebuf[16], EE_USER_PSWRD+((temp-1U)*8U), 8);  // write empty buffer to user eeprom address
                                            EE_ReadBuffer (&ebuf[24], EE_USER_PSWRD+((temp-1U)*8U), 8);  // read this buffer again to confirm command
                                            if (memcmp(&ebuf[16], &ebuf[24], 8) != 0)// password data not erased 
                                            {
                                                respb = NAK;    // error while erasing password
                                                break;          // exit loop to send negativ response
                                            }
                                        }
                                        else if (*par == ',') // skeep comma to next byte
                                        {
                                            ++par;
                                            lpwd = atoi(par); // convert password to decimal
                                            ebuf[16] = temp;
                                            ebuf[17] = ((lpwd >> 16) & 0xFF); // password    MSB 
                                            ebuf[18] = ((lpwd >>  8) & 0xFF); 
                                            ebuf[19] =  (lpwd        & 0xFF); // password    LSB
                                            par = strchr(par, ','); // find password expiry time data
                                            if (!par)
                                            {
                                                respb = NAK;    // invalid password expiry time data
                                                break;          // exit loop to send negativ response
                                            }
                                            else ++par;
                                            tm.Seconds = 0;
                                            Str2Hex(par,   &tm.Hours,  2);
                                            Str2Hex(par+2, &tm.Minutes,2);
                                            Str2Hex(par+4, &dt.Date,   2);
                                            Str2Hex(par+6, &dt.Month,  2);
                                            Str2Hex(par+8, &dt.Year,   2);
                                            //par += 10U;
                                            lpwd = rtc2unix(&tm, &dt); // convert to unix time
                                            ebuf[20] = ((lpwd >> 24) & 0xFF);  // password    MSB 
                                            ebuf[21] = ((lpwd >> 16) & 0xFF); 
                                            ebuf[22] = ((lpwd >>  8) & 0xFF); 
                                            ebuf[23] =  (lpwd        & 0xFF); // password    LSB
                                            EE_WriteBuffer (&ebuf[16], EE_USER_PSWRD+((temp-1U)*8U), 8U);  // write empty buffer to user eeprom address
                                            EE_ReadBuffer  (&ebuf[24], EE_USER_PSWRD+((temp-1U)*8U), 8U);  // read this buffer again to confirm command
                                            if (memcmp(&ebuf[16], &ebuf[24], 8) != 0)
                                            {
                                                respb = NAK;    // fail to write password
                                                break;          // exit loop to send negativ response
                                            }
                                        }
                                    }
                                    else // other users password
                                    {
                                        lpwd = atoi(par); // convert password to decimal
                                        ebuf[16] = ((lpwd >> 16) & 0xFF); // password    MSB 
                                        ebuf[17] = ((lpwd >>  8) & 0xFF); 
                                        ebuf[18] =  (lpwd        & 0xFF); // password    LSB
                                        if      (*par == USERGRP_HANDMAID) EE_WriteBuffer(&ebuf[16], EE_MAID_PSWRD,   3); 
                                        else if (*par == USERGRP_MANAGER)  EE_WriteBuffer(&ebuf[16], EE_MNGR_PSWRD,   3); 
                                        else if (*par == USERGRP_SERVICE)  EE_WriteBuffer(&ebuf[16], EE_SRVC_PSWRD,   3);
                                    }
                                }
                                par = strchr(par, ','); // set pointer to next coma
                                if (par) ++par;
                            }
                            while(par); // search till end of payload 
                        }
                        else respb = NAK;
                        break;
                    }
                
                    default:
					{
                        resps = 1;
                        respb = NAK;
						break;
					}
				}				
			}
		}
		else if (packet_type == STX)
		{
			if (next_packet_number == ((rsbuf[0]<<8)|rsbuf[1]))
			{
                MX_QSPI_Init();
                QSPI_Write  (&rsbuf[2], flash_destination, rs485_pak_dlen-2U);
                MX_QSPI_Init();
                QSPI_MemMapMode();				
                flash_destination += (rs485_pak_dlen-2U);
				if (next_packet_number == total_packet_number)
				{
                    CRC_ResetDR();
                    crc_32_calculated = CRC_Calculate8 ((uint8_t*)RT_NEW_FILE_ADDR, total_bytes_in_file);
					if (crc_32_calculated == crc_32_file)
					{	
						if      (activ_cmd == DWNLD_FWR_IMG)     // RC firmware IMG20.RAW
						{                            
						}
                        else if (activ_cmd == DWNLD_BLDR_IMG)    // RC bootloader IMG21.RAW
						{
						}
                        else if (activ_cmd == RT_DWNLD_FWR)      // RT firmware IMG22.RAW
						{
						}
                        else if (activ_cmd == RT_DWNLD_BLDR)     // RT bootloader IMG23.RAW
						{
						}
                        else if (activ_cmd == RT_DWNLD_LOGO)     // RT user logo PNG IMG24.RAW
						{
                            if (QSPI2QSPI_Copy (RT_NEW_FILE_ADDR, RT_LOGO_ADDR, total_bytes_in_file) == QSPI_OK)
                            {
                                LogEvent.log_event = IMG_UPDATED;
                                SYS_UpdOkSet();
                            }
                            else
                            {
                                LogEvent.log_event = IMG_UPD_FAIL;
                                SYS_UpdFailSet();
                            }
                            LOGGER_Write();                            
						}
                        else if (activ_cmd == RT_DWNLD_LANG)     // RT language translation IMG25.RAW
						{
						}
						else if ((activ_cmd >= DWNLD_DISP_IMG_1) // RC display image / RT display PNG elements
                        ||       (activ_cmd <= DWNLD_DISP_IMG_20))  
						{
							SYS_UpdOkSet();
						}						
						SYS_FileRxOkSet();
                        rsbuf[7] = FILE_RECEIVED;
					}
					else
					{
						SYS_FileRxFailSet();
                        rsbuf[7] = FILE_CRC_FAIL;
                        respb = NAK; // something wrong
					}                    
                    activ_cmd = 0;
                    flash_destination = 0;
                    RS485_OWStopUpdate();
                    LogEvent.log_event = rsbuf[7];   // write log event for file transfer ressult
                    LOGGER_Write();
                    resps = 2;
				}
				else ++next_packet_number;
                upd_tout_tmr = HAL_GetTick();
			}
			else respb = NAK; // something wrong
            rsbuf[0] = next_packet_number;
            rsbuf[7] = next_packet_number>>8;
            resps = 2;
		}		
		if (COM_Link == P2P) RS485_Response(respb, resps);
        if (restart == 1) Restart();
		RS485_Init();
	}
}
/**
  * @brief  
  * @param  
  * @retval 
  */
void RS485_RxCpltCallback(void)
{
    
    uint32_t i, cksm = 0U;
    uint32_t resps = 1, respb = ACK, poscmd = 0;

    RS485_StartTimer(RX_TOUT);

    switch (++receive_pcnt)
    {
        case 1:
            if ((rec == SOH) || (rec == STX)) packet_type = rec;
            break;
        case 2:
        case 4:    
            rsbuf[0] = rec;
            break;
        case 3:
            if      ((rsbuf[0] == rsifa[0]) && (rec == rsifa[1]))  COM_Link = P2P;
            else if ((rsbuf[0] == rsgra[0]) && (rec == rsgra[1]))  COM_Link = GROUP; 
            else if ((rsbuf[0] == rsbra[0]) && (rec == rsbra[1]))  COM_Link = BROADCAST; 
            else
            {
                receive_pcnt = 0;
                RS485_StartTimer(MUTE_DEL);
                return;
            }
            break;
        case 5:
            rs485_sender_addr = (rsbuf[0]<<8)|rec;
            break;
        case 6:
            rs485_pak_dlen = rec;
            break;
        case 7:
        case 8:
            receive_pcnt = 7;
            rsbuf[rec_bcnt++] = rec;
            if (rec_bcnt == (rs485_pak_dlen+3))
            {
                --rec_bcnt;
                RS485_StopTimer();
                for (i = 0; i != rs485_pak_dlen; i++)
                {
                    cksm += rsbuf[i];
                }
                if (((cksm >> 0x8U) == rsbuf[rec_bcnt-2]) 
                &&  ((cksm & 0xFFU) == rsbuf[rec_bcnt-1])
                &&  (EOT            == rsbuf[rec_bcnt]))
                {
                    __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
                    huart1.RxState = HAL_UART_STATE_READY;
                    huart1.gState = HAL_UART_STATE_READY;
                    eComState = COM_PACKET_RECEIVED;
                    RS485_StartTimer(REC_TOUT);
                    rxtx_tmr = HAL_GetTick();
                }
                else
                {
                    RS485_Init();
                    return;
                }
            }
            break;
        default:
            RS485_Init();
            return;
    }
    
	if (eComState == COM_PACKET_RECEIVED)
	{
		if (packet_type == SOH)
		{ 
            switch(rsbuf[0])
            {
                case RESTART_CTRL:
                {
                    poscmd = 1;
                    break;
                }
                
                case RT_SET_BTN_STA:
                {
                    if (IsRoomThstConfig())
                    {
                        // ROOM CLEANING DISABLE TOUCH EVENTS
                        if      ((rsbuf[1] & (1U<<0)) != 0U)    RoomCleaningSet();
                        else if ((rsbuf[1] & (1U<<0)) == 0U)    RoomCleaningReset();
                        // BUTTON DND STATE
                        if      (((rsbuf[1] & (1U<<1)) != 0U) && !IsBTNDndActiv())  BTNDndEnable();
                        else if (((rsbuf[1] & (1U<<1)) == 0U) &&  IsBTNDndActiv())  BTNDndDisable();
                        // BUTTON SOS STATE
                        if      (((rsbuf[1] & (1U<<2)) != 0U) && !IsBTNSosActiv())  BTNSosEnable();
                        else if (((rsbuf[1] & (1U<<2)) == 0U) &&  IsBTNSosActiv())  BTNSosDisable();
                        // BUTTON_CALMAID_STATE
                        if      (((rsbuf[1] & (1U<<3)) != 0U) && !IsBTNMaidActiv()) BTNMaidEnable();
                        else if (((rsbuf[1] & (1U<<3)) == 0U) &&  IsBTNMaidActiv()) BTNMaidDisable();
                        // ROOM CONTROLLER HVAC CONTACTOR STATUS
                        if      (((rsbuf[1] & (1U<<4)) != 0U) && !IsExtHVACPowerActiv())ExtHVACPowerEnable();
                        else if (((rsbuf[1] & (1U<<4)) == 0U) &&  IsExtHVACPowerActiv())ExtHVACPowerDisable();
                        // ROOM CONTROLLER BALCONY AND WINDOWS SWITCH STATUS
                        if      (((rsbuf[1] & (1U<<5)) != 0U) && !IsExtHVACCtrlActiv()) ExtHVACCtrlEnable();
                        else if (((rsbuf[1] & (1U<<5)) == 0U) &&  IsExtHVACCtrlActiv()) ExtHVACCtrlDisable();
                        // ROOM CONTROLER POWER CONTACTOR STATUS
                        if      (((rsbuf[1] & (1U<<6)) != 0U) && !IsExtRoomPowerActiv())ExtRoomPowerEnable();
                        else if (((rsbuf[1] & (1U<<6)) == 0U) &&  IsExtRoomPowerActiv())ExtRoomPowerDisable();                         
                        // ROOM CONTROLLER DOOR BELL TASTER STATUS
                        if      (((rsbuf[1] & (1U<<7)) != 0U) && !IsExtBTNBellActiv())  ExtBTNBellEnable();
                        else if (((rsbuf[1] & (1U<<7)) == 0U) &&  IsExtBTNBellActiv())  ExtBTNBellDisable();
                    }
                    else respb = NAK;
                    break;
                }
                
                case RT_SET_DISP_STA:             
                {
                    if      (IsRoomThstConfig() && ((rsbuf[1] != 1U) || (rsbuf[2] == 60U))) respb = NAK;  // skeep room controller screen if room thermostat config activ
                    else if (IsRoomCtrlConfig() && ((rsbuf[1] != 2U) || (rsbuf[2] ==  0U))) respb = NAK;  // skeep room thermostat screen if room controller config activ
                    else
                    {                                                   // room thermostat id = 1 , room controller id = 2
                        disp_img_id     = rsbuf[2];                     // display image number
                        disp_img_time   = rsbuf[3];                     // display image timeout
                        BUZZ_State      = (SIG_BUZZER_TypeDef)rsbuf[4]; // buzzer signal mode
                        DISPUpdateSet(); 
                        BTNOkReset();
                    }
                    break;
                }
                
                case RT_UPD_WFC:
                {
                    if (rs485_pak_dlen == WFC_DSIZE)
                    {
                        memcpy(wfc_buff, &rsbuf[1], WFC_DSIZE);
                        WFC_UpdateSet();
                    }
                    else respb = NAK;
                    break;
                }
                
                case RT_DISP_MSG:
                {
                    respb = DISPMessage(&rsbuf[1]);  // set response from display function
                    break;
                }
                               
                
                case GET_SYS_STAT:
                {                
                    CharToBinStr((char*)&rsbuf[7], (sysfl & 0xff));
                    resps = 9;                    
                    break;
                }
                
                case SET_APPL_STAT:
                {							
                    if (rsbuf[1] > 0xBU) respb = NAK;
                    else 
                    {
                        ROOM_Status = (ROOM_StatusTypeDef) rsbuf[1];
                        OWStatUpdSet();
                    }
                    break;						
                }
                
                
                case UPD_FW_INFO:
                {
                    appinf.ld_addr = FW_ADDR;
                    newinf.size    = ((rsbuf[1] <<24)|(rsbuf[2] <<16)|(rsbuf[3] <<8)|rsbuf[4]);
                    newinf.crc32   = ((rsbuf[5] <<24)|(rsbuf[6] <<16)|(rsbuf[7] <<8)|rsbuf[8]);
                    newinf.version = ((rsbuf[9] <<24)|(rsbuf[10]<<16)|(rsbuf[11]<<8)|rsbuf[12]);
                    newinf.wr_addr = ((rsbuf[13]<<24)|(rsbuf[14]<<16)|(rsbuf[15]<<8)|rsbuf[16]);
                    if      (ValidateFwInfo(&newinf))           ResetFwInfo(&newinf);
                    else if (GetFwInfo(&appinf))                ResetFwInfo(&appinf);
                    else if (!IsNewFwUpdate(&appinf, &newinf))  IsSYS_FwRqSet();
                    break;
                }
                
                case GET_APPL_STAT:
                {
                    mem_set(&rsbuf[7], 'X', 11U);
                    rsbuf[7] = TOCHAR(ROOM_Status);
                    if (ROOM_Status > 9) rsbuf[7] += 7U; // convert hex to char
                    if (IsTempSenDS18Active() || IsTempLuxRTActive() || IsNtcValidActiv()) 	
                    {
                        if (room_temp < 0) 
                        {
                            rsbuf[8] = '-';
                            Int2Str((char*)&rsbuf[9], (room_temp * -1), 3);
                        }
                        else 
                        {
                            rsbuf[8] = '+';
                            Int2Str((char*)&rsbuf[9], room_temp, 3);
                        }
                        if (IsTempRegHeating())     rsbuf[12] = 'H';
                        if (IsTempRegCooling())     rsbuf[12] = 'C';
                        if (IsTempRegActiv())       rsbuf[13] = 'E';
                        else                        rsbuf[13] = 'D';
                        if (IsTempRegEnabled())     rsbuf[14] = 'E';
                        else                        rsbuf[14] = 'D';                        
                        if (IsTempRegOutputActiv()) rsbuf[15] = 'E';
                        else                        rsbuf[15] = 'D';
                        if (!DOUT_HVACRequest())    rsbuf[16] = 'E';
                        else                        rsbuf[16] = 'D'; 
                        Int2Str((char*)&rsbuf[17], thst_sp,  2);
                        Int2Str((char*)&rsbuf[19], thst_dif, 2);
                    }
                    mem_set(&rsbuf[21], '0', 12);
                    if (IsHVACCtrlActiv())      rsbuf[21] = '1';
                    if (IsBTNDndActiv())        rsbuf[22] = '1';
                    if (IsBTNSosActiv())        rsbuf[23] = '1';
                    if (IsBTNMaidActiv())       rsbuf[24] = '1';
                    if (IsBTNOpenActiv())       rsbuf[25] = '1';
                    if (IsBTNOkActiv())         rsbuf[26] = '1';
                    if (IsNtcErrorActiv())      rsbuf[27] = '1';
                    if (IsOWStatUpdActiv())     rsbuf[28] = '1';
                    if (IsWFCValidActiv())      rsbuf[29] = '1';
                    if (IsRtcTimeValid())       rsbuf[30] = '1';
                    if (IsTSCleanActiv())       rsbuf[31] = '1';
                    if (IsBTNOkActiv())         rsbuf[32] = '1';
                                       
                    if  (IsRoomCtrlConfig())
                    {
                        rsbuf[33] = 'R';
                        rsbuf[34] = 'C';
                    }
                    else if (IsRoomThstConfig())
                    {
                        
                        rsbuf[33] = 'R';
                        rsbuf[34] = 'T';
                    }
                    rsbuf[41] = version>>16;
                    rsbuf[42] = version>> 8;
                    rsbuf[43] = version;
                    Hex2Str ((char*)&rsbuf[35], &rsbuf[41], 6);
                    EE_ReadBuffer  (&rsbuf[43], EE_BEDNG_REPL_ADD, 1);
                    Int2Str ((char*)&rsbuf[41], rsbuf[43], 2);
                    Int2Str ((char*)&rsbuf[43], disp_img_id, 2);
                    Int2Str ((char*)&rsbuf[45], disp_img_time,2);
                    Int2Str ((char*)&rsbuf[47], BUZZ_State,  2);   
                    Int2Str ((char*)&rsbuf[49], owifa,  2);
                    rsbuf[51] = 0U;
                    resps = 46U;
                    break;						
                }
                /**
                ***********************************************
                *   ALL COMMANDS REQUIRED I2C HAL DRIVER ACCESS
                *   WILL BE EXECUTED FROM MAIN LOOP TO PREVENT
                *   HARDWARE FAULT AND LOST OF DATA CAUSED BY
                *   RESTART TRIGGERED AFTER WATHDOG OVERFLOW  
                ***********************************************
                */
                case SET_ROOM_TEMP:
                case UPDATE_BLDR:
                case START_BLDR:
                case SET_BR2OW:
                case SET_BEDDING_REPL:
                case GET_LOG_LIST:
                case DEL_LOG_LIST:
                case SET_RS485_CFG:
                case SET_PERMITED_GROUP:
                case SET_DISPL_BCKLGHT:
                case SET_SYSTEM_ID:  
                case SET_RTC_DATE_TIME:
                case SET_DOUT_STATE:
                case RT_UPD_QRC:
                case RT_DISP_QRC:
                case SET_PASSWORD:
                default:
                {
                    return;
                }
            }
            if (COM_Link == P2P) RS485_Response(respb, resps);
            if (poscmd == 1) Restart();
            RS485_Init();
        }
	}
    else HAL_UART_Receive_IT(&huart1, &rec, 1);
}
/**
* @brief : all data send from buffer ?
* @param : what  should one to say   ? well done,   
* @retval: well done, and there will be more..
*/
void RS485_TxCpltCallback(void)
{
}
/**
* @brief : usart error occured during transfer
* @param : clear error flags and reinit usaart
* @retval: and wait for address mark from master 
*/
void RS485_ErrorCallback(void) 
{
    __HAL_UART_CLEAR_PEFLAG(&huart1);
    __HAL_UART_CLEAR_FEFLAG(&huart1);
    __HAL_UART_CLEAR_NEFLAG(&huart1);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    __HAL_UART_CLEAR_OREFLAG(&huart1);
	__HAL_UART_FLUSH_DRREGISTER(&huart1);
	huart1.ErrorCode = HAL_UART_ERROR_NONE;
	RS485_Init();
}

/**
* @brief : set usart for comunication ower rs485 interface, default to receive 9 bit, 115200bps
* @param : as in onewire interface, 9 bit will be used for addressing and 8 bit for all other 
* @retval: data and controll data exchange untill link is closed. data rate should be negotiable.
*        : and data controll dirrection should be mainteninh in hradware with GPIOA PIN 12
*/
void RS485_SetUsart(uint8_t brate, uint8_t bsize)
{
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    HAL_UART_DeInit(&huart1);
	huart1.Instance        		= USART1;
    huart1.Init.BaudRate        = bps[brate];
    if      (bsize == WL_9BIT)  huart1.Init.WordLength = UART_WORDLENGTH_9B;
	else if (bsize == WL_8BIT)  huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits   		= UART_STOPBITS_1;
	huart1.Init.Parity     		= UART_PARITY_NONE;
	huart1.Init.HwFlowCtl  		= UART_HWCONTROL_NONE;
	huart1.Init.Mode       		= UART_MODE_TX_RX;
	huart1.Init.OverSampling	= UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling  = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    HAL_NVIC_SetPriority(USART1_IRQn, 3, 0);
	HAL_NVIC_EnableIRQ(USART1_IRQn);
    
    if(bsize == WL_9BIT)
    {
        if (HAL_MultiProcessor_Init(&huart1, STX, UART_WAKEUPMETHOD_ADDRESSMARK) != HAL_OK) ErrorHandler(RS485_FUNC, USART_DRV);
        HAL_MultiProcessor_EnableMuteMode(&huart1);
        HAL_MultiProcessor_EnterMuteMode (&huart1);        
    }
    else  if (bsize == WL_8BIT)
    {
        if (HAL_UART_Init(&huart1) != HAL_OK)   ErrorHandler(RS485_FUNC, USART_DRV);
    }
}
/**
  * @brief
  * @param
  * @retval
  */
void RS485_Response(uint8_t resp, uint8_t size)
{
    uint32_t bc, pakcksm = 0U; 
    rsbuf[6] = rsbuf[0];
    rsbuf[5] =	size;
    rsbuf[4] = rsifa[1];
    rsbuf[3] = rsifa[0];
    rsbuf[2] = (rs485_sender_addr      & 0xFFU);
    rsbuf[1] = ((rs485_sender_addr>>8) & 0xFFU);
    rsbuf[0] = resp;

    for (bc = 6U; bc < (rsbuf[5]+6U); bc++)
    {
        pakcksm += rsbuf[bc];
    }
    rsbuf[rsbuf[5]+6] = ((pakcksm>>8) & 0xFFU);
    rsbuf[rsbuf[5]+7] =  (pakcksm     & 0xFFU);
    rsbuf[rsbuf[5]+8] = EOT;
    while ((HAL_GetTick() - rxtx_tmr) < RX2TX_DEL) 
    {
    }
    rxtx_tmr = 0U;
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
    if (HAL_UART_Transmit(&huart1, rsbuf, rsbuf[5]+9, RESP_TOUT) != HAL_OK) ErrorHandler(RS485_FUNC, USART_DRV);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
}
/**
  * @brief
  * @param
  * @retval
  */
uint8_t UpdateBootloader(void)
{
    
    uint8_t runfw = 0U;
    uint8_t bkpfw = 0U;
    uint8_t updfw = 0U;
	uint8_t newfw = 0U;
    FwInfoTypeDef RunFwInf;
    FwInfoTypeDef NewFwInf;
    FwInfoTypeDef BkpFwInf;
    ResetFwInfo(&RunFwInf);
    ResetFwInfo(&NewFwInf);
    ResetFwInfo(&BkpFwInf);
    RunFwInf.ld_addr = RT_BLDR_ADDR;
    NewFwInf.ld_addr = RT_NEW_FILE_ADDR;
    runfw = GetFwInfo (&RunFwInf);
    newfw = GetFwInfo (&NewFwInf);
    updfw = IsNewFwUpdate(&RunFwInf, &NewFwInf); // check is new file update
    if (!updfw) // all version info ok and new file is update
    {   // if fail to backup bootloader, write error log and abort update 
        if (FLASH2QSPI_Copy (RunFwInf.ld_addr, RT_BLDR_BKP_ADDR, RunFwInf.size) != QSPI_OK) 
        {   // if fail to backup firmware, write error log and abort update 
            MX_QSPI_Init(); // reinit interface again to 
            QSPI_MemMapMode(); // reinit qspi interface to execute sector erase command
            return(1U);
        }   // if backup copy succseed, write new bootloader
        else if (QSPI2FLASH_Copy (NewFwInf.ld_addr, NewFwInf.wr_addr, NewFwInf.size) != QSPI_OK)
        {
            MX_QSPI_Init(); // reinit interface again to 
            QSPI_MemMapMode(); // reinit qspi interface to execute sector erase command
            return(1U);
        }
    }
    /**
    *********************************************************************
    *   after previous function  erase and copy  check again 
    *   bootloader for error and try recovery from backup if eny
    *********************************************************************
    */
    ResetFwInfo(&RunFwInf);
    ResetFwInfo(&BkpFwInf);
    RunFwInf.ld_addr = RT_BLDR_ADDR;
    BkpFwInf.ld_addr = RT_BLDR_BKP_ADDR;
    runfw = GetFwInfo (&RunFwInf);
    bkpfw = GetFwInfo (&BkpFwInf);
    if(runfw) // if running bootloader not valid
    {   
        if (!bkpfw) // fail again, this is critical error, inform system controller and do not restart or power off 
        {   // before bootloader is recovered, it will fail to boot, leaving icsp programming as only option
            if (QSPI2FLASH_Copy (BkpFwInf.ld_addr, BkpFwInf.wr_addr, BkpFwInf.size) != QSPI_OK)
            {   // if fail to backup firmware, write error log and abort update 
                MX_QSPI_Init(); // reinit interface again to 
                QSPI_MemMapMode(); // reinit qspi interface to execute sector erase command
                return(1U);
            }
        }
        else return(1U);
    } 
	return (0U);
}
/************************ (C) COPYRIGHT JUBERA D.O.O Sarajevo ************************/
