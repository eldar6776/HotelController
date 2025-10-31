/**
 ******************************************************************************
 * File Name          : display.c
 * Date               : 21/08/2016 20:59:16
 * Description        : display GUI function set
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
#include "spi_flash.h"
#include "tftpserver.h"
#include "hotel_ctrl.h"
#include "stm32f4x7_eth.h"
#include "stm32f429i_lcd.h"


#if (__DISP_H__ != FW_BUILD)
    #error "display header version mismatch"
#endif


/* Imported variables --------------------------------------------------------*/
extern GUI_CONST_STORAGE GUI_BITMAP bmNetworkError;
extern GUI_CONST_STORAGE GUI_BITMAP bmNetworkConnected;
extern GUI_CONST_STORAGE GUI_BITMAP bmusd_ok;
extern GUI_CONST_STORAGE GUI_BITMAP bmusd_error;


/* Private define ------------------------------------------------------------*/
WM_HWIN hSELECTED;
BUTTON_Handle hBUTTON_Settings;
#define GUI_ID_BUTTON_Settings   				0x801
BUTTON_Handle hBUTTON_Tools;
#define GUI_ID_BUTTON_Tools   					0x802
BUTTON_Handle hBUTTON_Control;
#define GUI_ID_BUTTON_Control   				0x803
BUTTON_Handle hBUTTON_Unlock;
#define GUI_ID_BUTTON_Unlock   					0x804
BUTTON_Handle hBUTTON_Scan;
#define GUI_ID_BUTTON_Scan   					0x805
BUTTON_Handle hBUTTON_AddressSet;
#define GUI_ID_BUTTON_AddressSet   				0x806
BUTTON_Handle hBUTTON_ExitRS485Scanner;
#define GUI_ID_BUTTON_ExitRS485Scanner  		0x807
BUTTON_Handle hBUTTON_ExitInfo;
#define GUI_ID_BUTTON_ExitInfo   				0x808

BUTTON_Handle hBUTTON_SettingsOk;
#define GUI_ID_BUTTON_SettingsOk   				0x80a
BUTTON_Handle hBUTTON_SettingsCancel;
#define GUI_ID_BUTTON_SettingsCancel   			0x80b
BUTTON_Handle hBUTTON_SettingsApply;
#define GUI_ID_BUTTON_SettingsApply   			0x80c
BUTTON_Handle hBUTTON_SettingsEdit;
#define GUI_ID_BUTTON_SettingsEdit   			0x80d

BUTTON_Handle hBUTTON_ToolScanRS485;
#define GUI_ID_BUTTON_ToolScanRS485   			0x810
BUTTON_Handle hBUTTON_ToolUploadImage;
#define GUI_ID_BUTTON_ToolUploadImage   		0x811
BUTTON_Handle hBUTTON_ToolUploadFirmware;
#define GUI_ID_BUTTON_ToolUploadFirmware   		0x812
BUTTON_Handle hBUTTON_ToolUpdateFW;
#define GUI_ID_BUTTON_ToolUpdateFW   			0x813
BUTTON_Handle hBUTTON_ToolDisplayCalibration;
#define GUI_ID_BUTTON_ToolDisplayCalibration  	0x814
BUTTON_Handle hBUTTON_ToolFileList;
#define GUI_ID_BUTTON_ToolFileList   			0x815
BUTTON_Handle hBUTTON_ToolFormatCard;
#define GUI_ID_BUTTON_ToolFormatCard   			0x816
BUTTON_Handle hBUTTON_ToolLoadList;
#define GUI_ID_BUTTON_ToolLoadList   			0x817
BUTTON_Handle hBUTTON_ToolExit;
#define GUI_ID_BUTTON_ToolExit   				0x818

BUTTON_Handle hBUTTON_OSK_1;
#define GUI_ID_BUTTON_OSK_1   		            0x819
BUTTON_Handle hBUTTON_OSK_2;
#define GUI_ID_BUTTON_OSK_2   		            0x81a
BUTTON_Handle hBUTTON_OSK_3;
#define GUI_ID_BUTTON_OSK_3   		            0x81b
BUTTON_Handle hBUTTON_OSK_4;
#define GUI_ID_BUTTON_OSK_4   		            0x81c
BUTTON_Handle hBUTTON_OSK_5;
#define GUI_ID_BUTTON_OSK_5   		            0x81d
BUTTON_Handle hBUTTON_OSK_6;
#define GUI_ID_BUTTON_OSK_6   		            0x81e
BUTTON_Handle hBUTTON_OSK_7;
#define GUI_ID_BUTTON_OSK_7   		            0x81f
BUTTON_Handle hBUTTON_OSK_8;
#define GUI_ID_BUTTON_OSK_8   		            0x820
BUTTON_Handle hBUTTON_OSK_9;
#define GUI_ID_BUTTON_OSK_9   		            0x821
BUTTON_Handle hBUTTON_OSK_0;
#define GUI_ID_BUTTON_OSK_0   		            0x818
BUTTON_Handle hBUTTON_OSK_Ok;
#define GUI_ID_BUTTON_OSK_Ok   		            0x822
BUTTON_Handle hBUTTON_OSK_Back;
#define GUI_ID_BUTTON_OSK_Back  		        0x823

GUI_HWIN hCHKBOX_EnableDHCP;
#define GUI_ID_CHECK_EnableDHCP					0x850
GUI_HWIN hCHKBOX_EnableBroadcastTime;
#define GUI_ID_CHECK_EnableBroadcastTime		0x851
GUI_HWIN hCHKBOX_EnableHTTP;
#define GUI_ID_CHECK_EnableHTTP					0x852
GUI_HWIN hCHKBOX_EnableWebConfig;
#define GUI_ID_CHECK_EnableWebConfig			0x853
GUI_HWIN hCHKBOX_EnableTFTP;
#define GUI_ID_CHECK_EnableTFTP					0x854

GUI_HWIN hEDIT_TcpIpAddress;
#define GUI_ID_EDIT_TcpIpAddress				0x900
GUI_HWIN hEDIT_TcpIpGateway;
#define GUI_ID_EDIT_TcpIpGateway				0x901
GUI_HWIN hEDIT_TcpIpSubnet;
#define GUI_ID_EDIT_TcpIpSubnet					0x902
GUI_HWIN hEDIT_RS485_InterfaceAddress;
#define GUI_ID_EDIT_RS485_InterfaceAddress		0x903
GUI_HWIN hEDIT_RS485_BroadcastAddress;
#define GUI_ID_EDIT_RS485_BroadcastAddress		0x904
GUI_HWIN hEDIT_DateTime;
#define GUI_ID_EDIT_DateTime					0x905
GUI_HWIN hEDIT_Password;
#define GUI_ID_EDIT_Password					0x906
GUI_HWIN hEDIT_SysID;
#define GUI_ID_EDIT_SysID						0x907

GUI_HWIN hSPINBOX_StartAddress;
#define GUI_ID_SPINBOX_StartAddress				0xa00
GUI_HWIN hSPINBOX_EndAddress;
#define GUI_ID_SPINBOX_EndAddress				0xa01
GUI_HWIN hSPINBOX_OldAddress;
#define GUI_ID_SPINBOX_OldAddress				0xa02
GUI_HWIN hSPINBOX_NewAddress;
#define GUI_ID_SPINBOX_NewAddress				0xa03
GUI_HWIN hSPINBOX_SetAddress;
#define GUI_ID_SPINBOX_SetAddress				0xa04

GUI_HWIN hLISTVIEW_AddresseFound;
#define GUI_ID_LISTVIEW_AddresseFound			0xa50

GUI_HWIN hRADIO_ScanMode;
#define GUI_ID_RADIO_ScanMode					0xb00

GUI_HMEM hPROGBAR_FileTransfer;
#define GUI_ID_PROGBAR_FileTransfer  			0xb50

WM_HWIN              hTree;
TREEVIEW_ITEM_Handle hItem;
TREEVIEW_ITEM_Handle hNode;
#define GUI_ID_TREEVIEW_FileList				0xba0

#define OSK_IDLE					0
#define OSK_EDIT_IPADD			    1
#define OSK_EDIT_GWADD			    2
#define OSK_EDIT_SUBNET		        3
#define OSK_EDIT_RS485_INTERFACE	4
#define OSK_EDIT_RS485_BROADCAST	5
#define OSK_EDIT_PASSWORD			6
#define OSK_ENTER_PASSWORD			7
#define OSK_EDIT_SYS_ID				8

#define USD_H_POS                   15
#define USD_V_POS                   5
#define USD_H_SIZE                  40
#define USD_V_SIZE                  70
#define USD_TX_H_POS                (USD_H_POS + (USD_H_SIZE / 2))
#define USD_TX_V_POS                (USD_V_POS + 55)
#define ICO_H_TAB                   20
#define NET_H_POS                   (USD_H_POS + USD_H_SIZE+ ICO_H_TAB)
#define NET_V_POS                   5
#define NET_H_SIZE                  40
#define NET_V_SIZE                  70
#define NET_TX_H_POS                (NET_H_POS + (NET_H_SIZE / 2))
#define NET_TX_V_POS                (NET_V_POS + 55)

#define USER_LOGO_H_POS             120
#define USER_LOGO_V_POS             0
#define USER_LOGO_H_SIZE            360
#define USER_LOGO_V_SIZE            60
/* Exported types ------------------------------------------------------------*/
typedef enum
{
	DEFAULT     = ((uint8_t)0x0U),
    SETTINGS    = ((uint8_t)0x1U),
    CONTROL     = ((uint8_t)0x2U),
    TOOLS       = ((uint8_t)0x3U),
    EDIT        = ((uint8_t)0x4U),
    RS485       = ((uint8_t)0x5U)
    
}DISP_ActiveScreenTypeDef;

DISP_ActiveScreenTypeDef ActiveScreen;
/* Private variables ---------------------------------------------------------*/
static char string[84];
static char string1[84];
static char string2[84];
static char string3[84];
static char string4[84];
static char string5[84];
static char osk_entry[6];
static uint8_t osk_bcnt;
static uint32_t strval;
uint8_t row = 0;
uint8_t usd_status;
uint8_t cursor_pos;
uint8_t osk_ipedit;
uint32_t osk_call;
uint32_t disp_flag;
uint32_t disp_tmr;
uint32_t disp_toutmr;
int32_t ret_1, ret_2, ret_3, ret_4;
char password[6] = {'4','3','8','9','1',NUL};
/* Function prototypes -------------------------------------------------------*/
void SAVE_Settings(void);
void DISP_CreateKeypad(void);
void DISP_DeleteKeypad(void);
void DISP_UpdateDateTime(void);
void DISP_FoundDeviceList(void);
void DISP_CreateToolsScreen(void);
void DISP_DeleteToolsScreen(void);
void DISP_CreateDefaultScreen(void);
void DISP_DeleteDefaultScreen(void);
void DISP_CreateSettingsScreen(void);
void DISP_DeleteSettingsScreen(void);
void DISP_CreateScanRS485BusScreen(void);
void DISP_DeleteScanRS485BusScreen(void);
void DISP_Keypad(uint8_t item, uint8_t key);
int APP_GetData(void * p, const unsigned char ** ppData, unsigned NumBytes, unsigned long Off);
/* Private functions ---------------------------------------------------------*/
void DISP_Init(void)
{
	GUI_Init();	
	GUI_SelectLayer(0);
	GUI_Clear();
	
	if (f_mount(&fatfs, "0:", 0x0U) == FR_OK)
	{
		if (f_opendir(&dir_1, "/") == FR_OK)
		{
			if (f_open(&file_SD, "BCK_GND.BMP", FA_READ) == FR_OK) 
            {
				GUI_BMP_DrawEx(APP_GetData, (void *) &file_SD, 0x0U, 0x0U);
				DISP_uSDCardReadySet();
			}
			else DISP_uSDCardErrorSet();
		}
		else DISP_uSDCardErrorSet();
	}
	else DISP_uSDCardErrorSet();
	
	f_mount(NULL, "0:", 0x0U);
	GUI_SelectLayer(1);
	GUI_SetBkColor(GUI_TRANSPARENT); 
	GUI_Clear();
	GUI_CURSOR_Select(&GUI_CursorCrossM);
	RADIO_SetDefaultSkin(RADIO_SKIN_FLEX);
	BUTTON_SetDefaultSkin(BUTTON_SKIN_FLEX);
	SPINBOX_SetDefaultSkin(SPINBOX_SKIN_FLEX);
	SPINBOX_SetDefaultButtonSize(20);
	RADIO_SetDefaultTextColor(GUI_WHITE);
	EDIT_SetDefaultTextAlign(GUI_TA_RIGHT);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);
	GUI_SetColor(GUI_WHITE);
	GUI_SetFont(&GUI_Font20B_1);
	hPROGBAR_FileTransfer = PROGBAR_Create(120, 110, 355, 25, WM_CF_SHOW);
	PROGBAR_SetMinMax(hPROGBAR_FileTransfer, 0, 100);
	PROGBAR_SetTextColor(hPROGBAR_FileTransfer, 0, GUI_BLACK);
	PROGBAR_SetTextColor(hPROGBAR_FileTransfer, 1, GUI_WHITE);
	PROGBAR_SetBarColor(hPROGBAR_FileTransfer, 0, GUI_LIGHTGREEN);
	PROGBAR_SetBarColor(hPROGBAR_FileTransfer, 1, GUI_LIGHTGRAY);
	PROGBAR_SetTextAlign(hPROGBAR_FileTransfer, GUI_TA_HCENTER);
	PROGBAR_SetFont(hPROGBAR_FileTransfer, &GUI_Font13_1);
	PROGBAR_SetText(hPROGBAR_FileTransfer, "0%");
	PROGBAR_SetSkin(hPROGBAR_FileTransfer, PROGBAR_SKIN_FLEX);	
	WM_HideWindow(hPROGBAR_FileTransfer);
	DISP_CreateDefaultScreen();
}

void DISP_Service(void)
{
    char str[16];
    uint8_t add, attempts;
    static uint8_t tmp_sec = 0x0U;
//    static uint32_t stat_flg = 0x0U;
    static uint32_t touch_timing = 0x0U;
    static RTC_t rtc_dtm;
    
    
	if((Get_SysTick() - touch_timing) >= 10U)
	{
		touch_timing = Get_SysTick();
		GUI_TOUCH_Exec();
		GUI_Exec();	
	}
	else return;

	switch(ActiveScreen)
	{	
		case DEFAULT:
		{
			if(IsDISP_TimerExpired())
			{
				if(IsDISP_RefreshActiv())
				{
					DISP_RefreshReset();
					
					if(IsDISP_KeypadActiv())
					{
						DISP_KeypadReset();
						DISP_DeleteKeypad();
					}
					
					DISP_DeleteDefaultScreen();
					DISP_CreateDefaultScreen();
				}
				
			}
			
			if(IsDISP_TimeoutTimerExpired())
			{
				if(IsDISP_UnlockActiv())
				{
					DISP_UnlockReset();
					DISP_DeleteDefaultScreen();
					DISP_CreateDefaultScreen();
				}
			}
			
			DISP_UpdateDateTime();
			
			if(BUTTON_IsPressed(hBUTTON_Tools) && IsDISP_UnlockActiv()) 
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_Tools)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				DISP_DeleteDefaultScreen();
				DISP_CreateToolsScreen();
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}				
			else if(BUTTON_IsPressed(hBUTTON_Settings) && IsDISP_UnlockActiv()) 
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_Settings)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				DISP_DeleteDefaultScreen();
				DISP_CreateSettingsScreen();
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}	
			else if(BUTTON_IsPressed(hBUTTON_Unlock) && !IsDISP_KeypadActiv())
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_Unlock)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				if(IsDISP_UnlockActiv())
				{
					DISP_TimeoutTimerStop();
				}
				else
				{				
					DISP_CreateKeypad();
					ZEROFILL(osk_entry, sizeof(osk_entry));
					osk_bcnt = 0x0U;
					GUI_SetColor(GUI_WHITE);
					GUI_SetFont(&GUI_Font24B_1);
					GUI_SetTextMode(GUI_TEXTMODE_TRANS);
					GUI_DispStringAt("ENTER PASSWORD", 150, 15);
					DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
					DISP_RefreshSet();
					GUI_Delay(100);
				}				
			}
			else if(BUTTON_IsPressed(hBUTTON_Control) && IsDISP_UnlockActiv())
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_Control)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				DISP_DeleteDefaultScreen();
				DISP_TimeoutTimerStop();
				DISP_TimerStop();
				GUI_Delay(100);
			}
			else if((rs485_bus_sta == RS485_BUS_CONNECTED) && !IsRS485_BusConnected())
			{
				RS485_BusConnected();
				GUI_ClearRect(NET_H_POS, NET_V_POS, NET_H_POS + NET_H_SIZE, NET_V_POS + NET_V_SIZE);
				GUI_DrawBitmap(&bmNetworkConnected, NET_H_POS, NET_V_POS);
                GUI_SetColor(GUI_GREEN);
                GUI_SetBkColor(GUI_BLACK);
				GUI_SetTextMode(GUI_TEXTMODE_NORMAL);
				GUI_SetTextAlign(GUI_TA_HCENTER|GUI_TA_VCENTER);
                GUI_GotoXY(NET_TX_H_POS, NET_TX_V_POS);
				GUI_SetFont(&GUI_Font16B_1);
				GUI_DispString("RUN");
				GUI_SetBkColor(GUI_TRANSPARENT);
			}
			else if((rs485_bus_sta != RS485_BUS_CONNECTED) && IsRS485_BusConnected())
			{
				RS485_BusDisconnected();
				GUI_ClearRect(NET_H_POS, NET_V_POS, NET_H_POS + NET_H_SIZE, NET_V_POS + NET_V_SIZE);
				GUI_DrawBitmap(&bmNetworkError, NET_H_POS, NET_V_POS);
				GUI_SetColor(GUI_RED);
                GUI_SetBkColor(GUI_BLACK);
                GUI_SetTextMode(GUI_TEXTMODE_NORMAL);
                GUI_SetTextAlign(GUI_TA_HCENTER|GUI_TA_VCENTER);
                GUI_GotoXY(NET_TX_H_POS, NET_TX_V_POS);
				GUI_SetFont(&GUI_Font16B_1);
				if      (rs485_bus_sta == RS485_BUS_ERROR)          GUI_DispString("ERROR");
				else if (rs485_bus_sta == RS485_BUS_DISCONNECTED)   GUI_DispString(" STOP");
				GUI_SetBkColor(GUI_TRANSPARENT);
			}
			
			
			if(IsDISP_KeypadActiv())
			{
				hSELECTED = 0;
				
				if      (BUTTON_IsPressed(hBUTTON_OSK_0)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_0)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(OSK_ENTER_PASSWORD, '0');					
				}
				else if (BUTTON_IsPressed(hBUTTON_OSK_1)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_1)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(OSK_ENTER_PASSWORD, '1');
				}
				else if (BUTTON_IsPressed(hBUTTON_OSK_2)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_2)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(OSK_ENTER_PASSWORD, '2');
				}
				else if (BUTTON_IsPressed(hBUTTON_OSK_3)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_3)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(OSK_ENTER_PASSWORD, '3');
				}
				else if (BUTTON_IsPressed(hBUTTON_OSK_4)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_4)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(OSK_ENTER_PASSWORD, '4');
				}
				else if (BUTTON_IsPressed(hBUTTON_OSK_5)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_5)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(OSK_ENTER_PASSWORD, '5');
				}
				else if (BUTTON_IsPressed(hBUTTON_OSK_6)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_6)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(OSK_ENTER_PASSWORD, '6');
				}
				else if (BUTTON_IsPressed(hBUTTON_OSK_7)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_7)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(OSK_ENTER_PASSWORD, '7');
				}
				else if(BUTTON_IsPressed(hBUTTON_OSK_8)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_8)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(OSK_ENTER_PASSWORD, '8');
				}
				else if (BUTTON_IsPressed(hBUTTON_OSK_9)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_9)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(OSK_ENTER_PASSWORD, '9');
				}
				else if (BUTTON_IsPressed(hBUTTON_OSK_Ok)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_Ok)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(OSK_ENTER_PASSWORD, 'O');
				}
				else if (BUTTON_IsPressed(hBUTTON_OSK_Back)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_Back)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(OSK_ENTER_PASSWORD, 'B');
				}
			}
			break;
		}
		
		
        case SETTINGS:
		{
			RTC_GetDateTime(&rtc_dtm);
			
			if(BUTTON_IsPressed(hBUTTON_SettingsCancel) && !IsDISP_KeypadActiv())
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_SettingsCancel)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
                RAM_Init();
				DISP_DeleteSettingsScreen();
				DISP_CreateDefaultScreen();
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			else if(BUTTON_IsPressed(hBUTTON_SettingsOk)  && !IsDISP_KeypadActiv())
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_SettingsOk)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
                
				SAVE_Settings();
				
				DISP_DeleteSettingsScreen();
				DISP_CreateDefaultScreen();
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			else if(BUTTON_IsPressed(hBUTTON_SettingsApply)  && !IsDISP_KeypadActiv())
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_SettingsApply)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				SAVE_Settings();
				GUI_Delay(100);
			}
			else if(BUTTON_IsPressed(hBUTTON_SettingsEdit)  && !IsDISP_KeypadActiv())
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_SettingsEdit)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}		
				
				if(hSELECTED != 0)
				{
					if(hSELECTED == hEDIT_TcpIpAddress)                 osk_call = OSK_EDIT_IPADD;
					else if(hSELECTED == hEDIT_TcpIpGateway)            osk_call = OSK_EDIT_GWADD;
					else if(hSELECTED == hEDIT_TcpIpSubnet)             osk_call = OSK_EDIT_SUBNET;
					else if(hSELECTED == hEDIT_RS485_InterfaceAddress)  osk_call = OSK_EDIT_RS485_INTERFACE;
					else if(hSELECTED == hEDIT_RS485_BroadcastAddress)  osk_call = OSK_EDIT_RS485_BROADCAST;
					else if(hSELECTED == hEDIT_Password)                osk_call = OSK_EDIT_PASSWORD;
					else if(hSELECTED == hEDIT_SysID)                   osk_call = OSK_EDIT_SYS_ID;
					
					osk_bcnt = 0x0U;
                    ZEROFILL(osk_entry, sizeof(osk_entry));
                    ZEROFILL(string, sizeof (string));
					EDIT_GetText(hSELECTED, string, 24);
					DISP_DeleteSettingsScreen();
					GUI_SetBkColor(GUI_BLACK);
					GUI_Clear();
					DISP_CreateKeypad();				
					ActiveScreen = EDIT;
				}
				
				DISP_Keypad(osk_call, cursor_pos);
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			else if(tmp_sec != rtc_dtm.seconds)				
			{
				tmp_sec = rtc_dtm.seconds;
                ZEROFILL(string, COUNTOF(string));
				Hex2Str(string,     &rtc_dtm.hours, 0x2U);
				string[2] = ':';
                Hex2Str(&string[3], &rtc_dtm.minutes, 0x2U);
				string[5] = ':';
                Hex2Str(&string[6], &rtc_dtm.seconds, 0x2U);
				string[8] = ' ';
				string[9] = ' ';
                Hex2Str(&string[10], &rtc_dtm.date, 0x2U);
				string[12] = '.';
                Hex2Str(&string[13], &rtc_dtm.month, 0x2U);
				string[15] = '.';
				string[16] = '2';
				string[17] = '0';
                Hex2Str(&string[18], &rtc_dtm.year, 0x2U);
				EDIT_SetText(hEDIT_DateTime, string);
			}
			else
			{
				if (WM_HasFocus(hEDIT_TcpIpAddress) == 1) 
				{
					hSELECTED = hEDIT_TcpIpAddress;
					cursor_pos = EDIT_GetCursorCharPos(hEDIT_TcpIpAddress);
				}
				else if(WM_HasFocus(hEDIT_TcpIpGateway) == 1)
				{					
					hSELECTED = hEDIT_TcpIpGateway;
					cursor_pos = EDIT_GetCursorCharPos(hEDIT_TcpIpGateway);
				}
				else if(WM_HasFocus(hEDIT_TcpIpSubnet) == 1)
				{					
					hSELECTED = hEDIT_TcpIpSubnet;
					cursor_pos = EDIT_GetCursorCharPos(hEDIT_TcpIpSubnet);
				}
				else if(WM_HasFocus(hEDIT_RS485_InterfaceAddress) == 1)
				{					
					hSELECTED = hEDIT_RS485_InterfaceAddress;
					cursor_pos = EDIT_GetCursorCharPos(hEDIT_RS485_InterfaceAddress);
				}
				else if(WM_HasFocus(hEDIT_RS485_BroadcastAddress) == 1)
				{					
					hSELECTED = hEDIT_RS485_BroadcastAddress;
					cursor_pos = EDIT_GetCursorCharPos(hEDIT_RS485_BroadcastAddress);
				}
				else if(WM_HasFocus(hEDIT_DateTime) == 1)
				{					
					hSELECTED = hEDIT_DateTime;
					cursor_pos = EDIT_GetCursorCharPos(hEDIT_DateTime);
				}
				else if(WM_HasFocus(hEDIT_Password) == 1)
				{					
					hSELECTED = hEDIT_Password;
					cursor_pos = EDIT_GetCursorCharPos(hEDIT_Password);
				}
				else if(WM_HasFocus(hEDIT_SysID) == 1)
				{					
					hSELECTED = hEDIT_SysID;
					cursor_pos = EDIT_GetCursorCharPos(hEDIT_SysID);
				}
				else 
				{					
					hSELECTED = 0;
					cursor_pos = 0x0U;
				}
				
				if (hSELECTED == 0) WM_DisableWindow(hBUTTON_SettingsEdit);
				else WM_EnableWindow(hBUTTON_SettingsEdit);
			}
			break;
		}
        
        
        case CONTROL:
        {
            break;
        }
        
        
        case TOOLS:
		{
			if(BUTTON_IsPressed(hBUTTON_ToolScanRS485)) 
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_ToolScanRS485)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				DISP_DeleteToolsScreen();
				DISP_CreateScanRS485BusScreen();
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			else if(BUTTON_IsPressed(hBUTTON_ToolDisplayCalibration)) 
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_ToolDisplayCalibration)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				DISP_DeleteToolsScreen();
				Calibration(480,272);
				GUI_Delay(1000);
				GUI_Clear();
				DISP_CreateToolsScreen();
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			else if(BUTTON_IsPressed(hBUTTON_ToolUpdateFW)) 
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_ToolUpdateFW)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);				
			}
			else if(BUTTON_IsPressed(hBUTTON_ToolUploadImage)) 
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_ToolUploadImage)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			else if(BUTTON_IsPressed(hBUTTON_ToolUploadFirmware)) 
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_ToolUploadFirmware)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);				
			}
			else if(BUTTON_IsPressed(hBUTTON_ToolFileList)) 
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_ToolFileList)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				hItem = 0;
				ret_1 = 0;
				if (f_mount(&fatfs, "0:", 0) != FR_OK) break;
				fresult =  f_opendir(&dir_1, "/");                   /* Open the directory */
				if (fresult == FR_OK)
				{
					hNode = TREEVIEW_InsertItem(hTree, TREEVIEW_ITEM_TYPE_NODE, 0, 0, "uSD_Card");
					for (;;) 
					{
						ret_1 = hItem ? TREEVIEW_INSERT_BELOW : TREEVIEW_INSERT_FIRST_CHILD;
						 /* Read a directory item */
						fresult = f_readdir(&dir_1, &finfo);                  
						if (fresult != FR_OK || finfo.fname[0] == 0x0U)
						{	/* Break on error or end of dir */
							f_closedir(&dir_1);
							break;  							
						}
						if (finfo.fattrib & AM_DIR) 
						{   /* It is a directory */							
							hItem = TREEVIEW_ITEM_Create(0x1U,  finfo.fname, 0x0U);
							if(hItem == 0x0U)
							{
								f_closedir(&dir_1);
								break; 
							}
						} 
						else 
						{   /* It is a file. */
							hItem = TREEVIEW_ITEM_Create(0x0U,  finfo.fname, 0x0U);
							if(hItem == 0)
							{
								f_closedir(&dir_1);
								break; 
							}
						}						
						TREEVIEW_AttachItem(hTree, hItem, hNode, ret_1);
						GUI_Exec();
					}
					f_closedir(&dir_1);
				}
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			else if(BUTTON_IsPressed(hBUTTON_ToolFormatCard)) 
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_ToolFormatCard)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			else if(BUTTON_IsPressed(hBUTTON_ToolLoadList)) 
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_ToolLoadList)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			else if(BUTTON_IsPressed(hBUTTON_ToolExit)) 
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_ToolExit)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				DISP_DeleteToolsScreen();
				DISP_CreateDefaultScreen();
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			break;
		}
        
        
        case EDIT:
		{
			if(IsDISP_KeypadActiv())
			{
				if(BUTTON_IsPressed(hBUTTON_OSK_0)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_0)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(osk_call, '0');					
				}
				else if(BUTTON_IsPressed(hBUTTON_OSK_1)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_1)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(osk_call, '1');
				}
				else if(BUTTON_IsPressed(hBUTTON_OSK_2)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_2)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(osk_call, '2');
				}
				else if(BUTTON_IsPressed(hBUTTON_OSK_3)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_3)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(osk_call, '3');
				}
				else if(BUTTON_IsPressed(hBUTTON_OSK_4)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_4)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(osk_call, '4');
				}
				else if(BUTTON_IsPressed(hBUTTON_OSK_5)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_5)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(osk_call, '5');
				}
				else if(BUTTON_IsPressed(hBUTTON_OSK_6)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_6)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(osk_call, '6');
				}
				else if(BUTTON_IsPressed(hBUTTON_OSK_7)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_7)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(osk_call, '7');
				}
				else if(BUTTON_IsPressed(hBUTTON_OSK_8)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_8)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(osk_call, '8');
				}
				else if(BUTTON_IsPressed(hBUTTON_OSK_9)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_9)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(osk_call, '9');
				}
				else if(BUTTON_IsPressed(hBUTTON_OSK_Ok)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_Ok)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(osk_call, 'O');
				}
				else if(BUTTON_IsPressed(hBUTTON_OSK_Back)) 
				{
					while(1)
					{
						if(!BUTTON_IsPressed(hBUTTON_OSK_Back)) break;
						GUI_TOUCH_Exec();
						GUI_Delay(50);
					}
					
					DISP_Keypad(osk_call, 'B');
				}
			}
			
			DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
			DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
			DISP_RefreshSet();
			GUI_Delay(100);
			break;
		}
        
        
        case RS485:
		{
			if(BUTTON_IsPressed(hBUTTON_Scan))
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_Scan)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
//				ZEROFILL(hc_buff, COUNTOF(hc_buff));
				ret_1 = SPINBOX_GetValue(hSPINBOX_StartAddress);
				ret_2 = SPINBOX_GetValue(hSPINBOX_EndAddress);
				ret_3 = RADIO_GetValue(hRADIO_ScanMode);
				ret_4 = HC_ScanRS485(ret_1, ret_2, ret_3);

				if (ret_4 == -1)
				{
					
				}
				else if (ret_4 == 0)
				{
					
				}
				else if ((ret_3 == 2) || (ret_3 == 3))
				{
					add = 0;
					attempts = RS485_MAX_ATTEMPTS;
					
					while (ret_4)
					{
						ret_1 = fwrupd_add_list[add];
						ret_2 = ret_1 + 1;
						
						if (HC_ScanRS485(ret_1, ret_2, FIND_ADDR) == 1)
						{
							DISP_FoundDeviceList();
							--ret_4;
							++add;
							GUI_Delay(100);
						}
						else
						{
							--attempts;
							
							if(attempts == 0)
							{
								--ret_4;
								++add;
								attempts = RS485_MAX_ATTEMPTS;
							}
							
							GUI_Delay(100);
						}
					}					
				}
				else
				{
					DISP_FoundDeviceList();
				}
				
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			else if(BUTTON_IsPressed(hBUTTON_AddressSet))
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_AddressSet)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
//				ZEROFILL(hc_buff, COUNTOF(hc_buff));
				ret_1 = SPINBOX_GetValue(hSPINBOX_OldAddress);
//				Int2Str(hc_buff, ret_1, 0x0U);
				ret_2 = 0;
//				while(hc_buff[ret_2] != 0x0U) ++ret_2;
				++ret_2;
				ret_4 = SPINBOX_GetValue(hSPINBOX_NewAddress);
//				Int2Str(&hc_buff[ret_2], ret_4, 0x0U);
				request = SET_RS485_CFG;
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			else if(BUTTON_IsPressed(hBUTTON_ExitRS485Scanner))
			{
				while(1)
				{
					if(!BUTTON_IsPressed(hBUTTON_ExitRS485Scanner)) break;
					GUI_TOUCH_Exec();
					GUI_Delay(50);
				}
				
				DISP_DeleteScanRS485BusScreen();
				DISP_CreateToolsScreen();
				SPINBOX_SetValue(hSPINBOX_SetAddress, strval);
				DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				DISP_RefreshSet();
				GUI_Delay(100);
			}
			else
			{
                ZEROFILL(str, sizeof(str));
				strval = LISTVIEW_GetSelUnsorted(hLISTVIEW_AddresseFound);
				LISTVIEW_GetItemText(hLISTVIEW_AddresseFound,0, strval, str, 16);				
				strval = atoi(str);
			}
			break;
		}
		
		
		default:
		{
			GUI_Delay(100);
			break;
		}
	}
}


void DISP_UpdateLog(void)
{
	uint32_t js, i;

	if(ActiveScreen != DEFAULT) return;
	
	memcpy(string5, string4, sizeof(string5));
    memcpy(string4, string3, sizeof(string4));
    memcpy(string3, string2, sizeof(string3));
    memcpy(string2, string1, sizeof(string2));
    ZEROFILL(string1, COUNTOF(string1));
    i = ((eebuff[3] << 8) & 0xFF00U) | eebuff[4];   // conver log source address to 16 bit int
    if (i == 0x0U)  memcpy(string1, "HOTEL CTRL", 12);      // log source is hotel controller event
    else            
    {
        memcpy(string1, "RS485 ADDR: ", 12);                // log source is rs485 addressed device
        Int2Str(&string1[12], i, 0x0U);                       // display device rs485 address
    }
    js = strlen(string1);
    string1[js] = ' ';
    ++js;
	
	switch (eebuff[2])  // display log event tekst
	{
		case GUEST_CARD:
			memcpy(&string1[js], "GUEST CARD VALID", 16);
			break;		
		case HANDMAID_CARD:
			memcpy(&string1[js], "HANDMAID CARD VALID", 19);
			break;		
		case ENTRY_DOOR_CLOSED:
			memcpy(&string1[js], "ENTRY DOOR CLOSED", 17);
			break;		
		case HANDMAID_SERVICE_END:
			memcpy(&string1[js], "HANDMAID SERVICE END", 20);
			break;		
		case MANAGER_CARD:
			memcpy(&string1[js], "MANAGER CARD USED", 17);
			break;		
		case SERVICE_CARD:
			memcpy(&string1[js], "SERVICE CARD USED", 17);
			break;		
		case ENTRY_DOOR_OPENED:
			memcpy(&string1[js], "ENTRY DOOR OPENED", 17);
			break;		
		case MINIBAR_USED:
			memcpy(&string1[js], "MINIBAR USED", 12);
			break;		
		case BALCON_DOOR_OPENED:
			memcpy(&string1[js], "BALCONY DOOR OPENED", 19);
			break;		
		case BALCON_DOOR_CLOSED:
			memcpy(&string1[js], "BALCONY DOOR CLOSED", 19);
			break;		
		case CARD_STACKER_ON:
			memcpy(&string1[js], "CARD IN STACKER", 15);
			break;		
		case CARD_STACKER_OFF:
			memcpy(&string1[js], "CARD OUT OF STACKER", 19);
			break;		
		case HANDMAID_SWITCH_ON:
			memcpy(&string1[js], "HANDMAID CALL ON", 16);
			break;		
		case HANDMAID_SWITCH_OFF:
			memcpy(&string1[js], "HANDMAID CALL OFF", 17);
			break;		
		case SOS_ALARM_TRIGGER:
			memcpy(&string1[js], "SOS ALARM TRIGGER", 17);
			break;				
		case SOS_ALARM_RESET:
			memcpy(&string1[js], "SOS ALARM RESET", 15);
			break;		
		case FIRE_ALARM_TRIGGER:
			memcpy(&string1[js], "FIRE ALARM TRIGGER", 18);
			break;		
		case FIRE_ALARM_RESET:
			memcpy(&string1[js], "FIRE ALARM RESET", 16);
			break;		
		case UNKNOWN_CARD:		
			memcpy(&string1[js], "UNKNOWN CARD", 12);
			break;		
		case DO_NOT_DISTURB_SWITCH_ON:		
			memcpy(&string1[js], "DO NOT DISTURB ON", 17);
			break;
		case DO_NOT_DISTURB_SWITCH_OFF:						
			memcpy(&string1[js], "DO NOT DISTURB OFF", 18);	
			break;		
		case CARD_EXPIRED:					
			memcpy(&string1[js], "CARD TIME EXPIRED", 17);
			break;		
		case WRONG_ROOM:							
			memcpy(&string1[js], "WRONG ROOM", 10);
			break;		
		case WRONG_SYSID:										
			memcpy(&string1[js], "WRONG SYSTEM ID", 15);
			break;		
		case ENTRY_DOOR_NOT_CLOSED:
			memcpy(&string1[js], "ENTRY DOOR NOT CLOSED", 21);
			break;		
		case DOOR_BELL_ACTIVE:
			memcpy(&string1[js], "DOOR BELL ACTIV", 15);
			break;        
        case DOOR_LOCK_USER_OPEN:
			memcpy(&string1[js], "DOOR OPEN BY USER", 17);
			break;		
		case PIN_RESET:
			memcpy(&string1[js], "PIN RESET", 9);
			break;		
		case POWER_ON_RESET:
			memcpy(&string1[js], "POWER-ON RESET", 14);
			break;		
		case SOFTWARE_RESET:
			memcpy(&string1[js], "SOFTWARE  RESET", 15);
			break;		
		case IWDG_RESET:
			memcpy(&string1[js], "IWDG RESET", 10);
			break;		
		case WWDG_RESET:
			memcpy(&string1[js], "WWDG RESET", 10);
			break;		
		case LOW_POWER_RESET:
			memcpy(&string1[js], "LOW POWER RESET", 15);
			break;		
		case FW_UPDATED:
			memcpy(&string1[js], "FIRMWARE UPDATED", 16);
			break;		
		case IMG_UPD_FAIL:
            js = strlen(string1);
            switch (eebuff[5])
            {
                case FS_DRIVE_ERR:
                    memcpy(&string1[js], "uSD CARD FILE SYSTEM ERROR" , 26);
                    break;
                case FS_DIRECTORY_ERROR:
                    memcpy(&string1[js], "uSD CARD DIRECTORY ERROR" , 24);
                    break;
                case FS_FILE_ERROR:
                    memcpy(&string1[js], "uSD CARD FILE ERROR" , 19);
                    break;
            }
			break;		
		case FW_UPD_FAIL:
			memcpy(&string1[js], "FIRMWARE UPDATE FAIL", 20);
            break;
		case IMG_UPDATED:
			memcpy(&string1[js], "UPDATED IMAGE ", 14);
            js = strlen(string1);
            Int2Str(&string1[js], eebuff[5], 0x0U);
			break;		
		case BLDR_UPDATED:
			memcpy(&string1[js], "BOOTLOADER UPDATED", 18);
			break;		
		case BLDR_UPD_FAIL:
			memcpy(&string1[js], "BOOTLOADER UPDATE FAIL", 22);
			break;		
		case RS485_BUS_ERROR:
			memcpy(&string1[js], "RS485 BUS ERROR", 15);
			break;		
		case DEV_NOT_RESP:
			memcpy(&string1[js], "DEVICE NOT RESPONDING", 21);
			break;      
        case FUNC_OR_DRV_FAIL:
			memcpy(&string1[js], "FUNCTION OR DRIVER FAIL", 23);
//            case DIO_FUNC:
//            case CAP_FUNC:
//            case RC522_FUNC:
//            case DISP_FUNC:
//                memcpy(&string1[js], "DISPLAY ERROR", 13);
			break;        
        case RT_RPM_SENS_ERR:
            memcpy(&string1[js], "FANCOIL RPM SENSOR ERROR", 23);
			break;        
        case RT_FANC_NTC_ERR:
            memcpy(&string1[js], "FANCOIL NTC SENSOR ERROR", 24);
			break;
        case RT_LO_TEMP_ERR:
            memcpy(&string1[js], "FANCOIL TEMPER. TOO LOW", 24);
			break;        
        case RT_HI_TEMP_ERR:
            memcpy(&string1[js], "FANCOIL TEMPER. TOO HIGH", 25);
			break;        
        case RT_FREEZ_PROT:
            memcpy(&string1[js], "FANCOIL FREEZING ALARM", 23);
			break;        
        case RT_DISP_NTC_ERR:
            memcpy(&string1[js], "THERMOSTAT NTC SENSOR ERROR", 28);
			break;
        case ADDRESS_LIST_uSD_ERR:
            memcpy(&string1[js], "uSD CARD ADDR. LIST ERROR", 25);
            break;
        case ADDRESS_LIST_ERR:
            memcpy(&string1[js], "NO ADDREESS LIST", 25);
            break;
        default:
            memcpy(&string1[js], "UNKNOWN EVENT", 13);
            break;
	}
	
	js = strlen(string1);
    memset(&string1[js], ' ', sizeof(string1) - js);
	string1[sizeof(string1) - 1] = 0x0U;
    GUI_ClearRect(120, 110, 480, 220);
    GUI_SetTextAlign(GUI_TA_LEFT|GUI_TA_TOP);
    GUI_SetBkColor(GUI_TRANSPARENT);
    GUI_SetFont(&GUI_Font16B_1);
	GUI_SetColor(GUI_WHITE);
	GUI_DispStringAt(string1, 125, 200);
	GUI_DispStringAt(string2, 125, 180);
	GUI_DispStringAt(string3, 125, 160);
	GUI_DispStringAt(string4, 125, 140);
	GUI_DispStringAt(string5, 125, 120);
	GUI_DispCEOL();
	GUI_ClearRect(120, 65, 475, 105);
	GUI_SetColor((0x62ul << 24) | GUI_BLACK);
	GUI_FillRect(120, 65, 475, 105);
    GUI_SetTextAlign(GUI_TA_LEFT|GUI_TA_TOP);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);
	GUI_SetFont(&GUI_Font16B_1);
    GUI_SetColor(GUI_WHITE);
	GUI_GotoXY(125, 80);
	GUI_DispString(string1);
	GUI_Exec();
}


void DISP_FileTransferState(uint8_t nsta)
{
	if (ActiveScreen != DEFAULT) return;
	GUI_ClearRect(120, 65, 475, 105);
	GUI_SetColor((0x62ul << 24) | GUI_BLACK);
	GUI_FillRect(120, 65, 475, 105);
    GUI_SetTextAlign(GUI_TA_LEFT|GUI_TA_TOP);
    GUI_SetTextMode(GUI_TEXTMODE_TRANS);
    GUI_SetFont(&GUI_Font16B_1);
	GUI_SetColor(GUI_WHITE);
	GUI_GotoXY(125, 80);
	GUI_DispString("RS485 ADDR: ");
	GUI_DispDecMin(rs485_txaddr);
    
	if      (nsta == FS_DRIVE_ERR) 			GUI_DispString(" UPDATE FAIL - FILE SYS. ERROR");
	else if (nsta == FS_DIRECTORY_ERROR)    GUI_DispString(" UPDATE FAIL - FILE DIR. ERROR");
	else if (nsta == FS_FILE_ERROR)         GUI_DispString(" UPDATE FAIL - FILE ERROR");
	else if (nsta == FW_UPDATE_RUN)         GUI_DispString(" FIRMWARE UPDATE RUN");
    else if (nsta == UPD_BINARY)            GUI_DispString(" OLD FIRMWARE UPDATE RUN");
    else if (nsta == FW_UPDATE_END)         GUI_DispString(" FIRMWARE UPDATE SUCCESS");
	else if (nsta == BLDR_UPDATED)          GUI_DispString(" BOOTLOADER UPDATE SUCCESS");
	else if (nsta == FILE_UPDATE_FINISHED)  GUI_DispString(" FILE UPDATE SUCCESS");
	else if (nsta == FILE_UPDATE_FAIL)      GUI_DispString(" FILE UPDATE FAIL - TRANSFER  ERROR");
    else if (nsta == FW_UPDATE_FAIL)        GUI_DispString(" FW UPDATE FAIL - TRANSFER  ERROR");
	else if (nsta == UPD_FILE)
	{
        GUI_DispString(" IMAGE ");
        GUI_DispDecMin(filupd_list[filupd_list_cnt]);
        GUI_DispString(" UPDATE RUN");
    }
	
	GUI_Exec();
}


void DISP_ProgbarSetNewState(uint8_t nsta)
{
    switch (nsta)
    {
        case 0:
        {
            WM_HideWindow(hPROGBAR_FileTransfer);
            GUI_ClearRect(120, 110, 475, 135);
            break;
        }
        
        case 1:
        {
            PROGBAR_SetValue(hPROGBAR_FileTransfer, 0);
            WM_ShowWindow(hPROGBAR_FileTransfer);
            break;
        }
    }
}


void DISP_uSDCardSetNewState(uint8_t nsta)
{
    switch (nsta)
    {
        case 0:
        {
            DISP_uSDCardErrorSet();
            GUI_ClearRect(USD_H_POS, USD_V_POS, USD_H_POS + USD_H_SIZE, USD_V_POS + USD_V_SIZE);
            GUI_DrawBitmap(&bmusd_error, USD_H_POS, USD_V_POS);
            GUI_SetColor(GUI_RED);
            GUI_SetBkColor(GUI_BLACK);
            GUI_SetTextMode(GUI_TEXTMODE_NORMAL);
            GUI_SetTextAlign(GUI_TA_HCENTER|GUI_TA_VCENTER);
            GUI_GotoXY(USD_TX_H_POS, USD_TX_V_POS);
            GUI_SetFont(&GUI_Font16B_1);
            GUI_DispString("uSD");
            GUI_SetBkColor(GUI_TRANSPARENT);
            break;
        }
        
        case 1:
        {
            DISP_uSDCardReadySet();
            GUI_ClearRect(USD_H_POS, USD_V_POS, USD_H_POS + USD_H_SIZE, USD_V_POS + USD_V_SIZE);
            GUI_DrawBitmap(&bmusd_ok, USD_H_POS, USD_V_POS);
            GUI_SetColor(GUI_GREEN);
            GUI_SetBkColor(GUI_BLACK);
            GUI_SetTextMode(GUI_TEXTMODE_NORMAL);
            GUI_SetTextAlign(GUI_TA_HCENTER|GUI_TA_VCENTER);
            GUI_GotoXY(USD_TX_H_POS, USD_TX_V_POS);
            GUI_SetFont(&GUI_Font16B_1);
            GUI_DispString("uSD");
            GUI_SetBkColor(GUI_TRANSPARENT);
            break;
        }
    }
}


int APP_GetData(void * p, const U8 * * ppData, unsigned NumBytesReq, U32 Off) 
{
	UINT NumBytesRead;
	FIL *phFile = (FIL*)p;
	static char _acBuffer[0x1000];
	
	f_lseek(phFile,Off);
	f_read(phFile, _acBuffer, NumBytesReq, &NumBytesRead);
	*ppData = (const U8*) &_acBuffer;
	return NumBytesRead;
}


void DISP_CreateDefaultScreen(void)
{
	GUI_SetBkColor(GUI_TRANSPARENT); 
	GUI_Clear();
	GUI_SetFont(&GUI_Font8x16);
	GUI_SetColor((0x62ul << 24) | GUI_BLACK);
	GUI_FillRect(120, 65, 475, 105);
	GUI_FillRect(5, 65, 115, 225);
	GUI_FillRect(5, 230, 475, 270);
//	GUI_FillRect(USER_LOGO_H_POS, USER_LOGO_V_POS, USER_LOGO_H_POS + USER_LOGO_H_SIZE, USER_LOGO_V_POS + USER_LOGO_V_SIZE);
	if (rs485_bus_sta == RS485_BUS_CONNECTED) RS485_BusDisconnected();
	else RS485_BusConnected();	
	if(IsDISP_uSDCardReady() != 0x0U) DISP_uSDCardSetNewState(0x1U);
	else DISP_uSDCardSetNewState(0x0U);	
	hBUTTON_Settings = BUTTON_Create(10, 240, 80, 30, GUI_ID_BUTTON_Settings, WM_CF_SHOW); // Settings
	BUTTON_SetText(hBUTTON_Settings, "Settings");
	hBUTTON_Tools = BUTTON_Create(135, 240, 80, 30, GUI_ID_BUTTON_Tools, WM_CF_SHOW); // Tools
	BUTTON_SetText(hBUTTON_Tools, "Tools");
	hBUTTON_Control = BUTTON_Create(260, 240, 80, 30, GUI_ID_BUTTON_Control, WM_CF_SHOW); //Control
	BUTTON_SetText(hBUTTON_Control, "Control");
	hBUTTON_Unlock = BUTTON_Create(390, 240, 80, 30, GUI_ID_BUTTON_Unlock, WM_CF_SHOW); //Unlock
	if(IsDISP_UnlockActiv()) BUTTON_SetText(hBUTTON_Unlock, "Lock");
	else BUTTON_SetText(hBUTTON_Unlock, "Unlock");
	DISP_UpdateTimeSet();
	ActiveScreen = DEFAULT;
}


void DISP_DeleteDefaultScreen(void)
{
	WM_DeleteWindow(hBUTTON_Settings);
	WM_DeleteWindow(hBUTTON_Tools);
	WM_DeleteWindow(hBUTTON_Control);
	WM_DeleteWindow(hBUTTON_Unlock);
}


void DISP_CreateToolsScreen(void)
{
	GUI_SetBkColor(GUI_BLACK);
	GUI_Clear();
	GUI_SetColor(GUI_WHITE);

	hBUTTON_ToolFileList = BUTTON_Create(190, 10, 60, 60, GUI_ID_BUTTON_ToolFileList, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_ToolFileList, "uSD\nFILE LIST");	
	hBUTTON_ToolUpdateFW = BUTTON_Create(300, 10, 60, 60, GUI_ID_BUTTON_ToolUpdateFW, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_ToolUpdateFW, "UPDATE\nFW");
	hBUTTON_ToolLoadList = BUTTON_Create(410, 10, 60, 60, GUI_ID_BUTTON_ToolLoadList, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_ToolLoadList, "LOAD\nCTRL LIST");
	
	hBUTTON_ToolScanRS485 = BUTTON_Create(190, 85, 60, 60, GUI_ID_BUTTON_ToolScanRS485, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_ToolScanRS485, "BUS\nSCAN");
	hBUTTON_ToolUploadImage = BUTTON_Create(300, 85, 60, 60, GUI_ID_BUTTON_ToolUploadImage, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_ToolUploadImage, "IMAGE\nUPLOAD");
	hBUTTON_ToolUploadFirmware = BUTTON_Create(410, 85, 60, 60, GUI_ID_BUTTON_ToolUploadFirmware, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_ToolUploadFirmware, "FIRMW.\nUPLOAD");

	hBUTTON_ToolFormatCard = BUTTON_Create(300, 160, 60, 60, GUI_ID_BUTTON_ToolFormatCard, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_ToolFormatCard, "uSD\nFORMAT");	
	hBUTTON_ToolDisplayCalibration = BUTTON_Create(410, 160, 60, 60, GUI_ID_BUTTON_ToolDisplayCalibration, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_ToolDisplayCalibration, "DISPLAY\nCALIBR.");
	
	GUI_DispStringAt("CONTROLLER", 190, 160);
	hSPINBOX_SetAddress = SPINBOX_CreateEx(190, 180, 80, 40, 0, WM_CF_SHOW, GUI_ID_SPINBOX_SetAddress, 10, 65500);
	SPINBOX_SetValue(hSPINBOX_SetAddress, 100);
	SPINBOX_SetEdge(hSPINBOX_SetAddress, SPINBOX_EDGE_CENTER);
	
	hBUTTON_ToolExit = BUTTON_Create(390, 240, 80, 30, GUI_ID_BUTTON_ToolExit, WM_CF_SHOW); //Info
	BUTTON_SetText(hBUTTON_ToolExit, "EXIT");

	hTree = TREEVIEW_CreateEx(10, 10, 160, 260, 0, WM_CF_SHOW, 0, GUI_ID_TREEVIEW_FileList);
	TREEVIEW_SetAutoScrollV(hTree, 1);
	TREEVIEW_SetSelMode(hTree, TREEVIEW_SELMODE_ROW);
	ActiveScreen = TOOLS;
}	

void DISP_DeleteToolsScreen(void)
{
	WM_DeleteWindow(hBUTTON_ToolScanRS485);
	WM_DeleteWindow(hBUTTON_ToolDisplayCalibration);
	WM_DeleteWindow(hBUTTON_ToolUpdateFW);
	WM_DeleteWindow(hBUTTON_ToolUploadImage);
	WM_DeleteWindow(hBUTTON_ToolUploadFirmware);
	WM_DeleteWindow(hBUTTON_ToolFileList);
	WM_DeleteWindow(hBUTTON_ToolFormatCard);
	WM_DeleteWindow(hBUTTON_ToolLoadList);
	WM_DeleteWindow(hBUTTON_ToolExit);
	WM_DeleteWindow(hSPINBOX_SetAddress);
	WM_DeleteWindow(hTree);
}


void DISP_CreateScanRS485BusScreen(void)
{
	GUI_SetBkColor(GUI_BLACK);
	GUI_Clear();
	hLISTVIEW_AddresseFound = LISTVIEW_Create(10, 10, 460, 145, 0, GUI_ID_LISTVIEW_AddresseFound, WM_CF_SHOW, 0x0U);
	LISTVIEW_AddColumn(hLISTVIEW_AddresseFound, 50, "Address", GUI_TA_HCENTER | GUI_TA_VCENTER);
	LISTVIEW_AddColumn(hLISTVIEW_AddresseFound, 205, "Hardware Version", GUI_TA_HCENTER | GUI_TA_VCENTER);
	LISTVIEW_AddColumn(hLISTVIEW_AddresseFound, 205, "Firmware Version", GUI_TA_HCENTER | GUI_TA_VCENTER);
	LISTVIEW_SetGridVis(hLISTVIEW_AddresseFound, 1);
	LISTVIEW_SetAutoScrollV(hLISTVIEW_AddresseFound, 1);
	hSPINBOX_StartAddress = SPINBOX_CreateEx(10, 185, 80, 30, 0, WM_CF_SHOW, GUI_ID_SPINBOX_StartAddress, 10, 65500);
	SPINBOX_SetValue(hSPINBOX_StartAddress, 100);
	SPINBOX_EnableBlink(hSPINBOX_StartAddress, 250, 1);
	SPINBOX_SetEdge(hSPINBOX_StartAddress, SPINBOX_EDGE_CENTER);
	hSPINBOX_EndAddress = SPINBOX_CreateEx(135, 185, 80, 30, 0, WM_CF_SHOW, GUI_ID_SPINBOX_EndAddress, 10, 65500);
	SPINBOX_EnableBlink(hSPINBOX_EndAddress, 250, 1);
	SPINBOX_SetValue(hSPINBOX_EndAddress, 100);
	SPINBOX_SetEdge(hSPINBOX_EndAddress, SPINBOX_EDGE_CENTER);
	hSPINBOX_OldAddress = SPINBOX_CreateEx(10, 240, 80, 30, 0, WM_CF_SHOW, GUI_ID_SPINBOX_OldAddress, 10, 65500);
	SPINBOX_EnableBlink(hSPINBOX_OldAddress, 250, 1);
	SPINBOX_SetValue(hSPINBOX_OldAddress, 100);
	SPINBOX_SetEdge(hSPINBOX_OldAddress, SPINBOX_EDGE_CENTER);
	hSPINBOX_NewAddress = SPINBOX_CreateEx(135, 240, 80, 30, 0, WM_CF_SHOW, GUI_ID_SPINBOX_NewAddress, 10, 65500);
	SPINBOX_EnableBlink(hSPINBOX_NewAddress, 250, 1);
	SPINBOX_SetValue(hSPINBOX_StartAddress, 100);
	SPINBOX_SetEdge(hSPINBOX_NewAddress, SPINBOX_EDGE_CENTER);	
	hRADIO_ScanMode = RADIO_CreateEx( 260, 165, 90, 0, 0,WM_CF_SHOW, 0, GUI_ID_RADIO_ScanMode, 4, 15);
	RADIO_SetText(hRADIO_ScanMode, "Find FIRST", 0);
	RADIO_SetText(hRADIO_ScanMode, "Find NEXT", 1);
	RADIO_SetText(hRADIO_ScanMode, "Find ALL NEW", 2);
	RADIO_SetText(hRADIO_ScanMode, "Find ALL", 3);
	RADIO_SetValue(hRADIO_ScanMode, 3);
	hBUTTON_Scan = BUTTON_Create(390, 185, 80, 30, GUI_ID_BUTTON_Scan, WM_CF_SHOW); // Settings
	BUTTON_SetText(hBUTTON_Scan, "SCAN");
	hBUTTON_AddressSet = BUTTON_Create(260, 240, 80, 30, GUI_ID_BUTTON_AddressSet, WM_CF_SHOW); // Tools
	BUTTON_SetText(hBUTTON_AddressSet, "SET ADDRESS");
	hBUTTON_ExitRS485Scanner = BUTTON_Create(390, 240, 80, 30, GUI_ID_BUTTON_ExitRS485Scanner, WM_CF_SHOW); //Info
	BUTTON_SetText(hBUTTON_ExitRS485Scanner, "EXIT");
	GUI_SetFont(&GUI_Font13_ASCII);
	GUI_GotoXY(10, 168);
	GUI_DispString("START ADDRESS");
	GUI_GotoXY(135, 168);
	GUI_DispString("END ADDRESS");
	GUI_GotoXY(10, 223);
	GUI_DispString("OLD ADDRESS");
	GUI_GotoXY(135, 223);
	GUI_DispString("NEW ADDRESS");
	row = 0;
	ActiveScreen = RS485;
}

void DISP_DeleteScanRS485BusScreen(void)
{
	WM_DeleteWindow(hLISTVIEW_AddresseFound);
	WM_DeleteWindow(hSPINBOX_StartAddress);
	WM_DeleteWindow(hSPINBOX_EndAddress);;
	WM_DeleteWindow(hSPINBOX_OldAddress);
	WM_DeleteWindow(hSPINBOX_NewAddress);
	WM_DeleteWindow(hRADIO_ScanMode);
	WM_DeleteWindow(hBUTTON_Scan);
	WM_DeleteWindow(hBUTTON_AddressSet);
	WM_DeleteWindow(hBUTTON_ExitRS485Scanner);
}


void DISP_CreateSettingsScreen(void)
{
	GUI_SetBkColor(GUI_BLACK);
	GUI_Clear();
	GUI_SetColor(GUI_GREEN);
	
	hBUTTON_SettingsEdit = BUTTON_Create(10, 240, 80, 30, GUI_ID_BUTTON_SettingsEdit, WM_CF_SHOW); // Settings Apply
	BUTTON_SetText(hBUTTON_SettingsEdit, "Edit");
	WM_DisableWindow(hBUTTON_SettingsEdit);
	hBUTTON_SettingsOk = BUTTON_Create(133, 240, 80, 30, GUI_ID_BUTTON_SettingsOk, WM_CF_SHOW); // Settings OK
	BUTTON_SetText(hBUTTON_SettingsOk, "OK");
	hBUTTON_SettingsApply = BUTTON_Create(260, 240, 80, 30, GUI_ID_BUTTON_SettingsApply, WM_CF_SHOW); // Settings Apply
	BUTTON_SetText(hBUTTON_SettingsApply, "Apply");
	hBUTTON_SettingsCancel = BUTTON_Create(390, 240, 80, 30, GUI_ID_BUTTON_SettingsCancel, WM_CF_SHOW); // Settings Cancel
	BUTTON_SetText(hBUTTON_SettingsCancel, "Cancel");
	
	hCHKBOX_EnableDHCP = CHECKBOX_Create(10, 110, 80, 20, 0, GUI_ID_CHECK_EnableDHCP, WM_CF_SHOW);
	CHECKBOX_SetTextColor(hCHKBOX_EnableDHCP, GUI_GREEN);	
	CHECKBOX_SetText(hCHKBOX_EnableDHCP, "DHCP client");
    if (IsDHCP_ClientEnabled()) CHECKBOX_SetState(hCHKBOX_EnableDHCP, 1);
    else CHECKBOX_SetState(hCHKBOX_EnableDHCP, 0);
	
	hCHKBOX_EnableBroadcastTime = CHECKBOX_Create(200, 65, 100, 20, 0, GUI_ID_CHECK_EnableBroadcastTime, WM_CF_SHOW);	
	CHECKBOX_SetTextColor(hCHKBOX_EnableBroadcastTime, GUI_GREEN);	
	CHECKBOX_SetText(hCHKBOX_EnableBroadcastTime, "Broadcast Time");
    if (IsTIME_BroadcastEnabled()) CHECKBOX_SetState(hCHKBOX_EnableBroadcastTime, 1);
    else CHECKBOX_SetState(hCHKBOX_EnableBroadcastTime, 0);
	
	hCHKBOX_EnableHTTP = CHECKBOX_Create(355, 35, 96, 20, 0, GUI_ID_CHECK_EnableHTTP, WM_CF_SHOW);
	CHECKBOX_SetTextColor(hCHKBOX_EnableHTTP, GUI_GREEN);		
	CHECKBOX_SetText(hCHKBOX_EnableHTTP, "Enable Server");
    if (IsHTTP_ServerEnabled()) CHECKBOX_SetState(hCHKBOX_EnableHTTP, 1);
    else CHECKBOX_SetState(hCHKBOX_EnableHTTP, 0);
	
	hCHKBOX_EnableWebConfig = CHECKBOX_Create(355, 60, 120, 20, 0, GUI_ID_CHECK_EnableWebConfig, WM_CF_SHOW);	
	CHECKBOX_SetTextColor(hCHKBOX_EnableWebConfig, GUI_GREEN);
	CHECKBOX_SetText(hCHKBOX_EnableWebConfig, "Enable Web Config");
    if (IsWEB_ConfigEnabled()) CHECKBOX_SetState(hCHKBOX_EnableWebConfig, 1);
    else CHECKBOX_SetState(hCHKBOX_EnableWebConfig, 0);
	
	hCHKBOX_EnableTFTP = CHECKBOX_Create(355, 110, 96, 20, 0, GUI_ID_CHECK_EnableTFTP, WM_CF_SHOW);
	CHECKBOX_SetTextColor(hCHKBOX_EnableTFTP, GUI_GREEN);
	CHECKBOX_SetText(hCHKBOX_EnableTFTP, "Enable Server");
    if (IsTFTP_ServerEnabled()) CHECKBOX_SetState(hCHKBOX_EnableTFTP, 1);
    else CHECKBOX_SetState(hCHKBOX_EnableTFTP, 0);
	
	hEDIT_TcpIpAddress = EDIT_Create(10, 35, 100, 20, GUI_ID_EDIT_TcpIpAddress, 16, WM_CF_SHOW);
	EDIT_SetText(hEDIT_TcpIpAddress, (char *)iptxt);
	EDIT_EnableBlink(hEDIT_TcpIpAddress, 250, 1);
	
	hEDIT_TcpIpGateway = EDIT_Create(10, 60, 100, 20, GUI_ID_EDIT_TcpIpGateway, 16, WM_CF_SHOW);
	EDIT_SetText(hEDIT_TcpIpGateway, (char *)gtwtxt);
	EDIT_EnableBlink(hEDIT_TcpIpGateway, 250, 1);
	
	hEDIT_TcpIpSubnet = EDIT_Create(10, 85, 100, 20, GUI_ID_EDIT_TcpIpSubnet, 16, WM_CF_SHOW);
	EDIT_SetText(hEDIT_TcpIpSubnet, (char *)sbnttxt);
	EDIT_EnableBlink(hEDIT_TcpIpSubnet, 250, 1);
	
	hEDIT_RS485_InterfaceAddress = EDIT_Create(10, 170, 50, 20, GUI_ID_EDIT_RS485_InterfaceAddress, 5, WM_CF_SHOW);
    ZEROFILL(string, sizeof (string));
	Int2Str(string, rsifa, 0x0U);
	EDIT_SetText(hEDIT_RS485_InterfaceAddress, string);
	EDIT_EnableBlink(hEDIT_RS485_InterfaceAddress, 250, 1);
	
	hEDIT_RS485_BroadcastAddress = EDIT_Create(10, 195, 50, 20, GUI_ID_EDIT_RS485_BroadcastAddress, 5, WM_CF_SHOW);
	ZEROFILL(string, sizeof (string));
	Int2Str(string, rsbra, 0x0U);
	EDIT_SetText(hEDIT_RS485_BroadcastAddress, string);
	EDIT_EnableBlink(hEDIT_RS485_BroadcastAddress, 250, 1);
	
	hEDIT_Password = EDIT_Create(200, 170, 50, 20, GUI_ID_EDIT_Password, 5, WM_CF_SHOW);
	EDIT_SetText(hEDIT_Password, password);
	EDIT_EnableBlink(hEDIT_Password, 250, 1);
	
	hEDIT_SysID = EDIT_Create(355, 170, 50, 20, GUI_ID_EDIT_SysID, 5, WM_CF_SHOW);
	ZEROFILL(string, sizeof (string));
	Int2Str(string, system_id, 0x0U);
	EDIT_SetText(hEDIT_SysID, string);
	EDIT_EnableBlink(hEDIT_SysID, 250, 1);
	
	hEDIT_DateTime = EDIT_Create(200, 35, 120, 20, GUI_ID_EDIT_DateTime, 21, WM_CF_SHOW);
	EDIT_EnableBlink(hEDIT_DateTime, 250, 1);
	
	GUI_SetFont(&GUI_Font13_ASCII);
	GUI_GotoXY(5, 15);
	GUI_DispString("TCP/IP SETTINGS");
	GUI_GotoXY(115, 37);
	GUI_DispString("IPv4 Address");
	GUI_GotoXY(115, 62);
	GUI_DispString("Def. Gateway");
	GUI_GotoXY(115, 87);
	GUI_DispString("Subnet Mask");
	GUI_GotoXY(195, 15);
	GUI_DispString("DATE & TIME");
	GUI_GotoXY(350, 15);
	GUI_DispString("HTTP SERVER");
	GUI_GotoXY(350, 90);
	GUI_DispString("TFTP SERVER");
	GUI_GotoXY(5, 150);
	GUI_DispString("RS485 INTERFACE");
	GUI_GotoXY(67, 173);
	GUI_DispString("Interface Address");
	GUI_GotoXY(67, 198);
	GUI_DispString("Broadcast Address");
	GUI_GotoXY(195, 150);
	GUI_DispString("PASSSWORD");
	GUI_GotoXY(350, 150);
	GUI_DispString("SYSTEM ID");
	ActiveScreen = SETTINGS;
}

void DISP_DeleteSettingsScreen(void)
{
	WM_DeleteWindow(hBUTTON_SettingsEdit);
	WM_DeleteWindow(hBUTTON_SettingsOk);
	WM_DeleteWindow(hBUTTON_SettingsCancel);
	WM_DeleteWindow(hBUTTON_SettingsApply);
	WM_DeleteWindow(hCHKBOX_EnableDHCP);
	WM_DeleteWindow(hCHKBOX_EnableBroadcastTime);
	WM_DeleteWindow(hCHKBOX_EnableHTTP);
	WM_DeleteWindow(hCHKBOX_EnableWebConfig);
	WM_DeleteWindow(hCHKBOX_EnableTFTP);
	WM_DeleteWindow(hEDIT_TcpIpAddress);
	WM_DeleteWindow(hEDIT_TcpIpGateway);
	WM_DeleteWindow(hEDIT_TcpIpSubnet);
	WM_DeleteWindow(hEDIT_RS485_InterfaceAddress);
	WM_DeleteWindow(hEDIT_RS485_BroadcastAddress);
	WM_DeleteWindow(hEDIT_DateTime);
	WM_DeleteWindow(hEDIT_Password);
	WM_DeleteWindow(hEDIT_SysID);
}


void DISP_CreateKeypad(void)
{
	GUI_SetBkColor(GUI_BLACK);
	GUI_SetColor(GUI_BLACK);
	GUI_FillRect(130, 0, 390, 240);
	hBUTTON_OSK_1 = BUTTON_Create(140, 50, 60, 60, GUI_ID_BUTTON_OSK_1, WM_CF_SHOW); 
	BUTTON_SetText(hBUTTON_OSK_1, "1");
	hBUTTON_OSK_2 = BUTTON_Create(200, 50, 60, 60, GUI_ID_BUTTON_OSK_2, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_OSK_2, "2");	
	hBUTTON_OSK_3 = BUTTON_Create(260, 50, 60, 60, GUI_ID_BUTTON_OSK_3, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_OSK_3, "3");
	hBUTTON_OSK_4 = BUTTON_Create(320, 50, 60, 60, GUI_ID_BUTTON_OSK_4, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_OSK_4, "4");
	hBUTTON_OSK_5 = BUTTON_Create(140, 110, 60, 60, GUI_ID_BUTTON_OSK_5, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_OSK_5, "5");
	hBUTTON_OSK_6 = BUTTON_Create(200, 110, 60, 60, GUI_ID_BUTTON_OSK_6, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_OSK_6, "6");
	hBUTTON_OSK_7 = BUTTON_Create(260, 110, 60, 60, GUI_ID_BUTTON_OSK_7, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_OSK_7, "7");
	hBUTTON_OSK_8 = BUTTON_Create(320, 110, 60, 60, GUI_ID_BUTTON_OSK_8, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_OSK_8, "8");
	hBUTTON_OSK_9 = BUTTON_Create(140, 170, 60, 60, GUI_ID_BUTTON_OSK_9, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_OSK_9, "9");
	hBUTTON_OSK_0 = BUTTON_Create(200, 170, 60, 60, GUI_ID_BUTTON_OSK_0, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_OSK_0, "0");
	hBUTTON_OSK_Ok = BUTTON_Create(260, 170, 60, 60, GUI_ID_BUTTON_OSK_Ok, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_OSK_Ok, "OK");
	hBUTTON_OSK_Back = BUTTON_Create(320, 170, 60, 60, GUI_ID_BUTTON_OSK_Back, WM_CF_SHOW);
	BUTTON_SetText(hBUTTON_OSK_Back, "<BCK");
	DISP_KeypadSet();
}


void DISP_DeleteKeypad(void)
{
	WM_DeleteWindow(hBUTTON_OSK_0);
	WM_DeleteWindow(hBUTTON_OSK_1);
	WM_DeleteWindow(hBUTTON_OSK_2);
	WM_DeleteWindow(hBUTTON_OSK_3);
	WM_DeleteWindow(hBUTTON_OSK_4);
	WM_DeleteWindow(hBUTTON_OSK_5);
	WM_DeleteWindow(hBUTTON_OSK_6);
	WM_DeleteWindow(hBUTTON_OSK_7);
	WM_DeleteWindow(hBUTTON_OSK_8);
	WM_DeleteWindow(hBUTTON_OSK_9);
	WM_DeleteWindow(hBUTTON_OSK_Ok);
	WM_DeleteWindow(hBUTTON_OSK_Back);
}


void DISP_Keypad(uint8_t item, uint8_t key)
{
	uint8_t k;
	static uint8_t pswrd_bcnt = 0x5U;
    
	if((item == OSK_EDIT_IPADD) || (item == OSK_EDIT_GWADD) || (item == OSK_EDIT_SUBNET))
	{
		GUI_SetColor(GUI_BLACK);
		GUI_FillRect(130, 0, 360, 50);
		GUI_SetFont(&GUI_Font24B_1);
		GUI_SetColor(GUI_WHITE);
		GUI_SetTextMode(GUI_TEXTMODE_TRANS);
		
		if(key == 'B')
		{				
			if      (osk_ipedit == 0x1U)
			{
				string1[osk_bcnt] = NUL;
				if(osk_bcnt) --osk_bcnt;
				string1[osk_bcnt] = NUL;
				strval = atoi(string1);
			}
			else if (osk_ipedit == 0x2U)
			{
				string2[osk_bcnt] = NUL;
				if(osk_bcnt) --osk_bcnt;
				string2[osk_bcnt] = NUL;
				strval = atoi(string2);
			}
			else if (osk_ipedit == 0x3U)
			{
				string3[osk_bcnt] = NUL;
				if(osk_bcnt) --osk_bcnt;
				string3[osk_bcnt] = NUL;
				strval = atoi(string3);
			}
			else if (osk_ipedit == 0x4U)
			{
				string4[osk_bcnt] = NUL;
				if(osk_bcnt) --osk_bcnt;
				string4[osk_bcnt] = NUL;
				strval = atoi(string4);
			}
			
			GUI_GotoXY(150, 15);
			GUI_DispDecMin(strval);
		}
		else if(key == 'O')
		{
			if      (osk_ipedit == 1)
			{
				strval = atoi(string1);
				if      (item == OSK_EDIT_IPADD)    ip_add[0] = strval;
				else if (item == OSK_EDIT_GWADD)    gw_add[0] = strval;
				else if (item == OSK_EDIT_SUBNET)   subnet[0] = strval;
			}
			else if (osk_ipedit == 2)
			{
				strval = atoi(string2);
				if      (item == OSK_EDIT_IPADD)    ip_add[1] = strval;
				else if (item == OSK_EDIT_GWADD)    gw_add[1] = strval;
				else if (item == OSK_EDIT_SUBNET)   subnet[1] = strval;
			}
			else if (osk_ipedit == 3)
			{
				strval = atoi(string3);
				if(item == OSK_EDIT_IPADD)          ip_add[2] = strval;
				else if (item == OSK_EDIT_GWADD)    gw_add[2] = strval;
				else if (item == OSK_EDIT_SUBNET)   subnet[2] = strval;
			}
			else if (osk_ipedit == 4)
			{
				strval = atoi(string4);
				if      (item == OSK_EDIT_IPADD)    ip_add[3] = strval;
				else if (item == OSK_EDIT_GWADD)    gw_add[3] = strval;
				else if (item == OSK_EDIT_SUBNET)   subnet[3] = strval;
			}
			
			
			if      (item == OSK_EDIT_IPADD) 
			{
                ZEROFILL(iptxt, sizeof (iptxt));
				sprintf((char*)iptxt, "%d.%d.%d.%d", ip_add[0], ip_add[1], ip_add[2], ip_add[3]);
			}
			else if (item == OSK_EDIT_GWADD)
			{
                memset(gtwtxt, 0x0U, sizeof (gtwtxt));
				sprintf((char*)gtwtxt, "%d.%d.%d.%d", gw_add[0], gw_add[1], gw_add[2], gw_add[3]);
			}
			else if (item == OSK_EDIT_SUBNET)
			{
                memset(sbnttxt, 0x0U, sizeof (sbnttxt));
				sprintf((char*)sbnttxt, "%d.%d.%d.%d", subnet[0], subnet[1], subnet[2], subnet[3]);	
			}

			DISP_DeleteKeypad();
			DISP_CreateSettingsScreen();
			DISP_KeypadReset();
			return;
		}
		else if(ISVALIDDEC(key))
		{		
			if      (osk_ipedit == 1)
			{
				string1[osk_bcnt++] = key;
				if(osk_bcnt > 3) 
				{
					--osk_bcnt;
					string1[osk_bcnt] = NUL;
				}
				strval = atoi(string1);
			}
			else if (osk_ipedit == 2)
			{
				string2[osk_bcnt++] = key;
				if(osk_bcnt > 3) 
				{
					--osk_bcnt;
					string2[osk_bcnt] = NUL;
				}
				strval = atoi(string2);
			}
			else if (osk_ipedit == 3)
			{
				string3[osk_bcnt++] = key;
				if(osk_bcnt > 3) 
				{
					--osk_bcnt;
					string3[osk_bcnt] = NUL;
				}
				strval = atoi(string3);
			}
			else if (osk_ipedit == 4)
			{
				string4[osk_bcnt++] = key;
				if(osk_bcnt > 3) 
				{						
					--osk_bcnt;
					string4[osk_bcnt] = NUL;
				}
				strval = atoi(string4);
			}
			GUI_GotoXY(150, 15);
			GUI_DispDecMin(strval);
		}
		else
		{
            ZEROFILL(string1, COUNTOF(string1));
			ZEROFILL(string2, COUNTOF(string2));
            ZEROFILL(string3, COUNTOF(string3));
            ZEROFILL(string4, COUNTOF(string4));
			k = 0;
			osk_bcnt = 0;
			while(IS09(&string[osk_bcnt])) string1[k++] = string[osk_bcnt++];
			k = 0;
			++osk_bcnt;
			while(IS09(&string[osk_bcnt])) string2[k++] = string[osk_bcnt++];
			k = 0;
			++osk_bcnt;
			while(IS09(&string[osk_bcnt])) string3[k++] = string[osk_bcnt++];
			k = 0;
			++osk_bcnt;
			while(IS09(&string[osk_bcnt])) string4[k++] = string[osk_bcnt++];
			osk_ipedit = 0;
			osk_bcnt = 0;			
			while(IS09(&string[osk_bcnt])) 
			{
				if(osk_bcnt <= key) 
				{
					osk_ipedit = 1;
					strval = atoi(string1);
				}	
				++osk_bcnt;
			}
			++osk_bcnt;
			while(IS09(&string[osk_bcnt]))
			{
				if(osk_bcnt <= key) 
				{
					osk_ipedit = 2;
					strval = atoi(string2);
				}	
				++osk_bcnt;
			}
			++osk_bcnt;
			while(IS09(&string[osk_bcnt]))
			{
				if(osk_bcnt <= key) 
				{
					osk_ipedit = 3;
					strval = atoi(string3);
				}	
				++osk_bcnt;
			}
			++osk_bcnt;
			while(IS09(&string[osk_bcnt]))
			{
				if(osk_bcnt <= key) 
				{
					osk_ipedit = 4;
					strval = atoi(string4);
				}	
				++osk_bcnt;
			}
			
			osk_bcnt = 0;
			if      (osk_ipedit == 1) while(string1[osk_bcnt] != NUL) ++osk_bcnt;
			else if (osk_ipedit == 2) while(string2[osk_bcnt] != NUL) ++osk_bcnt;
			else if (osk_ipedit == 3) while(string3[osk_bcnt] != NUL) ++osk_bcnt;
			else if (osk_ipedit == 4) while(string4[osk_bcnt] != NUL) ++osk_bcnt;				
			GUI_GotoXY(150, 15);
			GUI_DispDecMin(strval);
		}
	}
	else
	{
		switch(item)
		{
			case OSK_EDIT_RS485_INTERFACE:
			{
				GUI_SetColor(GUI_BLACK);
				GUI_FillRect(130, 0, 360, 50);
				GUI_SetFont(&GUI_Font24B_1);
				GUI_SetColor(GUI_WHITE);
				GUI_SetTextMode(GUI_TEXTMODE_TRANS);
				
				if      (key == 'B')
				{
					osk_entry[osk_bcnt] = NUL;				
					if(osk_bcnt > 0) --osk_bcnt;
					osk_entry[osk_bcnt] = NUL;
					strval = atoi(osk_entry);
					GUI_GotoXY(150, 15);
					GUI_DispDecMin(strval);
				}
				else if (key == 'O')
				{
					rsifa = atoi(osk_entry);
					DISP_DeleteKeypad();
					DISP_CreateSettingsScreen();
					DISP_KeypadReset();
					return;
				}
				else if (ISVALIDDEC(key))
				{				
					osk_entry[osk_bcnt++] = key;
					if(osk_bcnt > 5) --osk_bcnt;
					strval = atoi(osk_entry);
					GUI_GotoXY(150, 15);
					GUI_DispDecMin(strval);
				}
				else
				{
					strval = atoi(string);
					GUI_GotoXY(150, 15);
					GUI_DispDecMin(strval);
				}
				break;
			}
			
			
			case OSK_EDIT_RS485_BROADCAST:
			{
				GUI_SetColor(GUI_BLACK);
				GUI_FillRect(130, 0, 360, 50);
				GUI_SetFont(&GUI_Font24B_1);
				GUI_SetColor(GUI_WHITE);
				GUI_SetTextMode(GUI_TEXTMODE_TRANS);
				
				if      (key == 'B')
				{
					osk_entry[osk_bcnt] = NUL;				
					if(osk_bcnt > 0) --osk_bcnt;
					osk_entry[osk_bcnt] = NUL;
					strval = atoi(osk_entry);
					GUI_GotoXY(150, 15);
					GUI_DispDecMin(strval);
				}
				else if (key == 'O')
				{
					rsbra = atoi(osk_entry);
					DISP_DeleteKeypad();
					DISP_CreateSettingsScreen();
					DISP_KeypadReset();
					return;
				}
				else if (ISVALIDDEC(key))
				{				
					osk_entry[osk_bcnt++] = key;
					if(osk_bcnt > 5) --osk_bcnt;
					strval = atoi(osk_entry);
					GUI_GotoXY(150, 15);
					GUI_DispDecMin(strval);
				}
				else
				{
					strval = atoi(string);
					GUI_GotoXY(150, 15);
					GUI_DispDecMin(strval);
				}
				break;
			}
			
			
			case OSK_EDIT_PASSWORD:
			{
				GUI_SetColor(GUI_BLACK);
				GUI_FillRect(130, 0, 360, 50);
				GUI_SetFont(&GUI_Font24B_1);
				GUI_SetColor(GUI_WHITE);
				GUI_SetTextMode(GUI_TEXTMODE_TRANS);
				
				if      (key == 'B')
				{
					osk_entry[osk_bcnt] = NUL;				
					if(osk_bcnt > 0) --osk_bcnt;
					osk_entry[osk_bcnt] = NUL;
					strval = atoi(osk_entry);
					GUI_GotoXY(150, 15);
					GUI_DispDecMin(strval);
				}
				else if (key == 'O')
				{
                    ZEROFILL(password, COUNTOF(password));
					osk_entry[5] = NUL;
					pswrd_bcnt = strlen(osk_entry);
					memcpy(password, osk_entry, pswrd_bcnt);
					pswrd_bcnt = strlen(osk_entry);
					DISP_DeleteKeypad();
					DISP_CreateSettingsScreen();
					DISP_KeypadReset();
					return;
				}
				else if (ISVALIDDEC(key))
				{				
					osk_entry[osk_bcnt++] = key;
					if(osk_bcnt > 5) --osk_bcnt;
                    strval = atoi(osk_entry);
					GUI_GotoXY(150, 15);
					GUI_DispDecMin(strval);
				}
				else
				{
                    strval = atoi(string);
					GUI_GotoXY(150, 15);
					GUI_DispDecMin(strval);
				}
				break;
			}
			
			
			case OSK_ENTER_PASSWORD:
			{
				if((key == 'B') || (key == 'O'))		
				{
					GUI_SetColor(GUI_BLACK);
					GUI_FillRect(130, 0, 360, 50);
					GUI_SetFont(&GUI_Font24B_1);
					GUI_SetColor(GUI_WHITE);
					GUI_SetTextMode(GUI_TEXTMODE_TRANS);
					
					if      (key == 'B')
					{
						osk_entry[osk_bcnt] = NUL;
						if(osk_bcnt > 0x0U)
						{
							--osk_bcnt;
							ret_1 = 0x0U;
							while(ret_1 < osk_bcnt)
							{
								GUI_DispCharAt('*', ((ret_1 * 40U) + 180U), 15U);
								++ret_1;
							}
						}
						osk_entry[osk_bcnt] = NUL;	
						DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
					}
					else if (key == 'O')
					{
						osk_entry[5] = 0x0U;
						
						if (strncmp(osk_entry, password, pswrd_bcnt) == 0)
						{
							GUI_DispStringAt("PASSWORD OK", 150, 15);
							DISP_UnlockSet();
						}
						else
						{
							GUI_DispStringAt("WRONG PASSWORD", 150, 15);
							DISP_UnlockReset();
						}
						
						DISP_TimerStart(DISP_KEYPAD_DELAY);
					}
				}
				else if (ISVALIDDEC(key))
				{
					if(osk_bcnt == 0x0U)
					{
						GUI_SetColor(GUI_BLACK);
						GUI_FillRect(130, 0, 360, 50);
					}
					GUI_SetColor(GUI_WHITE);
					GUI_SetTextMode(GUI_TEXTMODE_TRANS);				
					GUI_SetFont(&GUI_Font24B_1);
					if(osk_bcnt < 0x5U) GUI_DispCharAt('*', ((osk_bcnt * 40U) + 180U), 15U);
					osk_entry[osk_bcnt++] = key;
					if(osk_bcnt > 0x5U) --osk_bcnt;
					DISP_TimerStart(DISP_UNLOCK_TIMEOUT);
				}
				if(IsDISP_UnlockActiv()) DISP_TimeoutTimerStart(DISP_UNLOCK_TIMEOUT);			
				GUI_SetTextMode(GUI_TEXTMODE_NORMAL);			
				DISP_RefreshSet();
				break;
			}
			
			
			case OSK_EDIT_SYS_ID:
			{
				GUI_SetColor(GUI_BLACK);
				GUI_FillRect(130, 0, 360, 50);
				GUI_SetFont(&GUI_Font24B_1);
				GUI_SetColor(GUI_WHITE);
				GUI_SetTextMode(GUI_TEXTMODE_TRANS);
				
				if      (key == 'B')
				{
					osk_entry[osk_bcnt] = 0x0U;				
					if(osk_bcnt > 0) --osk_bcnt;
					osk_entry[osk_bcnt] = 0x0U;
					strval = atoi(osk_entry);
					GUI_GotoXY(150, 15);
					GUI_DispDecMin(strval);
				}
				else if (key == 'O')
				{
					system_id = atoi(osk_entry);
					DISP_DeleteKeypad();
					DISP_CreateSettingsScreen();
					DISP_KeypadReset();
					return;
				}
				else if (ISVALIDDEC(key))
				{				
					osk_entry[osk_bcnt++] = key;
					if (osk_bcnt > 5) --osk_bcnt;
                    strval = atoi(osk_entry);
					GUI_GotoXY(150, 15);
					GUI_DispDecMin(strval);
				}
				else
				{
					strval = atoi(string);
					GUI_GotoXY(150, 15);
					GUI_DispDecMin(strval);
				}
				break;
			}
			
			
			default:
			{
				break;
			}
		}		
	}
}


void DISP_UpdateDateTime(void)
{
    RTC_t drtc;
    char disp[16];
	static uint8_t old_min = 60U;
    
    RTC_GetDateTime(&drtc);
	if(ActiveScreen != DEFAULT) return;
	else if((old_min != drtc.minutes) || IsDISP_UpdateTimeActiv())
	{
		DISP_UpdateTimeReset();
		old_min = drtc.minutes;
		GUI_ClearRect(5, 65, 115, 200);
		GUI_SetColor((0x62U << 24U) | GUI_BLACK);
		GUI_FillRect(5, 65, 115, 200);
		GUI_SetTextMode(GUI_TEXTMODE_NORMAL);
        Hex2Str(disp, &drtc.hours, 0x2U);
		disp[2] = ':';
        Hex2Str(&disp[3], &drtc.minutes, 0x2U);
		GUI_GotoXY(60, 100);
		GUI_SetColor(GUI_RED);
		GUI_SetFont(&GUI_Font32B_1);
		GUI_SetTextAlign(GUI_TA_HCENTER|GUI_TA_VCENTER);
		GUI_DispString(disp);
        if      (drtc.weekday == 0x0U)  drtc.weekday = 0x7U;
		if      (drtc.weekday == 0x1U) memcpy(disp, "Ponedjeljak\0", 12);
		else if (drtc.weekday == 0x2U) memcpy(disp, "  Utorak   \0", 12);
		else if (drtc.weekday == 0x3U) memcpy(disp, "  Srijeda  \0", 12);
		else if (drtc.weekday == 0x4U) memcpy(disp, "  Cetvrtak \0", 12);
		else if (drtc.weekday == 0x5U) memcpy(disp, "   Petak   \0", 12);
		else if (drtc.weekday == 0x6U) memcpy(disp, "  Subota   \0", 12);
		else if (drtc.weekday == 0x7U) memcpy(disp, "  Nedelja  \0", 12);
        
		GUI_GotoXY(60, 130);		
		GUI_SetColor(GUI_WHITE);
		GUI_SetFont(&GUI_Font24_1);
		GUI_SetTextAlign(GUI_TA_HCENTER|GUI_TA_VCENTER);
		GUI_DispString(disp);
		
        Hex2Str(disp, &drtc.date, 0x2U);
		disp[2] = '.';
		if      (drtc.month == 0x01U) memcpy(&disp[3], "Januar\0",   7);		
		else if (drtc.month == 0x02U) memcpy(&disp[3], "Februar\0",  8);
		else if (drtc.month == 0x03U) memcpy(&disp[3], "Mart\0",     5);
		else if (drtc.month == 0x04U) memcpy(&disp[3], "April\0",    6);
		else if (drtc.month == 0x05U) memcpy(&disp[3], "Maj\0",      4);
		else if (drtc.month == 0x06U) memcpy(&disp[3], "Juni\0",     5);
		else if (drtc.month == 0x07U) memcpy(&disp[3], "Juli\0",     5);
		else if (drtc.month == 0x08U) memcpy(&disp[3], "August\0",   7);
		else if (drtc.month == 0x09U) memcpy(&disp[3], "Septembar\0",10);
		else if (drtc.month == 0x10U) memcpy(&disp[3], "Oktobar\0",  8);
		else if (drtc.month == 0x11U) memcpy(&disp[3], "Novembar\0", 8);
		else if (drtc.month == 0x12U) memcpy(&disp[3], "Decembar\0", 9);
		
        uint32_t x = strlen(disp);
		if(x > 10U) GUI_SetFont(&GUI_Font16B_1);
		else GUI_SetFont(&GUI_Font20B_1);
		GUI_SetColor(GUI_RED);
		GUI_GotoXY(60, 160);
		GUI_SetTextAlign(GUI_TA_HCENTER|GUI_TA_VCENTER);
		GUI_DispString(disp);	
        
		memcpy(disp, "20  \0", 0x5U);
        Hex2Str(&disp[2], &drtc.year, 0x2U);
		GUI_SetFont(&GUI_Font20B_1);
		GUI_SetColor(GUI_WHITE);
		GUI_GotoXY(60, 190);
		GUI_SetTextAlign(GUI_TA_HCENTER|GUI_TA_VCENTER);
		GUI_DispString(disp);
		GUI_Exec();
	}
}


void DISP_FoundDeviceList(void)
{
	LISTVIEW_AddRow(hLISTVIEW_AddresseFound, 0);
//	Int2Str((char *)hc_buff, fwrupd_add_list[0], 0x0U);
//	LISTVIEW_SetItemText(hLISTVIEW_AddresseFound, 0, row, (char *)hc_buff);
	LISTVIEW_SetItemText(hLISTVIEW_AddresseFound, 1, row, "DE-200216_03");
    ZEROFILL(hc_buff, HC_BSIZE);
	ret_1 = 34U;
	ret_2 = 0x0U;
//	while(ret_1 < 44U) hc_buff[ret_2++] = rx_buff[ret_1++];
//	LISTVIEW_SetItemText(hLISTVIEW_AddresseFound, 2, row, (char *)hc_buff);
	++row;
	GUI_Exec();
}


void SAVE_Settings(void)
{
    uint8_t ebuf[32];
    //
    //  get system flags current state state 
    //
    if(CHECKBOX_GetState(hCHKBOX_EnableDHCP) == 1) DHCP_ClientEnable();
    else DHCP_ClientDisable();
    
    if(CHECKBOX_GetState(hCHKBOX_EnableBroadcastTime) == 1) TIME_BroadcastEnable();
    else TIME_BroadcastDisable();
    
    if(CHECKBOX_GetState(hCHKBOX_EnableHTTP) == 1) HTTP_ServerEnable();
    else HTTP_ServerDisable();

    if(CHECKBOX_GetState(hCHKBOX_EnableWebConfig) == 1) WEB_ConfigEnable();
    else WEB_ConfigDisable();

    if(CHECKBOX_GetState(hCHKBOX_EnableTFTP) == 1) TFTP_ServerEnable();
    else TFTP_ServerDisable();
    //
    //  copy all parameter to single buffer for single write call
    //
    ZEROFILL(ebuf, COUNTOF(ebuf));
    memcpy(&ebuf[EE_ETH_IP_ADD],  ip_add, 0x4U);
    memcpy(&ebuf[EE_ETH_SUB_ADD], subnet, 0x4U);
    memcpy(&ebuf[EE_ETH_GW_ADD],  gw_add, 0x4U);
    ebuf[EE_RS485_IFADD]        = ((rsifa >> 8)  & 0xFFU);
    ebuf[EE_RS485_IFADD + 0x1U] = (rsifa         & 0xFFU);
    ebuf[EE_RS485_GRADD]        = ((rsgra >> 8)  & 0xFFU);
    ebuf[EE_RS485_GRADD + 0x1U] = (rsgra         & 0xFFU);
    ebuf[EE_RS485_BRADD]        = ((rsbra >> 8)  & 0xFFU);
    ebuf[EE_RS485_BRADD + 0x1U] = (rsbra         & 0xFFU);
    ebuf[EE_RS485_BAUD_ADD]     = (rsbps + '0'); // convert to char digit 0-9
    ebuf[EE_SYS_CFG_ADD]        = ((sys_cfg >> 24)  & 0xFFU);
    ebuf[EE_SYS_CFG_ADD + 0x1U] = ((sys_cfg >> 16)  & 0xFFU);
    ebuf[EE_SYS_CFG_ADD + 0x2U] = ((sys_cfg >>  8)  & 0xFFU);
    ebuf[EE_SYS_CFG_ADD + 0x3U] = (sys_cfg          & 0xFFU);
    ebuf[EE_SYS_ID_ADD]         = ((system_id>> 8)  & 0xFFU);
    ebuf[EE_SYS_ID_ADD + 0x1U]  = (system_id        & 0xFFU);
    memcpy(&ebuf[EE_PASSWORD_ADD], password, 0x6U);
    //
    //  write all parameter to eeprom
    //
    if (I2CEE_WriteBytes16(I2CEE_PAGE_0, EE_ETH_IP_ADD, ebuf, sizeof(ebuf)) != 0U) ErrorHandler(DISP_FUNC, I2C_DRV);
    DelayMs(I2CEE_WRITE_DELAY);
    //
    //  reload all written parameter from eeprom
    //
    RAM_Init();
    //
    //  apply all changes to system varible
    //
#ifndef USE_CONSTANT_IP
    IP4_ADDR(&new_ip, ip_add[0], ip_add[1], ip_add[2], ip_add[3]);
    IP4_ADDR(&new_nm, subnet[0], subnet[1], subnet[2], subnet[3]);
    IP4_ADDR(&new_gw, gw_add[0], gw_add[1], gw_add[2], gw_add[3]);				
    netif_set_addr(&netif, &new_ip , &new_nm, &new_gw);
#endif    
}


/*************************** End of file ****************************/
