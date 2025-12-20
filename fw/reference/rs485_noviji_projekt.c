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


/* Includes ------------------------------------------------------------------*/
#include "display.h"
#include "logger.h"
#include "eeprom.h"
#include "rc522.h"
#include "rs485.h"
#include "owire.h"
#include "room.h"
#include "main.h"
#include "dio.h"


#if (__RS485_H__ != FW_BUILD)
#error "rs485 header version mismatch"
#endif
// 0,1,2,3,4,5,6,7
//         4
// 0,1,2,3,5,6,7,x
//
/* Imported Types  -----------------------------------------------------------*/
/* Imported Variables --------------------------------------------------------*/
/* Imported Functions    -----------------------------------------------------*/
/* Private Typedef -----------------------------------------------------------*/
eComStateTypeDef eComState = COM_INIT;
/* Private Define  -----------------------------------------------------------*/
#define BUFF_SIZE   256U
/* Private Variables  --------------------------------------------------------*/
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
uint8_t sys_stat;
uint8_t activ_cmd;
uint8_t rs485_bps;
uint8_t fw_upd_stat;
uint8_t rs485_ifa[2];
uint8_t rs485_gra[2];
uint8_t rs485_bra[2];
uint8_t rs485_buff[RS_BSIZE];
/* Private macros   ----------------------------------------------------------*/
/* Private Function Prototypes -----------------------------------------------*/
static void BackupOldFirmware(void);
static uint8_t UpdateBootloader(void);
static void RS485_Response(uint8_t resp, uint8_t size);
static void FormatFileStorage(uint32_t start_address, uint8_t number_of_blocks);
static uint8_t FLASH_If_Write(uint32_t destination, uint32_t *p_source, uint32_t size);
static void CopyFile(uint32_t source_address, uint32_t destination_address, uint32_t size);
/* Program Code  -------------------------------------------------------------*/
/**
  * @brief
  * @param
  * @retval
  */
void RS485_Init(void)
{
    rec = 0;
    rec_bcnt = 0;
    receive_pcnt = 0;
    packet_type = 0;
    COM_Link = NOLINK;
    RS485_StopTimer();
    ZEROFILL(rs485_buff, COUNTOF(rs485_buff));
    HAL_GPIO_WritePin(RS485_DIR_Port, RS485_DIR_Pin, GPIO_PIN_RESET);

    if(huart1.RxState == HAL_UART_STATE_BUSY_RX)
    {
        __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
        huart1.RxState = HAL_UART_STATE_READY;
        huart1.gState = HAL_UART_STATE_READY;
    }

    if(HAL_UART_Receive_IT(&huart1, &rec, 1) != HAL_OK) ErrorHandler(RS485_FUNC, USART_DRV);
    eComState = COM_PACKET_PENDING;
}
/**
  * @brief
  * @param
  * @retval
  */
void RS485_Service(void)
{
    uint8_t ee_buff[24];
    uint8_t respb = ACK;    // default response byte
    uint8_t resps = 1;   // default response data size
    uint8_t posrst = 0;
    uint32_t tmp = 0U;
    static __IO uint32_t upd_tout_tmr;
    static __IO uint32_t total_bytes_in_file;
    static __IO uint32_t next_packet_number;
    static __IO uint32_t total_packet_number;
    static __IO uint32_t crc_32_calculated;
    static __IO uint32_t crc_32_file;
    static __IO uint32_t flash_destination;
    static __IO uint32_t file_copy_src_addr;
    static __IO uint32_t file_copy_dest_addr;
    static __IO uint32_t file_copy_size;
    static __IO uint32_t post_process;
    static __IO uint32_t fversion = 0;
    static __IO uint32_t waddress = 0;


    if(IsRS485_UpdateActiv())
    {
        if((HAL_GetTick() - upd_tout_tmr) >= REC_TOUT)
        {
            RS485_StopUpdate();
            SYS_FileTransferFailSet();
            SYS_FileTransferSuccessReset();
        }
    }


    if(eComState == COM_PACKET_RECEIVED)
    {
#ifdef	USE_WATCHDOG
        HAL_IWDG_Refresh(&hiwdg);
#endif
        if(packet_type == SOH)
        {
            if((rs485_buff[0] >= DWNLD_DISP_IMG_1) && (rs485_buff[0] <= DWNLD_DISP_IMG_25))
            {
                post_process        = 0x0U;
                next_packet_number  = 0x1U;
                activ_cmd           = rs485_buff[0];
                total_packet_number = (rs485_buff[1] << 8) | rs485_buff[2];
                total_bytes_in_file = (rs485_buff[3] << 24) | (rs485_buff[4] << 16) | (rs485_buff[5] << 8) | rs485_buff[6];
                crc_32_file         = (rs485_buff[7] << 24) | (rs485_buff[8] << 16) | (rs485_buff[9] << 8) | rs485_buff[10];
                fversion            = (rs485_buff[11] << 24) | (rs485_buff[12] << 16) | (rs485_buff[13] << 8) | rs485_buff[14];
                waddress            = (rs485_buff[15] << 24) | (rs485_buff[16] << 16) | (rs485_buff[17] << 8) | rs485_buff[18];

                if((activ_cmd != DWNLD_FWR_IMG) && (activ_cmd != DWNLD_BLDR_IMG))
                {
                    SYS_ImageUpdateRequestReset();
                    flash_destination   = EE_NEW_IMAGE_ADDR;
                    post_process        = COPY_DISP_IMG;
                    file_copy_src_addr  = EE_NEW_IMAGE_ADDR;
                    file_copy_dest_addr = ((activ_cmd - DWNLD_DISP_IMG_1) * 0x00030000U);
                    file_copy_size      = total_bytes_in_file;
                    FormatFileStorage(EE_NEW_IMAGE_ADDR, 0x3U);
                }
                else
                {
                    SYS_FwrUpdRequestReset();
                    flash_destination   = ((activ_cmd - DWNLD_DISP_IMG_1) * 0x00030000U);
                    if(activ_cmd == DWNLD_FWR_IMG)  FormatFileStorage(EE_NEW_FW_ADDR, 0x3U);
                    else if(activ_cmd == DWNLD_BLDR_IMG) FormatFileStorage(EE_RC_NEW_BLDR_ADDR, 0x3U);
                }
                upd_tout_tmr        = HAL_GetTick();
                SYS_FileTransferFailReset();
                RS485_StartUpdate();
            }
            else
            {
                switch(rs485_buff[0])
                {
                case UPDATE_BLDR:
                {
                    upd_tout_tmr = HAL_GetTick();
                    RS485_StartUpdate();

                    if(UpdateBootloader() != 0x0U)
                    {
                        LogEvent.log_event = BLDR_UPD_FAIL;
                        DISP_BldrUpdFailSet();
                        SYS_UpdateFailSet();
                        respb = NAK;
                    }
                    else
                    {
                        LogEvent.log_event = BLDR_UPDATED;
                        DISP_BldrUpdSet();
                        SYS_UpdateSuccessSet();
                    }
                    LOGGER_Write();
                    break;
                }

                case RESTART_CTRL:
                {
                    posrst = 0x1U;
                    break;
                }

                case START_BLDR:
                {
                    ee_buff[0] = BLDR_CMD_RUN;
                    EEPROM_Save(EE_FW_UPDATE_STATUS, &ee_buff[0], 1U);
                    BackupOldFirmware();
                    posrst = 0x1U;
                    break;
                }

                case SET_BR2OW:
                {
                    respb = OW_RS485_Bridge(rs485_buff);
                    if(respb == ESC) COM_Link = GROUP;   // do not send response to group request
                    else resps = rs485_buff[5];       // response packet size
                    break;  // response, to broadcast or group addressed package
                }

                case SET_BEDDING_REPL:
                {
                    bedd_tim = rs485_buff[1];
                    EEPROM_Save(EE_BEDNG_REPL_ADD, &bedd_tim, 1U);
                    break;
                }

                case GET_LOG_LIST:
                {
                    /* log read fast response fail because i2c driver was busy */
                    if(LOGGER_Read(&rs485_buff[7]) == LOGGER_OK) resps = LOG_DSIZE + 0x2U;
                    break;
                }

                case DEL_LOG_LIST:
                {
                    /* log delete fast response fail because i2c driver was busy */
                    LOGGER_Delete();
                    break;
                }

                case SET_RS485_CFG:
                {
                    rs485_ifa[0] = rs485_buff[1];
                    rs485_ifa[1] = rs485_buff[2];
                    rs485_gra[0] = rs485_buff[3];
                    rs485_gra[1] = rs485_buff[4];
                    rs485_bra[0] = rs485_buff[5];
                    rs485_bra[1] = rs485_buff[6];
                    rs485_bps    = rs485_buff[7];
                    EEPROM_Save(EE_RSIFA, &rs485_buff[1], 7U);
                    break;
                }

                case SET_PERMITED_GROUP:
                {
                    EEPROM_Save(EE_USRGR_ADD, &rs485_buff[1], 16U);
                    break;
                }

                case SET_DISPL_BCKLGHT:
                {
                    lcd_bcklght = (rs485_buff[1] << 8) + rs485_buff[2];
                    if((lcd_bcklght) > 900U) lcd_bcklght = 900U;
                    else if((lcd_bcklght) < 100U) lcd_bcklght = 100U;
                    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, lcd_bcklght);
                    EEPROM_Save(EE_LCD_BRIGHTNESS, &rs485_buff[1], 0x2U);
                    break;
                }

                case SET_ROOM_TEMP:
                {
                    if(rs485_buff[1])
                    {
                        // update setpoint only if new value send
                        temp_sp   = rs485_buff[1];
                        if(temp_sp > 40U) temp_sp = 40U;       // setpoint max. limit
                        else if(temp_sp < 10U) temp_sp = 10U;  // setpoint min. limit
                        EEPROM_Save(EE_ROOM_TEMP_SP, &temp_sp, 0x1U); // save set point
                    }

                    if(rs485_buff[2])
                    {
                        // update difference only if new value send
                        temp_dif  = rs485_buff[2];
                        if(temp_dif > 0x9U) temp_dif = 0x1U;
                        EEPROM_Save(EE_ROOM_TEMP_DIFF, &temp_dif, 0x1U); // save temp. diff.
                    }

                    if(IsTempRegNewCfg(rs485_buff[3]))
                    {
                        // update config only if new value send
                        if(IsTempRegNewSta(rs485_buff[3]))  // is new state config
                        {
                            if(IsTempRegSta(rs485_buff[3])) TempRegOn();
                            else                            TempRegOff();
                        }

                        if(IsTempRegNewMod(rs485_buff[3]))  // is new mode config
                        {
                            if(IsTempRegMod(rs485_buff[3])) TempRegHeating();
                            else                            TempRegCooling();
                        }

                        if(IsTempRegNewCtr(rs485_buff[3]))  // is new controll state
                        {
                            if(IsTempRegCtr(rs485_buff[3])) TempRegEnable();
                            else                            TempRegDisable();
                        }

                        if(IsTempRegNewOut(rs485_buff[3]))  // is new output state
                        {
                            if(IsTempRegOut(rs485_buff[3])) TempRegOutputOn();
                            else                            TempRegOutputOff();
                        }
                        EEPROM_Save(EE_ROOM_TEMP_CFG, &temp_cfg, 0x1U); // save config
                    }

                    rs485_buff[0] = STX;
                    rs485_buff[1] = ow_bra;
                    rs485_buff[2] = ow_ifa;
                    rs485_buff[3] = 4;
                    rs485_buff[4] = SET_ROOM_TEMP; // copy command
                    rs485_buff[5] = temp_sp;
                    rs485_buff[6] = temp_dif;
                    rs485_buff[7] = temp_cfg;
                    respb = OW_RS485_Bridge(rs485_buff);
                    break;
                }

                case SET_SYSTEM_ID:
                {
                    system_id[0] = rs485_buff[1];
                    system_id[1] = rs485_buff[2];
                    EEPROM_Save(EE_SYSID, system_id, 0x2U);
                    break;
                }

                case SET_RTC_DATE_TIME:
                {
                    rdate.WeekDay = rs485_buff[1];
                    if(rdate.WeekDay == 0x0U) rdate.WeekDay = 0x7U;
                    rdate.Date    = rs485_buff[2];
                    rdate.Month   = rs485_buff[3];
                    rdate.Year    = rs485_buff[4];
                    rtime.Hours   = rs485_buff[5];
                    rtime.Minutes = rs485_buff[6];
                    rtime.Seconds = rs485_buff[7];
                    HAL_RTC_SetTime(&hrtc, &rtime, RTC_FORMAT_BCD);
                    HAL_RTC_SetDate(&hrtc, &rdate, RTC_FORMAT_BCD);
                    RtcTimeValidSet();
                    /**
                    *   propagate broadcast packet to onewire
                    *   and radio connected interfaces
                    */
                    rs485_buff[0] = STX;
                    rs485_buff[1] = ow_bra;
                    rs485_buff[2] = ow_ifa;
                    rs485_buff[3] = 0x8U;
                    rs485_buff[4] = SET_RTC_DATE_TIME; // copy command
                    rs485_buff[5] = rdate.WeekDay;
                    rs485_buff[6] = rdate.Date;
                    rs485_buff[7] = rdate.Month;
                    rs485_buff[8] = rdate.Year;
                    rs485_buff[9] = rtime.Hours;
                    rs485_buff[10] = rtime.Minutes;
                    rs485_buff[11] = rtime.Seconds;
                    OW_RS485_Bridge(rs485_buff);
                    break;
                }

                case SET_DOUT_STATE:
                {
                    for(tmp = 0; tmp < 8; tmp++) // set high bits in loop
                    {
                        switch(rs485_buff[tmp + 1])
                        {
                        case '0':   // digital output forced off, output config set to enabled
                            dout_0_7_rem &= (uint16_t)(~(0x1U << tmp));
                            dout_cfg[tmp] = 2;
                            break;
                        case '1':   // digital output forced on, output config set to enabled
                            dout_0_7_rem |= (uint16_t)(0x1U << tmp);
                            dout_cfg[tmp] = 2;
                            break;
                        case '2':   // digital output is enabled and processed in program
                        case '3':   // digital output set to constant disabled state and saved
                        case '4':   // digital output set to constant on state and saved
                        case '5':   // digital output set to constant off state and saved
                            dout_cfg[tmp] = TODEC(rs485_buff[tmp + 1]);
                            break;
                        default:    // if not other defined, default is enabled state
                            dout_cfg[tmp] = 2;
                            break;
                        }
                    }
                    if(rs485_buff[9] == '1') dout_0_7_rem |= (0x1U << 8);       // digital output remote control enable
                    else if(rs485_buff[9] == '0') dout_0_7_rem &= (~(1U << 8)); // digital output remote control disable
                    /*  try to save digital output configuration in eeprom */
                    if(HAL_I2C_IsDeviceReady(&hi2c1, I2CEE_ADD, DRV_TRIAL, DRV_TOUT) != HAL_OK) respb = NAK;
                    else EEPROM_Save(EE_DOUT_CFG_ADD_1, dout_cfg, 8);
                    break;
                }


                case SET_DIN_CFG:
                {
                    for(tmp = 0; tmp < 8; tmp++) // set high bits in loop
                    {
                        switch(rs485_buff[tmp + 1])
                        {
                        case '3':   // digital output set to constant disabled state and saved
                        case '4':   // digital output set to constant on state and saved
                        case '5':   // digital output set to constant off state and saved
                            din_cfg[tmp] = TODEC(rs485_buff[tmp + 1]);
                            break;
                        case '0':
                        case '1':
                        case '2':
                        default:    // if not other defined, default is enabled state
                            din_cfg[tmp] = 2;
                            break;
                        }
                    }
                    /*  try to save digital output configuration in eeprom */
                    if(HAL_I2C_IsDeviceReady(&hi2c1, I2CEE_ADD, DRV_TRIAL, DRV_TOUT) != HAL_OK) respb = NAK;
                    else EEPROM_Save(EE_DIN_CFG_ADD_1, din_cfg, 8);
                    break;
                }

                default:
                {
                    respb = NAK;
                    resps = 0x1U;
                    break;
                }
                }
            }
        }
        else if(packet_type == STX)
        {
            if((next_packet_number == ((rs485_buff[0] << 8) | rs485_buff[1])) && flash_destination)
            {
                if(next_packet_number == 0x1U) CRC_ResetDR();
                FLASH_WritePage(flash_destination, &rs485_buff[2], rs485_pak_dlen - 2U);
                if(!FLASH_WaitReadyStatus(DRV_TOUT)) ErrorHandler(EEPROM_FUNC, SPI_DRV);
                crc_32_calculated = CRC_Calculate8(&rs485_buff[2], rs485_pak_dlen - 2U);
                flash_destination += (rs485_pak_dlen - 2U);
                if(next_packet_number == total_packet_number)
                {
                    if(crc_32_calculated == crc_32_file)
                    {
                        if(activ_cmd == DWNLD_FWR_IMG)
                        {
                            ee_buff[0] = EE_FW_UPDATE_BYTE_CNT >> 8;
                            ee_buff[1] = EE_FW_UPDATE_BYTE_CNT;
                            ee_buff[2] = (total_bytes_in_file >> 24);
                            ee_buff[3] = (total_bytes_in_file >> 16);
                            ee_buff[4] = (total_bytes_in_file >>  8);
                            ee_buff[5] = total_bytes_in_file;
                            if(HAL_I2C_Master_Transmit(&hi2c1, I2CEE_ADD, ee_buff, 0x6U, DRV_TOUT) != HAL_OK)  ErrorHandler(RS485_FUNC, I2C_DRV);
                            if(HAL_I2C_IsDeviceReady(&hi2c1, I2CEE_ADD, DRV_TRIAL, DRV_TOUT) != HAL_OK)        ErrorHandler(RS485_FUNC, I2C_DRV);
                        }
                        else if(post_process == COPY_DISP_IMG)
                        {
                            CopyFile(file_copy_src_addr, file_copy_dest_addr, file_copy_size);
                            SYS_UpdateSuccessSet();
                        }
                        SYS_FileTransferSuccessSet();
                    }
                    else
                    {
                        SYS_FileTransferFailSet();
                        respb = NAK; // something wrong
                    }
                    activ_cmd = 0x0U;
                    post_process = 0x0U;
                    flash_destination = 0x0U;
                    RS485_StopUpdate();
                }
                else ++next_packet_number;
                upd_tout_tmr = HAL_GetTick();
            }
            else respb = NAK; // something wrong
            rs485_buff[0] = next_packet_number;
        }
        if(COM_Link == P2P) RS485_Response(respb, resps);
        if(posrst == 0x1U) BootloaderExe();
        RS485_Init();
    }
}
/**
  * @brief
  * @param
  * @retval
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint32_t i, cksm = 0U;
    uint32_t resps = 1, respb = ACK;

    RS485_StartTimer(RX_TOUT);

    switch(++receive_pcnt)
    {
    case 1:
        if((rec == SOH) || (rec == STX)) packet_type = rec;
        break;
    case 2:
    case 4:
        rs485_buff[0] = rec;
        break;
    case 3:
        if((rs485_buff[0] == rs485_ifa[0]) && (rec == rs485_ifa[1]))  COM_Link = P2P;
        else if((rs485_buff[0] == rs485_gra[0]) && (rec == rs485_gra[1]))  COM_Link = GROUP;
        else if((rs485_buff[0] == rs485_bra[0]) && (rec == rs485_bra[1]))  COM_Link = BROADCAST;
        else
        {
            receive_pcnt = 0;
            RS485_StartTimer(MUTE_DEL);
            return;
        }
        break;
    case 5:
        rs485_sender_addr = (rs485_buff[0] << 8) | rec;
        break;
    case 6:
        rs485_pak_dlen = rec;
        break;
    case 7:
    case 8:
        receive_pcnt = 7;
        rs485_buff[rec_bcnt++] = rec;
        if(rec_bcnt == (rs485_pak_dlen + 3))
        {
            --rec_bcnt;
            RS485_StopTimer();
            for(i = 0; i != rs485_pak_dlen; i++)
            {
                cksm += rs485_buff[i];
            }
            if(((cksm >> 0x8U) == rs485_buff[rec_bcnt - 2])
                    && ((cksm & 0xFFU) == rs485_buff[rec_bcnt - 1])
                    && (EOT            == rs485_buff[rec_bcnt]))
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




    if(eComState == COM_PACKET_RECEIVED)
    {
        if(packet_type == SOH)
        {
            switch(rs485_buff[0])
            {
            case DWNLD_JRNL:
            {
                ZEROFILL(jrnl_buf, COUNTOF(jrnl_buf));
                mem_cpy(jrnl_buf, &rs485_buff[1], rs485_pak_dlen);
                break;
            }

            case GET_SYS_STAT:
            {
                resps = 0x9U;
                CharToBinStr((char*)&rs485_buff[7], sys_stat);
                break;
            }



            case RESET_SOS_ALARM:
            {
                if(ROOM_Status == ROOM_SOS_ALARM)
                {
                    ROOM_Status = ROOM_RESET_ALARM;
                    StatUpdSet();
                }
                break;
            }


            case PREVIEW_DISPL_IMG:
            {
                DISP_PreviewImage();
                break;
            }

            case UPD_FW_INFO:
            {
                appinf.ld_addr = FW_ADDR;
                newinf.size    = ((rs485_buff[1] << 24) | (rs485_buff[2] << 16) | (rs485_buff[3] << 8) | rs485_buff[4]);
                newinf.crc32   = ((rs485_buff[5] << 24) | (rs485_buff[6] << 16) | (rs485_buff[7] << 8) | rs485_buff[8]);
                newinf.version = ((rs485_buff[9] << 24) | (rs485_buff[10] << 16) | (rs485_buff[11] << 8) | rs485_buff[12]);
                newinf.wr_addr = ((rs485_buff[13] << 24) | (rs485_buff[14] << 16) | (rs485_buff[15] << 8) | rs485_buff[16]);
                if(ValidateFwInfo(&newinf))           ResetFwInfo(&newinf);
                else if(GetFwInfo(&appinf))                ResetFwInfo(&appinf);
                else if(!IsNewFwUpdate(&appinf, &newinf))  SYS_FwrUpdRequestSet();
                break;
            }

            case GET_APPL_STAT:
            {
                mem_set(&rs485_buff[7], 'X', 52U);// sensor error
                rs485_buff[7] = TOCHAR(ROOM_Status);
                if(ROOM_Status > 9) rs485_buff[7] += 7U;  // convert hex to char
                if(IsTempSenDS18Active() || IsTempLuxRTActive())
                {
                    if(temp_mv < 0)
                    {
                        rs485_buff[8] = '-';
                        Int2Str((char*)&rs485_buff[9], (temp_mv * -10), 3U);
                    }
                    else
                    {
                        rs485_buff[8] = '+';
                        Int2Str((char*)&rs485_buff[9], (temp_mv * 10), 3U);
                    }
                    if(IsTempRegEnabled())    rs485_buff[12] = 'E';
                    else                            rs485_buff[12] = 'D';
                    if(IsTempRegHeating())    rs485_buff[13] = 'H';
                    else if(IsTempRegCooling())    rs485_buff[13] = 'C';
                    Int2Str((char*)&rs485_buff[14], temp_sp, 2U);
                    Int2Str((char*)&rs485_buff[16], temp_dif, 2U);
                }


                CharToBinStr((char*)&rs485_buff[18], din_0_7);  // write din register state
                CharToBinStr((char*)&rs485_buff[26], dout_0_7); // write dout register state
                i = 0;
                while(i < 8U)
                {
                    if(din_cfg[i]  > 2U) rs485_buff[18 + i] = TOCHAR(din_cfg[i]);       // rewrite din state with config value
                    else if(din_cfg[i]  < 2U) rs485_buff[18 + i] = 'X';                 // or mark state invalid with char x
                    if(dout_cfg[i] > 2U) rs485_buff[26 + i] = TOCHAR(dout_cfg[i]);      // rewrite dout state with cofig value
                    else if(dout_cfg[i] < 2U) rs485_buff[26 + i] = 'X';                 // or markk as invalid with char x
                    ++i;
                }
                rs485_buff[34] = 'R';
                rs485_buff[35] = 'C';
                rs485_buff[42] = (version >> 16);
                rs485_buff[43] = (version >>  8);
                rs485_buff[44] = version;
                Hex2Str((char*)&rs485_buff[36], &rs485_buff[42], 0x6U);
                Int2Str((char*)&rs485_buff[42], bedd_tim, 2);
                rs485_buff[44] = TOCHAR(ow_dev);
                if(ow_dev)
                {
                    Int2Str((char*)&rs485_buff[46], rt_img, 0x2U);
                    rs485_buff[48] = rs485_buff[14];
                    rs485_buff[49] = rs485_buff[15];
                    rs485_buff[50] = rs485_buff[9];
                    rs485_buff[51] = rs485_buff[10];
                    rs485_buff[54] = rs485_buff[12];
                    rs485_buff[55] = rs485_buff[13];
                    rs485_buff[56] = '3';
                }
                rs485_buff[57] = 0x0U;
                resps = 52U;
                break;
            }

            case SET_APPL_STAT:
            {
                if(rs485_buff[1] > 0xBU) respb = NAK;
                else
                {
                    ROOM_Status = (ROOM_StatusTypeDef) rs485_buff[1];
                    StatUpdSet();
                }
                break;
            }

            case GET_LOG_LIST:
            {
                /*  try to read log with fast response   */
                if(HAL_I2C_IsDeviceReady(&hi2c1, I2CEE_ADD, DRV_TRIAL, DRV_TOUT) == HAL_OK)
                {
                    if(LOGGER_Read(&rs485_buff[7]) == LOGGER_OK) resps = LOG_DSIZE + 0x2U;  // try to read lod
                    break;
                }
            }

            case DEL_LOG_LIST:
            {
                /*  try to delete log with fast response   */
                if(HAL_I2C_IsDeviceReady(&hi2c1, I2CEE_ADD, DRV_TRIAL, DRV_TOUT) == HAL_OK)
                {
                    LOGGER_Delete();
                    break;
                }
            }

            case SET_ROOM_TEMP:
            {
                if(HAL_I2C_IsDeviceReady(&hi2c1, I2CEE_ADD, DRV_TRIAL, DRV_TOUT) == HAL_OK)
                {
                    if(rs485_buff[1])
                    {
                        // update setpoint only if new value send
                        temp_sp   = rs485_buff[1];
                        if(temp_sp > 40U) temp_sp = 40U;       // setpoint max. limit
                        else if(temp_sp < 10U) temp_sp = 10U;  // setpoint min. limit
                        EEPROM_Save(EE_ROOM_TEMP_SP, &temp_sp, 0x1U); // save set point
                    }

                    if(rs485_buff[2])
                    {
                        // update difference only if new value send
                        temp_dif  = rs485_buff[2];
                        if(temp_dif > 0x9U) temp_dif = 0x9U;
                        EEPROM_Save(EE_ROOM_TEMP_DIFF, &temp_dif, 0x1U); // save temp. diff.
                    }

                    if(IsTempRegNewCfg(rs485_buff[3]))
                    {
                        // update config only if new value send
                        if(IsTempRegNewSta(rs485_buff[3]))  // is new state config
                        {
                            if(IsTempRegSta(rs485_buff[3])) TempRegOn();
                            else                            TempRegOff();
                        }

                        if(IsTempRegNewMod(rs485_buff[3]))  // is new mode config
                        {
                            if(IsTempRegMod(rs485_buff[3])) TempRegHeating();
                            else                            TempRegCooling();
                        }

                        if(IsTempRegNewCtr(rs485_buff[3]))  // is new controll state
                        {
                            if(IsTempRegCtr(rs485_buff[3])) TempRegEnable();
                            else                            TempRegDisable();
                        }

                        if(IsTempRegNewOut(rs485_buff[3]))  // is new output state
                        {
                            if(IsTempRegOut(rs485_buff[3])) TempRegOutputOn();
                            else                            TempRegOutputOff();
                        }
                        EEPROM_Save(EE_ROOM_TEMP_CFG, &temp_cfg, 0x1U); // save config
                    }

                    rs485_buff[0] = STX;
                    rs485_buff[1] = ow_bra;
                    rs485_buff[2] = ow_ifa;
                    rs485_buff[3] = 0x2U;
                    rs485_buff[4] = SET_ROOM_TEMP; // copy command
                    rs485_buff[5] = temp_sp;
                    respb = OW_RS485_Bridge(rs485_buff);
                    break;
                }
            }

            case SET_DOUT_STATE:
            case SET_DIN_CFG:
            case UPDATE_BLDR:
            case START_BLDR:
            case SET_RTC_DATE_TIME:
            case SET_BEDDING_REPL:
            case SET_RS485_CFG:
            case SET_DISPL_BCKLGHT:
            case SET_PERMITED_GROUP:
            case SET_SYSTEM_ID:
            case RESTART_CTRL:
            case SET_BR2OW:
            default:
            {
                return;
            }
            }

            if(COM_Link == P2P) RS485_Response(respb, resps);
            RS485_Init();
        }
    }
    else HAL_UART_Receive_IT(&huart1, &rec, 1);
}
/**
  * @brief
  * @param
  * @retval
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{

}
/**
  * @brief
  * @param
  * @retval
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    __HAL_UART_CLEAR_PEFLAG(&huart1);
    __HAL_UART_CLEAR_FEFLAG(&huart1);
    __HAL_UART_CLEAR_NEFLAG(&huart1);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    __HAL_UART_FLUSH_DRREGISTER(&huart1);
    huart->ErrorCode = HAL_UART_ERROR_NONE;
    receive_pcnt = 0x0U;
}

/**
  * @brief
  * @param
  * @retval
  */
static void RS485_Response(uint8_t resp, uint8_t size)
{
    uint32_t bc, pakcksm = 0x0U;
    rs485_buff[6] = rs485_buff[0];
    rs485_buff[5] =	size;
    rs485_buff[4] = rs485_ifa[1];
    rs485_buff[3] = rs485_ifa[0];
    rs485_buff[2] = rs485_sender_addr;
    rs485_buff[1] = rs485_sender_addr >> 8;
    rs485_buff[0] = resp;

    for(bc = 0x6U; bc < (rs485_buff[5] + 0x6U); bc++)
    {
        pakcksm += rs485_buff[bc];
    }
    rs485_buff[rs485_buff[5] + 6] = (pakcksm >> 8);
    rs485_buff[rs485_buff[5] + 7] = pakcksm;
    rs485_buff[rs485_buff[5] + 8] = EOT;
    while((HAL_GetTick() - rxtx_tmr) < RX2TX_DEL)
    {
    }
    rxtx_tmr = 0x0U;
    HAL_GPIO_WritePin(RS485_DIR_Port, RS485_DIR_Pin, GPIO_PIN_SET);
    if(HAL_UART_Transmit(&huart1, rs485_buff, rs485_buff[5] + 9, RESP_TOUT) != HAL_OK) ErrorHandler(RS485_FUNC, USART_DRV);
    HAL_GPIO_WritePin(RS485_DIR_Port, RS485_DIR_Pin, GPIO_PIN_RESET);
}
/**
  * @brief
  * @param
  * @retval
  */
static void FormatFileStorage(uint32_t start_address, uint8_t number_of_blocks)
{
    while(number_of_blocks)
    {
        FLASH_WriteStatusRegister(0);
        if(!FLASH_WaitReadyStatus(DRV_TOUT))               ErrorHandler(RS485_FUNC, SPI_DRV);
        FLASH_UnprotectSector(start_address);
        if(!FLASH_WaitReadyStatus(DRV_TOUT))               ErrorHandler(RS485_FUNC, SPI_DRV);
        FLASH_Erase(start_address, FLASH_64K_BLOCK_ERASE);
        if(!FLASH_WaitReadyStatus(FLASH_64K_ERASE_TIMEOUT))ErrorHandler(RS485_FUNC, SPI_DRV);
        start_address += 0x10000U;
        --number_of_blocks;
    }
}
/**
  * @brief
  * @param
  * @retval
  */
static void CopyFile(uint32_t source_address, uint32_t destination_address, uint32_t size)
{
    uint8_t buff[BUFF_SIZE];
    uint32_t cnt;

    cnt = size / 0x10000U;
    if(size > (cnt * 0x10000U)) ++cnt;
    FormatFileStorage(destination_address, cnt);

    while(size)
    {
        if(size >= BUFF_SIZE)
        {
            cnt = BUFF_SIZE;
            size -= BUFF_SIZE;
        }
        else
        {
            cnt = size;
            size = 0x0U;
        }

        FLASH_ReadPage(source_address, buff, cnt);
        FLASH_WritePage(destination_address, buff, cnt);
        if(!FLASH_WaitReadyStatus(DRV_TOUT)) ErrorHandler(RS485_FUNC, SPI_DRV);
        destination_address += cnt;
        source_address += cnt;
    }
}
/**
  * @brief
  * @param
  * @retval
  */
static uint8_t FLASH_If_Write(uint32_t destination, uint32_t *p_source, uint32_t size)
{
    uint32_t i = 0x0U;

    /* Unlock the Flash to enable the flash control register access *************/
    HAL_FLASH_Unlock();

    for(i = 0x0U; (i < size) && (destination <= (FLASH_END_ADDR - 0x4U)); i++)
    {
        /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
        be done by word */
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, destination, *(uint32_t*)(p_source + i)) == HAL_OK)
        {
            /* Check the written value */
            if(*(uint32_t*)destination != *(uint32_t*)(p_source + i))
            {
                /* Flash content doesn't match SRAM content */
                return(0x1U);
            }
            /* Increment FLASH destination address */
            destination += 0x4U;
        }
        else
        {
            /* Error occurred while writing data in Flash memory */
            return (0x1U);
        }
    }
    /* Lock the Flash to disable the flash control register access (recommended
    to protect the FLASH memory against possible unwanted operation) *********/
    HAL_FLASH_Lock();

    return (0x0U);
}
/**
  * @brief
  * @param
  * @retval
  */
static uint8_t UpdateBootloader(void)
{
    uint8_t buff[2048U];
    FLASH_EraseInitTypeDef FLASH_EraseInit;
    uint32_t page_erase_error;
    uint32_t fl_destination;
    uint32_t fl_address;
    uint32_t ram_source;
    uint32_t file_size;
    uint32_t bcnt;

    HAL_FLASH_Unlock();
    FLASH_EraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    FLASH_EraseInit.PageAddress = RC_BLDR_ADDR;
    FLASH_EraseInit.Banks = FLASH_BANK_1;
    FLASH_EraseInit.NbPages = 12U;
    if(HAL_FLASHEx_Erase(&FLASH_EraseInit, &page_erase_error) != HAL_OK) return (0x1U);
    HAL_FLASH_Lock();

    fl_destination = RC_BLDR_ADDR;
    fl_address = EE_RC_NEW_BLDR_ADDR;
    file_size = RC_BLDR_SIZE;

    while(file_size)
    {
        if(file_size >= 2048U) bcnt = 2048U;
        else bcnt = file_size;

        FLASH_ReadPage(fl_address, buff,  bcnt);
        ram_source = (uint32_t) buff;
        if(FLASH_If_Write(fl_destination, (uint32_t*) ram_source, bcnt / 4U)) return (0x1U);
        fl_destination += bcnt;
        fl_address += bcnt;
        file_size -= bcnt;
    }
    return (0x0U);
}
/**
  * @brief
  * @param
  * @retval
  */
static void BackupOldFirmware(void)
{
    uint8_t *flsrc;
    uint8_t bkpbuf[BUFF_SIZE];
    uint32_t fldst = EE_OLD_FW_ADDR;
    uint32_t bcnt = 0x0U;
    uint32_t btot = 0x0U;
    flsrc = (uint8_t *)RC_APPL_ADDR;
    FormatFileStorage(EE_OLD_FW_ADDR, 1);

    while(btot < RC_APPL_SIZE)
    {
        while(bcnt < sizeof(bkpbuf))
        {
            bkpbuf[bcnt] = *flsrc;
            ++flsrc;
            ++bcnt;
            ++btot;
        }
        FLASH_WritePage(fldst, bkpbuf, sizeof(bkpbuf));
        if(!FLASH_WaitReadyStatus(DRV_TOUT)) ErrorHandler(RS485_FUNC, SPI_DRV);
        fldst += sizeof(bkpbuf);
        bcnt = 0x0U;
    }
}
/************************ (C) COPYRIGHT JUBERA D.O.O Sarajevo ************************/
