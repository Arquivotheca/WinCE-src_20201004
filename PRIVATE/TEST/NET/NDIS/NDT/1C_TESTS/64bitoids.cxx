//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "ShellProc.h"
#include "NDTNdis.h"
#include "NDTMsgs.h"
#include "NDTError.h"
#include "NDTLog.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "ndt_1c.h"

//------------------------------------------------------------------------------

TEST_FUNCTION(Test64BitOids)
{
   TEST_ENTRY;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;
   HANDLE hAdapter = NULL;

   UINT uiPhysicalMedium = 0;
   UINT uiLinkSpeed = 0;
   BOOL bShould = FALSE;
   BOOL bError = FALSE;
   UINT nOids = 2;
   NDIS_OID aOid[2] = {0, 0};
   UINT ixOid = 0;
   UINT auiValue[2] = {0, 0};
   UINT cbUsed = 0;
   UINT cbRequired = 0;
   LPTSTR szInfo = NULL;
   
   // Let start
   NDTLogMsg(_T("Start 1c_64BitOID test on the adapter %s"), g_szTestAdapter);

   // Open adapter
   NDTLogMsg(_T("Open adapter"));
   hr = NDTOpen(g_szTestAdapter, &hAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
      goto cleanUp;
   }

   // Bind
   NDTLogMsg(_T("Bind to adapter"));
   hr = NDTBind(hAdapter, bForce30, ndisMedium);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailBind, hr);
      goto cleanUp;
   }

   hr = NDTGetPhysicalMedium(hAdapter, &uiPhysicalMedium);
   if (FAILED(hr)) goto cleanUp;

   hr = NDTGetLinkSpeed(hAdapter, &uiLinkSpeed);
   if (FAILED(hr)) goto cleanUp;

   if (uiLinkSpeed >= 10000000) {
      bShould = TRUE;
      NDTLogMsg(_T("Link speed is equal or higher than 1.0Gbps"));
      NDTLogMsg(_T("64-bit statistics are required"));
   } else {
      bShould = FALSE;
      NDTLogMsg(_T("Link speed is lower than 1.0Gbps"));
      NDTLogMsg(_T("64-bit statistics are optional"));
   }

   aOid[0] = OID_GEN_XMIT_OK;
   aOid[1] = OID_GEN_RCV_OK;

   NDTLogMsg(_T("Querying 64bit OIDs"));
   for (ixOid = 0; ixOid < nOids; ixOid++) {

      NDTLogMsg(_T("Query with a null buffer"));
      
      auiValue[0] = 0xFFFF;
      auiValue[1] = 0xFFFF;

      hr = NDTQueryInfo(
         hAdapter, aOid[ixOid], &auiValue[0], 0, &cbUsed, &cbRequired
      );
      if (FAILED(hr)) {
         if (
            hr != NDT_STATUS_BUFFER_TOO_SHORT && hr != NDT_STATUS_INVALID_LENGTH
         ) {
            NDTLogErr(g_szFailQueryInfo, hr);
            bError = TRUE;
            goto cleanUp;
         }
      }
      
      if (cbUsed != 0) {
         NDTLogWrn(
            _T("The miniport should have set BytesWritten to 0 not to %d"),
            cbUsed
         );
      }
      if (cbRequired != 8) {
         szInfo = _T("The miniport should have set BytesNeeded to 8 not to %d");
         if (bShould) {
            bError = TRUE;
            NDTLogErr(szInfo, cbRequired);
         } else {
            NDTLogWrn(szInfo, cbRequired);
         }
      }
            
      NDTLogMsg(_T("Query with a four byte buffer"));

      auiValue[0] = 0xFFFF;
      auiValue[1] = 0xFFFF;

      hr = NDTQueryInfo(
         hAdapter, aOid[ixOid], &auiValue[0], sizeof(UINT), &cbUsed, &cbRequired
      );
      if (FAILED(hr)) {
         NDTLogErr(g_szFailQueryInfo, hr);
         bError = TRUE;
         goto cleanUp;
      }
      
      if (cbUsed != 4) {
         bError = TRUE;
         NDTLogErr(
            _T("The miniport should have set BytesWritten to 4 not to %d"),
            cbUsed
         );
      }
      if (cbRequired != 8) {
         szInfo = _T("The miniport should have set BytesNeeded to 8 not to %d");
         if (bShould) {
            bError = TRUE;
            NDTLogErr(szInfo, cbRequired);
         } else {
            NDTLogWrn(szInfo, cbRequired);
         }
      }
      if (auiValue[0] == 0xFFFF) {
         bError = TRUE;
         NDTLogErr(
            _T("The buffer was not modifed when it should have been.")
         );
      }

      NDTLogMsg(_T("Query with an eight byte buffer"));

      auiValue[0] = 0xFFFF;
      auiValue[1] = 0xFFFF;

      hr = NDTQueryInfo(
         hAdapter, aOid[ixOid], auiValue, 2*sizeof(UINT), &cbUsed, &cbRequired
      );
      if (FAILED(hr)) {
         bError = TRUE;
         goto cleanUp;
      }
      if (cbUsed != 8) {
         szInfo = _T("The miniport should have set BytesWritten to 8 not to %d");
         if (bShould) {
            bError = TRUE;
            NDTLogErr(szInfo, cbUsed);
         } else {
            NDTLogWrn(szInfo, cbUsed);
         }
      }
      if (cbRequired != 8) {
         szInfo = _T("The miniport should have set BytesNeeded to 8 not to %d");
         if (bShould) {
            bError = TRUE;
            NDTLogErr(szInfo, cbRequired);
         } else {
            NDTLogWrn(szInfo, cbRequired);
         }
      }
      if (auiValue[0] == 0xFFFF && auiValue[1] == 0xFFFF) {
         szInfo = _T("The buffer was not modifed when it should have been");
         if (bShould) {
            bError = TRUE;
            NDTLogErr(szInfo);
         } else {
            NDTLogWrn(szInfo);
         }
      }
         
   }

   // Unbind
   NDTLogMsg(_T("Unbind adapter"));
   hr = NDTUnbind(hAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailUnbind, hr);
      bError = TRUE;
      goto cleanUp;
   }
   
cleanUp:
   // We have deside about test pass/fail there
   rc = bError ? TPR_FAIL : TPR_PASS;

   NDTLogMsg(_T("Close adapter"));
   hr = NDTClose(&hAdapter);
   if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);

   return rc;
}

//------------------------------------------------------------------------------
