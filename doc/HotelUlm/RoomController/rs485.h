/**
 ******************************************************************************
 * File Name          : rs485.h
 * Date               : 28/02/2016 23:16:19
 * Description        : rs485 communication modul header
 ******************************************************************************
 *
 ******************************************************************************
 */
 
#ifndef __RS485_H__
#define __RS485_H__                         FW_BUILD // version
/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx.h"
/* Exported Type  ------------------------------------------------------------*/
typedef enum
{
	COM_INIT = 0U,
	COM_PACKET_PENDING,
	COM_PACKET_RECEIVED,
	COM_RECEIVE_SUSPEND,
	COM_ERROR
	
}eComStateTypeDef;

extern eComStateTypeDef eComState;
/* Exported variables  -------------------------------------------------------*/
extern __IO uint32_t rstmr;
extern __IO uint32_t rsflg;
extern uint8_t  rsifa[2];
extern uint8_t  rsgra[2];
extern uint8_t  rsbra[2];
extern uint8_t  sysid[2];
extern uint8_t  rsbps;
/* Exported Define  ----------------------------------------------------------*/
/* Exported macros     -------------------------------------------------------*/
#define RS485_StartTimer(TIME)				(rstmr = TIME)
#define RS485_StopTimer()					(rstmr = 0U)
#define IsRS485_TimerExpired()				(rstmr == 0U)
#define RS485_OWStartUpdate()               (rsflg |=  (1U<<0))
#define RS485_OWStopUpdate()                (rsflg &=(~(1U<<0)))
#define IsRS485_UpdateActiv()				(rsflg &   (1U<<0))
#define RS485_ResponseSet()                 (rsflg |=  (1U<<1))
#define RS485_ResponseReset()               (rsflg &=(~(1U<<1)))
#define IsRS485_ResponseRdy()               (rsflg &   (1U<<1))
/* Exported functions ------------------------------------------------------- */
void RS485_Init(void);
void RS485_Service(void);
void RS485_RxCpltCallback(void);
void RS485_TxCpltCallback(void);
void RS485_ErrorCallback(void);
#endif
/************************ (C) COPYRIGHT JUBERA D.O.O Sarajevo ************************/
