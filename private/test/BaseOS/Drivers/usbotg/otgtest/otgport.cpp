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
    OTGPort.cpp

Abstract:  
    USB Port Class Definitions

Author:
    shivss


Functions:

Notes: 
--*/// --------------------------------------------------------------------
#include<windows.h>
#include<otgtest.h>
#define __THIS_FILE__ TEXT("Otgport.cpp")


HANDLE OTGPort::CreateFile(LPCTSTR lpFileName,   DWORD dwDesiredAccess,   DWORD dwShareMode,   LPSECURITY_ATTRIBUTES lpSecurityAttributes, 
      DWORD dwCreationDispostion,   DWORD dwFlagsAndAttributes,   HANDLE hTemplateFile )
{
    m_hOTGPort = ::CreateFile( lpFileName,dwDesiredAccess,dwShareMode,lpSecurityAttributes,dwCreationDispostion, 
                 dwFlagsAndAttributes,  hTemplateFile);

    g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d:  Open Device by Name(%s) return handle =%x"),
                     __THIS_FILE__, __LINE__, lpFileName,m_hOTGPort );
   
    return m_hOTGPort;

}

//Function Overloaded for obtaining Handle to the OTG Bus 
HANDLE OTGPort::CreateFile(LPCTSTR lpFileName,BOOL fBusAccess)
{

  TCHAR szActiveRegPath[MAX_PATH];
   if(fBusAccess)
   {

       const OTGTest * pTestContext = &g_OtgTestContext;
       if(pTestContext)
       {
                //Find the Active Reg Key for OTG Bus 
          if(pTestContext->FindActiveDevice( szActiveRegPath, _countof(szActiveRegPath)))
          {
               m_hOTGBus = CreateBusAccessHandle(szActiveRegPath);
               if(m_hOTGBus != INVALID_HANDLE_VALUE)
               {
                   g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d:  Get Bus Access for Device(%s) return handle =%x"),
                                 __THIS_FILE__, __LINE__, lpFileName,m_hOTGBus );
               }
          }   
       }       
    }
    return m_hOTGBus;
}
    

VOID  OTGPort::OTGClose(void)
{
        if(m_hOTGPort)
        CloseHandle(m_hOTGPort);

    return ;

}

//IOCTL Calls that can be called using File Handle
 BOOL OTGPort::OTGIOControl( DWORD dwCode, LPVOID pBufIn,DWORD dwLenIn, LPVOID pBufOut, DWORD dwLenOut,PDWORD pdwActualOut)
 {
    OVERLAPPED Overlapped;
 
    if(m_hOTGPort)
        return ::DeviceIoControl(m_hOTGPort,dwCode,pBufIn,dwLenIn,pBufOut,dwLenOut,pdwActualOut,&Overlapped);       
    else
        return FALSE;

 }

//IOCTL Calls that can be called using Bus Handle
  BOOL OTGPort::OTGIOControl( DWORD dwCode, LPVOID pBufIn,     DWORD dwLenIn)
  {
       if(m_hOTGBus)
       {
           if(!BusChildIoControl(m_hOTGBus,dwCode,pBufIn,dwLenIn))
           {
               g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d:BusChildIoControl Failed Error = %d "),
               __THIS_FILE__, __LINE__,GetLastError());    
               return FALSE;      
           }
           else 
               return TRUE;

       }
       else
           return FALSE;
 }


   
VOID OTGTest::FreeDeviceList()
{
    OTGTest * pTestContext = &g_OtgTestContext;
    
    if(pTestContext) 
    {
        OTGDevContext *pCurDevContext,*pPrevDevContext;
        pCurDevContext = pTestContext->m_OtgContext;
   
        while(pCurDevContext)
        {
           pPrevDevContext = pCurDevContext;
           pCurDevContext=pCurDevContext->next;
           free(pPrevDevContext);
        }
    }
}

BOOL OTGTest::GetOTGPort(LPTSTR szDeviceName)
{
    if(!szDeviceName)
        return FALSE;

    size_t szSize = 0;

    szSize = _countof(m_szOTGPort);
    
    if( szSize > 0 )
    {
        if(StringCchCopy(szDeviceName,szSize,m_szOTGPort)!=S_OK)
            return FALSE;
        else
            return TRUE;
    }
    else
        return FALSE;
    

}

 BOOL OTGTest::AddOneDevice( LPCTSTR szDeviceName)
 {
    if(!szDeviceName )
        return FALSE;
 
    OTGDevContext * pDevContext = (OTGDevContext *)malloc(sizeof(OTGDevContext));
    if(!pDevContext)
    {
        g_pKato->Log(LOG_FAIL,_T("OTGTest::AddOneDevice: Out of Memory  \n"));
        return FALSE;
    }

    pDevContext->next = NULL;
    if(StringCchCopy(pDevContext->szDeviceName,_countof(pDevContext->szDeviceName),szDeviceName) != S_OK)
        return FALSE;

    if(!m_OtgContext)
        m_OtgContext = pDevContext;

    else
    {
    OTGDevContext * curContext  = m_OtgContext;

    while(curContext->next != NULL)
        curContext = curContext->next;

    curContext->next = pDevContext ;

    }

    return TRUE;            
}    

 BOOL OTGTest::InitializeOTGTest(void)
 {
 

   BOOL bRet = FALSE;
   HKEY hKey=NULL;
   HKEY hSubKey=NULL;
   DWORD dwIndex= 0;
   TCHAR szSubKey[MAX_PATH];
   TCHAR szDriverName[MAX_PATH];
   
   TCHAR szDeviceName[6];
   DWORD dwSubKeySize = 0 ;
   DWORD dwKeySize = MAX_PATH ;
   DWORD dwDeviceName = 0 ;
   DWORD  dwResult;
   
   
    //Open OTG Active Reg Key
   if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, DRIVERS_ACTIVE, 0, 0, &hKey) != ERROR_SUCCESS)
   {
        return bRet;
   }

   DWORD dwEnumIndex = 0 ;
   
   for(;;)
   {
       dwSubKeySize=MAX_PATH;
       dwResult = RegEnumKeyEx(hKey,dwEnumIndex,szSubKey,&dwSubKeySize,NULL,NULL,NULL,NULL);
    
       if(dwResult == ERROR_NO_MORE_ITEMS)
           break;
   
       if(dwResult !=ERROR_SUCCESS)
           break;
    
       if(RegOpenKeyEx(hKey,szSubKey,0,0,&hSubKey)== ERROR_SUCCESS)
       {
            dwKeySize = sizeof(szDriverName);
            if(RegQueryValueEx(hSubKey,_T("Key"),NULL,NULL,(PBYTE)szDriverName,&dwKeySize) == ERROR_SUCCESS)
            {
                 if(_tcscmp(OTG_REG_KEY,szDriverName) == 0 )
                 {
                     g_pKato->Log(LOG_DETAIL,_T("OTGTest::InitializeOTGTest USB OTG Port Present in the Device  \n"));
                     dwDeviceName = sizeof(szDeviceName);
                     if(RegQueryValueEx(hSubKey,_T("Name"),NULL,NULL,(PBYTE)szDeviceName,&dwDeviceName) == ERROR_SUCCESS)
                     {
                         if(szDeviceName)
                             StringCchCopyN((LPTSTR)m_szotgPrefix, 4, szDeviceName,3);     /*example: OTG*/        
                             
                         if((dwIndex = GetOTGportIndex(szDeviceName))!=-1)
                             m_dwOTGMask[dwIndex] = 1;     
                         
                         if(!AddOneDevice(szDeviceName))
                         {
                             NKDbgPrintfW(_T(" OTGTest::InitializeOTGTest AddOneDevice Failed "));
                         }
                         else
                         {
                             if(!m_bOTG)
                                 m_bOTG = TRUE;
                             m_dwOTGCount++;
                             bRet= TRUE;
                         }
                     }
                 }
             }
         }

         if(hSubKey)
             RegCloseKey(hSubKey);

         dwEnumIndex++;
     }

     //If Only one OTG Device Present 
     if(m_dwOTGCount == 1)
     {
         if(m_OtgContext)
         {
             m_dwOTGx = GetOTGportIndex(m_OtgContext->szDeviceName);
             if(StringCchCopy(m_szOTGPort,_countof(m_szOTGPort),m_OtgContext->szDeviceName)!=S_OK)
                 bRet = FALSE;
         }
     }

  if(hKey)
      RegCloseKey(hKey);

  if(hSubKey)
      RegCloseKey(hSubKey);
  

  return bRet;




 }

 BOOL OTGTest::InitializeClientDrivers(void)
 {

     HKEY hKey;
  
       if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,USBFN_REG_KEY,0,0,&hKey) ==ERROR_SUCCESS)
       {
          g_pKato->Log(LOG_DETAIL,_T("OTGTest::InitializeClientDrivers: USB Function RegKey is Present  \n"));
          m_bUSBfn = TRUE;

       }

     if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,USBHCD_REG_KEY,0,0,&hKey)==ERROR_SUCCESS)
       {
        g_pKato->Log(LOG_DETAIL,_T("OTGTest::InitializeClientDrivers: USB Host RegKey is Present  \n"));
        m_bHcd= TRUE;

       }

     return TRUE;

 }

  BOOL OTGTest:: FindActiveDevice(LPTSTR szActiveDevPath, DWORD dwPathSize)
  {

 BOOL bRet = FALSE;

  if(!szActiveDevPath)
      return FALSE;

   HKEY hKey;
   HKEY hSubKey;
   TCHAR szSubKey[MAX_PATH];
   TCHAR szDriverName[MAX_PATH];
   DWORD dwResult=MAX_PATH;
   DWORD dwSubKeySize = 0 ;
   DWORD dwKeySize = 0 ;
        

     //Open OTG Active Reg Key
   if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, DRIVERS_ACTIVE, 0, 0, &hKey) != ERROR_SUCCESS)
   {
        return bRet;
   }

   DWORD dwEnumIndex = 0 ;
   
   for(;;)
   {
       dwSubKeySize=MAX_PATH;
       dwResult = RegEnumKeyEx(hKey,dwEnumIndex,szSubKey,&dwSubKeySize,NULL,NULL,NULL,NULL);
    
       if(dwResult == ERROR_NO_MORE_ITEMS)
           break;

       if(dwResult !=ERROR_SUCCESS)
           break;
    
       if(RegOpenKeyEx(hKey,szSubKey,0,0,&hSubKey)== ERROR_SUCCESS)
       {
            dwKeySize = sizeof(szDriverName);    
            if(RegQueryValueEx(hSubKey,_T("Key"),NULL,NULL,(PBYTE)szDriverName,&dwKeySize)== ERROR_SUCCESS)
            {
                 if(_tcscmp(USBFN_REG_KEY,szDriverName) == 0 )
                 {
                     g_pKato->Log(LOG_DETAIL,_T("OTGTest:: FindActiveDevice USB Function is Active  \n"));
                     swprintf_s(szActiveDevPath, dwPathSize, _T("%s\\%s"),DRIVERS_ACTIVE,szSubKey);
                     bRet= TRUE;
                     break;
                 }
                 else if(_tcscmp(USBHCD_REG_KEY,szDriverName) == 0 )
                 {   
                     g_pKato->Log(LOG_DETAIL,_T("OTGTest:: FindActiveDevice USB Host is Active  \n"));
                     swprintf_s(szActiveDevPath, dwPathSize, _T("%s\\%s"),DRIVERS_ACTIVE,szSubKey);
                     bRet = TRUE;
                     break;
                 }
             }
         }
         if(hSubKey)
             RegCloseKey(hSubKey);
         dwEnumIndex++;
    }

    if(hKey)
        RegCloseKey(hKey);

    return bRet; 
}

BOOL OTGTest::IsOTGDeviceActive(LPCTSTR szDeviceName)
{
    DEVMGR_DEVICE_INFORMATION di;
    memset(&di,0,sizeof(di));
    di.dwSize = sizeof(di);
    HANDLE hFirstDevice = FindFirstDevice(DeviceSearchByDeviceName,_T("OTG*"),&di);

    if(hFirstDevice != INVALID_HANDLE_VALUE)
    {
        do 
        {
            if(_tcscmp(di.szLegacyName, szDeviceName) == 0 )
                return TRUE ;

            memset(&di,0,sizeof(di));                
            di.dwSize = sizeof(di);
        }while(FindNextDevice(hFirstDevice,&di));
    }
    return FALSE;
}
