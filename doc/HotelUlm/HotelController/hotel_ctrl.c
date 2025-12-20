/**
 ******************************************************************************
 * File Name          : hotel_controller.c
 * Date               : 21/08/2016 20:59:16
 * Description        : hotel control process service
 ******************************************************************************
 *
 *
 ******************************************************************************
 */
 
/* Includes ------------------------------------------------------------------*/
#include "ff.h"
#include "rtc.h"
#include "main.h"
#include "gpio.h"
#include "uart.h"
#include "i2cee.h"
#include "httpd.h"
#include "fs5206.h"
#include "buzzer.h"
#include "eth_bsp.h"
#include "netconf.h"
#include "netbios.h"
#include "display.h"
#include "wiegand.h"
#include "spi_flash.h"
#include "tftpserver.h"
#include "hotel_ctrl.h"
#include "stm32f4x7_eth.h"
#include "stm32f429i_lcd.h"

#if (__HOTEL_CTRL_H__ != FW_BUILD)
    #error "hotel controller header version mismatch"
#endif
/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
HC_UpdatePacketTypeDef      HC_FilUpdPck;
HC_UpdatePacketTypeDef      HC_FwrUpdPck;
HC_LogTransferTypeDef       HC_LogTransfer;
HC_LogTransferTypeDef       HTTP_LogTransfer;
HC_LogMemoryTypeDef         HC_LogMemory;
HC_StateTypeDef             HC_State;
HC_UpdateTypeDef            HC_Update = UPD_INIT;

extern GUI_HMEM hPROGBAR_FileTransfer;
/* Private variables ---------------------------------------------------------*/
__IO  uint32_t hc_flag;
__IO  uint32_t sys_rx_timer;
__IO  uint32_t fw_upd_tmr;
__IO  uint32_t sys_tftp_file;
__IO  uint32_t response_timer;
__IO uint32_t file_crc8;

__IO uint16_t system_id;
__IO uint16_t rsifa;
__IO uint16_t rsbra;
__IO uint16_t rsgra;
__IO uint16_t rs485_txaddr;
uint16_t *addr_list;
__IO uint16_t addr_list_cnt;
__IO uint16_t fwrupd_add_cnt;
__IO uint16_t addr_list_size;
__IO uint16_t sys_file_progress;
__IO uint16_t rs485_packet_lenght;
__IO uint16_t rs485_pkt_chksum;
uint16_t *fwrupd_add_list;
__IO uint16_t filupd_list_cnt;
uint16_t *imgupd_add_list;
__IO uint16_t fwrupd_addlist_dsize;
__IO uint16_t imgupd_addlist_dsize;
__IO uint16_t imgupd_addlist_cnt;
__IO uint16_t jrnl_buff[JRNL_BSIZE];

__IO uint8_t request;
__IO uint8_t ow_ifaddr;
__IO uint8_t ow_txaddr;
__IO uint8_t http_cmdsta;
__IO uint8_t rs485_bus_sta;
__IO uint8_t rsbps;
__IO uint8_t sys_journal_list_item;
__IO uint8_t sta_req_cnt;
__IO uint8_t sys_rtupd_list[RT_UPD_LIST_BSIZE];
uint8_t filupd_list[FILUPD_LIST_BSIZE];
char hc_buff[HC_BSIZE];
uint32_t fw_indx = 0;
FwInfoTypeDef fw_inf; // rcapp,rcbldr,rtapp,rtbldr,crapp,crbldr,hcapp,hcbldr
/* Private macro -------------------------------------------------------------*/
/* Function prototypes -------------------------------------------------------*/
uint16_t HC_GetNextAddr(void);
void HC_CreateLogRequest(void);
uint8_t HC_CheckNewImageFile(void);
void HC_CreateTimeUpdatePacket(void);
void HC_CreateRoomCtrlStatReq(void);
void HC_CreateRoomStatusRequest(void);
void HC_CreateFileUpdateRequest(void);
uint8_t HC_CheckNewFirmwareFile(void);
void HC_CreateFirmwareUpdateRequest(void);
/* Private functions ---------------------------------------------------------*/
/**
  * @brief
  * @param
  * @retval
  */
void HC_Init(void)
{
    uint32_t temp_log_list_scan;
    
    RS485_MODE(RS485_RX);
    
    if      (HC_CreateAddrList() == FS_FILE_OK)     // load address list from spi flash memory 
    {                                               // this list is copied from uSD card text file CTRL_ADD.TXT.
        rs485_bus_sta = RS485_BUS_DISCONNECTED;     // If list successfully created, start polling status
    }                                                   
    else if (HC_LoadAddrList() == FS_FILE_OK)       // if address list file corrupted try to copy address list 
    {                                               // from micro SD card file. If file successfully copyed
        if(HC_CreateAddrList() == FS_FILE_OK)       // Try again load address list from newly created flash file
        {
            rs485_bus_sta = RS485_BUS_DISCONNECTED; // if all ok. continue to polling stage
        }
        else rs485_bus_sta = RS485_BUS_ERROR;       // another error display status, and write error log
    }
    else    rs485_bus_sta = RS485_BUS_ERROR;        // if micro SD file missing. write error log and display status
    
    COM_Link = NOLINK;
    COM_Bridge = BRNONE;
    ow_ifaddr = DEF_HC_OWIFA;   // set hotel controller onewire interface address to default
    ow_txaddr = DEF_RT_OWIFA;   // set room thermostat onewire interface address to first thermostat 
    HC_State = PCK_ENUM;
    HC_Update = UPD_IDLE;
    HC_LogTransfer.state = TRANSFER_IDLE;
    HC_LogMemory.Allocation = TYPE_ERROR;
    /**
    *    TYPE_1 -> log list is empty and next log address is first address
    *    0000000000000000000000000000000000000000000000000000000000000000000000000
    *
    *    TYPE_2 -> log list start at some addres, it's full till last address, next log address is first address and is free for write 
    *    000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    *
    *     TYPE_3 -> log list start at some addres, end at upper address, next log address is upper address and is free for write
    *    000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxx000000000000000000000000000
    *
    *    TYPE_4 -> log list start at first address, end at last address, it's full, next log address is first memory address, write is forbiden
    *    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    *    
    *    TYPE_5 -> log list start at first address, end at upper address, and next upper log address is free for write
    *    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00000000000000000000000000000000000000000
    *
    *    TYPE_6 -> log list start at upper address, end at lower address and next upper from end address is free for write
    *    xxxxxxxxxxxx0000000000000000000000000000000000000000000000xxxxxxxxxxxxxxx
    */    
    ZEROFILL(eebuff, COUNTOF(eebuff));
    temp_log_list_scan = EE_LOG_LIST_START_ADD;
    I2CEE_ReadBytes16(I2CEE_PAGE_0, temp_log_list_scan, eebuff, LOG_DSIZE);
    /* CHECK FOR TYPE_1 */
    if((eebuff[0] == 0x0U) && (eebuff[1] == 0x0U)) 
    {
        HC_LogMemory.Allocation    = TYPE_1;
        HC_LogMemory.first_addr     = EE_LOG_LIST_START_ADD;
        HC_LogMemory.last_addr      = EE_LOG_LIST_START_ADD;
        HC_LogMemory.next_addr      = EE_LOG_LIST_START_ADD;
        
        temp_log_list_scan += LOG_DSIZE;
        I2CEE_ReadBytes16(I2CEE_PAGE_0, temp_log_list_scan, eebuff, LOG_DSIZE);
        
        /* CHECK FOR TYPE_2 */
        while(temp_log_list_scan <= (EE_LOG_LIST_END_ADD - LOG_DSIZE))
        {
#ifdef USE_WATCHDOG
            IWDG_ReloadCounter();
#endif
            if((eebuff[0] != 0x0U) || (eebuff[1] != 0x0U))
            {
                HC_LogMemory.Allocation = TYPE_2;
                HC_LogMemory.first_addr = temp_log_list_scan;
                HC_LogMemory.last_addr  = (EE_LOG_LIST_END_ADD - LOG_DSIZE);
                ++HC_LogMemory.log_cnt;
                break;
            }
            else
            {
                temp_log_list_scan += LOG_DSIZE;
                
                if(temp_log_list_scan < I2CEE_PAGE_SIZE)
                {
                    I2CEE_ReadBytes16(I2CEE_PAGE_0, temp_log_list_scan, eebuff, LOG_DSIZE);
                }
                else
                {
                    I2CEE_ReadBytes16(I2CEE_PAGE_1, temp_log_list_scan, eebuff, LOG_DSIZE);
                }
            }
        }
        /* CHECK FOR TYPE_3 */
        if(HC_LogMemory.Allocation == TYPE_2)
        {
            temp_log_list_scan += LOG_DSIZE;
            
            if(temp_log_list_scan <= (EE_LOG_LIST_END_ADD - LOG_DSIZE))
            {
                if(temp_log_list_scan < I2CEE_PAGE_SIZE)
                {
                    I2CEE_ReadBytes16(I2CEE_PAGE_0, temp_log_list_scan, eebuff, LOG_DSIZE);
                }
                else
                {
                    I2CEE_ReadBytes16(I2CEE_PAGE_1, temp_log_list_scan, eebuff, LOG_DSIZE);
                }
                
                while(temp_log_list_scan <= (EE_LOG_LIST_END_ADD - LOG_DSIZE))
                {    
#ifdef USE_WATCHDOG
            IWDG_ReloadCounter();
#endif
                    if((eebuff[0] == 0x0U) && (eebuff[1] == 0x0U))
                    {
                        HC_LogMemory.Allocation = TYPE_3;
                        HC_LogMemory.last_addr  = temp_log_list_scan - LOG_DSIZE;
                        HC_LogMemory.next_addr  = temp_log_list_scan;
                        break;
                    }
                    else
                    {    
                        temp_log_list_scan += LOG_DSIZE;
                        ++HC_LogMemory.log_cnt;
                        
                        if(temp_log_list_scan < I2CEE_PAGE_SIZE)
                        {
                            I2CEE_ReadBytes16(I2CEE_PAGE_0, temp_log_list_scan, eebuff, LOG_DSIZE);
                        }
                        else
                        {
                            I2CEE_ReadBytes16(I2CEE_PAGE_1, temp_log_list_scan, eebuff, LOG_DSIZE);
                        }
                    }
                }
            }
        }
    }
    /* CHECK FOR TYPE_4 */
    else if((eebuff[0] != 0x0U) || (eebuff[1] != 0x0U))
    {
        HC_LogMemory.Allocation = TYPE_4;
        HC_LogMemory.first_addr = EE_LOG_LIST_START_ADD;
        HC_LogMemory.last_addr  = EE_LOG_LIST_END_ADD - LOG_DSIZE;
        HC_LogMemory.next_addr  = EE_LOG_LIST_START_ADD;
        ++HC_LogMemory.log_cnt;
        
        temp_log_list_scan += LOG_DSIZE;
        I2CEE_ReadBytes16(I2CEE_PAGE_0, temp_log_list_scan, eebuff, LOG_DSIZE);
        
        /* CHECK FOR TYPE_5 */
        while(temp_log_list_scan <= (EE_LOG_LIST_END_ADD - LOG_DSIZE))
        {    
#ifdef USE_WATCHDOG
            IWDG_ReloadCounter();
#endif
            if((eebuff[0] == 0x0U) && (eebuff[1] == 0x0U))
            {
                HC_LogMemory.Allocation = TYPE_5;
                HC_LogMemory.last_addr  = temp_log_list_scan - LOG_DSIZE;
                HC_LogMemory.next_addr  = temp_log_list_scan;
                break;
            }
            else
            {
                temp_log_list_scan += LOG_DSIZE;
                ++HC_LogMemory.log_cnt;

                if(temp_log_list_scan < I2CEE_PAGE_SIZE)
                {
                    I2CEE_ReadBytes16(I2CEE_PAGE_0, temp_log_list_scan, eebuff, LOG_DSIZE);
                }
                else
                {
                    I2CEE_ReadBytes16(I2CEE_PAGE_1, temp_log_list_scan, eebuff, LOG_DSIZE);
                }
            }
        }
        /* CHECK FOR TYPE_6 */
        if(HC_LogMemory.Allocation == TYPE_5)
        {
            temp_log_list_scan += LOG_DSIZE;
            
            if(temp_log_list_scan <= (EE_LOG_LIST_END_ADD - LOG_DSIZE))
            {
                if(temp_log_list_scan < I2CEE_PAGE_SIZE)
                {
                    I2CEE_ReadBytes16(I2CEE_PAGE_0, temp_log_list_scan, eebuff, LOG_DSIZE);
                }
                else
                {
                    I2CEE_ReadBytes16(I2CEE_PAGE_1, temp_log_list_scan, eebuff, LOG_DSIZE);
                }
#ifdef USE_WATCHDOG
            IWDG_ReloadCounter();
#endif
                while(temp_log_list_scan <= (EE_LOG_LIST_END_ADD - LOG_DSIZE))
                {    
                    if((eebuff[0] != 0x0U) || (eebuff[1] != 0x0U))
                    {
                        HC_LogMemory.Allocation = TYPE_6;
                        HC_LogMemory.first_addr = temp_log_list_scan;
                        HC_LogMemory.log_cnt += ((EE_LOG_LIST_END_ADD - temp_log_list_scan) / LOG_DSIZE);
                        break;
                    }
                    else
                    {    
                        temp_log_list_scan += LOG_DSIZE;
                        
                        if(temp_log_list_scan < I2CEE_PAGE_SIZE)
                        {
                            I2CEE_ReadBytes16(I2CEE_PAGE_0, temp_log_list_scan, eebuff, LOG_DSIZE);
                        }
                        else
                        {
                            I2CEE_ReadBytes16(I2CEE_PAGE_1, temp_log_list_scan, eebuff, LOG_DSIZE);
                        }
                    }                    
                }
            }
        }
    }
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_Service(void)
{
    uint8_t j, t;
    uint8_t fw_stat;
    uint8_t fl_stat;
    uint32_t bcnt = 0;
    uint32_t wf_unix = 0;
    static uint32_t rx_tmr = 0;
    static uint32_t rx_tout = 0;
    static uint8_t old_rxbcnt = 0;
    static uint32_t fw_upd_tmr = 0;
    static uint32_t fw_update_tout = 0;
    static uint32_t time_update_timer = 0;
    static uint32_t wforecast_update_timer = 0;
    
    switch (HC_State)
    {
        case PCK_ENUM:
        {
            if      ((HC_FwrUpdPck.state == FW_UPDATE_BLDR) || (HC_FwrUpdPck.state == FW_UPDATE_RUN))
            {
                if ((HC_FwrUpdPck.trial < 2) || (HC_FwrUpdPck.state == FW_UPDATE_BLDR))
                {
                    HC_CreateFirmwareUpdateRequest();
                    HC_State = PCK_SEND;
                    HC_Update = UPD_BINARY;
                }
                else if(HC_FwrUpdPck.trial >= MAXREP_CNT)
                {
                    HC_WriteLogEvent(FW_UPD_FAIL, DEV_NOT_RESP);                    
                    HC_CreateFirmwareUpdateRequest();
                    HC_State = PCK_ENUM;
                    HC_Update = UPD_IDLE;
                    DISP_ProgbarSetNewState(0);
                }
                else
                {
                    HC_State = PCK_SEND;
                    HC_Update = UPD_BINARY;
                }
            }
            else if ((HC_FwrUpdPck.state == FW_UPDATE_FAIL) || (HC_FwrUpdPck.state == FW_UPDATE_END))
            {    
                if (HC_FwrUpdPck.state == FW_UPDATE_FAIL)
                {
                    HC_WriteLogEvent(FW_UPD_FAIL, DEV_NOT_RESP);
                    HC_CreateFirmwareUpdateRequest();
                    HC_FwrUpdPck.state = 0x0U;
                    HC_Update = UPD_IDLE;
                }
                else if (HC_FwrUpdPck.state == FW_UPDATE_END)
                {    
                    HC_WriteLogEvent(FW_UPDATED, UPDATE_FWR);
                    HC_CreateCmdRequest(APP_EXE, hc_buff);
                    HC_State = PCK_SEND;
                    HC_Update = UPD_BINARY;
                }                
                DISP_ProgbarSetNewState(0);
                break;
            }
            else if ((HC_LogTransfer.state == TRANSFER_QUERY_LIST) || (HC_LogTransfer.state == TRANSFER_DELETE_LOG))
            {
                if (HC_LogTransfer.trial == 0x0U)
                {
                    HC_LogTransfer.trial = 0x1U;
                    HC_LogTransfer.last_trial = 0x1U;
                }
                else if ((HC_LogTransfer.state == TRANSFER_QUERY_LIST) && (HC_LogTransfer.trial == HC_LogTransfer.last_trial))
                {
                    HC_LogTransfer.state = TRANSFER_DELETE_LOG;
                    HC_LogTransfer.trial = 0x1U;
                    HC_LogTransfer.last_trial = 0x1U;
                }
                else if ((HC_LogTransfer.state == TRANSFER_DELETE_LOG) && (HC_LogTransfer.trial == HC_LogTransfer.last_trial))
                {
                    HC_LogTransfer.state = TRANSFER_QUERY_LIST;
                    HC_LogTransfer.trial = 0x1U;
                    HC_LogTransfer.last_trial = 0x1U;
                }
                else
                {
                    HC_LogTransfer.last_trial = HC_LogTransfer.trial;
                }
                HC_CreateLogRequest();
                HC_Update = UPD_LOG;
                HC_State = PCK_SEND;
            }
            else if ((HC_FilUpdPck.state >= DWNLD_DISP_IMG_1) && (HC_FilUpdPck.state <= DWNLD_DISP_IMG_25))
            {
                if(HC_FilUpdPck.trial < 2) 
                {
                    HC_CreateFileUpdateRequest();
                    HC_State = PCK_SEND;
                    HC_Update = UPD_FILE;
                }
                else if(HC_FilUpdPck.trial >= MAXREP_CNT) 
                {
                    if      (HC_FwrUpdPck.cmd == UPDATE_FWR)        HC_WriteLogEvent(FW_UPD_FAIL,   DEV_NOT_RESP);
                    else if (HC_FwrUpdPck.cmd == UPDATE_BLDR)       HC_WriteLogEvent(BLDR_UPD_FAIL, DEV_NOT_RESP);
                    else if (HC_FilUpdPck.cmd == DWNLD_DISP_IMG)    HC_WriteLogEvent(IMG_UPD_FAIL,  DEV_NOT_RESP);
                    HC_CreateFileUpdateRequest();
                    HC_State = PCK_ENUM;
                    HC_Update = UPD_IDLE;
                    DISP_ProgbarSetNewState(0);
                    
                    if ((HC_FwrUpdPck.cmd == UPDATE_FWR) || (HC_FwrUpdPck.cmd == UPDATE_BLDR))
                    {
                        ++fwrupd_add_cnt;
                        if (!fwrupd_add_list[fwrupd_add_cnt]) HC_FwrUpdPck.cmd = 0; 
                        else
                        {
                            request = DWNLD_DISP_IMG;
                            return;
                        }                        
                    }
                    else if(HC_FilUpdPck.cmd == DWNLD_DISP_IMG)
                    {
                        ++filupd_list_cnt;
                                
                        if (!filupd_list[filupd_list_cnt])
                        {
                            filupd_list_cnt = 0;
                            ++imgupd_addlist_cnt;
                
                            if (!imgupd_add_list[imgupd_addlist_cnt]) 
                            {
                                HC_FwrUpdPck.cmd = 0;
                                DISP_ProgbarSetNewState(0);
                            }
                            else
                            {
                                request = DWNLD_DISP_IMG;
                                return;
                            }
                        }
                        else 
                        {
                            request = DWNLD_DISP_IMG;
                            return;
                        }
                    }
                }
                else
                {
                    HC_State = PCK_SEND;
                    HC_Update = UPD_FILE;
                }                
            }
            else if (request)
            {
                if      (request == UPDATE_FWR)
                {
                    /* check for new firmware file on uSD card*/
                    fw_stat = HC_CheckNewFirmwareFile();
                    
                    if (fw_stat == FS_FILE_OK)
                    {
                        DISP_FileTransferState(UPD_BINARY);
                        DISP_uSDCardSetNewState(1);
                        HC_CreateFirmwareUpdateRequest();
                        HC_State = PCK_SEND;
                        HC_Update = UPD_BINARY;
                        RC_JournalUpdateReset();
                        RC_StatusUpdateReset();
                        RC_StatusRequestReset();
                    }
                    else
                    {
                        if      (HC_FwrUpdPck.cmd == UPDATE_FWR)        HC_WriteLogEvent(FW_UPD_FAIL,   fl_stat);
                        else if (HC_FwrUpdPck.cmd == UPDATE_BLDR)       HC_WriteLogEvent(BLDR_UPD_FAIL, fl_stat);
                        else if (HC_FilUpdPck.cmd == DWNLD_DISP_IMG)    HC_WriteLogEvent(IMG_UPD_FAIL,  fl_stat);
                        DISP_FileTransferState(fl_stat);
                        DISP_ProgbarSetNewState(0);
                        DISP_uSDCardSetNewState(0);
                        HC_FwrUpdPck.cmd = 0;
                        HC_FilUpdPck.cmd = 0;
                        HC_FilUpdPck.state = 0;
                        HC_Update = UPD_IDLE;
                        request = 0;
                    }
                }
                else if((request == GET_LOG_LIST) || (request == DEL_LOG_LIST))
                {
                    HC_Update = UPD_IDLE;
                }
                else if (request == DWNLD_DISP_IMG)
                {
                    if      ((HC_FwrUpdPck.cmd == UPDATE_FWR)     || (HC_FwrUpdPck.cmd == UPDATE_BLDR))
                    {
                        ZEROFILL(hc_buff, COUNTOF(hc_buff));
                        Int2Str(hc_buff, fwrupd_add_list[fwrupd_add_cnt], 0);
                        t = strlen(hc_buff)+1;
                        Int2Str(&hc_buff[t], filupd_list[filupd_list_cnt], 0);
                    }
                    else if ((HC_FilUpdPck.cmd == DWNLD_DISP_IMG) || (HC_FilUpdPck.cmd == RT_DWNLD_LOGO))
                    {
                        ZEROFILL(hc_buff, COUNTOF(hc_buff));
                        Int2Str(hc_buff, imgupd_add_list[imgupd_addlist_cnt], 0);
                        t = strlen(hc_buff)+1;
                        Int2Str(&hc_buff[t], filupd_list[filupd_list_cnt], 0);
                    }
                    
                    fl_stat = HC_CheckNewImageFile();
                    
                    if      (fl_stat == FS_FILE_OK)
                    {
                        DISP_FileTransferState(UPD_FILE);
                        DISP_uSDCardSetNewState(1);
                        HC_CreateFileUpdateRequest();
                        HC_State = PCK_SEND;
                        HC_Update = UPD_FILE;
                        RC_JournalUpdateReset();
                        RC_StatusUpdateReset();
                        RC_StatusRequestReset();
                    }
                    else
                    {
                        if      (HC_FwrUpdPck.cmd == UPDATE_FWR)        HC_WriteLogEvent(FW_UPD_FAIL,   fl_stat);
                        else if (HC_FwrUpdPck.cmd == UPDATE_BLDR)       HC_WriteLogEvent(BLDR_UPD_FAIL, fl_stat);
                        else if (HC_FilUpdPck.cmd == DWNLD_DISP_IMG)    HC_WriteLogEvent(IMG_UPD_FAIL,  fl_stat);
                        DISP_FileTransferState(fl_stat);
                        DISP_ProgbarSetNewState(0);
                        DISP_uSDCardSetNewState(0);
                        HC_Update = UPD_IDLE;
                        HC_FwrUpdPck.cmd = 0;
                        HC_FilUpdPck.cmd = 0;
                        HC_FilUpdPck.state=0;
                        request = 0;
                    }
                }
                else
                {
                    /**
                    *    prepare requested command packet in RS485 tx buffer
                    */
                    if (!HC_FwrUpdPck.cmd) 
                    {
                        rs485_txaddr = atoi(hc_buff);
                    }                    
                    HC_CreateCmdRequest(request, hc_buff);
                    HC_Update = UPD_HTTP_RQ;
                    HC_State = PCK_SEND;
                }                
                request = 0;
            }            
            else if (IsRC_StatusUpdated() && IsRC_StatusRequested())
            {
                if(++sta_req_cnt >= TXREP_CNT)
                {
                    RC_StatusRequestReset();
                    RC_StatusUpdateReset();
                }
                else
                {
                    HC_CreateRoomStatusRequest();
                    HC_Update = UPD_ROOM_STAT;
                    HC_State = PCK_SEND;
                }
            }
            else if (IsRC_JournalUpdateActiv())
            {
                RC_StatusRequestReset();
                RC_StatusUpdateReset();
                RC_JournalUpdateReset();
                SendFwInfo();
                ZEROFILL(hc_buff, COUNTOF(hc_buff));
                rs485_txaddr = rsbra;
                request = DWNLD_JRNL;                
                j = 0;
                t = 0;
                
                while(j < 48)
                {
                    Int2Str(&hc_buff[t],jrnl_buff[j], 0);
                    while(hc_buff[t]) ++t;
                    hc_buff[t] = ',';
                    ++j;
                    ++t;
                }                
                hc_buff[t-1] = ';';
                HC_CreateCmdRequest(request, hc_buff);
                HC_State = PCK_SEND;
            }
            else if ((Get_SysTick() - time_update_timer) >= RTC_UPD_TIME)
            {
                time_update_timer = Get_SysTick();
                HC_CreateTimeUpdatePacket();
                HC_Update = UPD_TIME;
                HC_State = PCK_SEND;
            }
            else if ((Get_SysTick() - wforecast_update_timer) >= WFC_UPD_TIME)
            {
                wforecast_update_timer = Get_SysTick();
                if (RTC_State != RTC_VALID) break;
                if (I2CEE_ReadBytes16(I2CEE_PAGE_0, EE_FORECAST_ADD, eebuff, WFC_DSIZE)) ErrorHandler(HOTEL_CTRL_FUNC, I2C_DRV);
                wf_unix = (eebuff[0]<<24)|(eebuff[1]<<16)|(eebuff[2]<<8)|eebuff[3];
                if (wf_unix && (wf_unix != 0xFFFFFFFF))
                {
                    uint32_t unix = rtc2unix(&rtc_time, &rtc_date);
                    if((unix < (wf_unix + SECONDS_PER_DAY)) && (wf_unix < (unix + SECONDS_PER_DAY)))
                    {
                        COM_Bridge = BR2OW;
                        rs485_txaddr = rsbra;
                        ZEROFILL(hc_buff, COUNTOF(hc_buff));
                        mem_cpy(hc_buff, eebuff, WFC_DSIZE);
                        HC_CreateCmdRequest(RT_UPD_WFC, hc_buff);
                        HC_Update = UPD_IDLE;
                        HC_State  = PCK_SEND;
                        COM_Bridge = BRNONE;
                    }                    
                }
            }
            else 
            {
                rs485_txaddr = HC_GetNextAddr();
                HC_CreateRoomCtrlStatReq();
                HC_Update = UPD_RC_STAT;
                HC_State = PCK_SEND;
            }            
            break;
        }
        
        case PCK_SEND:
        {    
            if ((Get_SysTick() - rx_tmr) >= rx_tout)
            {
                ZEROFILL(rx_buff, COUNTOF(rx_buff));
                RS485_Send (tx_buff, (tx_buff[5]+9));
                HC_State = PCK_RECEIVING;
                rx_tmr = Get_SysTick();
                rx_tout = RESP_TOUT;
                old_rxbcnt = 0;
                rxbcnt = 0;
            
                switch (HC_Update)
                {
                    case UPD_LOG:
                    case UPD_RC_STAT:    
                    case UPD_ROOM_STAT:
                        break; 
                    case UPD_BINARY:
                        if      (HC_FwrUpdPck.state == FW_UPDATE_RUN)
                        {
                            if(HC_FwrUpdPck.pck_send == 0x0U)
                            {
                                fw_upd_tmr = Get_SysTick();
                                fw_update_tout = FWR_UPLD_DEL;
                            }
                        }
                        else if (HC_FwrUpdPck.state == FW_UPDATE_BLDR)
                        {
                            fw_upd_tmr = Get_SysTick();
                            fw_update_tout = BLDR_START_DEL;
                        }
                        else if (HC_FwrUpdPck.state == FW_UPDATE_END)
                        {
                            fw_upd_tmr = Get_SysTick();
                            fw_update_tout = APP_START_DEL;
                        }
                        break;
                    case UPD_FILE:
                        rx_tout = BIN_TOUT;
                        if (COM_Link == GROUP) HC_State = PCK_RECEIVED; // no response from group address
                        /* first request for file transfer will */
                        /* execute erase flash sectors to store  */
                        /* new file, this erase need some time */
                        if ((HC_FwrUpdPck.cmd == UPDATE_FWR) 
                        ||  (HC_FwrUpdPck.cmd == UPDATE_BLDR))
                        {  
                            if ((HC_FilUpdPck.pck_send == 0x0U) 
                            ||  (HC_FilUpdPck.pck_send == HC_FilUpdPck.pck_total)) rx_tout = IMG_COPY_DEL; // 4567U wait timeout
                        }
                        if (HC_FilUpdPck.cmd == DWNLD_DISP_IMG)
                        {
                            if ((HC_FilUpdPck.pck_send == 0U)                       // delay to format ext. flash temp storage for new file
                            ||  (HC_FilUpdPck.pck_send == HC_FilUpdPck.pck_total))  // delay to copy file from temp storage to image address
                            {
                                rx_tout = FWR_COPY_DEL; // 1567U wait timeout
                            }
                        }
                        break;
                    case UPD_HTTP_RQ:
                        if ((HC_FwrUpdPck.cmd == UPDATE_FWR) || (HC_FwrUpdPck.cmd == UPDATE_BLDR))
                        {                        
                            if      (HC_FwrUpdPck.cmd == UPDATE_FWR) 
                            {
                                HC_WriteLogEvent(IMG_UPDATED, filupd_list[filupd_list_cnt]);
                                DISP_FileTransferState(FW_UPDATE_END);
                            }
                            else if (HC_FwrUpdPck.cmd == UPDATE_BLDR) 
                            {
                                HC_WriteLogEvent(IMG_UPDATED, filupd_list[filupd_list_cnt]);
                                DISP_FileTransferState(BLDR_UPDATED);
                            }
                            
                            ++fwrupd_add_cnt;
                            if (fwrupd_add_list[fwrupd_add_cnt] == 0x0U) 
                            {
                                rx_tout = FWR_COPY_DEL;
                                HC_FwrUpdPck.cmd = 0x0U;
                                COM_Link = NOLINK;
                                COM_Bridge = BRNONE;
                            }
                            else
                            {
                                rx_tout = APP_START_DEL;
                                request = DWNLD_DISP_IMG;
                                HC_State = PCK_ENUM;
                                return;
                            }                        
                        }
                        break;
                    case UPD_TIME:    
                    case UPD_IDLE:
                    default:
                        rx_tout = RX2TX_DEL;
                        HC_State = PCK_ENUM;    
                        break;
                }
            }
            break;
        }
        
        
        case PCK_RECEIVING:
        {
            if (rxbcnt > 9)    // search receiver buffer for valid packet
            {
                bcnt = 0x0U; 
                while (bcnt < (rxbcnt-9))
                {
                    if ((rx_buff[bcnt]      <= US)                              // first packet byte is ascii controll character
                    &&  (rx_buff[bcnt+1] == ((rsifa >> 8)       & 0xFFU)) // second is rs485 interface address msb 
                    &&  (rx_buff[bcnt+2] ==  (rsifa             & 0xFFU))        // third is rs485 interface address lsb
                    &&  (rx_buff[bcnt+3] == ((rs485_txaddr>>8)  & 0xFFU)) // second is receiver interface address msb 
                    &&  (rx_buff[bcnt+4] ==  (rs485_txaddr      & 0xFFU))        // third is receiver interface address lsb
                    &&  (rx_buff[rx_buff[bcnt+5]+8] == EOT))          // last packet byte is ascii controll "end of transmission"
                    {
                        rs485_pkt_chksum = 0x0U;
                        for (j = bcnt+6; j < (rx_buff[bcnt+5]+6); j++)
                        {
                            rs485_pkt_chksum += rx_buff[j];
                        }

                        if ((rx_buff[rx_buff[bcnt+5]+6] == ((rs485_pkt_chksum >> 8) & 0xFFU)) 
                        &&  (rx_buff[rx_buff[bcnt+5]+7] ==  (rs485_pkt_chksum       & 0xFFU)))
                        {
                            HC_State = PCK_RECEIVED;
                            bcnt = rxbcnt;
                        }
                        else ++bcnt;
                    }
                    else ++bcnt;
                }                
            }
            
            if (old_rxbcnt != rxbcnt) // restart byte receive timeout timer
            {
                old_rxbcnt = rxbcnt;
                rx_tmr = Get_SysTick();
                rx_tout = RX_TOUT; // rx_tout = RESP_TOUT;
            }
            else if (((Get_SysTick() - rx_tmr) >= rx_tout) && ((Get_SysTick() - fw_upd_tmr) >= fw_update_tout)) 
            {
                if      (HC_FwrUpdPck.state == FW_UPDATE_BLDR)
                {
                    HC_FwrUpdPck.state = FW_UPDATE_FAIL;
                    DISP_FileTransferState(FW_UPDATE_FAIL);
                    DISP_ProgbarSetNewState(0);
                }
                else if (HC_FwrUpdPck.state == FW_UPDATE_RUN)
                {
                    if(HC_FwrUpdPck.pck_send == 0x0U)
                    {
                        HC_FwrUpdPck.state = FW_UPDATE_BLDR;
                    }
                    ++HC_FwrUpdPck.trial;
                }
                else if (HC_FwrUpdPck.state == FW_UPDATE_END)
                {
                    HC_FwrUpdPck.state = 0x0U;
                }
                else if ((HC_LogTransfer.state == TRANSFER_QUERY_LIST) || (HC_LogTransfer.state == TRANSFER_DELETE_LOG))
                {
                    ++HC_LogTransfer.trial;
                }
                else if ((HC_FilUpdPck.state >= DWNLD_DISP_IMG_1) && (HC_FilUpdPck.state <= DWNLD_DISP_IMG_25))
                {
                    ++HC_FilUpdPck.trial;
                    if (rx_buff[0] == ACK)              // support for old firmware
                    {                                   // response with ack only
                        HC_State = PCK_RECEIVED;        // shold be removed in future
//                        rx_tmr = Get_SysTick();
//                        rx_tout = RX2TX_DEL;
//                        return;                         
                    }                                  
                }
                else if (http_cmdsta == HTTP_GET_ROOM_STAT)  http_cmdsta = 0;
                rx_tmr = Get_SysTick();
                rx_tout = RX2TX_DEL;
                HC_State = PCK_ENUM;
            }
            break;
        }
        
        
        case PCK_RECEIVED:
        {
            if((Get_SysTick() - rx_tmr) >= rx_tout)
            {
                switch (HC_Update)
                {
                    case UPD_RC_STAT:
                    {
                        if (rx_buff[0] == ACK)
                        {
                            if      (rx_buff[13] == '1')
                            {
                                HC_FwrUpdPck.cmd = 0;
                                request = DWNLD_DISP_IMG;
                                HC_FilUpdPck.cmd = DWNLD_DISP_IMG;
                                HC_LogTransfer.state = 0;
                                imgupd_addlist_cnt = 0;
                                imgupd_add_list[imgupd_addlist_cnt] = rs485_txaddr;
                                ++imgupd_addlist_cnt;
                                imgupd_add_list[imgupd_addlist_cnt] = 0;
                                imgupd_addlist_cnt = 0;
                                filupd_list_cnt = 0;
                                j = 1;
                                while(j <= 14) filupd_list[filupd_list_cnt++] = j++;
                                filupd_list[filupd_list_cnt] = 0;
                                filupd_list_cnt = 0;                        
                            }
                            else if (rx_buff[14] == '1')
                            {
                                HC_FilUpdPck.cmd = 0;
                                request = DWNLD_DISP_IMG;
                                HC_FwrUpdPck.cmd = UPDATE_FWR;
                                filupd_list[0] = 20;
                                filupd_list[1] = 0;
                                filupd_list_cnt = 0;
                                fwrupd_add_cnt = 0;
                                fwrupd_add_list[fwrupd_add_cnt] = rs485_txaddr;
                                ++fwrupd_add_cnt;
                                fwrupd_add_list[fwrupd_add_cnt] = 0;
                                fwrupd_add_cnt = 0;    
                            }
                            else if ((rx_buff[7] == '1') || (rx_buff[8] == '1'))
                            {
                                HC_LogTransfer.state = TRANSFER_QUERY_LIST;
                                HC_LogTransfer.trial = 0;
                            }
                            else
                            {
                                /**
                                *   addressed controller responded, check for room status update activ
                                *   and if so, enable next room status request
                                */
                                if(IsRC_StatusUpdated() && !IsRC_StatusRequested())
                                {
                                    RC_StatusRequestSet();
                                    sta_req_cnt = 0;
                                }
                            } 
                        }                    
                        break;
                    }
                    
                    
                    case UPD_ROOM_STAT:
                    {
                        if ((rx_buff[0] == ACK) && (rx_buff[6] == GET_APPL_STAT))
                        {
                            RC_StatusRequestReset();
                            sys_journal_list_item = 0;
                            
                            while(rs485_txaddr > 199)
                            {
                                rs485_txaddr -= 100;
                                ++sys_journal_list_item;
                            }
                            
                            switch(rx_buff[7])    // room status
                            {
                                case ROOM_CLEANING_REQ:
                                    jrnl_buff[(sys_journal_list_item*4)]    += 1;
                                    jrnl_buff[(sys_journal_list_item*4)+3]  += 1;
                                    jrnl_buff[20] += 1;
                                    jrnl_buff[23] += 1;
                                    jrnl_buff[44] += 1;
                                    jrnl_buff[47] += 1;
                                    break;                            
                                case ROOM_BEDDING_REQ:
                                    jrnl_buff[(sys_journal_list_item*4)+1] += 1;
                                    jrnl_buff[(sys_journal_list_item*4)+3] += 1;
                                    jrnl_buff[21] += 1;
                                    jrnl_buff[23] += 1;
                                    jrnl_buff[45] += 1;
                                    jrnl_buff[47] += 1;
                                    break;                            
                                case ROOM_GENERAL_REQ:
                                    jrnl_buff[(sys_journal_list_item*4)+2] += 1;
                                    jrnl_buff[(sys_journal_list_item*4)+3] += 1;
                                    jrnl_buff[22] += 1;
                                    jrnl_buff[23] += 1;
                                    jrnl_buff[46] += 1;
                                    jrnl_buff[47] += 1;
                                    break;                            
                                default:
                                    break;
                            }
                        }
                        break;
                    }
                    
                    case UPD_BINARY:
                    {
                        if      (rx_buff[0] == ACK)
                        {
                            if (HC_FwrUpdPck.pck_send == HC_FwrUpdPck.pck_total)
                            {
                                HC_FwrUpdPck.state = FW_UPDATE_END;
                                DISP_FileTransferState(FW_UPDATE_END);
                                DISP_ProgbarSetNewState(0);
                                f_close(&file_SD);
                                f_mount(0x0UL, "0:", 0);                        
                            }
                            else if (HC_FwrUpdPck.state == FW_UPDATE_RUN)
                            {
                                if ((!HC_FwrUpdPck.pck_send) && ((Get_SysTick() - fw_upd_tmr) < fw_update_tout))
                                {
                                    return;
                                }                        
                                HC_FwrUpdPck.last_pck_send = HC_FwrUpdPck.pck_send;
                                ++HC_FwrUpdPck.pck_send;    
                                HC_FwrUpdPck.trial = 1;                    
                            }
                            else if ((HC_FwrUpdPck.state == FW_UPDATE_BLDR) && ((Get_SysTick() - fw_upd_tmr) < fw_update_tout)) 
                            {
                                return;                
                            }                    
                        }
                        else if (rx_buff[0] == NAK)
                        {
                            if ((HC_FwrUpdPck.state == FW_UPDATE_BLDR) || (HC_FwrUpdPck.state == FW_UPDATE_RUN))
                            {
                                ++HC_FwrUpdPck.trial;
                            }
                        }
                        break;
                    }
                    
                    
                    case UPD_FILE:
                    {
                        if ((rx_buff[0] == ACK) || (COM_Link == GROUP))
                        {
                            if (HC_FilUpdPck.pck_send == HC_FilUpdPck.pck_total)
                            {
                                DISP_FileTransferState(FILE_UPDATE_FINISHED);
                                DISP_ProgbarSetNewState(0);
                                HC_FilUpdPck.state = 0;
                                HC_State = PCK_ENUM;
                                HC_Update = UPD_IDLE;
                                request = 0;
                                f_close(&file_SD);
                                f_mount(0x0UL, "0:", 0);
                                
                                if      (HC_FwrUpdPck.cmd == UPDATE_FWR)
                                {
                                    rs485_txaddr = fwrupd_add_list[fwrupd_add_cnt];
                                    request = START_BLDR;
                                    rx_tmr = Get_SysTick();
                                    rx_tout = APP_START_DEL;
                                    HC_State = PCK_ENUM;
                                    return;
                                }
                                else if (HC_FwrUpdPck.cmd == UPDATE_BLDR)
                                {
                                    rs485_txaddr = fwrupd_add_list[fwrupd_add_cnt];
                                    request = UPDATE_BLDR;
                                    rx_tmr = Get_SysTick();
                                    rx_tout = BLDR_START_DEL;
                                    HC_State = PCK_ENUM;
                                    return;
                                }
                                else if ((HC_FilUpdPck.cmd == DWNLD_DISP_IMG) || (HC_FilUpdPck.cmd == RT_DWNLD_LOGO))
                                {
                                    HC_WriteLogEvent(IMG_UPDATED, filupd_list[filupd_list_cnt]);
                                    ++filupd_list_cnt;
                                    
                                    if (!filupd_list[filupd_list_cnt])
                                    {
                                        ++imgupd_addlist_cnt;
                                        filupd_list_cnt = 0;
                                        if (!imgupd_add_list[imgupd_addlist_cnt]) 
                                        {
                                            COM_Link = NOLINK;
                                            COM_Bridge = BRNONE;
                                            HC_FilUpdPck.cmd = 0;
                                        }
                                        else request = DWNLD_DISP_IMG;
                                    }
                                    else 
                                    {
                                        rs485_txaddr = imgupd_add_list[imgupd_addlist_cnt];
                                        request = DWNLD_DISP_IMG;   
                                    }
//                                    rx_tout = IMG_COPY_DEL;
                                    rx_tmr = Get_SysTick();
                                    HC_State = PCK_ENUM;
                                    return;
                                }
                            }
                            
                            if ((HC_FilUpdPck.state >= DWNLD_DISP_IMG_1) && (HC_FilUpdPck.state <= DWNLD_DISP_IMG_25))
                            {
                                if (COM_Link == GROUP) // if send to broadcast or group address
                                {   // there is no transfer control response from device to ack or nak packet
                                    if (HC_FilUpdPck.trial < TXREP_CNT)  // for redundancy of file transfer
                                    {   // every packet will be repeated TXREP_CNT times
                                        ++HC_FilUpdPck.trial;  // increase packet trial number till TXREP_CNT times
                                    }
                                    else
                                    {
                                        /* and for direct addressed p2p device use ack/nak control */
                                        HC_FilUpdPck.last_pck_send = HC_FilUpdPck.pck_send;
                                        ++HC_FilUpdPck.pck_send; 
                                        HC_FilUpdPck.trial = 1;                                    
                                    }
                                } 
                                else
                                {
                                    /* and for direct addressed p2p device use ack/nak control */
                                    HC_FilUpdPck.last_pck_send = HC_FilUpdPck.pck_send;
                                    ++HC_FilUpdPck.pck_send; 
                                    HC_FilUpdPck.trial = 1;                                    
                                }
                            }
                        }
                        else if(rx_buff[0] == NAK)
                        {
                            if ((HC_FilUpdPck.state >= DWNLD_DISP_IMG_1) && (HC_FilUpdPck.state <= DWNLD_DISP_IMG_25))
                            {   /* create same size variable from received packet number request */
                                /* and file packet send counter to prevent size overflow error when compare*/
                                uint32_t pktreq = ((HC_FilUpdPck.pck_send & 0xFFFFFF00) | rx_buff[6]);
                                /* now try to correct error in packet number by increasing */
                                /* packet number and dropping allready send packets */
                                if (HC_FilUpdPck.pck_send < pktreq)
                                {
                                    HC_FilUpdPck.last_pck_send = HC_FilUpdPck.pck_send;
                                    ++HC_FilUpdPck.pck_send;
                                    HC_FilUpdPck.trial = 1;
                                }
                                else ++HC_FilUpdPck.trial;
                                /* if this is not sufficient error recovery     */
                                /* writting down packet loader from requested   */
                                /* packet number is best sollution to problem   */
                                /* stat load_pkt(txbuf, *HC_FilUpdPck)          */
                            }
                        }
                        break;
                    }
                    
                    
                    case UPD_LOG:
                    {
                        if      (rx_buff[0] == ACK)  // command acknowledged
                        {
                            if      (rx_buff[5] == 1)  // is response log or ack packet
                            {
                                if      (rx_buff[6] == GET_LOG_LIST)   // if get log ack packet received, log list is empty
                                {
                                    HC_LogTransfer.state = TRANSFER_IDLE;
                                    if (IsRC_StatusUpdated()) HC_Update = UPD_ROOM_STAT;
                                }
                                else if (rx_buff[6] == DEL_LOG_LIST)  // it's log delete ack packet
                                {
                                    // 
                                    // log deleted successfully
                                    // 
                                }
                            }                      
                            else if ((rx_buff[5] == (LOG_DSIZE+2)) && (rx_buff[6] == GET_LOG_LIST))
                            {
                                memcpy (eebuff, &rx_buff[7], LOG_DSIZE);    // copy received log to eeprom buffer
                                eebuff[3] = ((rs485_txaddr>>8)  & 0xFFU);   // insert sender rs485 address
                                eebuff[4] =  (rs485_txaddr      & 0xFFU);   // for log source 
                                if (rx_buff[9] != FUNC_OR_DRV_FAIL)  // insert log details for sw/hw error event  
                                {
                                    eebuff[5] = rx_buff[10]; // insert fauilure event function detail
                                    eebuff[6] = rx_buff[11]; // and error causal driver
                                }
                                HC_WriteLog();    // append log to log list
                                DISP_UpdateLog(); // write to display new event from log
                            }
                        }
                        else if (rx_buff[0] == NAK) // increase number of trial attempt to try again if limit not reahed
                        {
                            ++HC_LogTransfer.trial;
                        }
                        break;
                    }
                    
                    
                    case UPD_HTTP_RQ:
                    {
                        if      (rx_buff[0] == ACK) 
                        {
                            if (http_cmdsta == HTTP_GET_ROOM_STAT)
                            {
                                http_cmdsta = HTTP_ROOM_STAT_READY;
                            }
                        }
                        else if (rx_buff[0] == NAK)
                        {
                            http_cmdsta = 0;
                        }
                        break;
                    }
                    case UPD_IDLE:
                    case UPD_TIME:
                    default:
                        break;
                }
                rx_tmr = Get_SysTick();
                rx_tout = RX2TX_DEL;
                HC_State = PCK_ENUM;
                break;                
            }
            break;
        }
    }
}
/**
  * @brief
  * @param
  * @retval
  */
uint8_t HC_LoadAddrList(void)
{
    uint8_t add_buf[8];
    uint32_t b_cnt, b_total, b_read;
    uint32_t rc_add, rc_add_cnt;
    uint32_t flash_add, buf_cnt;
    
    if (f_mount(&fatfs, "0:", 0x0U) != FR_OK)
    {
        return FS_DRIVE_ERR;
    }
    else if (f_open(&file_SD, "CTRL_ADD.TXT", FA_READ) != FR_OK) 
    {
        f_mount(0x0UL, "0:", 0x0U);
        return FS_FILE_ERROR;
    }
    /**
    *    format address list flash memory storage
    */
    SPIFLASH_SectorErase(ADDR_LIST_START_ADDR, SPIFLASH_ER64K);
    SPIFLASH_WaitBusy();
    /**
    *    calculate address list size
    */
    b_read = 0x0U;
    b_total = 0x0U;
    rc_add_cnt = 0x0U;
    flash_add = ADDR_LIST_START_ADDR;
    
    while(b_total < file_SD.obj.objsize)        
    {
        ZEROFILL(hc_buff, COUNTOF(hc_buff));
        
        if(f_read (&file_SD, hc_buff, HC_BSIZE - 0x2U, &b_read) != FR_OK)
        {
            f_close(&file_SD);
            f_mount(0x0UL, "0:", 0x0U);
            return FS_FILE_ERROR;
        }        
        b_cnt = 0x0U;
        b_total += b_read;
        
        while(b_cnt < b_read)
        {
            buf_cnt = 0x0U;
            ZEROFILL(add_buf, COUNTOF(add_buf));
            
            while((hc_buff[b_cnt] != 0x0U)  &&
                  (hc_buff[b_cnt] != ',')   &&
                  (hc_buff[b_cnt] != ';'))
            {
                add_buf[buf_cnt++] = hc_buff[b_cnt++];
            }
            
            rc_add = atoi((char*)add_buf);
            add_buf[0] = (rc_add >> 8)  & 0xFFU;
            add_buf[1] = rc_add         & 0xFFU;
            SPIFLASH_Write(flash_add, add_buf, 0x2U);
            SPIFLASH_WaitBusy();
            flash_add += 0x2U;
            ++rc_add_cnt;
            ++b_cnt;
        }        
        b_read = 0x0U;
    }
    
    add_buf[0] = (rc_add_cnt >> 24) & 0xFFU;
    add_buf[1] = (rc_add_cnt >> 16) & 0xFFU;
    add_buf[2] = (rc_add_cnt >>  8) & 0xFFU;
    add_buf[3] =  rc_add_cnt         & 0xFFU;
    SPIFLASH_Write((ADDR_LIST_END_ADDR - 0x3U), add_buf, 0x4U);
    SPIFLASH_WaitBusy();
    return FS_FILE_OK;
}
/**
  * @brief
  * @param
  * @retval
  */
uint8_t HC_CreateAddrList(void)
{    
    uint8_t addbuf[4];
    uint32_t bcnt, flashadd, rcaddcnt;
        
    addr_list = 0x0U;
    addr_list_cnt = 0x0U;
    addr_list_size = 0x0U;
    fwrupd_add_cnt = 0x0U;
    fwrupd_add_list = 0x0U;
    imgupd_add_list = 0x0U;
    imgupd_addlist_cnt = 0x0U;
    fwrupd_addlist_dsize = 0x0U;
    imgupd_addlist_dsize = 0x0U;
    ZEROFILL(addbuf, COUNTOF(addbuf));
    flashadd = (ADDR_LIST_END_ADDR - 0x3U);    // ssize of address list is saved in last four bytes of 64K block
    SPIFLASH_Read(flashadd, addbuf, 0x4U);          // read from spi flash memory size of address list 
    rcaddcnt = (((addbuf[0] << 24) & 0xFF000000U) | 
                ((addbuf[1] << 16) & 0xFF0000U)   |
                ((addbuf[2] <<  8) & 0xFF00U)     | 
                ( addbuf[3]         & 0xFFU));
    if ((rcaddcnt == 0x0U) || (rcaddcnt > 4095U)) return FS_FILE_ERROR;   // address list size abnormal
    addr_list = mem_malloc(rcaddcnt * 0x2U);    
    if (addr_list == 0x0UL) return OUT_OF_MEMORY_ERROR;
    addr_list_size = rcaddcnt;
    ZEROFILL(addr_list, rcaddcnt * 0x2U);
    fwrupd_add_list = mem_malloc(rcaddcnt * 0x2U);    
    if (fwrupd_add_list == 0x0UL) return OUT_OF_MEMORY_ERROR;
    fwrupd_addlist_dsize = rcaddcnt;
    ZEROFILL(fwrupd_add_list, rcaddcnt * 0x2U);
    imgupd_add_list = mem_malloc(rcaddcnt * 0x2U);    
    if (imgupd_add_list == 0x0UL) return OUT_OF_MEMORY_ERROR;
    imgupd_addlist_dsize = rcaddcnt;
    ZEROFILL(imgupd_add_list, rcaddcnt * 0x2U);
    bcnt = 0x0U;
    flashadd = ADDR_LIST_START_ADDR;
    
    while(bcnt < rcaddcnt)
    {
        addbuf[0] = 0x0U;
        addbuf[1] = 0x0U;
        SPIFLASH_Read(flashadd, addbuf, 0x2U);
        addr_list[bcnt] = (((addbuf[0] << 8) & 0xFF00U) |(addbuf[1] & 0xFFU));
        flashadd += 0x2U;
        ++bcnt;
    }
    return FS_FILE_OK;
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_FormatLogList(void)
{
    uint32_t delete_cnt;

    ZEROFILL (eebuff, COUNTOF(eebuff));                
    delete_cnt = EE_LOG_LIST_START_ADD;
    while(delete_cnt < EE_LOG_LIST_END_ADD)
    {
#ifdef USE_WATCHDOG
        IWDG_ReloadCounter();
#endif
        if (delete_cnt< I2CEE_PAGE_SIZE)    I2CEE_WriteBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, I2CEE_BLOCK);
        else                                I2CEE_WriteBytes16(I2CEE_PAGE_1, HC_LogMemory.first_addr, eebuff, I2CEE_BLOCK);
        DelayMs(I2CEE_WRITE_DELAY);
        delete_cnt += I2CEE_BLOCK;
    }
    
    HC_LogMemory.log_cnt    = 0U;
    HC_LogMemory.Allocation = TYPE_1;        
    HC_LogMemory.first_addr = EE_LOG_LIST_START_ADD;
    HC_LogMemory.last_addr  = EE_LOG_LIST_START_ADD;
    HC_LogMemory.next_addr  = EE_LOG_LIST_START_ADD;
    if (HTTP_LogTransfer.state == HTTP_FORMAT_LOG_LIST) HTTP_LogTransfer.state = HTTP_LOG_FORMATED;
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_ReadLogListBlock(void)
{
    uint32_t read_cnt;
    /**
    *    TYPE_1 -> log list is empty and next log address is first address
    *    0000000000000000000000000000000000000000000000000000000000000000000000000
    *
    *    TYPE_2 -> log list start at some addres, it's full till last address, next log address is first address and is free for write 
    *    000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    *
    *     TYPE_3 -> log list start at some addres, end at upper address, next log address is next upper from end address and is free for write
    *    000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxx000000000000000000000000000
    *
    *    TYPE_4 -> log list start at first address, end at last address, it's full, next log address is first memory address, write is forbiden
    *    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    *    
    *    TYPE_5 -> log list start at first address, end at upper address, and next upper log address is free for write
    *    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00000000000000000000000000000000000000000
    *
    *    TYPE_6 -> log list start at upper address, end at lower address and next upper from end address is free for write
    *    xxxxxxxxxxxx0000000000000000000000000000000000000000000000xxxxxxxxxxxxxxx
    */    
    switch (HC_LogMemory.Allocation)
    {
       case TYPE_2:
            read_cnt = HC_LogMemory.first_addr;                
            while (read_cnt >= I2CEE_BLOCK) read_cnt -= I2CEE_BLOCK;                
            if (read_cnt != 0U) read_cnt = (I2CEE_BLOCK - read_cnt);
            else read_cnt = I2CEE_BLOCK;
            if (HC_LogMemory.first_addr < I2CEE_PAGE_SIZE)      I2CEE_ReadBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, read_cnt);
            else                                                I2CEE_ReadBytes16(I2CEE_PAGE_1, HC_LogMemory.first_addr, eebuff, read_cnt);
            Hex2Str((char*)hc_buff, eebuff, (read_cnt * 2U));
            break;
        case TYPE_3:
            read_cnt = HC_LogMemory.first_addr;                
            while (read_cnt >= I2CEE_BLOCK) read_cnt -= I2CEE_BLOCK;                
            if (read_cnt != 0U) read_cnt = (I2CEE_BLOCK - read_cnt);
            else read_cnt = I2CEE_BLOCK;
            if (HC_LogMemory.first_addr < I2CEE_PAGE_SIZE)      I2CEE_ReadBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, read_cnt);
            else                                                I2CEE_ReadBytes16(I2CEE_PAGE_1, HC_LogMemory.first_addr, eebuff, read_cnt);
            Hex2Str((char*)hc_buff, eebuff, (read_cnt * 2U));
            break;
        case TYPE_4:
            I2CEE_ReadBytes16 (I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, I2CEE_BLOCK);
            Hex2Str((char*)hc_buff, eebuff, (I2CEE_BLOCK * 2U));
            HTTP_LogTransfer.last_addr = EE_LOG_LIST_START_ADD + I2CEE_BLOCK;
            break;
        case TYPE_5:
            read_cnt = HC_LogMemory.first_addr;                
            while (read_cnt >= I2CEE_BLOCK) read_cnt -= I2CEE_BLOCK;                
            if (read_cnt != 0U) read_cnt = (I2CEE_BLOCK - read_cnt);
            else read_cnt = I2CEE_BLOCK;
            if (HC_LogMemory.first_addr < I2CEE_PAGE_SIZE)      I2CEE_ReadBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, read_cnt);
            else                                                I2CEE_ReadBytes16(I2CEE_PAGE_1, HC_LogMemory.first_addr, eebuff, read_cnt);                
            Hex2Str((char*)hc_buff, eebuff, (read_cnt * 2U));
            break;
        case TYPE_6:
            read_cnt = HC_LogMemory.first_addr;                
            while (read_cnt >= I2CEE_BLOCK) read_cnt -= I2CEE_BLOCK;                
            if (read_cnt != 0U) read_cnt = (I2CEE_BLOCK - read_cnt);
            else read_cnt = I2CEE_BLOCK;
            if (HC_LogMemory.first_addr < I2CEE_PAGE_SIZE)      I2CEE_ReadBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, read_cnt);
            else                                                I2CEE_ReadBytes16(I2CEE_PAGE_1, HC_LogMemory.first_addr, eebuff, read_cnt);
            Hex2Str((char*)hc_buff, eebuff, (read_cnt * 2U));
            break;
        case TYPE_1:
        default:
            break;
    }
    if (HTTP_LogTransfer.state == HTTP_GET_LOG_LIST) HTTP_LogTransfer.state = HTTP_LOG_READY;
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_DeleteLogListBlock(void)
{
    uint32_t delete_cnt;
    ZEROFILL(eebuff, COUNTOF(eebuff));
    /**
    *    TYPE_1 -> log list is empty and next log address is first address
    *    0000000000000000000000000000000000000000000000000000000000000000000000000
    *
    *    TYPE_2 -> log list start at some addres, it's full till last address, next log address is first address and is free for write 
    *    000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    *
    *     TYPE_3 -> log list start at some addres, end at upper address, next log address is next upper from end address and is free for write
    *    000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxx000000000000000000000000000
    *
    *    TYPE_4 -> log list start at first address, end at last address, it's full, next log address is first memory address, write is forbiden
    *    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    *    
    *    TYPE_5 -> log list start at first address, end at upper address, and next upper log address is free for write
    *    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00000000000000000000000000000000000000000
    *
    *    TYPE_6 -> log list start at upper address, end at lower address and next upper from end address is free for write
    *    xxxxxxxxxxxx0000000000000000000000000000000000000000000000xxxxxxxxxxxxxxx
    */
    switch (HC_LogMemory.Allocation)
    {
        case TYPE_2:
            delete_cnt = HC_LogMemory.first_addr;                
            while(delete_cnt >= I2CEE_BLOCK) delete_cnt -= I2CEE_BLOCK;                
            if (delete_cnt != 0U) delete_cnt = (I2CEE_BLOCK - delete_cnt);
            else delete_cnt = I2CEE_BLOCK;
            if (HC_LogMemory.first_addr < I2CEE_PAGE_SIZE)  I2CEE_WriteBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, delete_cnt);
            else                                            I2CEE_WriteBytes16(I2CEE_PAGE_1, HC_LogMemory.first_addr, eebuff, delete_cnt);
            DelayMs(I2CEE_WRITE_DELAY);
            if((HC_LogMemory.first_addr + delete_cnt) >= EE_LOG_LIST_END_ADD)
            {
                if  (HC_LogMemory.next_addr != EE_LOG_LIST_START_ADD) HC_LogMemory.Allocation = TYPE_5;
                else HC_LogMemory.Allocation = TYPE_1;
                HC_LogMemory.first_addr = EE_LOG_LIST_START_ADD;
            }
            else 
            {
                if (HC_LogMemory.next_addr != EE_LOG_LIST_START_ADD) HC_LogMemory.Allocation = TYPE_6;
                HC_LogMemory.first_addr += delete_cnt;
            }
            HC_LogMemory.log_cnt -= (delete_cnt / LOG_DSIZE);
            break;            
        case TYPE_3:
            delete_cnt = HC_LogMemory.first_addr;                
            while (delete_cnt >= I2CEE_BLOCK) delete_cnt -= I2CEE_BLOCK;                
            if (delete_cnt != 0U) delete_cnt = (I2CEE_BLOCK - delete_cnt);
            else delete_cnt = I2CEE_BLOCK;            
            if ((HC_LogMemory.first_addr + delete_cnt) >= (HC_LogMemory.last_addr + LOG_DSIZE))
            {
                delete_cnt = (HC_LogMemory.last_addr + LOG_DSIZE) - HC_LogMemory.first_addr;
                if (HC_LogMemory.first_addr < I2CEE_PAGE_SIZE)  I2CEE_WriteBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, delete_cnt);
                else                                            I2CEE_WriteBytes16(I2CEE_PAGE_1, HC_LogMemory.first_addr, eebuff ,delete_cnt);
                DelayMs(I2CEE_WRITE_DELAY);
                HC_LogMemory.first_addr = EE_LOG_LIST_START_ADD;
                HC_LogMemory.last_addr  = EE_LOG_LIST_START_ADD;
                HC_LogMemory.next_addr  = EE_LOG_LIST_START_ADD;
                HC_LogMemory.log_cnt = 0U;
                HC_LogMemory.Allocation = TYPE_1;
            }
            else
            {
                if (HC_LogMemory.first_addr < I2CEE_PAGE_SIZE)  I2CEE_WriteBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, delete_cnt);
                else                                            I2CEE_WriteBytes16 (I2CEE_PAGE_1, HC_LogMemory.first_addr, eebuff, delete_cnt);
                DelayMs(I2CEE_WRITE_DELAY);
                HC_LogMemory.log_cnt -= (delete_cnt / LOG_DSIZE);
                HC_LogMemory.first_addr += delete_cnt;
            }
            break;            
        case TYPE_4:
            I2CEE_WriteBytes16 (I2CEE_PAGE_0, EE_LOG_LIST_START_ADD, eebuff, I2CEE_BLOCK);
            DelayMs(I2CEE_WRITE_DELAY);
            HC_LogMemory.first_addr = HTTP_LogTransfer.last_addr;
            HC_LogMemory.last_addr = EE_LOG_LIST_END_ADD - LOG_DSIZE;
            HC_LogMemory.next_addr = EE_LOG_LIST_START_ADD;
            HC_LogMemory.log_cnt -= (I2CEE_BLOCK / LOG_DSIZE);
            HC_LogMemory.Allocation = TYPE_2;
            break;        
        case TYPE_5:
            delete_cnt = HC_LogMemory.first_addr;                
            while (delete_cnt >= I2CEE_BLOCK) delete_cnt -= I2CEE_BLOCK;                
            if (delete_cnt != 0U) delete_cnt = (I2CEE_BLOCK - delete_cnt);
            else delete_cnt = I2CEE_BLOCK;
            if((HC_LogMemory.first_addr + delete_cnt) >= (HC_LogMemory.last_addr + LOG_DSIZE))
            {
                delete_cnt = (HC_LogMemory.last_addr + LOG_DSIZE) - HC_LogMemory.first_addr;
                if(HC_LogMemory.first_addr < I2CEE_PAGE_SIZE)   I2CEE_WriteBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, delete_cnt);
                else                                            I2CEE_WriteBytes16(I2CEE_PAGE_1, HC_LogMemory.first_addr, eebuff, delete_cnt);
                DelayMs(I2CEE_WRITE_DELAY);
                HC_LogMemory.first_addr = EE_LOG_LIST_START_ADD;
                HC_LogMemory.last_addr  = EE_LOG_LIST_START_ADD;
                HC_LogMemory.next_addr  = EE_LOG_LIST_START_ADD;
                HC_LogMemory.log_cnt = 0U;
                HC_LogMemory.Allocation = TYPE_1;
            }
            else
            {
                if (HC_LogMemory.first_addr < I2CEE_PAGE_SIZE)  I2CEE_WriteBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, delete_cnt);
                else                                            I2CEE_WriteBytes16(I2CEE_PAGE_1, HC_LogMemory.first_addr, eebuff, delete_cnt);
                DelayMs(I2CEE_WRITE_DELAY);
                HC_LogMemory.Allocation = TYPE_3;
                HC_LogMemory.log_cnt -= (delete_cnt / LOG_DSIZE);
                HC_LogMemory.first_addr += delete_cnt;
            }
            break;            
        case TYPE_6:           
            delete_cnt = HC_LogMemory.first_addr;                
            while(delete_cnt >= I2CEE_BLOCK) delete_cnt -= I2CEE_BLOCK;                
            if(delete_cnt != 0U) delete_cnt = (I2CEE_BLOCK - delete_cnt);
            else delete_cnt = I2CEE_BLOCK;
            if (HC_LogMemory.first_addr < I2CEE_PAGE_SIZE)  I2CEE_WriteBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, delete_cnt);
            else                                            I2CEE_WriteBytes16(I2CEE_PAGE_1, HC_LogMemory.first_addr, eebuff, delete_cnt);
            DelayMs(I2CEE_WRITE_DELAY);
            if((HC_LogMemory.first_addr + delete_cnt) >= EE_LOG_LIST_END_ADD)
            {
                HC_LogMemory.Allocation = TYPE_5;
                HC_LogMemory.first_addr = EE_LOG_LIST_START_ADD;
            }
            else HC_LogMemory.first_addr += delete_cnt;
            HC_LogMemory.log_cnt -= (delete_cnt / LOG_DSIZE);
            break;
        case TYPE_1:
        default:
            break;
    }
    if (HTTP_LogTransfer.state == HTTP_DEL_LOG_LIST) HTTP_LogTransfer.state = HTTP_LOG_DELETED;
   
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_WriteLogEvent(uint8_t event, uint8_t type)
{
    static uint16_t log_id = 0x0U;
    ++log_id;
    eebuff[0] = ((log_id>>8) & 0xFFU);
    eebuff[1] =  (log_id     & 0xFFU);
    eebuff[2] = event;                          // log event
    eebuff[3] = ((rs485_txaddr>>8)  & 0xFFU);   // log source address msb
    eebuff[4] = (rs485_txaddr       & 0xFFU);   // log source address lsb
    eebuff[5] = type;                           // rf card id b0 or log type 
    eebuff[6] = 0x0U;    
    if ((event >= FUNC_ERR_FIRST_CODE)          // for function/driver fail event 
    &&  (event <= FUNC_ERR_LAST_CODE))          // clear device address
    {
        eebuff[3] = 0x0U;
        eebuff[4] = 0x0U;
        eebuff[6] = FUNC_OR_DRV_FAIL;           // rf card id b1  or log group
    } 
    eebuff[7] = 0x0U;                           // rf card id b2
    eebuff[8] = 0x0U;                           // rf card id b3
    eebuff[9] = 0x0U;                           // rf card id b4 msb
    eebuff[10] = rtc_date.RTC_Date;
    eebuff[11] = rtc_date.RTC_Month;
    eebuff[12] = rtc_date.RTC_Year;
    eebuff[13] = rtc_time.RTC_Hours;
    eebuff[14] = rtc_time.RTC_Minutes;
    eebuff[15] = rtc_time.RTC_Seconds;
    HC_WriteLog();
    DISP_UpdateLog();
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_CreateLogRequest(void)
{
    uint32_t i;
    
    if (HC_LogTransfer.trial >= MAXREP_CNT)
    {
        HC_LogTransfer.state = TRANSFER_IDLE;
        return;
    }
    else if (HC_LogTransfer.state == TRANSFER_QUERY_LIST)
    {
        tx_buff[6] = GET_LOG_LIST;
    }
    else if (HC_LogTransfer.state == TRANSFER_DELETE_LOG)
    {
        tx_buff[6] = DEL_LOG_LIST;
    }

    tx_buff[0] = SOH;
    tx_buff[1] = ((rs485_txaddr>>8) & 0xFFU);
    tx_buff[2] = (rs485_txaddr      & 0xFFU);
    tx_buff[3] = ((rsifa>>8) & 0xFFU);
    tx_buff[4] =  (rsifa     & 0xFFU);
    tx_buff[5] = 1U;
    tx_buff[7] = 0U;
    tx_buff[8] = tx_buff[6];
    tx_buff[9] = EOT;
    
    //
    //  for rs485 to onewire bridge message insert 4 byte header
    //
    if (COM_Bridge > BRNONE)
    {
        tx_buff[10]= tx_buff[6];
        tx_buff[5] = 5U; // add header size to packet lenght
        tx_buff[6] = SET_BR2OW; // set bridge command
        tx_buff[7] = ow_txaddr;  // onewire receiver address
        tx_buff[8] = ow_ifaddr; // onewire interface address
        tx_buff[9] = 1U; // onewire data payload size
        rs485_pkt_chksum = 0U;
        for (i = 6U; i < (tx_buff[5] + 6U); i++)
        {
            rs485_pkt_chksum += tx_buff[i];
        }
        tx_buff[11] = (rs485_pkt_chksum >> 8)  & 0xFFU;
        tx_buff[12] = rs485_pkt_chksum         & 0xFFU;
        tx_buff[13] = EOT;
    }
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_WriteLog(void)
{
    /**
    *    TYPE_1 -> log list is empty and next log address is first address
    *    0000000000000000000000000000000000000000000000000000000000000000000000000
    *
    *    TYPE_2 -> log list start at some addres, it's full till last address, next log address is first address and is free for write 
    *    000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    *
    *     TYPE_3 -> log list start at some addres, end at upper address, next log address is next upper from end address and is free for write
    *    000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxx000000000000000000000000000
    *
    *    TYPE_4 -> log list start at first address, end at last address, it's full, next log address is first memory address, write is forbiden
    *    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    *    
    *    TYPE_5 -> log list start at first address, end at upper address, and next upper log address is free for write
    *    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00000000000000000000000000000000000000000
    *
    *    TYPE_6 -> log list start at upper address, end at lower address and next upper from end address is free for write
    *    xxxxxxxxxxxx0000000000000000000000000000000000000000000000xxxxxxxxxxxxxxx
    */
    switch (HC_LogMemory.Allocation)
    {
        case TYPE_1:
            I2CEE_WriteBytes16(I2CEE_PAGE_0, HC_LogMemory.next_addr, eebuff, LOG_DSIZE);
            DelayMs(I2CEE_WRITE_DELAY);
            ++HC_LogMemory.log_cnt;
            HC_LogMemory.Allocation = TYPE_5;
            HC_LogMemory.next_addr += LOG_DSIZE;
            break;        
        case TYPE_2:
            if (HC_LogMemory.next_addr < I2CEE_PAGE_SIZE)  I2CEE_WriteBytes16(I2CEE_PAGE_0, HC_LogMemory.next_addr, eebuff, LOG_DSIZE); 
            else I2CEE_WriteBytes16(I2CEE_PAGE_1, HC_LogMemory.next_addr,eebuff, LOG_DSIZE); 
            DelayMs(I2CEE_WRITE_DELAY);
            HC_LogMemory.last_addr = HC_LogMemory.next_addr;
            HC_LogMemory.next_addr += LOG_DSIZE;
            ++HC_LogMemory.log_cnt; 
            if (HC_LogMemory.next_addr == HC_LogMemory.first_addr) HC_LogMemory.Allocation = TYPE_4;
            break;            
        case TYPE_3: 
            if (HC_LogMemory.next_addr < I2CEE_PAGE_SIZE) I2CEE_WriteBytes16(I2CEE_PAGE_0, HC_LogMemory.next_addr, eebuff, LOG_DSIZE);
            else I2CEE_WriteBytes16(I2CEE_PAGE_1, HC_LogMemory.next_addr, eebuff, LOG_DSIZE); 
            DelayMs(I2CEE_WRITE_DELAY);
            HC_LogMemory.last_addr = HC_LogMemory.next_addr;
            HC_LogMemory.next_addr += LOG_DSIZE;
            ++HC_LogMemory.log_cnt;
            if (HC_LogMemory.next_addr > (EE_LOG_LIST_END_ADD - LOG_DSIZE)) HC_LogMemory.next_addr = EE_LOG_LIST_END_ADD;
            else if (HC_LogMemory.next_addr == HC_LogMemory.first_addr) HC_LogMemory.Allocation = TYPE_4;
            break;        
        case TYPE_5:
            if (HC_LogMemory.next_addr < I2CEE_PAGE_SIZE) I2CEE_WriteBytes16(I2CEE_PAGE_0, HC_LogMemory.next_addr, eebuff, LOG_DSIZE);
            else I2CEE_WriteBytes16(I2CEE_PAGE_1, HC_LogMemory.next_addr, eebuff, LOG_DSIZE);
            DelayMs(I2CEE_WRITE_DELAY);
            HC_LogMemory.last_addr = HC_LogMemory.next_addr;
            HC_LogMemory.next_addr += LOG_DSIZE;
            ++HC_LogMemory.log_cnt;
            if (HC_LogMemory.next_addr > (EE_LOG_LIST_END_ADD - LOG_DSIZE)) HC_LogMemory.Allocation = TYPE_4;
            break;            
        case TYPE_6:
            if (HC_LogMemory.next_addr < I2CEE_PAGE_SIZE) I2CEE_WriteBytes16(I2CEE_PAGE_0, HC_LogMemory.next_addr, eebuff, LOG_DSIZE);
            else I2CEE_WriteBytes16(I2CEE_PAGE_1, HC_LogMemory.next_addr, eebuff, LOG_DSIZE);
            DelayMs(I2CEE_WRITE_DELAY);
            HC_LogMemory.last_addr = HC_LogMemory.next_addr;
            HC_LogMemory.next_addr += LOG_DSIZE;
            ++HC_LogMemory.log_cnt;
            if (HC_LogMemory.next_addr == HC_LogMemory.first_addr) HC_LogMemory.Allocation = TYPE_4;
            break;
        case TYPE_4:
        default:
            break;
    }
}
/**
  * @brief
  * @param
  * @retval
  */
int HC_ScanRS485(uint16_t fadd, uint16_t ladd, uint8_t option)
{
    int new_fnd;
    static uint16_t address_offset;
    static uint32_t rs485_rx_tmr = 0x0U;
    uint8_t scn_pcnt;
    uint8_t tmp_j;
    uint16_t tmp_address, rx_chksm;
    
    static enum
    {
        INIT    = 0x0U,
        SEND    = 0x1U,
        PENDING = 0x2U,
        RECEIVE = 0x3U,
        SETUP   = 0x4U
        
    }ScanState;
    
    if(option == FIND_NEXT) ScanState = SETUP;
    else ScanState = INIT;
    
    rxbcnt = 0x0U;
    scn_pcnt = 0x0U;
    
    while(scn_pcnt == 0x0U)
    {
        switch(ScanState)
        {
            case INIT:
            {
                if ((fadd <= rsifa) || (fadd >= ladd)) return (-1);            
                new_fnd = 0x0U;
                address_offset = 0x0U;
                ScanState = SETUP;
                ZEROFILL(rx_buff, COUNTOF(rx_buff));
                ZEROFILL(tx_buff, COUNTOF(tx_buff));
                break;    
            }
            
            
            case SEND:
            {
                if((Get_SysTick() - rs485_rx_tmr) >= RESP_TOUT)
                {     
                    RS485_Send(tx_buff, (tx_buff[5] + 0x9U));
                    rs485_rx_tmr = Get_SysTick();
                    rxbcnt = 0x0U;
                    ScanState = PENDING;                    
                }        
                break;    
            }
            
            
            case PENDING:
            {    
                if (((rx_buff[1] ==((rsifa >> 0x8U) & 0xFFU))
                &&   (rx_buff[2] == (rsifa & 0xFFU)))
                &&  ((rx_buff[3] ==((rs485_txaddr >> 0x8U) & 0xFFU))
                &&   (rx_buff[4] == (rs485_txaddr & 0xFFU)))
                &&   (rx_buff[rx_buff[5] + 0x8U] == EOT))
                {
                    rx_chksm = 0x0U;
                    for (tmp_j = 0x6U; tmp_j < (rx_buff[5] + 0x6U); tmp_j++) rx_chksm += rx_buff[tmp_j];

                    if ((rx_buff[rx_buff[5] + 0x6U] == ((rx_chksm >> 0x8U) & 0xFFU)) &&
                        (rx_buff[rx_buff[5] + 0x7U] ==  (rx_chksm & 0xFFU)))
                    {
                        ScanState = RECEIVE;
                    }
                }
                else if((Get_SysTick() - rs485_rx_tmr) >= RESP_TOUT)
                {
                    rs485_rx_tmr = Get_SysTick();
                    if(option == FIND_ADDR) return (0);
                    else ScanState = SETUP;
                }
                break;
            }
            
            
            case RECEIVE:
            {    
                fwrupd_add_list[new_fnd] = rs485_txaddr;
                
                if ((option == FIND_FIRST) 
                ||  (option == FIND_NEXT) 
                ||  (option == FIND_ADDR))
                {
                    return (1);
                }
                else
                {
                    ScanState = SETUP;
                    ++new_fnd;
                }    
                break;
            }
            
            
            case SETUP:
            {
                if(option == FIND_NEW)
                {
                    tmp_address = 0x0U;
                    addr_list_cnt = 0x0U;
                    
                    while(addr_list[addr_list_cnt + 0x1U] != 0x0U)    // find if call address is allready used 
                    {
                        tmp_address = HC_GetNextAddr();
                        
                        if((fadd + address_offset) == tmp_address)
                        {
                            tmp_address = 0x0U;
                            ++address_offset;
                            addr_list_cnt = 0x0U;
                        }
                    }
                }                
                else if(option == FIND_ADDR)
                {
                    address_offset = 0x0U;
                }
                
                rs485_txaddr = (fadd + address_offset);
                if ((fadd + address_offset) > ladd) scn_pcnt = 0x1U;
                ++address_offset;
                HC_CreateCmdRequest(GET_APPL_STAT, (char*)rx_buff);
                ZEROFILL(hc_buff, COUNTOF(hc_buff));
                ScanState = SEND;            
                break;
            }
        }
    }
    
    return (new_fnd);
}
/**
  * @brief
  * @param
  * @retval
  */
uint16_t HC_GetNextAddr(void)
{
    static uint8_t room_status_update_cycle_cnt = 0x0U;
    uint16_t current_address;

    current_address = addr_list[addr_list_cnt];
    ++addr_list_cnt;

    if (addr_list[addr_list_cnt] == 0x0U)
    {
        addr_list_cnt = 0x0U;
        rs485_bus_sta = RS485_BUS_CONNECTED;
        ++room_status_update_cycle_cnt;
        if(room_status_update_cycle_cnt == JRNUPD_CNT)
        {
            room_status_update_cycle_cnt = 0x0U;
            
            if(!IsRC_StatusUpdated()) 
            {
                RC_StatusUpdateSet();
                ZEROFILL(hc_buff, COUNTOF(hc_buff));
            }
        }
        else if(IsRC_StatusUpdated()) 
        {
            RC_JournalUpdateSet();
            RC_StatusUpdateReset();
        }
    }

    return current_address;
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_CreateTimeUpdatePacket(void)
{
    uint8_t i;  
    /**
    *   time sync is always sent as broadcast  
    *   packet without response from receivers
    *   every receiver will propagate this packet 
    *   to all up-and-running connection interfaces
    *   this function enable broadcast packet to
    *   reach every connected device regarded
    *   to interface, protocol, baudrate....
    */    
    tx_buff[0] = SOH;
    tx_buff[1] = ((rsbra>>8) & 0xFFU); 
    tx_buff[2] = (rsbra      & 0xFFU);
    tx_buff[3] = ((rsifa>>8) & 0xFFU);
    tx_buff[4] = (rsifa      & 0xFFU);
    tx_buff[5] = 0x8U;
    tx_buff[6] = SET_RTC_DATE_TIME;
    tx_buff[7] = rtc_date.RTC_WeekDay;
    if(rtc_date.RTC_WeekDay == 0x0U) tx_buff[7] = 0x7U;
    tx_buff[8] = rtc_date.RTC_Date;
    tx_buff[9] = rtc_date.RTC_Month;
    tx_buff[10]= rtc_date.RTC_Year;
    tx_buff[11]= rtc_time.RTC_Hours;
    tx_buff[12]= rtc_time.RTC_Minutes;
    tx_buff[13]= rtc_time.RTC_Seconds;
    rs485_pkt_chksum = 0x0U;
    for (i = 0x6U; i < 14U; i++)
    {
        rs485_pkt_chksum += tx_buff[i];
    }
    tx_buff[14] = ((rs485_pkt_chksum >> 8) & 0xFFU);
    tx_buff[15] = (rs485_pkt_chksum        & 0xFFU);
    tx_buff[16] = EOT;
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_CreateRoomCtrlStatReq(void)
{
    tx_buff[0] = SOH;
    tx_buff[1] = (rs485_txaddr >> 8) & 0xFFU;
    tx_buff[2] = rs485_txaddr & 0xFFU;
    tx_buff[3] = (rsifa >> 8) & 0xFFU;
    tx_buff[4] = rsifa & 0xFFU;
    tx_buff[5] = 0x1U;
    tx_buff[6] = GET_SYS_STAT;
    tx_buff[7] = 0x0U;
    tx_buff[8] = GET_SYS_STAT;
    tx_buff[9] = EOT;
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_CreateRoomStatusRequest(void)
{
    tx_buff[0] = SOH;
    tx_buff[1] = (rs485_txaddr >> 8) & 0xFFU;;
    tx_buff[2] = rs485_txaddr & 0xFFU;
    tx_buff[3] = (rsifa >> 8) & 0xFFU;
    tx_buff[4] = rsifa & 0xFFU;
    tx_buff[5] = 0x1U;
    tx_buff[6] = GET_APPL_STAT;
    tx_buff[7] = 0x0U;
    tx_buff[8] = GET_APPL_STAT;
    tx_buff[9] = EOT;
}
/**
  * @brief
  * @param
  * @retval
  */
uint8_t HC_CheckNewFirmwareFile(void)
{ 
    if (f_mount(&fatfs, "0:", 0x0U) != FR_OK)
    {
        return FS_DRIVE_ERR;
    }

    if (f_opendir(&dir_1, "/") != FR_OK)
    {
        f_mount(0x0UL,"0:", 0x0U);
        return FS_DIRECTORY_ERROR;
    }
        
    if (f_open(&file_SD, "NEW.BIN", FA_READ) != FR_OK) 
    {
        f_mount(0x0UL,"0:", 0x0U);
        return FS_FILE_ERROR;
    }
    
    rs485_txaddr = atoi(hc_buff);
    HC_FwrUpdPck.state = FW_UPDATE_INIT;
    HC_FwrUpdPck.trial = 0x0U;
    HC_FwrUpdPck.pck_send = 0x0U;
    HC_FwrUpdPck.last_pck_send = 0x0U;
    HC_FwrUpdPck.pck_total = file_SD.obj.objsize / HC_PCK_BSIZE;

    if ((HC_FwrUpdPck.pck_total * HC_PCK_BSIZE) < file_SD.obj.objsize)
    {
        ++HC_FwrUpdPck.pck_total;
    }

    DISP_ProgbarSetNewState(1);
    return FS_FILE_OK;    
}
/**
  * @brief
  * @param
  * @retval
  */
uint8_t HC_CheckNewImageFile(void)
{
    uint32_t bcnt, i, dr;
    char name[16];
    
    if (f_mount(&fatfs, "0:", 0x0U) != FR_OK)
    {
        return FS_DRIVE_ERR;
    }
    ZEROFILL(name, COUNTOF(name));
    rs485_txaddr = atoi(hc_buff);
    if (HC_FilUpdPck.cmd == DWNLD_DISP_IMG)  
    {  // image format address_imgnum.RAW eg(101_3.RAW)   */
        name[0] = '/';
        Int2Str(&name[1], imgupd_add_list[imgupd_addlist_cnt], 0x0U);
        if (f_opendir(&dir_1, name) != FR_OK)
        {
            f_mount(0x0UL,"0:", 0x0U);
            return FS_DIRECTORY_ERROR;
        }
        ZEROFILL(name, COUNTOF(name));
        Int2Str(name, imgupd_add_list[imgupd_addlist_cnt], 0x0U);
        i = strlen(name);
        name[i++] = '/';
        Int2Str(&name[i], imgupd_add_list[imgupd_addlist_cnt], 0x0U);
        i = strlen(name);
        name[i++] = '_';
        name[i] = 0x0U;
        Int2Str(&name[i], filupd_list[filupd_list_cnt] , 0x0U);
        i = strlen(name);
        strncat(&name[i], ".RAW", 0x4U);
        if (f_open(&file_SD, name, FA_READ) == FR_OK)                    
        {
            HC_FilUpdPck.state =  DWNLD_DISP_IMG + filupd_list[filupd_list_cnt]; // create room controller command from list
        }
        else
        {
            f_mount(0x0UL,"0:", 0x0U);
            return FS_FILE_ERROR;
        }
    }
    else    
    {    /* image format IMGxx.RAW    */
        bcnt = strlen(hc_buff) + 0x1U;    // find start of image number string
        dr = atoi(&hc_buff[bcnt]);      // convert image number to int for command 
        i = strlen(&hc_buff[bcnt]);     // take size of image number string
        memcpy(name, "IMG", 0x3U);    // set image name first
        memcpy(&name[3], &hc_buff[bcnt], i);   // copy image number to next
        bcnt = strlen(name);       // get file name size till now
        memcpy(&name[bcnt], ".RAW", 0x4U); // append file type string to end of file name
        if (f_open(&file_SD, name, FA_READ) != FR_OK) // try to open file with created file name
        {
            f_mount(0x0UL,"0:", 0x0U);
            return FS_FILE_ERROR;
        }
        else HC_FilUpdPck.state = DWNLD_DISP_IMG + dr; // create command from image number
    }
    /** ==========================================================================*/
    /**     C A L C U L A T E    C R C  3 2    F O R    I M A G E   F I L E       */
    /** ==========================================================================*/
    dr = 0x0U;
    CRC_ResetDR();
    file_crc8 = 0x0U;
    HC_FilUpdPck.read_bcnt = 0x0U;
    while (HC_FilUpdPck.read_bcnt != file_SD.obj.objsize)
    {
        f_read(&file_SD, hc_buff, HC_BSIZE, &dr);
        HC_FilUpdPck.read_bcnt += dr;
        file_crc8 = CRC_Calculate8((uint8_t*)hc_buff, dr);
    }
    /*
    * reopen file
    */
    f_close(&file_SD);
    if (f_open(&file_SD, name, FA_READ) != FR_OK)
    {
        f_mount(0x0UL,"0:", 0x0U);
        return FS_FILE_ERROR;
    }
    HC_FilUpdPck.pck_send = 0x0U;
    HC_FilUpdPck.trial = 0x0U;
    HC_FilUpdPck.last_pck_send = 0x0U;
    HC_FilUpdPck.pck_total = file_SD.obj.objsize / HC_PCK_BSIZE;
    if ((HC_FilUpdPck.pck_total * HC_PCK_BSIZE) < file_SD.obj.objsize)
    {
        ++HC_FilUpdPck.pck_total;
    }
    sys_file_progress = 0x0U;
    DISP_ProgbarSetNewState(1);
    return FS_FILE_OK;
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_CreateFirmwareUpdateRequest(void)
{
    uint32_t i;

    if (HC_FwrUpdPck.trial >= MAXREP_CNT)
    {
        HC_FwrUpdPck.state = FW_UPDATE_FAIL;
    }

    if ((HC_FwrUpdPck.state == FW_UPDATE_FAIL) || (HC_FwrUpdPck.state == FW_UPDATE_END))
    {
        f_close(&file_SD);
        f_mount(0x0UL,"0:", 0x0U);
        COM_Link = NOLINK;
        COM_Bridge = BRNONE;
        HC_FwrUpdPck.state = 0x0U;
        return;
    }
    else if (HC_FwrUpdPck.state == FW_UPDATE_INIT)
    {
        tx_buff[0] = SOH;
        tx_buff[5] = 1U;
        tx_buff[6] = START_BLDR;
        HC_FwrUpdPck.state = FW_UPDATE_BLDR;
    }
    else if (HC_FwrUpdPck.state == FW_UPDATE_BLDR)
    {
        tx_buff[0] = SOH;
        tx_buff[5] = 0x3U;
        tx_buff[6] = DWNLD_FWR;
        tx_buff[7] = (HC_FwrUpdPck.pck_total >> 8) & 0xFFU;
        tx_buff[8] = HC_FwrUpdPck.pck_total & 0xFFU;
        HC_FwrUpdPck.state = FW_UPDATE_RUN;
    }
    else if (HC_FwrUpdPck.state == FW_UPDATE_RUN)
    {
        tx_buff[0] = STX;
        tx_buff[5] = HC_PCK_BSIZE + 0x2U;
        tx_buff[6] = (HC_FwrUpdPck.pck_send >> 8) & 0xFFU;
        tx_buff[7] = HC_FwrUpdPck.pck_send & 0xFFU;
        f_read(&file_SD, &tx_buff[8], HC_PCK_BSIZE, (UINT*) (&HC_FwrUpdPck.read_bcnt));
        if (HC_FwrUpdPck.pck_send == 0x1U) DelayMs(500);        
    }

    tx_buff[1] = (rs485_txaddr >> 8);
    tx_buff[2] = rs485_txaddr;
    tx_buff[3] = (rsifa >> 8);
    tx_buff[4] = rsifa;
    rs485_pkt_chksum = 0x0U;

    for (i = 0x6U; i < (tx_buff[5] + 0x6U); i++)
    {
        rs485_pkt_chksum += tx_buff[i];
    }

    tx_buff[tx_buff[5] + 0x6U] = (rs485_pkt_chksum >> 8);
    tx_buff[tx_buff[5] + 0x7U] = rs485_pkt_chksum;
    tx_buff[tx_buff[5] + 0x8U] = EOT;
    sys_file_progress = ((HC_FwrUpdPck.pck_send * 100U) / HC_FwrUpdPck.pck_total);
    PROGBAR_SetValue(hPROGBAR_FileTransfer, sys_file_progress);
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_CreateFileUpdateRequest(void)
{
    uint32_t i, rd;
    
    if (HC_FilUpdPck.trial >= MAXREP_CNT)
    {
        HC_FilUpdPck.state = FILE_UPDATE_FAIL;
    }

    if ((HC_FilUpdPck.state == FILE_UPDATE_FAIL) || (HC_FilUpdPck.state == FILE_UPDATE_FINISHED))
    {
        f_close(&file_SD);
        f_mount(0x0UL,"0:", 0x0U);
        COM_Link = NOLINK;
        COM_Bridge = BRNONE;
        HC_FilUpdPck.state = 0x0U;
        return;
    }
    else if (HC_FilUpdPck.pck_send == 0x0U)
    {
        DelayMs(500); 
        tx_buff[0] = SOH;
        if (COM_Bridge == BR2OW) 
        {
            tx_buff[5] = 15U;
            tx_buff[6] = SET_BR2OW;
            tx_buff[7] = ow_txaddr;
            tx_buff[8] = ow_ifaddr;
            tx_buff[9] = 11U;
            tx_buff[10]= HC_FilUpdPck.state;
            tx_buff[11]= (HC_FilUpdPck.pck_total >> 8);
            tx_buff[12]= HC_FilUpdPck.pck_total;
            tx_buff[13]= (file_SD.obj.objsize >> 24);
            tx_buff[14]= (file_SD.obj.objsize >> 16);
            tx_buff[15]= (file_SD.obj.objsize >>  8);
            tx_buff[16]=  file_SD.obj.objsize;
            tx_buff[17]= (file_crc8 >> 24);
            tx_buff[18]= (file_crc8 >> 16);
            tx_buff[19]= (file_crc8 >>  8);
            tx_buff[20]= file_crc8;
        }
        else
        {
            tx_buff[5] = 11U;
            tx_buff[6] = HC_FilUpdPck.state;
            tx_buff[7] =(HC_FilUpdPck.pck_total >> 8);
            tx_buff[8] = HC_FilUpdPck.pck_total;
            tx_buff[9] =(file_SD.obj.objsize >> 24);
            tx_buff[10]=(file_SD.obj.objsize >> 16);
            tx_buff[11]=(file_SD.obj.objsize >>  8);
            tx_buff[12]= file_SD.obj.objsize;
            tx_buff[13]=(file_crc8 >> 24);
            tx_buff[14]=(file_crc8 >> 16);
            tx_buff[15]=(file_crc8 >>  8);
            tx_buff[16]= file_crc8;
        }
    }
    else
    {
        if (COM_Bridge == BR2OW) 
        {
            tx_buff[0] = SOH;
            f_read(&file_SD, (uint8_t*) &tx_buff[12], HC_PCK_BSIZE, &rd);
            tx_buff[5] =(rd + 0x6U);
            tx_buff[6] = SET_BR2OW;
            tx_buff[7] = ow_txaddr;
            tx_buff[8] = ow_ifaddr;
            tx_buff[9] =(rd + 0x2U);
            tx_buff[10]=(HC_FilUpdPck.pck_send >> 8);
            tx_buff[11]= HC_FilUpdPck.pck_send;
        }
        else
        {
            tx_buff[0] = STX;
            f_read(&file_SD, (uint8_t*) &tx_buff[8], HC_PCK_BSIZE, &rd);
            tx_buff[5] =(rd + 0x2U);
            tx_buff[6] =(HC_FilUpdPck.pck_send >> 8);
            tx_buff[7] = HC_FilUpdPck.pck_send;
        }   
    }

    tx_buff[1] =(rs485_txaddr >> 8); 
    tx_buff[2] = rs485_txaddr;
    tx_buff[3] =(rsifa >> 8);
    tx_buff[4] = rsifa;

    rs485_pkt_chksum = 0x0U;

    for (i = 0x6U; i < (tx_buff[5] + 0x6U); i++)
    {
        rs485_pkt_chksum += tx_buff[i];
    }

    tx_buff[tx_buff[5] + 0x6U] =(rs485_pkt_chksum >> 8);
    tx_buff[tx_buff[5] + 0x7U] = rs485_pkt_chksum;
    tx_buff[tx_buff[5] + 0x8U] = EOT;
    
    sys_file_progress = ((HC_FilUpdPck.pck_send * 100U) / HC_FilUpdPck.pck_total);
    PROGBAR_SetValue(hPROGBAR_FileTransfer, sys_file_progress);
}
/**
  * @brief
  * @param
  * @retval
  */
void HC_CreateCmdRequest(uint8_t cmd, char *ibuf)
{
    uint32_t i;
    uint32_t bcnt;
    tx_buff[0] = SOH;
    tx_buff[1] = rs485_txaddr>>8;
    tx_buff[2] = rs485_txaddr;
    tx_buff[3] = rsifa>>8;
    tx_buff[4] = rsifa;
    tx_buff[5] = 1;
    tx_buff[6] = cmd; 
    
    if      (cmd == SET_DISPL_BCKLGHT)
    {
        ibuf += strlen(ibuf) + 0x1U;
        i = atoi(ibuf);
        tx_buff[5] = 0x3U;
        tx_buff[7] = (i >> 8)   & 0xFFU; // display brightness MSB
        tx_buff[8] = i          & 0xFFU; // display brightness LSB
    }
    else if (cmd == DWNLD_JRNL)
    {    
        i = 0x7U;
        ibuf += strlen(ibuf) + 0x1U;
        while((*ibuf != 0x0U) && (*ibuf != ';')) tx_buff[i++] = *ibuf++;
        tx_buff[i++] = *ibuf++;
        tx_buff[i] = 0x0U;
        tx_buff[5] = (i - 0x5U);
    }
    else if (cmd == SET_RS485_CFG)
    {
        tx_buff[5] = 0x8U;
        ibuf += strlen(ibuf) + 0x1U;
        i = atoi(ibuf);
        tx_buff[7] = ((i>>8) & 0xFFU);
        tx_buff[8] = (i      & 0xFFU);
        ibuf += strlen(ibuf) + 0x1U;
        i = atoi(ibuf);
        tx_buff[9] = ((i>>8) & 0xFFU);
        tx_buff[10]= (i      & 0xFFU);
        ibuf += strlen(ibuf) + 0x1U;
        i = atoi(ibuf);
        tx_buff[11]= ((i>>8) & 0xFFU);
        tx_buff[12]= (i      & 0xFFU);
        ibuf += strlen(ibuf) + 0x1U;
        tx_buff[13]= *ibuf;
    }
    else if ((cmd== SET_BEDDING_REPL) || (cmd == SET_APPL_STAT))
    {
        ibuf += (strlen(ibuf)+1U);
        tx_buff[5] = 0x2U;
        tx_buff[7] = atoi(ibuf);
    }
    else if (cmd == SET_DOUT_STATE)
    {
        tx_buff[5] = 10U;
        memcpy (&tx_buff[7], ibuf, 9);
    }
    else if (cmd == SET_DIN_CFG)
    {
        tx_buff[5] = 9;
        memcpy (&tx_buff[7], ibuf, 8);
    }
    else if ((cmd== SET_AUTH_KEYA) || (cmd == SET_AUTH_KEYB))
    {
        tx_buff[5] = 13U;
    }
    else if (cmd == SET_PERMITED_GROUP)
    {
        ibuf += (strlen(ibuf)+1U);
        i = strlen(ibuf); 
        memset(&tx_buff[7], 'X',  16);
        memcpy(&tx_buff[7], ibuf, i);   // copy qr code to buffer
        tx_buff[5] = 17U;
    }
    else if (cmd == RESET_SOS_ALARM)
    {
    }
    else if (cmd == SET_ROOM_TEMP)
    {
        tx_buff[5] = 0x4U;
    }
    else if (cmd == SET_SYSTEM_ID)
    {        
        ibuf += (strlen(ibuf)+1U);
        i = atoi(ibuf);
        tx_buff[5] = 0x3U;
        tx_buff[7] = ((i >> 8) & 0xFFU);    // system id MSB
        tx_buff[8] = ( i       & 0xFFU);    // system id LSB
    }
    else if (cmd == RT_SET_DISP_STA)
    {
        uint32_t x = 0x7U;
        uint32_t y = strlen(ibuf) + 0x1U;// skeep interface address
        uint32_t z = y;
        
        while(y)
        {    
            tx_buff[x] = (atoi(&ibuf[z]) & 0xFFU);  // room thermostat id number 1~9 
            y = strlen(&ibuf[z]);                   // display image number
            z += (y + 0x1U);                        // display image timeout
            ++x;                                    // buzzer mode
            y = strlen(&ibuf[z]);                   // buzzer repeat timer
        }
        tx_buff[5] = x - 0x6U;
    }
    else if (cmd == RT_DISP_MSG)
    {
        ibuf += (strlen(ibuf)+1U);
        tx_buff[7] = (atoi(ibuf) & 0xFFU);
        ibuf += strlen(ibuf) + 0x1U;
        i = atoi(ibuf);
        tx_buff[8] = (i >> 8) & 0xFFU;
        tx_buff[9] = i & 0xFFU;
        ibuf += strlen(ibuf) + 0x1U;
        i = atoi(ibuf);
        tx_buff[10] = (i >> 8) & 0xFFU;
        tx_buff[11] = i & 0xFFU;
        ibuf += strlen(ibuf) + 0x1U;
        i = atoi(ibuf);
        tx_buff[12] = (i >> 8) & 0xFFU;
        tx_buff[13] = i & 0xFFU;
        ibuf += strlen(ibuf) + 0x1U;
        i = atoi(ibuf);
        tx_buff[14] = (i >> 8) & 0xFFU;
        tx_buff[15] = i & 0xFFU;
        ibuf += strlen(ibuf) + 0x1U;
        tx_buff[16] = (atoi(ibuf) & 0xFFU);
        ibuf += strlen(ibuf) + 0x1U;
        tx_buff[17] = (atoi(ibuf) & 0xFFU);
        ibuf += strlen(ibuf) + 0x1U;
        tx_buff[18] = (atoi(ibuf) & 0xFFU);
        ibuf += strlen(ibuf) + 0x1U;
        tx_buff[19] = (atoi(ibuf) & 0xFFU);
        ibuf += strlen(ibuf) + 0x1U;
        i = strlen(ibuf);
        memcpy(&tx_buff[20], ibuf, i);
        tx_buff[5] = i + 15U;
        tx_buff[tx_buff[5] + 5U] = 0x0U; // null terminate text string
    }
    else if (cmd == RT_UPD_WFC)
    {
        tx_buff[5] = (WFC_DSIZE + 0x1U);
        memcpy(&tx_buff[7], ibuf, WFC_DSIZE);
    }
    else if (cmd == RT_UPD_QRC)
    {
        ibuf += (strlen(ibuf)+1U);
        i = strlen(ibuf);               // get size of qr code text
        memcpy(&tx_buff[7], ibuf, i);   // copy qr code to buffer
        tx_buff[5] = i + 0x1U;          // add command byte to data payload size
    }
    else if (cmd == SET_PASSWORD)
    {
        ibuf += (strlen(ibuf)+1U);      // set pointer to password first number
        i = strlen(ibuf);               // get size of qr code text
        memcpy(&tx_buff[7], ibuf, i);   // copy user password data
        tx_buff[5] = i+1U;              // set packet payload size
    }
    //
    //  for rs485 to onewire bridge message insert 4 byte header
    //
    if (COM_Bridge == BR2OW)
    {
        for (bcnt = tx_buff[5]-1; bcnt; bcnt--)
        {
            tx_buff[bcnt+10U] = tx_buff[bcnt+6U]; // copy buffer to make space for header
        }        
        tx_buff[5] += 4U;       // add header size to packet lenght
        tx_buff[6] = SET_BR2OW; // set bridge command
        tx_buff[7] = ow_txaddr; // onewire receiver address
        tx_buff[8] = ow_ifaddr; // onewire interface address
        tx_buff[9] = tx_buff[5]-4U; // onewire data payload size
        tx_buff[10]= cmd;
    }    
    rs485_pkt_chksum = 0;
    for (i = 6U; i < (tx_buff[5]+6U); i++)
    {
        rs485_pkt_chksum += tx_buff[i];
    }
    tx_buff[tx_buff[5]+6] = ((rs485_pkt_chksum >> 8) & 0xFFU);
    tx_buff[tx_buff[5]+7] =  (rs485_pkt_chksum       & 0xFFU);
    tx_buff[tx_buff[5]+8] = EOT;
}
/**
  * @brief  Search a directory for firmware files and make list of newest files if more than one firmware
            file for same application type exist. This function is executed on new power cycle, tftp file 
            upload, or by request. After creation, list is advertised on bus to all conected devices. 
            Every device will receive firmware info and compare with own. If update is available, system 
            flag is set for update request. Same flag is set with manual request from service meny.
            After advertizing firmware info, for few address list cycles, requests are counted.Fastes  
            update option is then used. If more than 5 requests for same firmware update is activ,
            group update is executed or peet to peer for les. After all requests is serviced, any
            device withfirmware request flag set will be P2P updated, as single request.
  * @param  pointer to first firmware info struct to fill
  * @retval number of found firmware, zero if no file found
  */
uint8_t SendFwInfo(void)
{
    uint8_t         txb[32];    /* Buffer for fw info packet    */
    uint32_t        cnt, i;     /* Number of valid firmware     */
    uint32_t        crc;        /* crc32 calculated on file data*/
    FRESULT         fr;         /* Return value                 */
    FIL             fil;        /* File object                  */
    DIR             dj;         /* Directory search object      */
    FILINFO         fno;        /* File information             */
    FwInfoTypeDef   fi1;        /* Two temp info storages to    */
//    FwInfoTypeDef   fi2;        /* compare for newer file       */ 
    
    if (f_mount(&fatfs, "0:", 0x0U) != FR_OK) return (0x0U); /* try to mount file system */
    fr = f_findfirst(&dj, &fno, "", "*.bin");       
    while (fr == FR_OK && fno.fname[0])         /* Search for first firmware binary file */
    {                                           /* Repeat while an next fimware is found */
        fr += f_open  (&fil,fno.fname,FA_READ); /* Try to open file to read version info */
        fr += f_lseek (&fil, VERS_INF_OFFSET);  /* Set offset to file read pointer       */ 
        fr += f_read  (&fil, txb, 16, &cnt);    /* Read data from version info offset    */
        f_close(&fil);                          /* Close and reopen file to read from    */  
        fr += f_open (&fil,fno.fname, FA_READ); /* beggining of file. Then check is      */ 
        fi1.size    = ((txb[3] <<24)|(txb[2] <<16)|(txb[1] <<8)|txb[0]);
        fi1.crc32   = ((txb[7] <<24)|(txb[6] <<16)|(txb[5] <<8)|txb[4]);
        fi1.version = ((txb[11]<<24)|(txb[10]<<16)|(txb[9] <<8)|txb[8]);
        fi1.wr_addr = ((txb[15]<<24)|(txb[14]<<16)|(txb[13]<<8)|txb[12]);
        crc         = fi1.size;           /* set any value different from file crc */
        fr         += ValidateFwInfo(&fi1);     /* validate data to skeep time consuming */
        if (!fr)                                /* file crc32 check if validation fail   */
        {
            CRC_ResetDR();
            do
            {   /* Read a chunk of source file into buffer */
                if(crc >= sizeof(txb)) fr += f_read(&fil,txb,sizeof(txb),&cnt);  
                else  fr += f_read(&fil,txb,crc,&cnt); 
                CRC_Calculate8 (txb, cnt); 
                crc -=cnt;               
            }                               /* Calculate crc on data  */
            while (!fr && cnt && crc);             /* do so untill end of file     */
        }
//        if (CRC_GetCRC() == fi1.crc32) ++cnt;        /* add valid file to counter    */
//        else ResetFwInfo(&fi1);             /* or destroy invalid file info */
        fr = f_findnext(&dj, &fno);         /* Search for next item         */ 
        ZEROFILL(txb, COUNTOF(txb)); // clear buffer
        txb[0] = SOH; // command packet start of header control char
        txb[1] = DEF_RSBRA >> 8; // send firmware info to all device
        txb[2] = DEF_RSBRA & 0xFFU; // on all connected data lines
        txb[3] = rsifa >> 8;
        txb[4] = rsifa;
        txb[5] = 17;
        txb[6] = UPD_FW_INFO;
        txb[7] =(fi1.size >> 24);
        txb[8] =(fi1.size >> 16);
        txb[9] =(fi1.size >>  8);
        txb[10]= fi1.size;
        txb[11]=(fi1.crc32 >> 24);
        txb[12]=(fi1.crc32 >> 16);
        txb[13]=(fi1.crc32 >>  8);
        txb[14]= fi1.crc32;
        txb[15]=(fi1.version >> 24);
        txb[16]=(fi1.version >> 16);
        txb[17]=(fi1.version >>  8);
        txb[18]= fi1.version;
        txb[19]=(fi1.wr_addr >> 24);
        txb[20]=(fi1.wr_addr >> 16);
        txb[21]=(fi1.wr_addr >>  8);
        txb[22]= fi1.wr_addr;
        crc = 0x0U;
        for (i = 6; i < (txb[5]+6); i++)
        {
            crc += txb[i];
        }
        txb[23] =(crc >> 8);
        txb[24] = crc;
        txb[25] = EOT;
        i = TXREP_CNT;
        while(i--)
        {
            RS485_Send(txb, 26);
            DelayMs(RESP_TOUT);            
        }
    }
    f_closedir(&dj);
    return (cnt);
}
/**
  * @brief
  * @param
  * @retval
  */
uint8_t HC2RT_Link(uint8_t *txbuf, uint8_t *rxbuf)
{
    uint32_t rxtout, rxtmr = 0x0U;
    uint16_t rxwkp = (STX | 0x100U);                // set wackeup receivers 9 bit byte with address mark bit high
    uint8_t crc8, cnt;
    
    UART_ReInit(WL_9BIT, BR_115200);                // reinit usart to 9bit data and 115kbps
    while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET) // wait till transmitter ready to send
    {
    }
    GPIO_WriteBit(GPIOE,GPIO_Pin_4, Bit_SET);       // set rs485 driver to send data
    USART_SendData(USART2, rxwkp);                  // send wackeup byte
    while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET) // wait to transmitter send buffer
    {
    }
    GPIO_WriteBit(GPIOE,GPIO_Pin_4, Bit_RESET);     // set rs485 driver to receive data
    UART_ReInit(WL_8BIT, BR_115200);                // reinit usart to 8bit data and 115kbps
    txbuf[0] = STX;                                 // set to calculate crc on all send bytes
    CRC_ResetDR();                                  // reset crc unit
    txbuf[txbuf[5] + 0x6U] = CRC_Calculate8(txbuf, txbuf[5] + 0x6U);    // calculate crc8 on tx byffer
    GPIO_WriteBit(GPIOE,GPIO_Pin_4, Bit_SET);       // set rs485 driver to send data
    cnt = 0x0U;
    do
    {
        ++cnt;
        while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET)
        {
        } 
		USART_SendData(USART2, txbuf[cnt]);
    }
    while (cnt < (txbuf[5] + 0x6U));
    while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET)
    {
    } 
    GPIO_WriteBit(GPIOE,GPIO_Pin_4, Bit_RESET);     // set rs485 driver to receive data
    rxtmr = Get_SysTick();                          // start timeout timer for receive to complete
    rxtout = RESP_TOUT;                             // first timeout value is response time
    cnt = 0x0U;
    do
    {
        if (USART_GetFlagStatus(USARTx, USART_FLAG_RXNE) == SET) 
        {
            rxbuf[cnt] = USART_ReceiveData(USART2); // copy received byte to buffer
            rxtmr = Get_SysTick();                  // set timer start value
            rxtout = RX_TOUT;                      // after first byte received, short timout value to single byte transfer time
            ++cnt;                                  // increase received byte counter
        }
    }
    while ((Get_SysTick() - rxtmr) < rxtout);
    UART_Init();                                    // set uart for hotel controller
    if (cnt != 0x0U)                                // skeep crc function if nothing to check
    {                                               // null pointer passing to crc produce hardware HardFault
        CRC_ResetDR();                              // reset crc unit to default value
        crc8 = CRC_Calculate8(rxbuf, cnt - 0x1U);     // calculate packet crc and if equal to send value 
        if (rxbuf[cnt - 0x1U] == crc8) return (cnt);   // return number of received bytes to caller function
    }                                               // if crc check fail drop received packet
    return 0x0U;                                    // send 0 bytes received as no response from device
}
/**
  * @brief
  * @param
  * @retval
  */
uint8_t HTTP2RS485(void)
{
    uint8_t crc8; 
    uint32_t tout   = 0, tmr    = 0;
    uint32_t rxbcnt = 0, indx   = 0, inc    = 0;
    uint32_t rxtout = 0, rxtmr  = 0, rxsta  = 0;
    
    UART_ReInit(WL_8BIT, BR_115200);                // reinit usart to 9bit data and 115kbps
    tout = REC_TOUT/2;      // 2.5s timout to send command and check new state
    tmr = Get_SysTick();    // load timer 
    
    while((Get_SysTick() - tmr) < tout)
    {
        DelayMs(10); // insert min. rx2tx delay
        RS485_Send(tx_buff, tx_buff[5]+9); // send command packet
        rxtmr = Get_SysTick(); // start timeout timer for receive to complete
        rxtout = RESP_TOUT; // 45ms response timeout
        rxbcnt = 0; // received byte counter
        rxsta = 1; // receive flag
        do
        {
            if (USART_GetFlagStatus(USARTx, USART_FLAG_RXNE) == SET) 
            {
                rx_buff[rxbcnt] = USART_ReceiveData(USART2); // copy received byte to buffer
                rxtmr = Get_SysTick();  // set timer start value
                rxtout = RX_TOUT;   // after first byte received, short timout value to single byte transfer time
                if (++rxbcnt > 9)  // increase received byte counter and search for valid packet
                {
                    indx = 0; 
                    while (indx < (rxbcnt-9))
                    {
                        rxsta = 0;
                        if      (rx_buff[indx] > US)                    ++rxsta;    // first packet byte is ascii controll character
                        else if (rx_buff[indx+1] != (rsifa>>8))         ++rxsta;    // second is rs485 interface address msb 
                        else if (rx_buff[indx+2] != rsifa)              ++rxsta;    // third is rs485 interface address lsb
                        else if (rx_buff[indx+3] != (rs485_txaddr>>8))  ++rxsta;    // second is receiver interface address msb 
                        else if (rx_buff[indx+4] != rs485_txaddr)       ++rxsta;    // third is receiver interface address lsb
                        else if (rx_buff[rx_buff[indx+5]+8] != EOT)     ++rxsta;    // last packet byte is ascii controll "end of transmission"
                        else if (rxsta == 0)                                     // all previous check ok
                        {
                            crc8 = 0;
                            for (inc = indx+6; inc < (rx_buff[indx+5]+6); inc++)
                            {
                                crc8 += rx_buff[inc];
                            }
                            if ((rx_buff[rx_buff[indx+5]+6] == (crc8 >> 8)) 
                            &&  (rx_buff[rx_buff[indx+5]+7] ==  crc8))
                            {
                                RS485_Init();           // start irq receiving again
                                return(rx_buff[indx]);  // return response
                            }
                        }
                        ++indx;
                    }
                }              
            }
            if ((Get_SysTick() - rxtmr) > rxtout) rxsta = 0;
        }
        while (rxsta);
    }
    RS485_Init();   // start irq receiving again
    return 0;       // return timeout
}
/************************ (C) COPYRIGHT JUBERA D.O.O Sarajevo ************************/
