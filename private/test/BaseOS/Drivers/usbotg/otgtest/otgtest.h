//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name: 
    Tuxmain.cpp

Abstract:  
    USB OTG  tux test header file 

Author:
    shivss


Functions:

Notes: 
--*/// --------------------------------------------------------------------
#include <ceddk.h>
#include <ceotgbus.h>
#include <tux.h>
#include <kato.h>


#define OTG_REG_KEY  TEXT("Drivers\\BuiltIn\\UsbOtg")
#define USBFN_REG_KEY  TEXT("Drivers\\BuiltIn\\UsbOtg\\UsbFn")
#define USBHCD_REG_KEY  TEXT("Drivers\\BuiltIn\\UsbOtg\\Hcd")
#define DRIVERS_ACTIVE TEXT("Drivers\\Active")

#define OTG_LEGACY_NAME _T("OTG")
__declspec(selectany) TCHAR  SZ_OTG_PORT_FMT[] =  _T("OTG%u:");
__declspec(selectany) TCHAR SZ_OTG_SRCH_FMT[] = _T("%s*");


#define MAX_DEVICE_INDEX 10 
#define MAX_DEVICE_NAMELEN 10

//Feilds to Identify the Active USB Device
#define ACTIVE_USBDEV_FN            0x01
#define ACTIVE_USBDEV_HCD          0x02

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
extern CKato *g_pKato ;


////////////////////////////////////////////////////////////////////////////////
// Suggested log verbosities

#define LOG_EXCEPTION                 0
#define LOG_FAIL                      2
#define LOG_ABORT                     4
#define LOG_SKIP                      6
#define LOG_NOT_IMPLEMENTED           8
#define LOG_PASS                      10
#define LOG_DETAIL                    12
#define LOG_COMMENT                   14




//Structure for Holding OTG Device  Information 
typedef struct _OTGDevContext
{

  TCHAR szDeviceName[6];
  TCHAR dwPortIndex ;
  struct _OTGDevContext* next;

}OTGDevContext, *pOTGDevContext;


//Structure For Storing Test Context
typedef struct _OTGTestContext
{
    DWORD dwOTGMask[10];    //OTGx Determines the Valid OTG Ports 
    TCHAR  szotgPrefix[4];          //Global OTG Prefix 
    BOOL    bOTG;              //Is OTG Present
    DWORD dwOTGx ;        //OTG Port Index 
    OTGDevContext * OtgContext;    //List of Active Devices
}OTGTestContext , * pOTGTestContext;
    
//Class To Store OTG Test Params
class OTGTest 
{
public :
      OTGTest()
      {
         memset(m_dwOTGMask,0,sizeof(m_dwOTGMask));
            m_bOTG = FALSE;   
             m_dwOTGx = (DWORD) -1;    
             m_dwOTGCount = 0;            
           };
        ~OTGTest()
       {
        FreeDeviceList();
        };      
          BOOL GetOTGParams(pOTGTestContext pContext);
       BOOL InitializeOTGTest(void);
       BOOL InitializeClientDrivers(void);
       static BOOL FindActiveDevice( LPTSTR szActiveDevPath, DWORD dwPathSize);
       BOOL IsOTGDeviceActive( LPCTSTR szDeviceName);
       DWORD GetOTGCount()const{return m_dwOTGCount;};
       BOOL GetOTGPort(LPTSTR szDeviceName);
       BOOL OTGPortSelect()const { return (-1 != m_dwOTGx);};
       BOOL IsOTGPresent()const  {return m_bOTG;};
       static VOID FreeDeviceList();
       BOOL AddOneDevice(LPCTSTR szDeviceName);
       
public:
    DWORD  m_dwOTGMask[10];       //OTGx Determines the Valid OTG Ports 
    TCHAR   m_szotgPrefix[4];          //Global OTG Prefix 
     BOOL    m_bOTG ;                       //Is OTG Present
     BOOL    m_bUSBfn;                    //Is there a Reg Key for USBfn 
     BOOL    m_bHcd ;                       //Is there a Reg Key for Hcd
     DWORD m_dwOTGx ;                 //OTG Port Index 
     DWORD m_dwOTGCount ;
     TCHAR  m_szOTGPort[6];
     OTGDevContext * m_OtgContext;    //List of Active Devices
    


};

class OTGPort
{

public:
    OTGPort()
    {
        m_hOTGPort= NULL; 
        m_hOTGBus = NULL;
    }
    ~OTGPort()
    {
         if(m_hOTGPort)
          CloseHandle(m_hOTGPort);

    }
    
    //Function Supported 
      HANDLE  CreateFile(LPCTSTR lpFileName,   DWORD dwDesiredAccess,   DWORD dwShareMode,   LPSECURITY_ATTRIBUTES lpSecurityAttributes, 
      DWORD dwCreationDispostion,   DWORD dwFlagsAndAttributes,   HANDLE hTemplateFile);
    HANDLE CreateFile(LPCTSTR lpFileName,BOOL fBusAccess);
      VOID  OTGClose(void);      
      BOOL OTGIOControl( DWORD dwCode, LPVOID pBufIn,     DWORD dwLenIn, LPVOID  pBufOut, DWORD dwLenOut,PDWORD pdwActualOut);
      BOOL OTGIOControl( DWORD dwCode, LPVOID pBufIn,     DWORD dwLenIn);
           
 

 protected:
     TCHAR  m_szOTGPort[MAX_DEVICE_NAMELEN];
    HANDLE m_hOTGPort;
    HANDLE m_hOTGBus;
    BOOL     m_bHost;
    BOOL     m_bFunction;
         

};


extern OTGTest  g_OtgTestContext;

//Test Case API 
TESTPROCAPI OTG_OpenCloseTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI OTG_LoadUnloadTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI OTG_RequestDropTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI OTG_SuspedResumeTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI OTG_PowerManagementTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

inline BOOL ValidDx(CEDEVICE_POWER_STATE dx)
{
    return (dx >= D0 && dx <= D4);
}


inline BOOL SupportedDx(CEDEVICE_POWER_STATE dx, UCHAR deviceDx)
{
    return (DX_MASK(dx) & deviceDx);
}



//Utility Functions 
BOOL InitOTGTest(void);
DWORD QueryOTGPortCount(void);
BOOL  GetOTGportInfo(HANDLE hFile ,DEVMGR_DEVICE_INFORMATION* pddi  );
void LogPowerCapabilities(const POWER_CAPABILITIES *ppwrCaps);
DWORD  GetOTGportIndex( const TCHAR* lpszDeviceName );
