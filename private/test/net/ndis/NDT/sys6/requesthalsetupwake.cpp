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
#include "StdAfx.h"
#include "NDT.h"
#include "Protocol.h"
#include "Binding.h"
#include "RequestHalSetupWake.h"
#include "Marshal.h"
#include "Log.h"
#include "NdtLib.h"

//------------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////
// One helper function
///////////////////////////////////////////////////////////////////////////////
BOOL AddSecondsToFileTime(LPFILETIME lpFileTime, DWORD dwSeconds)
{
    __int64    iNum, iSecs;

    if ( !lpFileTime ) {
        return( FALSE );
    }

    iSecs = ( __int64 )dwSeconds * ( __int64 )10000000;
    iNum = ( ( ( __int64 )lpFileTime->dwHighDateTime ) << 32 ) +
        ( __int64 )lpFileTime->dwLowDateTime;

    iNum += iSecs;

    lpFileTime->dwHighDateTime = ( DWORD )( iNum >> 32 );
    lpFileTime->dwLowDateTime = ( DWORD )( iNum & 0xffffffff );

    return( TRUE );
}

//------------------------------------------------------------------------------

CRequestHalSetupWake::CRequestHalSetupWake() : 
   CRequest(NDT_REQUEST_HAL_EN_WAKE)
{
   m_dwMagic = NDT_MAGIC_REQUEST_HAL_SETUP_WAKE;
   m_uiWakeSrc = 0;
   m_uiFlags = 0;
   m_uiTimeOut = 0;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestHalSetupWake::Execute()
{
    NDIS_STATUS status = NDIS_STATUS_NOT_SUPPORTED;

    for (int i = 0; i < 1; i++)
    {
        SYSTEMTIME curSysTime = {0};
        FILETIME fileTime;
        HANDLE hAlarm = NULL;
        DWORD dwIOControl=0;
        BOOL fDoWakeUp=FALSE;
        BOOL fDoRTCTimer=FALSE;

        if (m_uiFlags & FLAG_NDT_ENABLE_WAKE_UP)
        {
            fDoWakeUp=TRUE;
            dwIOControl = IOCTL_HAL_ENABLE_WAKE;
        }
        else if (m_uiFlags & FLAG_NDT_DISABLE_WAKE_UP)
        {
            fDoWakeUp=TRUE;
            dwIOControl = IOCTL_HAL_DISABLE_WAKE;
        }

        if (m_uiFlags & FLAG_NDT_ENABLE_RTC_TIMER)
        {
            fDoRTCTimer=TRUE;
            hAlarm = CreateEvent(NULL, TRUE, FALSE, NULL);
            if(NULL == hAlarm)
            {
                DWORD dw = GetLastError();
                RETAILMSG(1, (_T("Failed to create the event Eror =%d"), dw));
                break;
            }
        }
        else if (m_uiFlags & FLAG_NDT_DISABLE_RTC_TIMER)
        {
            hAlarm=NULL;
            fDoRTCTimer=TRUE;
        }

        if (fDoWakeUp)
        {
            if(KernelIoControl(dwIOControl,            //dwIoControlCode
                &m_uiWakeSrc,                            //lpInBuf
                sizeof(m_uiWakeSrc),                    //nInBufSize
                NULL,                                    //lpOutBuf
                0,                                    //nOutBufSize
                NULL) == FALSE)                        //lpBytesReturned
            {
                RETAILMSG(1, (_T("The system does not support enabling waking up from suspend (IOCTL_HAL_ENABLE_WAKE)")));
                break;
            }
        }

        //2nd Setup RTC Timer.
        if (fDoRTCTimer)
        {
            GetLocalTime(&curSysTime);
            SystemTimeToFileTime(&curSysTime, &fileTime);
            AddSecondsToFileTime(&fileTime, m_uiTimeOut);
            FileTimeToSystemTime(&fileTime, &curSysTime);

            SetKernelAlarm(hAlarm, &curSysTime);
        }

        status = NDIS_STATUS_SUCCESS;
    }

   Complete();
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestHalSetupWake::UnmarshalInpParams(
   PVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   
   status = UnmarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_HAL_SETUP_WAKE, &m_uiWakeSrc, &m_uiFlags, &m_uiTimeOut);

   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
cleanUp:
   return status;
}

//------------------------------------------------------------------------------

