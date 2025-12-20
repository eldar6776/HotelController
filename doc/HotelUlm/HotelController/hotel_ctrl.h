/**
 ******************************************************************************
 * File Name          : hotel_controller.h
 * Date               : 21/08/2016 20:59:16
 * Description        : hotel room Rubicon controller data link module header
 ******************************************************************************
 *      
 ******************************************************************************/


#ifndef __HOTEL_CTRL_H__
#define __HOTEL_CTRL_H__					    FW_BUILD	// version


/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"
#include "DIALOG.h"
/* Exported types ------------------------------------------------------------*/
typedef enum 
{
    PCK_ENUM = ((uint8_t)0x0U),
    PCK_SEND,
	PCK_RECEIVING,
    PCK_RECEIVED

}HC_StateTypeDef;


typedef enum 
{
    UPD_INIT            = ((uint8_t)0x0U),
    UPD_TIME            = ((uint8_t)0x1U),
    UPD_RC_STAT         = ((uint8_t)0x2U),
	UPD_ROOM_STAT       = ((uint8_t)0x3U),
    UPD_BINARY          = ((uint8_t)0x4U),
    UPD_FILE            = ((uint8_t)0x5U),
    UPD_LOG             = ((uint8_t)0x6U),
    UPD_HTTP_RQ         = ((uint8_t)0x7U),
    UPD_IDLE            = ((uint8_t)0x8U)

}HC_UpdateTypeDef;


typedef enum
{
	TYPE_ERROR	        = ((uint8_t)0x0U),
	TYPE_1		        = ((uint8_t)0x1U),
	TYPE_2		        = ((uint8_t)0x2U),
	TYPE_3		        = ((uint8_t)0x3U),
	TYPE_4		        = ((uint8_t)0x4U),
	TYPE_5		        = ((uint8_t)0x5U),
	TYPE_6		        = ((uint8_t)0x6U)
	
}LOG_ListTypeDef;


typedef struct 
{
	LOG_ListTypeDef Allocation;
    uint16_t log_cnt;
	uint32_t first_addr;
    uint32_t last_addr;
	uint32_t next_addr;
	
}HC_LogMemoryTypeDef;


typedef struct
{
    uint8_t state;
    uint8_t trial;
    uint8_t last_trial;
	uint32_t last_addr;
	
}HC_LogTransferTypeDef;


typedef struct
{
	uint8_t cmd;
    uint8_t state;
    uint8_t trial;
    uint32_t pck_send;
    uint32_t pck_total;
    uint32_t last_pck_send;
    uint32_t read_bcnt;
    
}HC_UpdatePacketTypeDef;


extern HC_StateTypeDef              HC_State;
extern HC_UpdateTypeDef             HC_Update;
extern HC_LogMemoryTypeDef          HC_LogMemory;
extern HC_UpdatePacketTypeDef       HC_FilUpdPck;
extern HC_UpdatePacketTypeDef       HC_FwrUpdPck;
extern HC_LogTransferTypeDef        HTTP_LogTransfer;
/* Exported constants --------------------------------------------------------*/
/* Exported types    ---------------------------------------------------------*/
/* Exported variables  -------------------------------------------------------*/
extern __IO uint32_t hc_flag;
extern __IO uint32_t sys_rx_timer;
extern __IO uint32_t sys_tftp_file;
extern __IO uint32_t response_timer;
extern __IO uint32_t fw_upd_tmr;
extern __IO uint16_t system_id;
extern uint16_t *addr_list;
extern __IO uint16_t addr_list_cnt;
extern __IO uint16_t fwrupd_add_cnt;
extern __IO uint16_t addr_list_size;
extern __IO uint16_t rs485_txaddr;
extern __IO uint16_t rsifa;
extern __IO uint16_t rsgra;
extern __IO uint16_t rsbra;
extern uint16_t *fwrupd_add_list;
extern uint16_t *imgupd_add_list;
extern __IO uint16_t filupd_list_cnt;
extern __IO uint16_t fwrupd_addlist_dsize;
extern __IO uint16_t imgupd_addlist_dsize;
extern __IO uint16_t imgupd_addlist_cnt;
extern __IO uint16_t jrnl_buff[];
extern __IO uint8_t ow_ifaddr;
extern __IO uint8_t ow_txaddr;
extern __IO uint8_t request;
extern uint8_t *p_hc_buffer;
extern __IO uint8_t rsbps;
extern __IO uint8_t rs485_bus_sta;
extern __IO uint8_t http_cmdsta;
extern __IO uint8_t config_file_image_cnt;
extern __IO uint8_t sys_journal_list_item;
extern __IO uint8_t sta_req_cnt;
extern uint8_t filupd_list[];
extern char hc_buff[];
/* Exported macro ------------------------------------------------------------*/
#define RS485_BusConnected()        (hc_flag |=  (0x1U << 0))
#define RS485_BusDisconnected()     (hc_flag &=(~(0x1U << 0)))
#define IsRS485_BusConnected()      (hc_flag &   (0x1U << 0))
#define RC_StatusUpdateSet()        (hc_flag |=  (0x1U << 1))
#define RC_StatusUpdateReset()      (hc_flag &=(~(0x1U << 1)))
#define IsRC_StatusUpdated()        (hc_flag &   (0x1U << 1))
#define RC_StatusRequestSet()       (hc_flag |=  (0x1U << 2))
#define RC_StatusRequestReset()     (hc_flag &=(~(0x1U << 2)))
#define IsRC_StatusRequested()      (hc_flag &   (0x1U << 2))
#define RC_JournalUpdateSet()       (hc_flag |=  (0x1U << 3))
#define RC_JournalUpdateReset()     (hc_flag &=(~(0x1U << 3)))
#define IsRC_JournalUpdateActiv()   (hc_flag &   (0x1U << 3))
/* Exported functions  -------------------------------------------------------*/
void HC_Init(void);
void HC_Service(void);
void HC_WriteLog(void);
uint8_t SendFwInfo(void);
uint8_t HTTP2RS485(void);
void HC_FormatLogList(void);
uint8_t HC_LoadAddrList(void);
void HC_ReadLogListBlock(void);
uint8_t HC_CreateAddrList(void);
void HC_DeleteLogListBlock(void);
void HC_CreateCmdRequest(uint8_t cmd, char *ibuf);
uint8_t HC2RT_Link(uint8_t *txbuf, uint8_t *rxbuf);

void HC_WriteLogEvent(uint8_t event, uint8_t type);
int HC_ScanRS485(uint16_t fadd, uint16_t ladd, uint8_t option);
#endif
/************************ (C) COPYRIGHT JUBERA D.O.O Sarajevo ************************/

