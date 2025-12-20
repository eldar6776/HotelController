/**
 ******************************************************************************
 * File Name          : signaling.h
 * Date               : 28/02/2016 23:16:19
 * Description        : audio visual signaling software modul header
 ******************************************************************************
 *
 *
 ******************************************************************************
 */
#ifndef __ROOM_H__
#define __ROOM_H__					    FW_BUILD	// version

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx.h"
#include "common.h"
/* Defines    ----------------------------------------------------------------*/
#define ROOM_CLEANING_TIME			        0x08U   // 8h AM when clean up icon display start time
#define NORMAL_CHECK_OUT_TIME		        0x12U   // 									
#define RFID_LED_TOGGLE_TIME		        250U    // 250 ms rfid led normal blink time
#define ROOM_STATUS_TOGGLE_TIME		        500U    // 500 ms status toggle timer for DND modul to reset
#define BLOCK_SIZE                          16U
/* Types  --------------------------------------------------------------------*/
typedef struct
{
	uint8_t card_status;
    uint8_t user_group;
    uint16_t system_id;
    uint16_t controller_id;
	uint8_t expiry_time[6];
	uint8_t card_id[5];
	
}RC522_CardDataTypeDef;

typedef struct
{
    uint8_t block_0[BLOCK_SIZE];
    uint8_t block_1[BLOCK_SIZE];
    uint8_t block_2[BLOCK_SIZE];
	uint8_t block_3[BLOCK_SIZE];
} RC522_SectorTypeDef;

extern RC522_SectorTypeDef sector_0;
extern RC522_SectorTypeDef sector_1;
extern RC522_SectorTypeDef sector_2;
extern RC522_CardDataTypeDef sCard;
extern RC522_CardDataTypeDef sExtCard;
extern __IO ROOM_StatusTypeDef ROOM_Status;
extern __IO ROOM_StatusTypeDef ROOM_PreStatus;
/* Variables  ----------------------------------------------------------------*/
extern __IO uint32_t unix_room;
extern __IO uint32_t unix_rtc;
extern __IO uint32_t roomfl;
extern int16_t room_temp;
extern int16_t room_ntc_temp;
extern int8_t  ntc_offset;
extern uint8_t thst_sp;
extern uint8_t thst_dif;
extern uint8_t thst_min_sp;
extern uint8_t thst_max_sp;
extern uint8_t bdng_per;
extern uint8_t bdng_cnt;
extern uint8_t crsta;
extern uint8_t cardkeya[6];
extern uint8_t cardkeyb[6];
extern uint8_t rxbuf[68];
/* Macros     ----------------------------------------------------------------*/
/** ==========================================================================*/
/**       M E M O R I S E D     A N D     R E S T O R E D     F L A G S       */
/** ==========================================================================*/
#define ThermostatOn()                      (sysfl |=  (1U<<8))
#define ThermostatOff()                     (sysfl &=(~(1U<<8)))
#define IsThermostatActiv()                 (sysfl &   (1U<<8))

#define RoomCtrlConfig()                    (sysfl |=  (1U<<9))
#define RoomThstConfig()                    (sysfl &=(~(1U<<9)))
#define IsRoomCtrlConfig()                  (sysfl &   (1U<<9))
#define IsRoomThstConfig()                  (!(sysfl & (1U<<9)))

#define TempRegOn()                         (sysfl |=  (1U<<10))    // config On: controll loop is executed periodicaly
#define TempRegOff()                        (sysfl &=(~(1U<<10)))   // config Off:controll loop stopped, 
#define IsTempRegActiv()                    (sysfl &   (1U<<10))

#define TempRegHeating()                    (sysfl |=  (1U<<11))    // config Heating: output activ for setpoint value 
#define IsTempRegHeating()                  (sysfl &   (1U<<11))    // abbove measured value
#define IsTempRegCooling()                  (!(sysfl & (1U<<11)))
#define TempRegCooling()                    (sysfl &=(~(1U<<11)))   // config Cooling: opposite from heating

#define TempRegEnable()                     (sysfl |=  (1U<<12))    // conditional flag Enable: controll loop set output state
#define TempRegDisable()                    (sysfl &=(~(1U<<12)))   // conditional flag Disable:output is forced to inactiv state, controll loop cannot change outpu
#define IsTempRegEnabled()                  (sysfl &   (1U<<12))

#define TempRegOutputOn()                   (sysfl |=  (1U<<13))    // status On: output demand for actuator to inject energy in to system
#define TempRegOutputOff()                  (sysfl &=(~(1U<<13)))   // status Off:stop demanding energy for controlled system, setpoint is reached
#define IsTempRegOutputActiv()              (sysfl &   (1U<<13))

#define ScrnsvrClkSet()                     (sysfl |=  (1U<<14))    // flag for screensaver digital clock selection
#define ScrnsvrClkReset()                   (sysfl &=(~(1U<<14)))
#define IsScrnsvrClkActiv()                 (sysfl &   (1U<<14))

#define ScrnsvrSemiClkSet()                 (sysfl |=  (1U<<15))    // flag for screensaver small logo sze digital clock selection
#define ScrnsvrSemiClkReset()               (sysfl &=(~(1U<<15)))
#define IsScrnsvrSemiClkActiv()             (sysfl &   (1U<<15))

#define IsTempRegSta(x)                     (x & (1U<<0)) // remote state config flag
#define IsTempRegMod(x)                     (x & (1U<<1)) // remote mode config flag
#define IsTempRegCtr(x)                     (x & (1U<<2)) // remote control flag
#define IsTempRegOut(x)                     (x & (1U<<3)) // remote output flag
#define IsTempRegNewSta(x)                  (x & (1U<<4)) // use remote state config flag 
#define IsTempRegNewMod(x)                  (x & (1U<<5)) // use remote mode config flag
#define IsTempRegNewCtr(x)                  (x & (1U<<6)) // use remote control flag
#define IsTempRegNewOut(x)                  (x & (1U<<7)) // use remote output flag
#define IsTempRegNewCfg(x)                  (x & 0xF0) // use at least one remote flag
/***********************************************************************
**          T H E R M O S T A T		C O N T R O L L E R
***********************************************************************/
#define TempSenLuxRTSet()                   (roomfl |=  (1U<<0))
#define TempSenLuxRTReset()                 (roomfl &=(~(1U<<0)))
#define IsTempLuxRTActive()                 (roomfl &   (1U<<0))

#define TempSenDS18Set()                    (roomfl |=  (1U<<1))
#define TempSenDS18Reset()                  (roomfl &=(~(1U<<1)))
#define IsTempSenDS18Active()               (roomfl &   (1U<<1))

#define TempSenNTCSet()                     (roomfl |=  (1U<<2))
#define TempSenNTCReset()                   (roomfl &=(~(1U<<2)))
#define IsTempSenNTCActive()                (roomfl &   (1U<<2))

#define RstRoomSignSwSet()                  (roomfl |=  (1U<<3))
#define RstRoomSignSwReset()                (roomfl &=(~(1U<<3)))
#define IsRstRoomSignSwActiv()              (roomfl &   (1U<<3))

#define RtcTimeValidSet()                   (roomfl |=  (1U<<4))
#define RtcTimeValidReset()                 (roomfl &=(~(1U<<4)))
#define IsRtcTimeValid()                    (roomfl &   (1U<<4))

#define CardDataRdySet()                    (roomfl |=  (1U<<5))    // new card data ready
#define CardDataRdyReset()                  (roomfl &=(~(1U<<5)))   // clear flag after data processing to enable nexr read
#define IsCardDataRdyActiv()                (roomfl &   (1U<<5))    // check is new card data ready in bufer

#define RoomReentranceEnable()              (roomfl |=  (1U<<6))    // enable to handmaid to open room without clearing services
#define RoomReentranceDisable()             (roomfl &=(~(1U<<6)))
#define IsRoomReentranceActiv()             (roomfl &   (1U<<6))

#define RoomCleaningSet()                   (roomfl |=  (1U<<7))     // disable touch sensors due room cleaning
#define RoomCleaningReset()                 (roomfl &=(~(1U<<7)))
#define IsRoomCleaningActiv()               (roomfl &   (1U<<7))

#define DooLockTimeSet()                    (roomfl |=  (1U<<8))    // doorlock event from extenal source 
#define DooLockTimeReset()                  (roomfl &=(~(1U<<8)))
#define IsDooLockTimeActiv()                (roomfl &   (1U<<8)) 

#define NtcUpdateSet()                      (roomfl |=  (1U<<9)) 
#define NtcUpdateReset()                    (roomfl &=(~(1U<<9)))
#define IsNtcUpdateActiv()                  (roomfl &   (1U<<9))

#define NtcValidSet()                       (roomfl |=  (1U<<10))
#define NtcValidReset()                     (roomfl &=(~(1U<<10)))
#define IsNtcValidActiv()                   (roomfl &   (1U<<10))

#define NtcErrorSet()                       (roomfl |=  (1U<<11))
#define NtcErrorReset()                     (roomfl &=(~(1U<<11)))
#define IsNtcErrorActiv()                   (roomfl &   (1U<<11))

#define TSCleanSet()                        (roomfl |=  (1U<<12))
#define TSCleanReset()                      (roomfl &=(~(1U<<12)))
#define IsTSCleanActiv()                    (roomfl &   (1U<<12))

#define SetpointUpdateSet()                 (roomfl |=  (1U<<13)) 
#define SetpointUpdateReset()               (roomfl &=(~(1U<<13)))
#define IsSetpointUpdateActiv()             (roomfl &   (1U<<13))

#define RoomTempUpdateSet()                 (roomfl |=  (1U<<14)) 
#define RoomTempUpdateReset()               (roomfl &=(~(1U<<14)))
#define IsRoomTempUpdateActiv()             (roomfl &   (1U<<14))

#define ScrnsvrEnable()                     (roomfl |=  (1U<<15)) 
#define ScrnsvrDisable()                    (roomfl &=(~(1U<<15)))
#define IsScrnsvrEnabled()                  (roomfl &   (1U<<15))

#define ScrnsvrInitSet()                    (roomfl |=  (1U<<16))
#define ScrnsvrInitReset()                  (roomfl &=(~(1U<<16)))
#define IsScrnsvrInitActiv()                (roomfl &   (1U<<16))

#define ScrnsvrSet()                        (roomfl |=  (1U<<17)) 
#define ScrnsvrReset()                      (roomfl &=(~(1U<<17)))
#define IsScrnsvrActiv()                    (roomfl &   (1U<<17))

#define ScreenInitSet()                     (roomfl |=  (1U<<18))
#define ScreenInitReset()                   (roomfl &=(~(1U<<18)))
#define IsScreenInitActiv()                 (roomfl &   (1U<<18))

#define WFC_UpdateSet()                     (roomfl |=  (1U<<19))
#define WFC_UpdateReset()                   (roomfl &=(~(1U<<19)))
#define IsWFC_UpdateActiv()                 (roomfl &   (1U<<19))

#define WFC_ValidSet()                      (roomfl |=  (1U<<20))
#define WFC_ValidReset()                    (roomfl &=(~(1U<<20)))
#define IsWFCValidActiv()                   (roomfl &   (1U<<20))

#define SysInitSet()                        (roomfl |=  (1U<<21)) 
#define SysInitReset()                      (roomfl &=(~(1U<<21)))
#define IsSysInitActiv()                    (roomfl &   (1U<<21))
/* Function prototypes    ----------------------------------------------------*/
void ROOM_Init(void);
void ROOM_Service(void);
void CARD_Clear(void);
#endif
/************************ (C) COPYRIGHT JUBERA D.O.O Sarajevo ************************/
