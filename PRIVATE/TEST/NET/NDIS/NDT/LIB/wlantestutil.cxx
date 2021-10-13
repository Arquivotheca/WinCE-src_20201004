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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "StdAfx.h"
#include "NDTNdis.h"
#include "NDTMsgs.h"
#include "NDTError.h"
#include "NDTLog.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "ndtlibwlan.h"

// Caller should pass in a pointer with atleast 33 bytes allocated
void GetSsidText(TCHAR szSsidText[],NDIS_802_11_SSID ssid) 
{
      char dummy[33];
      if (!szSsidText)
         return;

      ULONG ulLen = (ssid.SsidLength <= 32 && ssid.SsidLength >= 0) ? ssid.SsidLength : 0;
      // Log ssid information
      memset (szSsidText, 0, sizeof(szSsidText));
      memcpy(dummy, ssid.Ssid, ulLen);
      dummy[ulLen] = '\0';
#ifdef UNICODE
      mbstowcs(szSsidText, dummy, sizeof(dummy));
#else
      strcpy(szSsidText,dummy);
#endif
}

// Caller should pass in a pointer with atleast 18 bytes allocated
void GetBssidText(TCHAR szBssidText[], BYTE bssid[]) 
{
      char dummy[18];
      static char szDigitToChar[] = { '0', '1', '2', '3', '4',
									'5', '6', '7', '8', '9',
									'a', 'b', 'c', 'd', 'e', 'f' };
      
      // Log bssid information      
      if (!szBssidText)
         return;
      
      memset (szBssidText, 0, sizeof(szBssidText));
      for (UINT ix=0; ix<6; ix++)
      {
         dummy[ix * 3] = szDigitToChar[(bssid[ix] & 0xF0) /16];
         dummy[ix *3 + 1] = szDigitToChar[(bssid[ix]& 0x0F)];
         if ((ix+1) <6)
            dummy[ix*3+2] = '-';
         else
            dummy[ix*3+2] = '\0';
      }

#ifdef UNICODE
      mbstowcs(szBssidText, dummy, sizeof(dummy));
      szBssidText[17]=L'\0';
#else
      strcpy(szBssidText,dummy);
#endif
}

// Caller should pass in a pointer with 80 bytes allocated 
void GetBssidExText(TCHAR szBssidText[], NDIS_WLAN_BSSID_EX nBssidEx) 
{
      char dummy[80];
      char* ptr = dummy;
      static char szDigitToChar[] = { '0', '1', '2','3','4',
									'5', '6', '7', '8', '9',
									'a', 'b', 'c', 'd', 'e', 'f' };
      

      if (!szBssidText)
         return;

      memset (szBssidText, 0, sizeof(szBssidText));

      strcpy(ptr,"Bssid = ");
      ptr += strlen(ptr);

      for (UINT ix=0; ix<6; ix++)
      {
         ptr[ix * 3] = szDigitToChar[(nBssidEx.MacAddress[ix] & 0xF0) /16];
         ptr[ix *3 + 1] = szDigitToChar[(nBssidEx.MacAddress[ix]& 0x0F)];
         if ((ix+1) <6)
            ptr[ix*3+2] = '-';
         else
            ptr[ix*3+2] = ' ';
      }
      ptr+=18;

      if (nBssidEx.Ssid.SsidLength <= 32 && nBssidEx.Ssid.SsidLength >= 0)
      {
         strcpy(ptr,"Ssid = ");
         ptr+= 7;
         memcpy(ptr, nBssidEx.Ssid.Ssid, nBssidEx.Ssid.SsidLength);
         ptr+= nBssidEx.Ssid.SsidLength;
      }

      *ptr = '\0 ';    
#ifdef UNICODE
      mbstowcs(szBssidText, dummy, ptr-dummy+1);
      szBssidText[ptr-dummy]=L'\0';
#else
      strcpy(szBssidText,dummy);
#endif
}


void GetNetworkTypeText(TCHAR szText[], DWORD WlanNetworkType)
{   
   switch(WlanNetworkType)
   {
     case Ndis802_11DS: 
      _tcscpy(szText,_T("Ndis802_11DS"));
      break;
     case Ndis802_11FH: 
      _tcscpy(szText,_T("Ndis802_11FH"));
      break;
     case Ndis802_11OFDM5: 
      _tcscpy(szText,_T("Ndis802_11OFDM5"));
      break;
     case Ndis802_11OFDM24: 
      _tcscpy(szText,_T("Ndis802_11OFDM24"));
      break;

     default:
      _tcscpy(szText,_T("Invalid Type"));
      break;      
   }   
}

void GetPowerModeText(TCHAR szText[], DWORD dwPowerMode)
{   
   switch(dwPowerMode)
   {
     case Ndis802_11PowerModeCAM: 
      _tcscpy(szText,_T("Ndis802_11PowerModeCAM"));
      break;
     case Ndis802_11PowerModeMAX_PSP: 
      _tcscpy(szText,_T("Ndis802_11PowerModeMAX_PSP"));
      break;
     case Ndis802_11PowerModeFast_PSP: 
      _tcscpy(szText,_T("Ndis802_11PowerModeFast_PSP"));
      break;
     case Ndis802_11PowerModeMax: 
      _tcscpy(szText,_T("Ndis802_11PowerModeMax"));
      break;

     default:
      _tcscpy(szText,_T("Invalid Type"));
      break;      
   }   
}

void GetAuthModeText(TCHAR szText[], DWORD dwAuthMode)
{  
   switch(dwAuthMode)
   {
     case Ndis802_11AuthModeOpen: 
      _tcscpy(szText,_T("Ndis802_11AuthModeOpen"));
      break;
     case Ndis802_11AuthModeShared: 
      _tcscpy(szText,_T("Ndis802_11AuthModeShared"));
      break;
     case Ndis802_11AuthModeAutoSwitch: 
      _tcscpy(szText,_T("Ndis802_11AuthModeAutoSwitch"));
      break;
     case Ndis802_11AuthModeWPA: 
      _tcscpy(szText,_T("Ndis802_11AuthModeWPA"));
      break;
     case Ndis802_11AuthModeWPAPSK: 
      _tcscpy(szText,_T("Ndis802_11AuthModeWPAPSK"));
      break;
     case Ndis802_11AuthModeWPANone: 
      _tcscpy(szText,_T("Ndis802_11AuthModeWPANone"));
      break;
     case Ndis802_11AuthModeWPA2: 
      _tcscpy(szText,_T("Ndis802_11AuthModeWPA2"));
      break; 
     case Ndis802_11AuthModeWPA2PSK: 
      _tcscpy(szText,_T("Ndis802_11AuthModeWPA2PSK"));
      break;      
     default:
      _tcscpy(szText,_T("Invalid Type"));
      break;      
   }   
}

void GetEncryptionText(TCHAR szText[], DWORD dwEncryption)
{  
   switch(dwEncryption)
   {
     case Ndis802_11WEPEnabled: 
      _tcscpy(szText,_T("Ndis802_11WEPEnabled/Ndis802_11Encryption1Enabled"));
      break;
     case Ndis802_11WEPDisabled: 
      _tcscpy(szText,_T("Ndis802_11WEPDisabled/Ndis802_11EncryptionDisabled"));
      break;
     case Ndis802_11WEPKeyAbsent: 
      _tcscpy(szText,_T("Ndis802_11WEPKeyAbsent/Ndis802_11Encryption1KeyAbsent"));
      break;
     case Ndis802_11WEPNotSupported: 
      _tcscpy(szText,_T("Ndis802_11WEPNotSupported/Ndis802_11EncryptionNotSupported"));
      break;
     case Ndis802_11Encryption2Enabled: 
      _tcscpy(szText,_T("Ndis802_11Encryption2Enabled"));
      break;
     case Ndis802_11Encryption2KeyAbsent: 
      _tcscpy(szText,_T("Ndis802_11Encryption2KeyAbsent"));
      break;
     case Ndis802_11Encryption3Enabled: 
      _tcscpy(szText,_T("Ndis802_11Encryption3Enabled"));
      break;        
     case Ndis802_11Encryption3KeyAbsent: 
      _tcscpy(szText,_T("Ndis802_11Encryption3KeyAbsent"));
      break;       
     default:
      _tcscpy(szText,_T("Invalid Type"));
      break;      
   }   
}

void GetInfrastructureModeText(TCHAR szText[], DWORD dwInfrastructureMode)
{      
   switch(dwInfrastructureMode)
   {
     case Ndis802_11IBSS: 
      _tcscpy(szText,_T("Ndis802_11IBSS"));
      break;
     case Ndis802_11Infrastructure: 
      _tcscpy(szText,_T("Ndis802_11Infrastructure"));
      break;
     case Ndis802_11AutoUnknown: 
      _tcscpy(szText,_T("Ndis802_11AutoUnknown"));
      break;
     case Ndis802_11InfrastructureMax: 
      _tcscpy(szText,_T("Ndis802_11InfrastructureMax"));
      break;
     default:
      _tcscpy(szText,_T("Invalid Type"));
      break;      
   }   
}