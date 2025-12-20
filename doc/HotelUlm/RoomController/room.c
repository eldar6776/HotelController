/**
 ******************************************************************************
 * File Name          : signaling.c
 * Date               : 28/02/2016 23:16:19
 * Description        : audio visual signaling software modul
 ******************************************************************************
 *
 *
 ******************************************************************************
 */
 
 #if (__ROOM_H__ != FW_BUILD)
    #error "room header version mismatch"
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
/* Defines    ----------------------------------------------------------------*/
#define CARD_RX_TOUT            300U    // card reader data transfer timeout
#define CARD_OK_TOUT            3210U   // 3 s reader unlisten time after card read
#define CARD_FAIL_TOUT          987U    // ~1 s reader unlisten time after card read
#define CARD_PWRUP_TOUT         8901U	// 8 s on power up reader is disbled 
#define CARD_HMRST_TOUT         2345U	// reset handmaid card status
#define SYS_INIT_TIME     8765U   // 8s application startup time
/** ==========================================================================*/
/**			M I F A R E		C A R D		S E C T O R		A D D R E S S E		  */
/** ==========================================================================*/
#define SECTOR_0                0x00U
#define SECTOR_1                0x04U
#define SECTOR_2                0x08U
#define SECTOR_3                0x0CU
#define SECTOR_4                0x10U
#define SECTOR_5                0x14U
#define SECTOR_6                0x18U
#define SECTOR_7                0x1CU
#define SECTOR_8                0x20U
/* Types  --------------------------------------------------------------------*/
typedef enum 
{
	MI_OK 			= 0x0U,
	MI_NOTAGERR		= 0x1U,
    MI_ERR			= 0x2U,
	MI_SKIP_OVER	= 0x3U

} RC522_StatusTypeDef;


RC522_SectorTypeDef sector_0;
RC522_SectorTypeDef sector_1;
RC522_SectorTypeDef sector_2;
RC522_CardDataTypeDef sCard;
RC522_CardDataTypeDef sExtCard;
__IO ROOM_StatusTypeDef ROOM_Status = ROOM_IDLE;
__IO ROOM_StatusTypeDef ROOM_PreStatus = ROOM_IDLE;
/* Macros     ----------------------------------------------------------------*/
/* Variables  ----------------------------------------------------------------*/
__IO uint32_t roomfl;
__IO uint32_t thstfl;
__IO uint32_t unix_rtc;
__IO uint32_t unix_room;
uint8_t crsta;
int16_t room_temp;
int16_t room_ntc_temp;
int8_t  ntc_offset;
uint8_t thst_sp;
uint8_t thst_dif;
uint8_t thst_min_sp;
uint8_t thst_max_sp;
uint8_t bdng_per;
uint8_t bdng_cnt;
uint8_t serial[6]; 
uint8_t cardkeya[6];
uint8_t cardkeyb[6];
uint8_t rxbuf[68];
/* Private prototypes    -----------------------------------------------------*/
static void CARD_Verify(void);
static void CARD_Service(void);
static void TEMP_Controller(void);
/* Program code   ------------------------------------------------------------*/
/**
  * @brief
  * @param
  * @retval
  */
void ROOM_Init(void)
{
    crsta = 0;
    CardDataRdyReset();
    HAL_UART_Receive_IT(&huart4, rxbuf, sizeof(rxbuf));
}
/**
  * @brief
  * @param
  * @retval
  */
void ROOM_Service(void)
{
    uint8_t brcnt, brtim, buf[8], wrsta = 100; // max 100 write trials
	static uint8_t _min = 60U;  // enable first entry
    static uint32_t init = 0;   // enable first entry
    static uint32_t crtmr  = 0; // card reader timer
    static uint32_t sysinit = 0;
    
    if (!sysinit) sysinit = HAL_GetTick();
    else if (IsSysInitActiv())
    {
        if ((HAL_GetTick() - sysinit) >= SYS_INIT_TIME)
        {
            SysInitReset();
            if      (IsRoomCtrlConfig()) DISPGuiSet(60);
            else if (IsRoomThstConfig()) DISPGuiSet(0);
        }
    } 
    if (IsRS485_UpdateActiv()) return;
    if (eComState == COM_PACKET_RECEIVED) RS485_Service();
    /****************************************************/
    /*      C H E C K       C A R D     R E A D E R     */
    /****************************************************/    
    if (IsRoomCtrlConfig())
    {
        if (crsta && ((HAL_GetTick() - crtmr) >= CARD_RX_TOUT))
        {
            crsta = 0;
            CARD_Clear();
            CardDataRdyReset();
            crtmr = HAL_GetTick();
            HAL_UART_Receive_IT(&huart4, rxbuf, sizeof(rxbuf));
        }
        else CARD_Service();
    }
    /****************************************************/
    /*      C H E C K       D O O R     B E L L         */
    /****************************************************/  
    if (IsRoomCtrlConfig())
    {
        if (!IsExtDoorLockActiv() && IsExtDoorLockEnabled())
        {
            CARD_Clear();
            DoorLockOn();
            ExtDoorLockSet();
            DISPDoorOpenSet();
            DooLockTimeSet();
            BUZZ_State = BUZZ_DOOR_BELL;
            LogEvent.log_event = DOOR_LOCK_USER_OPEN;
            LOGGER_Write();
        }
        else if (!IsExtDoorLockEnabled()) ExtDoorLockReset();
    }
    
    if (IsRoomThstConfig())
    {
        if (IsExtBTNBellEnabled() && ((disp_img_id == 0) || (disp_img_id > 9)))
        {
            ExtBTNBellSet();
            DISPGuiSet(1);
            disp_img_time = 1U;
        }
        else if (!IsExtBTNBellEnabled()) ExtBTNBellReset();
    }
    /** ================================================*/
	/**     C A R D   S T A C K E R   S T A T E         */
	/** ================================================*/
    if      (IsCardStackerEnabled() 
    ||      IsExtCardStackerEnabled() 
    ||      IsOWCardStackerEnabled() 
    ||      IsExtRoomPowerEnabled())
    {
        if (IsCardStackerEnabled() && !IsCardStackerActiv())   
        {
            CardStackerSet();
            OWStatUpdSet();
        }
        if (IsExtCardStackerEnabled() && !IsExtCardStackerActiv())   
        {
            ExtCardStackerSet();
            OWStatUpdSet();
        }
        if (IsOWCardStackerEnabled() && !IsOWCardStackerActiv()) 
        {
            OWCardStackerSet();
            OWStatUpdSet();
        }
        if (IsExtRoomPowerEnabled() && !IsExtRoomPowerActiv()) 
        {
            ExtRoomPowerSet();
            OWStatUpdSet();
        }
        if (!IsRoomPowerActiv() && !dout_0_7_rem)
        {
            RoomPowerOn();
            if (ROOM_Status == ROOM_READY) ROOM_Status = ROOM_BUSY;
            LogEvent.log_event = CARD_STACKER_ON;
            LOGGER_Write();  
            OWStatUpdSet();
            LEDRedOn();          
        }
        if (ROOM_Status != ROOM_CLEANING_RUN) DNDPanelPwrOn();        
        BTNBellEnable();
    }
    else if (!IsCardStackerEnabled() 
    &&      !IsExtCardStackerEnabled() 
    &&      !IsOWCardStackerEnabled() 
    &&      !IsExtRoomPowerEnabled())
    {
        if (!IsCardStackerEnabled() && IsCardStackerActiv())   
        {
            CardStackerReset();
            OWStatUpdSet();
        }
        if (!IsExtCardStackerEnabled() && IsExtCardStackerActiv())   
        {
            ExtCardStackerReset();
            OWStatUpdSet();
        }
        if (!IsOWCardStackerEnabled() && IsOWCardStackerActiv()) 
        {
            OWCardStackerReset();
            OWStatUpdSet();
        }
        if (!IsExtRoomPowerEnabled() && IsExtRoomPowerActiv()) 
        {
            ExtRoomPowerReset();
            OWStatUpdSet();
        }
        if (IsRoomPowerActiv() && !dout_0_7_rem)
        {        
            RoomPowerOff();
            LogEvent.log_event = CARD_STACKER_OFF;
            LOGGER_Write();
            OWStatUpdSet(); 
            LEDRedOff();
        }
        BTNBellDisable();
    }
    /** ================================================*/
	/**   E X T E R N A L    S W I T C H    S T A T E   */
	/** ================================================*/
    if      (IsHVACCtrlEnabled() || IsExtHVACCtrlEnabled())
    {
        if (IsHVACCtrlEnabled() && !IsHVACCtrlActiv())    
        {
            HVACCtrlSet();
            OWStatUpdSet();
        }
        if (IsExtHVACCtrlEnabled() && !IsExtHVACCtrlActiv()) 
        {
            ExtHVACCtrlSet();
            OWStatUpdSet();
        }
        if (!IsHVACPowerActiv() && !dout_0_7_rem)
        {        
            HVACPowerOn();
            LogEvent.log_event = BALCON_DOOR_CLOSED;
            LOGGER_Write();
            OWStatUpdSet();
        }
    }
    else if (!IsHVACCtrlEnabled() && !IsExtHVACCtrlEnabled())
    {
        if (!IsHVACCtrlEnabled() && IsHVACCtrlActiv())   
        {
            HVACCtrlReset();
            OWStatUpdSet();
        }
        if (!IsExtHVACCtrlEnabled() && IsExtHVACCtrlActiv()) 
        {
            ExtHVACCtrlReset();
            OWStatUpdSet();
        }
        if (IsHVACPowerActiv() && !dout_0_7_rem)
        {  
            HVACPowerOff();
            LogEvent.log_event = BALCON_DOOR_OPENED;
            LOGGER_Write();
            OWStatUpdSet();    
        }
    }    
    /****************************************************/
    /*      C H E C K       R O O M     P O W E R       */
    /****************************************************/
    if (IsRoomCtrlConfig() && unix_rtc && unix_room)
    {   /* skeep room power timer check till valid rtc time */
        if (unix_room <= unix_rtc)
        {
            unix_room = 0;
            if (IsRoomPowerActiv())
            {        
                RoomPowerOff();
                LogEvent.log_event = ROOM_TIME_POWER_OFF;
                LOGGER_Write();
                OWStatUpdSet();   
            }
        }
    }      
    /** ================================================*/
	/**     T E M P E R A T U R E   R E G U L A T O R   */
	/** ================================================*/
    if (IsHVACPowerActiv()
    &&  IsRoomPowerActiv())         TempRegEnable();    // if all condition true, enable output controll
    else                            TempRegDisable();
    TEMP_Controller();                                  // execute controll loop if regulator enabled
    if (IsTempRegActiv()
    &&  IsTempRegEnabled() 
    &&  IsTempRegOutputActiv())     HVACRequestOn();     // energize cool/heat actuator
    else                            HVACRequestOff();    // turn off cool/heat actuator
    /** ================================================*/
	/**     T H E R M O S T A T     S T A T U S         */
	/** ================================================*/
    if      (IsRoomThstConfig() && !IsTempRegEnabled() && (disp_img_id == 0U))  DISPGuiSet(10);  // enable thermostat if remote hvac contacter closed
    else if (IsRoomThstConfig() &&  IsTempRegEnabled() && (disp_img_id == 10U)) DISPGuiSet(0); // disable thermostat if remote hvac contacter open
    /****************************************************/
	/*	            R O O M         C L E A N I N G		*/
	/****************************************************/
	if (IsRoomThstConfig() && (_min != rtctm.Minutes))
	{
		_min = rtctm.Minutes;
		if(((ROOM_Status    == ROOM_BUSY)
        ||  (ROOM_Status    == ROOM_CLEANING_REQ)
        ||  (ROOM_Status    == ROOM_BEDDING_REQ))
        &&  (rtctm.Hours    == ROOM_CLEANING_TIME) 
        &&  (rtctm.Minutes  == 0)
        &&  IsPwrExpTimeActiv())
		{	/* compare both timers rounded to minute */
            if ((unix_room & 0xFFFFFFC0U) <= (unix_rtc & 0xFFFFFFC0U)) 
            {   /*  room general cleaning flag is set when room power timer expire */
                EE_WriteBuffer(&buf[0], EE_BEDNG_CNT_ADD, 1);
                ROOM_Status = ROOM_GENERAL_REQ;
                PwrExpTimeReset();  // this is only place to reset this flag
            }
            else if ((unix_room & 0xFFFFFFC0U) > (unix_rtc & 0xFFFFFFC0U))
            {   /* room cleaning request flag is daily set at 8 AM */
                ROOM_Status = ROOM_CLEANING_REQ;
                EE_ReadBuffer (&brtim, EE_BEDNG_REPL_ADD, 1);
                if (brtim)
                {   /*  check is bedding period set */
                    EE_ReadBuffer (&brcnt, EE_BEDNG_CNT_ADD,  1);
                    if (++brcnt >= brtim)
                    {   /* load, increase and check bedding counter */
                        brcnt = 0;   // reset counter if overload
                        ROOM_Status = ROOM_BEDDING_REQ; // set flag
                    }
                    EE_WriteBuffer(&brcnt, EE_BEDNG_CNT_ADD, 1);// write counter back to memory
                }
            }
		}		
	}
    /****************************************************/
	/*		        R O O M         S T A T U S         */
	/****************************************************/
    if (IsRoomCtrlConfig() && ((ROOM_PreStatus != ROOM_Status) || !init))
	{
        DISPRefreshSet(); // Refresh Display Screen
        RoomCleaningReset();
        ROOM_PreStatus = ROOM_Status;
        if (ROOM_Status != ROOM_CLEANING_RUN)
        {
            DISPCleanUpImgDel();
            DISPGenClnImgDelete();
            DISPBeddRepImgDelete();
        }        
        if (!init) wrsta = 0;
        init = 1;
        
        switch (ROOM_Status)
        {
            case ROOM_IDLE:
            {
                ZEROFILL(buf, sizeof(buf));
                EE_WriteBuffer(buf, EE_BEDNG_CNT_ADD, 2);
                EE_WriteBuffer(buf, EE_ROOM_PWRTOUT, 6);
                buf[2] = (sysfl >> 8);  // keep thermostat settings
                buf[3] = (sysfl & 0xff);// keep system state flags
                EE_WriteBuffer(buf, EE_THST_FLAGS, 4);
                ROOM_Status = ROOM_READY;
                TSCleanReset(); // enable touch screeen
                unix_room = 0; // clear room power timer
                RoomPowerOff(); // shutdown room power contactor
                dispfl = 0; // clear all display flags
                roomfl = 0; // clear all room flags
                SysInitSet(); // skeep init timer
                wrsta = 0;
                break;
            }	
            
            case ROOM_READY:
            {
                break;
            }
            
            case ROOM_BUSY:
            {
                break;
            }
            
            case ROOM_CLEANING_REQ:
            {
                DISPCleanUpImage();
                break;
            }
            
            case ROOM_BEDDING_REQ:
            {
                DISPBeddRepImg();
                break;
            }
            
            case ROOM_GENERAL_REQ:
            {
                DISPGeneralCleanUpImg();
                break;
            }
            
            case ROOM_CLEANING_RUN:    
            {
                RoomCleaningSet();
                wrsta = 0; // skipp writing new status
                break;
            }            
            
            case ROOM_UNUSABLE:
            {
                DISPOutOfSrvcImgSet();
                unix_room = 0; // clear room power timer
                break;
            }
            
            case ROOM_SOS_ALARM:            
            case ROOM_FIRE_ALARM:            
            case ROOM_FIRE_EXIT:            
            case ROOM_RESET_ALARM:            
            default:
                ROOM_Status = ROOM_IDLE;
                wrsta = 0; // skipp writing new status
                break;
        }
        
        while (wrsta)
        {
            --wrsta;
            buf[0] = ROOM_PreStatus;
            buf[1] = ROOM_Status;
            EE_WriteBuffer(&buf[0], EE_ROOM_PRESTAT_ADD, 2);
            EE_ReadBuffer (&buf[2], EE_ROOM_PRESTAT_ADD, 2);
            if ((buf[0] == buf[2]) && (buf[1] == buf[3])) break;
            else if (!wrsta) ErrorHandler(EEPROM_FUNC, I2C_DRV);
        }
	}
}
/**
  * @brief
  * @param
  * @retval
  */
static void TEMP_Controller(void)
{	
    /** ============================================================================*/
	/**    C A L C U L A T E   R O O M   T E M P E R A T U R E   R E G U L A T O R 	*/
	/** ============================================================================*/
    if      (IsTempRegHeating())
    {
        if     ((room_temp / 10) >= thst_sp)             TempRegOutputOff();
        else if((room_temp / 10) < (thst_sp - thst_dif)) TempRegOutputOn();	
    }
    else if (IsTempRegCooling())
    {
        if     ((room_temp / 10) > (thst_sp + thst_dif)) TempRegOutputOn();
        else if((room_temp / 10) < thst_sp)              TempRegOutputOff();
    }
}
/**
  * @brief
  * @param
  * @retval
  */
static void CARD_Service(void)
{	
    uint8_t ebuff[8];
    RTC_TimeTypeDef untm;
    RTC_DateTypeDef undt;
    static uint32_t mifare_time = 0U;    
	static uint32_t mifare_timer = 0U;
    static uint32_t handmaid_card_time = 0U;    
    static uint32_t handmaid_card_timer = 0;
	static uint8_t handmaid_card_cycles = 0U;
    
    if (IsDooLockTimeActiv())
    {
        DooLockTimeReset();
        mifare_timer = HAL_GetTick(); 
        mifare_time = CARD_OK_TOUT;
    }
    
	if ((HAL_GetTick() - handmaid_card_timer) >= handmaid_card_time)
	{
		if ((handmaid_card_cycles == 0x1U) && (ROOM_Status == ROOM_CLEANING_RUN)) RoomReentranceEnable();
		handmaid_card_cycles = 0x0U;
	}
    
    if (IsRS485_UpdateActiv()) return;
	if (eComState == COM_PACKET_RECEIVED) RS485_Service();	
    if ((HAL_GetTick() - mifare_timer) >= mifare_time) mifare_timer = HAL_GetTick();
    else return;
    
	if      (IsCardDataRdyActiv())
    {
        CardDataRdyReset();
        CARD_Verify();
		if ((sCard.system_id    != SYSTEMID_INVALID)
		&&	(sCard.system_id    != SYSTEMID_DATA_INVALID)
		&& ((sCard.card_status  == CARD_VALID)
		||	(sCard.user_group   == USERGRP_MANAGER)
		||	(sCard.user_group   == USERGRP_SERVICE)
		||	(sCard.user_group   == USERGRP_MAID)))
		{	
            mem_copy(LogEvent.log_card_id, sCard.card_id, 0x5U);
            if  ((sCard.user_group == USERGRP_MANAGER) || (sCard.user_group == USERGRP_SERVICE))
            {
                if      (sCard.user_group == USERGRP_MANAGER)   LogEvent.log_event = MANAGER_CARD;
                else if (sCard.user_group == USERGRP_SERVICE)   LogEvent.log_event = SERVICE_CARD;
                if (ROOM_Status < ROOM_UNUSABLE) unix_room = unix_rtc + SECONDS_PER_HOUR; // enable room power for one hour
                LOGGER_Write();
            }
			else if (sCard.user_group == USERGRP_MAID)
			{
                LogEvent.log_event = HANDMAID_CARD;
                if (!IsCardStackerActiv() && !IsOWCardStackerActiv() && !IsBTNDndActiv())
                {
                    ++handmaid_card_cycles;
                    BUZZ_State = BUZZ_DOOR_BELL;
                    mifare_time = CARD_FAIL_TOUT;
                    handmaid_card_time = CARD_HMRST_TOUT;
                    handmaid_card_timer = HAL_GetTick();
                    
                    if ((ROOM_Status == ROOM_CLEANING_REQ) 
                    ||  (ROOM_Status == ROOM_BEDDING_REQ) 
                    ||  (ROOM_Status == ROOM_GENERAL_REQ))
                    {
                        handmaid_card_cycles = 0U;
                        unix_room = unix_rtc + SECONDS_PER_HOUR; // enable room power for one hour 
                        ROOM_Status = ROOM_CLEANING_RUN;
                        LOGGER_Write();
                        TSCleanSet(); // only enable touch screen blockade if maid rfid card used
                    }
                    else if (handmaid_card_cycles > 0x2U)
                    {
                        ROOM_Status = ROOM_IDLE; // clear room conter and power timer
                        BUZZ_State = BUZZ_CLEANING_END;
                        mifare_time = CARD_OK_TOUT;
                        LogEvent.log_event = HANDMAID_SERVICE_END;
                        LOGGER_Write();
                        TSCleanReset(); // enable touch screen on room device
                    }
                    else if (ROOM_Status != ROOM_CLEANING_RUN)
                    {   /*  open room with handmaid card if alarm activ*/
                        
                        if (ROOM_Status < ROOM_UNUSABLE)
                        {   
                            handmaid_card_cycles = 0U;
                            unix_room = unix_rtc + SECONDS_PER_HOUR; // enable room power for one hour 
                            LOGGER_Write();
                        }
                    }
                }
                else if (IsCardStackerActiv() || IsOWCardStackerActiv() || IsBTNDndActiv())
                {
                    ++handmaid_card_cycles;
                    handmaid_card_time = CARD_HMRST_TOUT;
                    handmaid_card_timer = HAL_GetTick();
                    if (handmaid_card_cycles > 0x2U)
                    {                        
                        ROOM_Status = ROOM_BUSY;
                        BUZZ_State = BUZZ_CLEANING_END;
                        mifare_time = CARD_OK_TOUT;
                        LogEvent.log_event = HANDMAID_SERVICE_END;
                        LOGGER_Write();
                        TSCleanReset(); // enable touch screen on room device
                    }
                    else
                    {
                        BUZZ_State = BUZZ_DOOR_BELL;
                        mifare_time = CARD_FAIL_TOUT;
                    }
                }
			}
            else if (ROOM_Status == ROOM_UNUSABLE)
            {
                handmaid_card_cycles = 0x1U;
                DISPOutOfSrvcImgSet();
                BUZZ_State = BUZZ_CARD_INVALID;
                mifare_time = CARD_FAIL_TOUT;
            }
            else if (sCard.user_group == USERGRP_GUEST)
            {
                PwrExpTimeSet();    // room power expiry time valid
                undt.Date   = sCard.expiry_time[0];
                undt.Month  = sCard.expiry_time[1];
                undt.Year   = sCard.expiry_time[2];
                untm.Hours  = sCard.expiry_time[3];
                untm.Minutes= sCard.expiry_time[4];
                untm.Seconds= sCard.expiry_time[5];
                unix_room = rtc2unix(&untm, &undt);
                ebuff[0] = unix_room >> 24;
                ebuff[1] = unix_room >> 16;
                ebuff[2] = unix_room >>  8;
                ebuff[3] = unix_room;
                ebuff[4] = 0x0U;
                ebuff[5] = 0x1U;  // room power expiry time set by user
                EE_WriteBuffer(ebuff, EE_ROOM_PWRTOUT, 6);
                if (ROOM_Status == ROOM_READY) ROOM_Status = ROOM_BUSY;
                LogEvent.log_event = GUEST_CARD;
                LOGGER_Write();
            }
            
			if (!handmaid_card_cycles)
			{
				DoorLockOn();
				RoomPowerOn();
				DISPCardValidImage();		
                BUZZ_State = BUZZ_CARD_VALID;
                mifare_time = CARD_OK_TOUT;
			}
            else if (handmaid_card_cycles  > 0x2U) handmaid_card_cycles = 0x0U;
		}
		else
		{
			handmaid_card_cycles = 0U;
            BUZZ_State = BUZZ_CARD_INVALID;
            mifare_time = CARD_FAIL_TOUT;
			
			if (sCard.system_id == SYSTEMID_INVALID) 
			{
				DISPWrongRoomImage();
				LogEvent.log_event = WRONG_SYSID;
			}
			else if(sCard.user_group == USERGRP_GUEST) 
			{
				if (sCard.controller_id == ROOMADDR_INVALID) 
				{
					DISPWrongRoomImage();
					LogEvent.log_event = WRONG_ROOM;
				}
				else if(sCard.expiry_time[0] == EXPIRYTIME_INVALID)
				{
					DISPTimeExpiredImage();
					LogEvent.log_event = CARD_EXPIRED;
				}
				else
				{
					DISPCardInvalidImage();
					LogEvent.log_event = CARDID_INVALID;
				}
			}
			else 
			{
				LogEvent.log_event = UNKNOWN_CARD;
				DISPCardInvalidImage();
			}
			mem_copy(LogEvent.log_card_id, sCard.card_id, 5U);		
			LOGGER_Write();			
		}		
	}
	else if (IsRoomReentranceActiv())
	{
		LOGGER_Write();
		DoorLockOn();
		RoomPowerOn();
		DISPCardValidImage();			
		BUZZ_State = BUZZ_CARD_VALID;
		RoomReentranceDisable();
        mifare_time = CARD_OK_TOUT;
        unix_room += SECONDS_PER_HOUR;
	}
	else 
    {
        if(!(dout_0_7_rem & (0x1U << 6))) DoorLockOff();
        mifare_time = 45;
    }
}
/**
  * @brief
  * @param
  * @retval
  */
void CARD_Clear(void)
{
    sCard.card_status   = 0U;
    sCard.user_group    = 0U;
    sCard.system_id     = 0U;
    sCard.controller_id = 0U;
    ZEROFILL(serial, COUNTOF(serial));
	mem_set (sCard.card_id, 0U, sizeof(sCard.card_id));
	mem_set (sCard.expiry_time, 0U, sizeof(sCard.expiry_time));
    ZEROFILL(sector_0.block_0, COUNTOF(sector_0.block_0));
    ZEROFILL(sector_0.block_1, COUNTOF(sector_0.block_1));
    ZEROFILL(sector_0.block_2, COUNTOF(sector_0.block_2));
    ZEROFILL(sector_1.block_0, COUNTOF(sector_1.block_0));
    ZEROFILL(sector_1.block_1, COUNTOF(sector_1.block_1));
    ZEROFILL(sector_1.block_2, COUNTOF(sector_1.block_2));
    ZEROFILL(sector_2.block_0, COUNTOF(sector_2.block_0));
    ZEROFILL(sector_2.block_1, COUNTOF(sector_2.block_1));
    ZEROFILL(sector_2.block_2, COUNTOF(sector_2.block_2));
}
/**
  * @brief
  * @param
  * @retval
  */
static void CARD_Verify(void)
{
	uint8_t b_cnt= 0U, ebuf[16];
    EE_ReadBuffer(ebuf,EE_USRGR_ADD,16);
    mem_copy(sCard.card_id, sector_0.block_0, 5U);
    /**
	  *			U S E R S  G R O U P   C H E C K
      */
	sCard.user_group = USERGRP_INVALID;
	do
	{
        if (chk_chr((char*)ebuf, sector_1.block_0[b_cnt]) == 0U)
        {
            if((sector_1.block_0[b_cnt] == 0U) || (sector_1.block_0[b_cnt] == 0xFFU))
            {
                sCard.user_group = USERGRP_DATA_INVALID;
            }
            else
            {
                sCard.user_group = sector_1.block_0[b_cnt];
                break;
            }                
        }
        ++b_cnt;
	}
    while(b_cnt < 16U);
	/**
	*			S Y S T E M   I D   C H E C K
	**/
    if (!ISVALIDDEC(sector_1.block_1[0])) sCard.system_id = SYSTEMID_DATA_INVALID;
	else 
    {
        sCard.system_id = Str2Int((char*)sector_1.block_1, 5U);
        if (sCard.system_id != (((sysid[0]) << 8)|sysid[1])) sCard.system_id = SYSTEMID_INVALID;
    }
    /**
	*			C A R D   E X P I R Y   T I M E    C H E C K
	**/
    for(b_cnt = 0U; b_cnt < 6U; b_cnt++)
    {
        if (!ISVALIDBCD(sector_2.block_0[b_cnt]))  sCard.expiry_time[0] = EXPIRYTIME_DATA_INVALID;
    }    
    if (sCard.expiry_time[0] == EXPIRYTIME_DATA_INVALID) sCard.card_status = CARDID_INVALID;
	else if((sector_2.block_0[2] > rtcdt.Year)
    || ((sector_2.block_0[2] == rtcdt.Year) && (sector_2.block_0[1] > rtcdt.Month))
    || ((sector_2.block_0[2] == rtcdt.Year) && (sector_2.block_0[1] == rtcdt.Month) && (sector_2.block_0[0] > rtcdt.Date))
    || ((sector_2.block_0[2] == rtcdt.Year) && (sector_2.block_0[1] == rtcdt.Month) && (sector_2.block_0[0] == rtcdt.Date) && (sector_2.block_0[3] > rtctm.Hours))
    || ((sector_2.block_0[2] == rtcdt.Year) && (sector_2.block_0[1] == rtcdt.Month) && (sector_2.block_0[0] == rtcdt.Date) && (sector_2.block_0[3] == rtctm.Hours)
    &&  (sector_2.block_0[4] >= rtctm.Minutes)))
    {
        mem_copy(sCard.expiry_time, sector_2.block_0, 6U);
        
    }
    else sCard.expiry_time[0] = EXPIRYTIME_INVALID;
    /**
	*			C O N T R O L L E R    A D D R E S S   C H E C K
	**/
    sCard.controller_id = (sector_2.block_0[6] << 8)|sector_2.block_0[7];
	if      ((sCard.controller_id < FST_DEV_RSIFA) || (sCard.controller_id > LST_DEV_RSIFA)) sCard.controller_id = ROOMADDR_DATA_INVALID;
	else if  (sCard.controller_id != ((rsifa[0] << 8)|rsifa[1]))
	{
        sCard.controller_id = ROOMADDR_INVALID;
        EE_ReadBuffer(ebuf,EE_PERM_EXTADD1,16);
        for(b_cnt = 0; b_cnt < 8; b_cnt++)
        {
            if ((sector_2.block_0[6] == ebuf[b_cnt*2]) && (sector_2.block_0[7] == ebuf[(b_cnt*2)+1])) 
            {
                sCard.controller_id = ((sector_2.block_0[6]<<8)|sector_2.block_0[7]);
                b_cnt = 0x8U;
            }
        }
	}
	/**
	*			S E T   C A R D     S T A T U S
	**/
	if ((sCard.user_group       == USERGRP_INVALID)
    ||  (sCard.user_group       == USERGRP_DATA_INVALID)
    ||  (sCard.expiry_time[0]   == EXPIRYTIME_DATA_INVALID)
    ||  (sCard.controller_id    == ROOMADDR_INVALID)
    ||  (sCard.controller_id    == ROOMADDR_DATA_INVALID)
    ||  (sCard.system_id        == SYSTEMID_INVALID)
    ||  (sCard.system_id        == SYSTEMID_DATA_INVALID))
    {
		 sCard.card_status = CARDID_INVALID;
    }
    else if (sCard.expiry_time[0] == EXPIRYTIME_INVALID) sCard.card_status = EXPIRYTIME_INVALID;
    else  sCard.card_status = CARD_VALID;
}
/************************ (C) COPYRIGHT JUBERA D.O.O Sarajevo ************************/
