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
    USB OTG Test cases

Author:
    shivss


Functions:

Notes: 
--*/// --------------------------------------------------------------------


#include <WINDOWS.H>
#include <otgtest.h>


#define __THIS_FILE__ TEXT("OtgTest.cpp")


TESTPROCAPI OTG_OpenCloseTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /* lpFTE */)
{
   DWORD dwResult = TPR_PASS;
   DWORD dwOTGPort = 0 ;
   DWORD dwRet;
   DWORD dwInstance = 0 ;
   TCHAR szOTGPort[6];

       // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // start a thread for each comm port
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryOTGPortCount();
        dwRet = SPR_HANDLED;
        goto Exit;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        dwRet = TPR_NOT_HANDLED;
        goto Exit;
    }

   OTGTest * pTestContext = &g_OtgTestContext;

    if(!pTestContext->IsOTGPresent())
       return TPR_SKIP;
    
     //Is there a OTG port Selected 
     if(!pTestContext->OTGPortSelect())
     {
          dwOTGPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
     
        if(!pTestContext->m_dwOTGMask[dwOTGPort])
                return TPR_PASS;
        else
            swprintf_s(szOTGPort, _countof(szOTGPort), SZ_OTG_PORT_FMT, dwOTGPort);
     }
    else //Get the OTG Port Name
    {
        if(!pTestContext->GetOTGPort(szOTGPort)||!szOTGPort)
             g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  GetOTGPort "),__THIS_FILE__, __LINE__ );
    }
        

    //Open Close OTG Port 5 Times
    while(dwInstance <5)
    {
         
         //Create a new Instance of the OTG Port
         OTGPort * pOtgPort = new OTGPort;


          if(! pOtgPort->CreateFile(szOTGPort,  GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL ))
          {
               g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  Open Device (%s) "),
                                  __THIS_FILE__, __LINE__, szOTGPort  );
             dwResult = TPR_FAIL;
             goto Exit;
              }

        //Close Port   
        pOtgPort ->OTGClose();
        
           if(pOtgPort)
               delete(pOtgPort);

        dwInstance++;
    
        }

        //Open 5 Instances in Shared Mode 
        OTGPort * pOtgPort1 = new OTGPort;

    

        if(! pOtgPort1->CreateFile(szOTGPort,  GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL ))
        {
               g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  Open Device (%s)"),
                                  __THIS_FILE__, __LINE__, szOTGPort );
             dwResult = TPR_FAIL;
             goto Exit;
       }

     OTGPort * pOtgPort2 = new OTGPort;

         if(! pOtgPort2->CreateFile(szOTGPort,  GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, 0, NULL ))
        {
               g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  Open Device (%s) "),
                                  __THIS_FILE__, __LINE__, szOTGPort  );
             dwResult = TPR_FAIL;
             goto Exit;
       }
 
        OTGPort * pOtgPort3 = new OTGPort;

        
         if(! pOtgPort3->CreateFile(szOTGPort,  GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, 0, NULL ))
        {
               g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  Open Device (%s) "),
                                  __THIS_FILE__, __LINE__, szOTGPort  );
             dwResult = TPR_FAIL;
             goto Exit;
       }
        
      OTGPort * pOtgPort4 = new OTGPort;

   
         if(!pOtgPort4->CreateFile(szOTGPort,  GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, 0, NULL ))
        {
               g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  Open Device (%s) "),
                                  __THIS_FILE__, __LINE__, szOTGPort );
             dwResult = TPR_FAIL;
             goto Exit;
       }

    OTGPort * pOtgPort5 = new OTGPort;
       
        if(!pOtgPort5->CreateFile(szOTGPort,  GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, 0, NULL ))
        {
               g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  Open Device (%s) "),
                                  __THIS_FILE__, __LINE__, szOTGPort  );
             dwResult = TPR_FAIL;
             goto Exit;
       }

       //Close Handles 
         pOtgPort1 ->OTGClose();
      pOtgPort2 ->OTGClose();
      pOtgPort3 ->OTGClose();
      pOtgPort4 ->OTGClose();
      pOtgPort5 ->OTGClose();

    //Free Instances of OTG Port 
    if(pOtgPort1)
             delete(pOtgPort1);
    if(pOtgPort2)
             delete(pOtgPort2);
    if(pOtgPort3)
             delete(pOtgPort3);
    if(pOtgPort4)
             delete(pOtgPort4);
    if(pOtgPort5)
             delete(pOtgPort5);
    
  
Exit:

   return dwResult  ;
}




TESTPROCAPI OTG_LoadUnloadTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /* lpFTE */)
{

    
   DWORD dwResult = TPR_PASS;
   DEVMGR_DEVICE_INFORMATION di = {0};
   DWORD dwOTGPort = (DWORD) -1;
   TCHAR szOTGPort[6];
   DWORD dwRet = 0;
   DWORD dwCount = 0 ;
   HANDLE hParent = INVALID_HANDLE_VALUE;
   HANDLE hOtgPort = INVALID_HANDLE_VALUE;

    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // start a thread for each comm port
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryOTGPortCount();
        dwRet = SPR_HANDLED;
        goto Exit;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        dwRet = TPR_NOT_HANDLED;
        goto Exit;
    }

   OTGTest * pTestContext = &g_OtgTestContext;

    if(!pTestContext->IsOTGPresent())
       return TPR_SKIP;
    

    if(!pTestContext->OTGPortSelect())
    {
        dwOTGPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
    
        if(!pTestContext->m_dwOTGMask[dwOTGPort])
            return TPR_PASS;
        else
            swprintf_s(szOTGPort,_countof(szOTGPort), SZ_OTG_PORT_FMT, dwOTGPort);
    }
    else
    {
        if(!pTestContext->GetOTGPort(szOTGPort)||!szOTGPort)
             g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  GetOTGPort "),__THIS_FILE__, __LINE__ );
    }
    
    OTGPort * pOtgPort = new OTGPort;
    
    hOtgPort = pOtgPort->CreateFile(szOTGPort,  GENERIC_READ | GENERIC_WRITE, 0, NULL,
                            OPEN_EXISTING, 0, NULL );
    
    if(hOtgPort == INVALID_HANDLE_VALUE)
    {
        g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  Open Device (%s) "),
                        __THIS_FILE__, __LINE__, szOTGPort  );
        dwResult = TPR_FAIL;
        goto Exit;
    }
    
    memset(&di, 0, sizeof(di));
    di.dwSize = sizeof(di);
    
    if(!GetOTGportInfo(hOtgPort,&di ))
    {
        g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to find Device manager information for OTG Port   "),
                                  __THIS_FILE__, __LINE__);
        dwResult = TPR_FAIL;
    }
    
    if(di.hParentDevice == NULL)
    {
        g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d:OTG driver does not have a valid parent device handle!!!"),
                                             __THIS_FILE__, __LINE__);
        return TPR_FAIL;
    }

    //Get the Parent Driver Information 
    DEVMGR_DEVICE_INFORMATION diPar;
    memset(&diPar, 0, sizeof(diPar));
    diPar.dwSize = sizeof(diPar);
    if(GetDeviceInformationByDeviceHandle(di.hParentDevice, &diPar) == FALSE)
    {
       g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d:etDeviceInformationByDeviceHandle call failed! This device won't be able to be unloaded!!"),
                                             __THIS_FILE__, __LINE__);
         return TPR_FAIL;
    }

   
    hParent = CreateFile(diPar.szBusName, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
    if(hParent == INVALID_HANDLE_VALUE)
    {
        g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d:Open bus enum device %s failed!"),
                             __THIS_FILE__, __LINE__,di.szBusName);
        return TPR_FAIL;
    }

    while(dwCount <5)
    {
        //now try to deactivate OTG  controller driver
        BOOL fRet = DeviceIoControl(hParent, 
                                                         IOCTL_BUS_DEACTIVATE_CHILD, 
                                                         LPVOID(di.szBusName), 
                                                         (wcslen(di.szBusName)+1)*sizeof(TCHAR), 
                                                         NULL, 0, NULL, NULL);

        if(fRet != TRUE)
        {
             g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: IOCTL_BUS_DEACTIVATE_CHILD call failed! Error = %d "),
                                      __THIS_FILE__, __LINE__,GetLastError());
            CloseHandle(hParent);
            return TPR_FAIL;
        }


        Sleep(5000);

        if(pTestContext->IsOTGDeviceActive(szOTGPort) )
        {
            g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: OTG Device Still Active after Unloading  "),
                                  __THIS_FILE__, __LINE__);
        }
              
        
        //now try to reactivate OTG controller driver
        fRet = DeviceIoControl(hParent, 
                                 IOCTL_BUS_ACTIVATE_CHILD, 
                                 LPVOID(di.szBusName), 
                                 (wcslen(di.szBusName)+1)*sizeof(TCHAR), 
                                 NULL, 0, NULL, NULL);

        if(fRet != TRUE)
        {
           g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: IOCTL_BUS_ACTIVATE_CHILD call failed!  "),
                                  __THIS_FILE__, __LINE__);
           CloseHandle(hParent);
           return TPR_FAIL;
        }
        
        //Verify If the Device has been activated
        if(!pTestContext->IsOTGDeviceActive(szOTGPort) )
        {
           g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: OTG Device Not  Active after Reloading  "),
                                  __THIS_FILE__, __LINE__);
        }
        dwCount++;
   }   
Exit:

   if(hParent != INVALID_HANDLE_VALUE)
       CloseHandle(hParent);

   if(hOtgPort != INVALID_HANDLE_VALUE)
       CloseHandle(hOtgPort);

   return dwResult;
}

TESTPROCAPI OTG_RequestDropTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /* lpFTE */)
{
    DWORD dwResult = TPR_PASS;
    DWORD dwOTGPort = (DWORD) -1;
    TCHAR szOTGPort[6];
    DWORD dwRet = 0;
    HANDLE hOtgPort = INVALID_HANDLE_VALUE;

   // process incoming tux messages (other than TPM_EXECUTE)
   //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // start a thread for each comm port
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryOTGPortCount();
        dwRet = SPR_HANDLED;
        goto Exit;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        dwRet = TPR_NOT_HANDLED;
        goto Exit;
    }

    OTGTest * pTestContext = &g_OtgTestContext;

    if(!pTestContext->IsOTGPresent())
       return TPR_SKIP;
    
    if(!pTestContext->OTGPortSelect())
    {
        dwOTGPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
     
        if(!pTestContext->m_dwOTGMask[dwOTGPort])
            return TPR_PASS;
        else
            swprintf_s(szOTGPort, _countof(szOTGPort), SZ_OTG_PORT_FMT, dwOTGPort);
    }
    else
    {
        if(!pTestContext->GetOTGPort(szOTGPort)||!szOTGPort)
             g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  GetOTGPort "),__THIS_FILE__, __LINE__ );
    }

    OTGPort * pOtgPort = new OTGPort;

    hOtgPort = pOtgPort->CreateFile(szOTGPort,  GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL );

    if(hOtgPort == INVALID_HANDLE_VALUE)
    {
           g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  Open Device (%s) "),
                              __THIS_FILE__, __LINE__, szOTGPort  );
         dwResult = TPR_FAIL;
         goto Exit;
    }

    DWORD dwBufIn=0 ;
    DWORD dwBufOut= 0 ;    
    DWORD dwActualOut=0;
    DWORD dwCount = 0; 

    while( dwCount <2)
    {
       //Request Bus
        g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Calling IOCTL_BUS_USBOTG_REQUEST_BUS   "),__THIS_FILE__, __LINE__);
  
        if(!pOtgPort->OTGIOControl(IOCTL_BUS_USBOTG_REQUEST_BUS ,(PBYTE) &dwBufIn, sizeof(dwBufIn),(PBYTE)&dwBufOut,sizeof(dwBufOut),&dwActualOut))
        {
                        g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: IOCTL_BUS_USBOTG_REQUEST_BUS  call failed! Error = %d "),
                                    __THIS_FILE__, __LINE__,GetLastError());
                dwResult = TPR_FAIL;
  
        }
  
       Sleep(5000);
  
      //Drop Bus
        g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Calling IOCTL_BUS_USBOTG_DROP_BUS    "),__THIS_FILE__, __LINE__);
        if(!pOtgPort->OTGIOControl(IOCTL_BUS_USBOTG_DROP_BUS , (PBYTE)&dwBufIn, sizeof(dwBufIn),(PBYTE)&dwBufOut,sizeof(dwBufOut),&dwActualOut))
        {
              g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: IOCTL_BUS_USBOTG_DROP_BUS  call failed! Error = %d "),
                      __THIS_FILE__, __LINE__,GetLastError());
              dwResult = TPR_FAIL;
  
        }
        Sleep(5000);
  
       //Request Bus
        g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Calling IOCTL_BUS_USBOTG_REQUEST_BUS  "),__THIS_FILE__, __LINE__);
        if(!pOtgPort->OTGIOControl(IOCTL_BUS_USBOTG_REQUEST_BUS ,(PBYTE) &dwBufIn, sizeof(dwBufIn),(PBYTE)&dwBufOut,sizeof(dwBufOut),&dwActualOut))
        {
            g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: IOCTL_BUS_USBOTG_REQUEST_BUS  call failed! Error = %d "),
                      __THIS_FILE__, __LINE__,GetLastError());
            dwResult = TPR_FAIL;
        }
        Sleep(5000);
        dwBufIn=1;
        dwCount++;
    }     

Exit:
    if(hOtgPort != INVALID_HANDLE_VALUE)
       CloseHandle(hOtgPort);

    return dwResult;
}
TESTPROCAPI OTG_SuspedResumeTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /* lpFTE */)
{    
   DWORD dwResult = TPR_PASS;
   DWORD dwOTGPort = (DWORD) -1;
   TCHAR szOTGPort[6];
   DWORD dwBusRequest = TRUE;
   DWORD dwCount = 0 ;
   DWORD dwRet = 0;
   HANDLE hOtgPort = INVALID_HANDLE_VALUE;

    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // start a thread for each comm port
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryOTGPortCount();
        dwRet = SPR_HANDLED;
        goto Exit;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        dwRet = TPR_NOT_HANDLED;
        goto Exit;
    }
 
    OTGTest * pTestContext = &g_OtgTestContext;
    
    if(!pTestContext->IsOTGPresent())
       return TPR_SKIP;
    

    if(!pTestContext->OTGPortSelect())
    {
        dwOTGPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
    
        if(!pTestContext->m_dwOTGMask[dwOTGPort])
            return TPR_PASS;
        else
            swprintf_s(szOTGPort, _countof(szOTGPort), SZ_OTG_PORT_FMT, dwOTGPort);
    }
    else
    {
        if(!pTestContext->GetOTGPort(szOTGPort)||!szOTGPort)
             g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  GetOTGPort "),__THIS_FILE__, __LINE__ );
    }

    OTGPort * pOtgPort = new OTGPort;

    hOtgPort = pOtgPort->CreateFile(szOTGPort, dwBusRequest);
    if(hOtgPort == INVALID_HANDLE_VALUE)
    {
        g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  Open Device (%s) "),
                          __THIS_FILE__, __LINE__, szOTGPort  );
        dwResult = TPR_FAIL;
        goto Exit;
    }

    DWORD dwBufIn=0 ;     

    while(dwCount < 2)
    {
        //Suspend Bus
        g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Calling IOCTL_BUS_USBOTG_SUSPEND    "),__THIS_FILE__, __LINE__);
        if(!pOtgPort->OTGIOControl(IOCTL_BUS_USBOTG_SUSPEND,(PBYTE)&dwBufIn,sizeof(dwBufIn )))
        {
            g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: IOCTL_BUS_USBOTG_SUSPEND  call failed! Error = %d "),
                     __THIS_FILE__, __LINE__,GetLastError());
            dwResult = TPR_FAIL;
        
        }        
        Sleep(5000);
        
        //Resume Bus
        g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Calling IOCTL_BUS_USBOTG_RESUME  "),__THIS_FILE__, __LINE__);
        if(!pOtgPort->OTGIOControl(IOCTL_BUS_USBOTG_RESUME ,(PBYTE)&dwBufIn,sizeof(dwBufIn )))
        {
            g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: IOCTL_BUS_USBOTG_RESUME  call failed! Error = %d "),
                      __THIS_FILE__, __LINE__,GetLastError());
            dwResult = TPR_FAIL;
        
        }
        Sleep(5000);
        dwCount++;
    }

Exit:
    if(hOtgPort != INVALID_HANDLE_VALUE)
       CloseHandle(hOtgPort);
     
    return dwResult;
}

TESTPROCAPI OTG_PowerManagementTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /* lpFTE */)
{    
    DWORD dwResult = TPR_PASS;
    DWORD dwOTGPort= 0 ;
    CEDEVICE_POWER_STATE dx,checkDx,origDx;
    TCHAR szOTGPort[6];
    DWORD dwRet = 0;
    POWER_CAPABILITIES powerCap = {0};
    DWORD dwBufIn= 0 ;    
    DWORD dwActualOut=0;
    HANDLE hOtgPort = INVALID_HANDLE_VALUE;
    
    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // start a thread for each comm port
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = QueryOTGPortCount();
        dwRet = SPR_HANDLED;
        goto Exit;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        dwRet = TPR_NOT_HANDLED;
        goto Exit;
    }

    OTGTest * pTestContext = &g_OtgTestContext;

    if(!pTestContext->IsOTGPresent())
       return TPR_SKIP;
    

    if(!pTestContext->OTGPortSelect())
    {
        dwOTGPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
    
        if(!pTestContext->m_dwOTGMask[dwOTGPort])
            return TPR_PASS;
        else
            swprintf_s(szOTGPort, _countof(szOTGPort), SZ_OTG_PORT_FMT, dwOTGPort);
    }
    else
    {
        if(!pTestContext->GetOTGPort(szOTGPort)||!szOTGPort)
             g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  GetOTGPort "),__THIS_FILE__, __LINE__ );
    }
    
    OTGPort * pOtgPort = new OTGPort;
    
    hOtgPort = pOtgPort->CreateFile(szOTGPort,  GENERIC_READ | GENERIC_WRITE, 0, NULL,
                            OPEN_EXISTING, 0, NULL );

    if(hOtgPort == INVALID_HANDLE_VALUE)
    {
        g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: Failed to  Open Device (%s) "),
                      __THIS_FILE__, __LINE__, szOTGPort  );
        dwResult = TPR_FAIL;
        goto Exit;
    }
    

    //Get Power Capabilities 
    g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Calling IOCTL_POWER_CAPABILITIES    "),__THIS_FILE__, __LINE__);
    if(!pOtgPort->OTGIOControl(IOCTL_POWER_CAPABILITIES ,(PBYTE)&dwBufIn,sizeof(dwBufIn), (PBYTE)&powerCap, sizeof(powerCap),&dwActualOut))
    {
           g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: IOCTL_POWER_CAPABILITIES  call failed! Error = %d "),
                              __THIS_FILE__, __LINE__,GetLastError());
          dwResult = TPR_FAIL;
          goto Exit;
    
    }
    
    //Log the capabilities
    LogPowerCapabilities(&powerCap);
    
    //OTG MDD Doesnot Support IOCTL_GET_POWER as of Now 
    
    
    // Query the starting device power state so we can put it back to that
    // state when the test is complete
    g_pKato->Log(LOG_DETAIL,_T("Sending IOCTL_POWER_GET to device %s.."), szOTGPort);
    if(!pOtgPort->OTGIOControl(IOCTL_POWER_GET, NULL, 0,(PBYTE) &origDx, sizeof(origDx), &dwActualOut))
    {
        // the device did not support IOCTL_POWER_GET
        g_pKato->Log(LOG_FAIL,_T("Fail in %s at line %d : device %s failed IOCTL_POWER_GET query ")
                           ,_T(__FILE__),__LINE__,szOTGPort);    
        
        dwResult = TPR_FAIL;
    }

   //Set the Supported Power State 
    for(dx = D0; dx < PwrDeviceMaximum;dx = (CEDEVICE_POWER_STATE)((DWORD)dx + 1))
    {
        // is newDx a supported power state?
        if(SupportedDx(dx, powerCap.DeviceDx))
        {
            //Put the OTG Driver in the new power state
            g_pKato->Log(LOG_DETAIL,_T("sending IOCTL_POWER_SET with power state D%u to device %s"), dx, szOTGPort);
                            
            if(!pOtgPort->OTGIOControl(IOCTL_POWER_SET, NULL, 0, (PBYTE)&dx, sizeof(dx), &dwActualOut))
            {
                // The device did not support IOCTL_POWER_SET
                g_pKato->Log(LOG_FAIL,_T("Fail in %s at line %d : device %s failed IOCTL_POWER_SET with power state D%u "),
                               _T(__FILE__),__LINE__,szOTGPort,dx);    
                dwResult = TPR_FAIL;
                continue;
            } 

            //Query the power state-- should return the power state that was just set
            g_pKato->Log(LOG_DETAIL,_T("sending IOCTL_POWER_GET to device %s"), szOTGPort);
            if(!pOtgPort->OTGIOControl(IOCTL_POWER_GET, NULL, 0, (PBYTE)&checkDx, sizeof(checkDx), &dwActualOut))
            {
               // the device did not support IOCTL_POWER_GET
                g_pKato->Log(LOG_FAIL,_T("Fail in %s at line %d : %s failed IOCTL_POWER_GET query "),
                                               _T(__FILE__),__LINE__,szOTGPort);    
                dwResult = TPR_FAIL;
                continue;
            }

            if(!ValidDx(checkDx))
            {
                g_pKato->Log(LOG_FAIL,_T("Fail in %s at line %d : %s is in an unsupported power state: D%u "),
                                       _T(__FILE__),__LINE__,szOTGPort,checkDx);    

                dwResult = TPR_FAIL;
                    continue;
            }                    

            g_pKato->Log(LOG_DETAIL,_T("device %s reports a current power state of D%u"), szOTGPort, dx);
            
            // the IOCTL_POWER_SET call should have returned the power state set in the
            // input/output variable-- make sure it is the same as IOCTL_POWER_GET returns                        
            if(dx != checkDx)
            {
                g_pKato->Log(LOG_FAIL,_T("Fail in %s at line %d : %s IOCTL_POWER_SET returned D%u; expected D%u "),
                                   _T(__FILE__),__LINE__,szOTGPort,checkDx,dx);    
                dwResult = TPR_FAIL;
            }
        }
    }

   // put the driver back in the original power state
    g_pKato->Log(LOG_DETAIL,_T("sending IOCTL_POWER_SET with original power state D%u to device %s.."), origDx, szOTGPort);
    if(!pOtgPort->OTGIOControl(IOCTL_POWER_SET, NULL, 0,(PBYTE) &origDx, sizeof(origDx), &dwActualOut))
       {
        // the device did not support IOCTL_POWER_SET
        g_pKato->Log(LOG_FAIL,_T("Fail in %s at line %d : %s failed IOCTL_POWER_SET with power state D%u "),
                       _T(__FILE__),__LINE__,szOTGPort,origDx);    
        dwResult = TPR_FAIL;
    }
 Exit:

    if(hOtgPort != INVALID_HANDLE_VALUE)
       CloseHandle(hOtgPort);
    return dwResult;
}
