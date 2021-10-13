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

#include "osaxs_p.h"
// Nkshx.h defines these, so get rid of them before including DwCeDump.h
#ifdef ProcessorLevel
#undef ProcessorLevel
#endif
#ifdef ProcessorRevision
#undef ProcessorRevision
#endif
#include "DwCeDump.h"
#include "diagnose.h"



PTHREAD GetThreadPointer(HANDLE hThread)
{
    THREAD *pThread = NULL;
    
    if (h2pHDATA)
    {
        PHDATA phdata = h2pHDATA(hThread, g_pprcNK->phndtbl);

        if (phdata)
        {
           pThread =(THREAD *) phdata->pvObj;
        }
    }
    return pThread;
}

HRESULT GenerateStarvationDDxReport(StarvationReport *pStarvationReport)
{
   HRESULT hr = S_OK;
   UINT    index = g_nDiagnoses;

   BeginDDxLogging();

   g_nDiagnoses++;
   
   g_ceDmpDiagnoses[index].Type       = Type_Starvation;
   g_ceDmpDiagnoses[index].SubType    = 0;
   g_ceDmpDiagnoses[index].Scope      = Scope_Unknown;
   g_ceDmpDiagnoses[index].Depth      = Depth_Symptom;
   g_ceDmpDiagnoses[index].Severity   = Severity_Severe;
   g_ceDmpDiagnoses[index].Confidence = Confidence_Possible;
   g_ceDmpDiagnoses[index].ProcessId  = 0;
   g_ceDmpDiagnoses[index].pThread   = 0;


   ddxlog(L"\r\n");
   ddxlog(L"STARVATION DETECTED\r\n");
   ddxlog(L"\r\n");
   ddxlog(L"Threads of realtime priority %i have not been scheduled for a long time.\r\n", pStarvationReport->starvedCEPriority);
   ddxlog(L"There is a chance that a higher priority thread is taking all the CPU cycles and not allowing lower priority threads to run\r\n");
   ddxlog(L"-------------------\r\n");
   ddxlog(L"These are the threads consuming most of the CPU cycles (potentially causing the CPU starvation):\r\n");

   THREAD *pThread = NULL;
   THREAD *pStarverThread = NULL;
   
   for (int i=0; i < (sizeof(pStarvationReport->highCPUThreads)/sizeof(pStarvationReport->highCPUThreads[0])); i++)
   {
       if (pStarvationReport->highCPUThreads[i].threadId != 0)
       {
          pThread = GetThreadPointer((HANDLE)pStarvationReport->highCPUThreads[i].threadId);
          if (pThread != NULL)
          {
             ddxlog(L"%i.\tThread 0x%08X is taking %i%% of CPU cycles\r\n", i+1, pThread, pStarvationReport->highCPUThreads[i].cpuPercent);
             ddxlog(L"   \tStart Address 0x%08X\r\n", pThread->dwStartAddr);
             if (pThread->pprcOwner != NULL)
             {
                ddxlog(L"   \tProcess %s [pointer 0x%08X]\r\n", pThread->pprcOwner->lpszProcName, pThread->pprcOwner);
                AddAffectedProcess(pThread->pprcOwner);
             }
             ddxlog(L"   \tUser Time %i\r\n", pThread->dwUTime);
             ddxlog(L"   \tKernel Time %i\r\n", pThread->dwKTime);
             ddxlog(L"   \tBecame runnable at tick %i\r\n", pThread->dwTimeWhenRunnable);
             ddxlog(L"   \tLast time blocked at tick %i\r\n", pThread->dwTimeWhenBlocked);
             ddxlog(L"\r\n", pThread->dwTimeWhenBlocked);
             AddAffectedThread(pThread);

             if (pStarverThread == NULL)
             {
                pStarverThread = pThread;

                PPROCESS pprc = pThread->pprcOwner;

                g_ceDmpDiagnoses[index].pProcess  = (ULONG32) pprc;
                g_ceDmpDiagnoses[index].ProcessId = (ULONG32)(pprc ? pprc->dwId : NULL);
                g_ceDmpDiagnoses[index].pThread  = (ULONG32) pThread;
             }
          }
       }
   }
   ddxlog(L"\r\n");
   ddxlog(L"\r\n");
   ddxlog(L" \r\n");

   g_ceDmpDiagnoses[index].pDataTemp = NULL;
   g_ceDmpDiagnoses[index].Data.DataSize = 0;
   g_ceDmpDiagnoses[index].Description.DataSize = EndDDxLogging();

   if (pStarverThread != NULL)
   {
      hr = GetBucketParameters(&g_ceDmpDDxBucketParameters, pStarverThread, pStarverThread->pprcActv);   
   }
   else
   {
      hr = E_FAIL;
   }

   return hr;
}

DDxResult_e DiagnoseStarvation(void)
{
   DDxResult_e ddxResult = DDxResult_Error;

   //The DDX_Watchdog application raises the exception that takes us here
   //the watchdog does a kick delta check to capture which threads are consumping most of the CPU
   //at the moment the starvation condition is detected.
   //Once the watchdog has that information it raises an exception and gives us the delta information
   //as part of the exception custom data.
   //the first value on the custom data is a pointer to a StarvationReport structure.

   //1. Verify that the raised exception has the information that we need.
   if (DD_ExceptionState.exception != NULL)
   {
      //2. Verify that the parameter count matches to what we expect from the Watchdog
      if (DD_ExceptionState.exception->NumberParameters >= 1)
      {
         StarvationReport *pStarvationReport = (StarvationReport *)DD_ExceptionState.exception->ExceptionInformation[0];

         //3. Verify that the StarvationReport pointer passed by the watchdog is compatible (check size)
         if (pStarvationReport != NULL && pStarvationReport->dwSize == sizeof(StarvationReport))
         {
            //4. Generate diagnosis package.
            if (SUCCEEDED(GenerateStarvationDDxReport(pStarvationReport)))
            {
               ddxResult = DDxResult_Positive;
            }
         }
      }
   }
   
   return ddxResult;
}



