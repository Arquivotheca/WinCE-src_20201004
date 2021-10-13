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
#include "resultlog.h"
#include "loglib.h"
#include "PerfLoggerApi.h"
#include <strsafe.h>

extern int g_nTestBufferCount;
extern int* g_aTestBufferSizes;
extern double* g_aTestPassRate;
extern BOOL g_fConsumeCpu;


#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#define KILO_BYTES 1000
#define MEGA_BYTES 1000000
#define MILLISEC_TO_SEC 0.001
#define TEXT_MAX_LENGTH 250
#define TEMP_MAX_LENGTH 40

inline TCHAR* safe_strncat(
    TCHAR* szOutStr,
    TCHAR* szInStr,
    int    iOutStrMaxLen)
{
    // Make sure that the string is null-terminated
    szOutStr[iOutStrMaxLen - 1] = _T('\0');

    if (0 == _tcsncat_s(szOutStr, iOutStrMaxLen, szInStr, iOutStrMaxLen - 1 - _tcslen(szOutStr)))
        return szOutStr;
    else
        return NULL;
}

ResultLog::ResultLog(
    TCHAR *szTestName,
    DWORD dwTestOpt
    ) {

    _tcsncpy_s(m_szTestName, szTestName, MIN(MAX_TEST_NAME_LEN, _tcslen(szTestName) + 1));
    m_dwTestOpt = dwTestOpt;
}

void ResultLog::ShowHeaderSendRecv() {
    const int iMaxTextLen = 250;
    TCHAR text1[iMaxTextLen];
    TCHAR text2[iMaxTextLen];
    TCHAR text3[iMaxTextLen];

    text1[0] = text2[0] = text3[0] = _T('\0');

    safe_strncat(text1, _T("  "), iMaxTextLen);
    safe_strncat(text2, _T("  "), iMaxTextLen);
    safe_strncat(text3, _T("  "), iMaxTextLen);

    if (APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) & m_dwTestOpt) {
        safe_strncat(text1, _T("Send Packet  "), iMaxTextLen);
        safe_strncat(text2, _T("  (Bytes)    "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_RECV_PKT_SIZE) & m_dwTestOpt) {
        safe_strncat(text1, _T("Recv Packet  "), iMaxTextLen);
        safe_strncat(text2, _T("  (Bytes)    "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_BYTES_SENT) & m_dwTestOpt) {
        safe_strncat(text1, _T("Bytes Sent   "), iMaxTextLen);
        safe_strncat(text2, _T("  (Bytes)    "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_SEND_RATE) & m_dwTestOpt) {
        safe_strncat(text1, _T("Send Rate    "), iMaxTextLen);
        safe_strncat(text2, _T("  (KBps)     "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_SEND_PKT_LOSS) & m_dwTestOpt) {
        safe_strncat(text1, _T("Packets Recvd    "), iMaxTextLen);
        safe_strncat(text2, _T("( %% recv/sent)  "), iMaxTextLen);
        safe_strncat(text3, _T("---------------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_SEND_CPU_UTIL) & m_dwTestOpt) {
        safe_strncat(text1, _T("Send CPU Util  "), iMaxTextLen);
        safe_strncat(text2, _T("    (%%)       "), iMaxTextLen);
        safe_strncat(text3, _T("-------------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_BYTES_RECVD) & m_dwTestOpt) {
        safe_strncat(text1, _T("Bytes Recvd  "), iMaxTextLen);
        safe_strncat(text2, _T("  (Bytes)    "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_RECV_RATE) & m_dwTestOpt) {
        safe_strncat(text1, _T("Recv Rate    "), iMaxTextLen);
        safe_strncat(text2, _T("  (KBps)     "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_RECV_PKT_LOSS) & m_dwTestOpt) {
        safe_strncat(text1, _T("Packets Recvd    "), iMaxTextLen);
        safe_strncat(text2, _T("(%% recv/sent)   "), iMaxTextLen);
        safe_strncat(text3, _T("---------------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_RECV_CPU_UTIL) & m_dwTestOpt) {
        safe_strncat(text1, _T("Recv CPU Util  "), iMaxTextLen);
        safe_strncat(text2, _T("    (%%)       "), iMaxTextLen);
        safe_strncat(text3, _T("-------------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_LATENCY) & m_dwTestOpt) {
        safe_strncat(text1, _T("Latency      "), iMaxTextLen);
        safe_strncat(text2, _T("  (ms)       "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_JITTER) & m_dwTestOpt) {
        safe_strncat(text1, _T("Jitter       "), iMaxTextLen);
        safe_strncat(text2, _T("  (ms)       "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_CPU_UTIL) & m_dwTestOpt) {
        safe_strncat(text1, _T("CPU Util  "), iMaxTextLen);
        safe_strncat(text2, _T(" (%%)     "), iMaxTextLen);
        safe_strncat(text3, _T("--------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_PKT_LOSS) & m_dwTestOpt) {
        safe_strncat(text1, _T("Packets Recvd  "), iMaxTextLen);
        safe_strncat(text2, _T("    (%%)       "), iMaxTextLen);
        safe_strncat(text3, _T("------------   "), iMaxTextLen);
    }
    Log(REQUIRED_MSG, text1);
    Log(REQUIRED_MSG, text2);
    Log(REQUIRED_MSG, text3);
}
void ResultLog::ShowHeader() {
    const int iMaxTextLen = 160;
    TCHAR text1[iMaxTextLen];
    TCHAR text2[iMaxTextLen];
    TCHAR text3[iMaxTextLen];

    text1[0] = text2[0] = text3[0] = _T('\0');

    safe_strncat(text1, _T("  "), iMaxTextLen);
    safe_strncat(text2, _T("  "), iMaxTextLen);
    safe_strncat(text3, _T("  "), iMaxTextLen);

    if (APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) & m_dwTestOpt) {
        safe_strncat(text1, _T("Send Packet  "), iMaxTextLen);
        safe_strncat(text2, _T("(Bytes)      "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_RECV_PKT_SIZE) & m_dwTestOpt) {
        safe_strncat(text1, _T("Recv Packet  "), iMaxTextLen);
        safe_strncat(text2, _T("(Bytes)      "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_BYTES_SENT) & m_dwTestOpt) {
        safe_strncat(text1, _T("Bytes Sent   "), iMaxTextLen);
        safe_strncat(text2, _T("(Bytes)      "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_SEND_RATE) & m_dwTestOpt) {
        safe_strncat(text1, _T("Send Rate    "), iMaxTextLen);
        safe_strncat(text2, _T("(Kbps)       "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_BYTES_RECVD) & m_dwTestOpt) {
        safe_strncat(text1, _T("Bytes Recvd  "), iMaxTextLen);
        safe_strncat(text2, _T("(Bytes)      "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_RECV_RATE) & m_dwTestOpt) {
        safe_strncat(text1, _T("Recv Rate    "), iMaxTextLen);
        safe_strncat(text2, _T("(Kbps)       "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_LATENCY) & m_dwTestOpt) {
        safe_strncat(text1, _T("Latency      "), iMaxTextLen);
        safe_strncat(text2, _T("(ms)         "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_JITTER) & m_dwTestOpt) {
        safe_strncat(text1, _T("Jitter       "), iMaxTextLen);
        safe_strncat(text2, _T("(ms)         "), iMaxTextLen);
        safe_strncat(text3, _T("-----------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_CPU_UTIL) & m_dwTestOpt) {
        if(g_fConsumeCpu)
        {
            safe_strncat(text1, _T("System Load  "), iMaxTextLen);
        }
        else
        {
            safe_strncat(text1, _T("CPU Util  "), iMaxTextLen);
        }
        safe_strncat(text2, _T("(%%)       "), iMaxTextLen);
        safe_strncat(text3, _T("--------  "), iMaxTextLen);
    }
    if (APPLY_TO_CLIENT(SHOW_PKT_LOSS) & m_dwTestOpt) {
        safe_strncat(text1, _T("Packets Recvd"), iMaxTextLen);
        safe_strncat(text2, _T("   (%%) (#recv/#sent)"), iMaxTextLen);
        safe_strncat(text3, _T("------ -------------"), iMaxTextLen);
    }
    Log(REQUIRED_MSG, text1);
    Log(REQUIRED_MSG, text2);
    Log(REQUIRED_MSG, text3);
}

BOOL ResultLog::LogResult(
    DWORD dwSendPacketSize, // in bytes
    DWORD dwRecvPacketSize, // in bytes
    // Send:
    DWORD dwPacketsSent,    // number of packets sent
    DWORD dwSendTime,       // send time (in milliseconds)
    // Recv:
    DWORD dwPacketsRecvd,   // number of packets recvd
    DWORD dwRecvTime,       // recv time (in milliseconds)
    // Latency:
    DWORD dwLatencyUs,
    // Jitter:
    DWORD dwJitterUs,
    // CPU utilization
    DWORD dwCpuIdle,        // in milliseconds
    DWORD dwTotalTime,      // in milliseconds
    // Mark it
    BOOL fMarkIt,
    double dConsumedSysCpu
    ) {

    const int iTextMaxLen = 160;
    TCHAR text[iTextMaxLen];
    TCHAR temp[TEMP_MAX_LENGTH];
    TCHAR szPerfMark[MAX_MARKERNAME_LEN];
    const MARKER_ID iPerfMarkId = 1;
    BOOL logResult = TRUE;

    text[0] = TCHAR('\0');
    temp[0] = TCHAR('\0');

    if (fMarkIt)
        safe_strncat(text, _T("* "), iTextMaxLen);
    else
        safe_strncat(text, _T("  "), iTextMaxLen);

    StringCchCopy(szPerfMark, MAX_MARKERNAME_LEN, m_szTestName);
    szPerfMark[MAX_MARKERNAME_LEN - 1] = _T('\0');

    // Create buffer size at each end

    if (APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) & m_dwTestOpt) {
        // Send packet size
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11ld  "), dwSendPacketSize);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            StringCchPrintf(temp, TEMP_MAX_LENGTH, _T(" %ld"), dwSendPacketSize);
            safe_strncat(szPerfMark, temp, MAX_MARKERNAME_LEN);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_RECV_PKT_SIZE) & m_dwTestOpt) {
        // Recv packet size
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11ld  "), dwRecvPacketSize);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%c%ld"),
                APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) & m_dwTestOpt ? _T('/') : _T(' '),
                dwRecvPacketSize);
            safe_strncat(szPerfMark, temp, MAX_MARKERNAME_LEN);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_BYTES_SENT) & m_dwTestOpt) {
        // Number of bytes sent
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11ld  "), dwPacketsSent * dwSendPacketSize);
        safe_strncat(text, temp, iTextMaxLen);
    }

    if (!(APPLY_TO_CLIENT(HIGHLIGHT_SEND_RATE) & m_dwTestOpt) && fMarkIt) {
        if (!Perf_RegisterMark(iPerfMarkId, szPerfMark))
            fMarkIt = FALSE;
    }

    if (APPLY_TO_CLIENT(SHOW_SEND_RATE) & m_dwTestOpt) {


        // Send rate (Kbps)
        double sendRate = (double)(dwPacketsSent * dwSendPacketSize * 8.)/ (double)dwSendTime;
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11.2f  "), sendRate);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            if ((APPLY_TO_CLIENT(HIGHLIGHT_SEND_RATE) & m_dwTestOpt)) {
                StringCchPrintf(temp, TEMP_MAX_LENGTH, _T(" sent@ %d Mbps"),
                    (int)(sendRate / 1000. + 0.5));
                safe_strncat(szPerfMark, temp, MAX_MARKERNAME_LEN);
            } else {
                Perf_MarkAttributeDecimal(iPerfMarkId, _T("Send Rate (Kbps)"),
                    sendRate);
            }

           double sendRateInMb = (double)dwPacketsSent * dwSendPacketSize * 8. / (double)dwSendTime / 1000. ;
           for(int index = 0; index < g_nTestBufferCount; index ++)
           {
               if((UINT)g_aTestBufferSizes[index] == dwSendPacketSize)
               {
                   if(g_aTestPassRate[index] > sendRateInMb)
                   {
                       Log(ERROR_MSG, _T("Packet size: %d, Real send rate: %11.2f, expect send rate: %11.2f"),
                           dwSendPacketSize,sendRateInMb, g_aTestPassRate[index] );
                       logResult =  FALSE;
                   }
               }

           }
        }

    }
    if (APPLY_TO_CLIENT(SHOW_BYTES_RECVD) & m_dwTestOpt) {
        // Number of bytes recv'd
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11ld  "), dwPacketsRecvd * dwRecvPacketSize);
        safe_strncat(text, temp, iTextMaxLen);
    }

    if (((APPLY_TO_CLIENT(HIGHLIGHT_SEND_RATE) & m_dwTestOpt) &&
        !(APPLY_TO_CLIENT(HIGHLIGHT_RECV_RATE) & m_dwTestOpt)) &&
        fMarkIt) {
        if (!Perf_RegisterMark(iPerfMarkId, szPerfMark))
            fMarkIt = FALSE;
    }

    if (APPLY_TO_CLIENT(SHOW_RECV_RATE) & m_dwTestOpt) {

        // Recv rate (Kbps)
        double recvRate = (double)(dwPacketsRecvd * dwRecvPacketSize * 8. )/ (double)dwRecvTime;
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11.2f  "), recvRate);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            if ((APPLY_TO_CLIENT(HIGHLIGHT_RECV_RATE) & m_dwTestOpt)) {
                StringCchPrintf(temp, TEMP_MAX_LENGTH, _T(" recvd@ %d Mbps"),
                    (int)( recvRate/ 1000. + 0.5));
                safe_strncat(szPerfMark, temp, MAX_MARKERNAME_LEN);
            } else {
                Perf_MarkAttributeDecimal(iPerfMarkId, _T("Recv Rate (Kbps)"),
                    recvRate);
            }

           double recvRateInMb = (double)dwPacketsRecvd* dwRecvPacketSize * 8. / (double)dwRecvTime / 1000.;
           for(int index = 0; index < g_nTestBufferCount; index ++)
           {
               if((UINT)g_aTestBufferSizes[index] == dwRecvPacketSize)
               {
                   if(g_aTestPassRate[index] > recvRateInMb)
                   {
                       Log(ERROR_MSG, _T("Packet size: %d, Real send rate: %11.2f, expect send rate: %11.2f"),
                           dwRecvPacketSize,recvRateInMb, g_aTestPassRate[index] );
                       logResult =  FALSE;
                   }
               }

           }
        }
    }

    if ((!(APPLY_TO_CLIENT(HIGHLIGHT_SEND_RATE) & m_dwTestOpt) &&
         !(APPLY_TO_CLIENT(HIGHLIGHT_RECV_RATE) & m_dwTestOpt)) &&
        fMarkIt) {

        if (!Perf_RegisterMark(iPerfMarkId, szPerfMark))
            fMarkIt = FALSE;
    }

    if (APPLY_TO_CLIENT(SHOW_LATENCY) & m_dwTestOpt) {
        // Latency
        double latency = (double)dwLatencyUs / 1000.;
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11.2f  "), latency);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            Perf_MarkAttributeDecimal(iPerfMarkId, _T("Latency (ms)"),
                latency);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_JITTER) & m_dwTestOpt) {
        // Jitter
        double jitter =  (double)dwJitterUs / 1000.;
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11.2f  "),jitter);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            Perf_MarkAttributeDecimal(iPerfMarkId, _T("Jitter (ms)"),
                jitter);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_CPU_UTIL) & m_dwTestOpt) {
        if(g_fConsumeCpu)
        {
            // CPU utilization
            StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%8.2f  "), (double)dConsumedSysCpu);

            safe_strncat(text, temp, iTextMaxLen);
            if (fMarkIt) {
                Perf_MarkAttributeDecimal(iPerfMarkId, _T("System load (%)"),
                    (double)dConsumedSysCpu);
            }
        }
        else
        {
                // CPU utilization
                double cpuUtil =  (double)((dwTotalTime - dwCpuIdle) * 100. )/ (double)dwTotalTime;
                StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%8.2f  "),cpuUtil);

                safe_strncat(text, temp, iTextMaxLen);
                if (fMarkIt) {
                    Perf_MarkAttributeDecimal(iPerfMarkId, _T("CPU util (%)"),
                        cpuUtil);
                }
        }
    }
    if (APPLY_TO_CLIENT(SHOW_PKT_LOSS) & m_dwTestOpt) {
        // Packet loss
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%6.2f %ld/%ld"),
            (double)dwPacketsRecvd * 100. / (double)dwPacketsSent,
            dwPacketsRecvd, dwPacketsSent);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            Perf_MarkAttribute(iPerfMarkId, _T("Packets sent"), dwPacketsSent);
            Perf_MarkAttribute(iPerfMarkId, _T("Packets recvd"), dwPacketsRecvd);
        }
    }
    text[iTextMaxLen - 1] = _T('\0');
    Log(REQUIRED_MSG, text);

    return logResult;
}

BOOL ResultLog::LogResultSendRecv(
    DWORD dwSendPacketSize,     // in bytes
    DWORD dwRecvPacketSize,     // in bytes
    // Send:
    DWORD dwPacketsSent,        // number of packets sent
    DWORD dwSendTime,           // send time (in milliseconds)
    // Recv:
    DWORD dwPacketsRecvd,       // number of packets recvd
    DWORD dwRecvTime,           // recv time (in milliseconds)
    // Server Data:
    DWORD dwPacketsSentSrv,     // number of packets sent from server
    DWORD dwPacketsRecvdSrv,    // number of packets recvd at server
    // Latency:
    DWORD dwLatencyUs,
    // Jitter:
    DWORD dwJitterUs,
    // CPU utilization
    DWORD dwCpuIdle,            // in milliseconds
    DWORD dwTotalTime,          // in milliseconds
    // CPU utilization for Send
    DWORD dwCpuIdleSend,        // in milliseconds
    DWORD dwTotalTimeSend,      // in milliseconds
    // CPU utilization Recv
    DWORD dwCpuIdleRecv,        // in milliseconds
    DWORD dwTotalTimeRecv,      // in milliseconds
    // Mark it
    BOOL fMarkIt
    ) {
    TCHAR text[TEXT_MAX_LENGTH];
    TCHAR temp[TEMP_MAX_LENGTH];
    TCHAR szPerfMark[MAX_MARKERNAME_LEN];
    const MARKER_ID iPerfMarkId = 1;

    BOOL fLogResult = TRUE;

    text[0] = TCHAR('\0');
    temp[0] = TCHAR('\0');

    if (fMarkIt)
        safe_strncat(text, _T("* "), TEXT_MAX_LENGTH);
    else
        safe_strncat(text, _T("  "), TEXT_MAX_LENGTH);

    StringCchCopy(szPerfMark, MAX_MARKERNAME_LEN, m_szTestName);
    szPerfMark[MAX_MARKERNAME_LEN - 1] = _T('\0');

    // Create buffer size at each end

    if (APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) & m_dwTestOpt) {
        // Send packet size
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11ld  "), dwSendPacketSize);
        safe_strncat(text, temp, TEXT_MAX_LENGTH);
        if (fMarkIt) {
            StringCchPrintf(temp, TEMP_MAX_LENGTH, _T(" %ld"), dwSendPacketSize);
            safe_strncat(szPerfMark, temp, MAX_MARKERNAME_LEN);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_RECV_PKT_SIZE) & m_dwTestOpt) {
        // Recv packet size
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11ld  "), dwRecvPacketSize);
        safe_strncat(text, temp, TEXT_MAX_LENGTH);
        if (fMarkIt) {
            StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%c%ld"),
                APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) & m_dwTestOpt ? _T('/') : _T(' '),
                dwRecvPacketSize);
            safe_strncat(szPerfMark, temp, MAX_MARKERNAME_LEN);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_BYTES_SENT) & m_dwTestOpt) {
        // Number of bytes sent
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11ld  "), dwPacketsSent * dwSendPacketSize);
        safe_strncat(text, temp, TEXT_MAX_LENGTH);
    }

    if (!(APPLY_TO_CLIENT(HIGHLIGHT_SEND_RATE) & m_dwTestOpt) && fMarkIt) {
        if (!Perf_RegisterMark(iPerfMarkId, szPerfMark))
            fMarkIt = FALSE;
    }

    if (APPLY_TO_CLIENT(SHOW_SEND_RATE) & m_dwTestOpt) {
        // Send rate (KBps)
        double sendRateKbps =(double)(dwPacketsSent * dwSendPacketSize) / (double)(dwSendTime * MILLISEC_TO_SEC * KILO_BYTES);
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11.0f  "), sendRateKbps);
        safe_strncat(text, temp, TEXT_MAX_LENGTH);
        if (fMarkIt) {
            if ((APPLY_TO_CLIENT(HIGHLIGHT_SEND_RATE) & m_dwTestOpt)) {
                StringCchPrintf(temp, TEMP_MAX_LENGTH, _T(" sent@ %d MBps"),
                    (int)( (double)(dwPacketsSent * dwSendPacketSize) / (double)((dwSendTime * MILLISEC_TO_SEC)/ MEGA_BYTES + 0.5)));
                safe_strncat(szPerfMark, temp, MAX_MARKERNAME_LEN);
            } else {
                Perf_MarkAttributeDecimal(iPerfMarkId, _T("Send Rate (KBps)"),
                    sendRateKbps);
            }

            double lflSendRateInMb = (double)dwPacketsSent * dwSendPacketSize * 8. / (double)(dwSendTime * MILLISEC_TO_SEC/ MEGA_BYTES);
            for(int index = 0; index < g_nTestBufferCount; index ++)
            {
                if((UINT)g_aTestBufferSizes[index] == dwSendPacketSize)
                {
                    if(g_aTestPassRate[index] > lflSendRateInMb)
                    {
                        Log(ERROR_MSG, _T("Packet size: %d, Real send rate: %11.2f, expect send rate: %11.2f"),
                            dwSendPacketSize, lflSendRateInMb, g_aTestPassRate[index] );
                        fLogResult =  FALSE;
                    }
                }
           }
        }
    }

    if (APPLY_TO_CLIENT(SHOW_SEND_PKT_LOSS) & m_dwTestOpt) {
        // Packet loss
        double packetLoss = (double)dwPacketsRecvdSrv * 100. / (double)dwPacketsSent;
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("  %6.0f    "),
            packetLoss);
        safe_strncat(text, temp, TEXT_MAX_LENGTH);
        if (fMarkIt) {
            Perf_MarkAttribute(iPerfMarkId, _T("Packets sent from client"), dwPacketsSent);
            Perf_MarkAttribute(iPerfMarkId, _T("Packets recvd at server"), dwPacketsRecvdSrv);
        }
    }

    if (APPLY_TO_CLIENT(SHOW_SEND_CPU_UTIL) & m_dwTestOpt) {
        // CPU utilization
        double cpuUtil = (double)(dwTotalTimeSend - dwCpuIdleSend) * 100. / (double)dwTotalTimeSend;
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("  %8.0f    "), cpuUtil);
        safe_strncat(text, temp, TEXT_MAX_LENGTH);
        if (fMarkIt) {
            Perf_MarkAttributeDecimal(iPerfMarkId, _T("Send CPU util (%)"),
                cpuUtil);
        }
    }

    if (APPLY_TO_CLIENT(SHOW_BYTES_RECVD) & m_dwTestOpt) {
        // Number of bytes recv'd
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11ld  "), dwPacketsRecvd * dwRecvPacketSize);
        safe_strncat(text, temp, TEXT_MAX_LENGTH);
    }

    if (((APPLY_TO_CLIENT(HIGHLIGHT_SEND_RATE) & m_dwTestOpt) &&
        !(APPLY_TO_CLIENT(HIGHLIGHT_RECV_RATE) & m_dwTestOpt)) &&
        fMarkIt) {
        if (!Perf_RegisterMark(iPerfMarkId, szPerfMark))
            fMarkIt = FALSE;
    }

    if (APPLY_TO_CLIENT(SHOW_RECV_RATE) & m_dwTestOpt) {
        // Recv rate (KBps)
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11.0f  "), (double)(dwPacketsRecvd * dwRecvPacketSize) / (double)(dwRecvTime * MILLISEC_TO_SEC * KILO_BYTES));
        safe_strncat(text, temp, TEXT_MAX_LENGTH);
        if (fMarkIt) {
            if ((APPLY_TO_CLIENT(HIGHLIGHT_RECV_RATE) & m_dwTestOpt)) {
                StringCchPrintf(temp, TEMP_MAX_LENGTH, _T(" recvd@ %d MBps"),
                    (int)( (double)(dwPacketsRecvd * dwRecvPacketSize) / (double)((dwRecvTime * MILLISEC_TO_SEC)/ MEGA_BYTES + 0.5)));
                safe_strncat(szPerfMark, temp, MAX_MARKERNAME_LEN);
            } else {
                Perf_MarkAttributeDecimal(iPerfMarkId, _T("Recv Rate (KBps)"),
                    (double)(dwPacketsRecvd * dwRecvPacketSize) / (double)(dwRecvTime * MILLISEC_TO_SEC * KILO_BYTES));
            }

           double lflRecvRateInMb = (double)dwPacketsRecvd* dwRecvPacketSize * 8. / (double)(dwRecvTime * MILLISEC_TO_SEC/ MEGA_BYTES);
           for(int index = 0; index < g_nTestBufferCount; index ++)
           {
               if((UINT)g_aTestBufferSizes[index] == dwRecvPacketSize)
               {
                   if(g_aTestPassRate[index] > lflRecvRateInMb)
                   {
                       Log(ERROR_MSG, _T("Packet size: %d, Real send rate: %11.2f, expect send rate: %11.2f"),
                           dwRecvPacketSize,lflRecvRateInMb, g_aTestPassRate[index] );
                       fLogResult =  FALSE;
                   }
               }
           }
        }
    }

    if (APPLY_TO_CLIENT(SHOW_RECV_PKT_LOSS) & m_dwTestOpt) {
        // Packet loss
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("  %6.0f    "),
            (double)dwPacketsRecvd * 100. / (double)dwPacketsSentSrv);
        safe_strncat(text, temp, TEXT_MAX_LENGTH);
        if (fMarkIt) {
            Perf_MarkAttribute(iPerfMarkId, _T("Packets sent from server"), dwPacketsSentSrv);
            Perf_MarkAttribute(iPerfMarkId, _T("Packets recvd at client"), dwPacketsRecvd);
        }
    }

    if (APPLY_TO_CLIENT(SHOW_RECV_CPU_UTIL) & m_dwTestOpt) {
        // CPU utilization
        double cpuUtil = (double)(dwTotalTimeRecv - dwCpuIdleRecv) * 100. / (double)dwTotalTimeRecv;
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("  %8.0f    "), cpuUtil);

        safe_strncat(text, temp, TEXT_MAX_LENGTH);
        if (fMarkIt) {
            Perf_MarkAttributeDecimal(iPerfMarkId, _T("Recv CPU util (%)"),
                cpuUtil);
        }
    }

    if ((!(APPLY_TO_CLIENT(HIGHLIGHT_SEND_RATE) & m_dwTestOpt) &&
         !(APPLY_TO_CLIENT(HIGHLIGHT_RECV_RATE) & m_dwTestOpt)) &&
        fMarkIt) {

        if (!Perf_RegisterMark(iPerfMarkId, szPerfMark))
            fMarkIt = FALSE;
    }

    if (APPLY_TO_CLIENT(SHOW_LATENCY) & m_dwTestOpt) {
        // Latency
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11.2f  "), (double)dwLatencyUs / 1000.);
        safe_strncat(text, temp, TEXT_MAX_LENGTH);
        if (fMarkIt) {
            Perf_MarkAttributeDecimal(iPerfMarkId, _T("Latency (ms)"),
                ((double)dwLatencyUs) / 1000.);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_JITTER) & m_dwTestOpt) {
        // Jitter
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%11.2f  "), (double)dwJitterUs / 1000.);
        safe_strncat(text, temp, TEXT_MAX_LENGTH);
        if (fMarkIt) {
            Perf_MarkAttributeDecimal(iPerfMarkId, _T("Jitter (ms)"),
                (double)dwJitterUs / 1000.);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_CPU_UTIL) & m_dwTestOpt) {
        // CPU utilization
        double cpuUtil = (double)(dwTotalTime - dwCpuIdle) * 100. / (double)dwTotalTime;
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%8.2f  "), cpuUtil);

        safe_strncat(text, temp, TEXT_MAX_LENGTH);
        if (fMarkIt) {
            Perf_MarkAttributeDecimal(iPerfMarkId, _T("CPU util (%)"),
                cpuUtil);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_PKT_LOSS) & m_dwTestOpt) {
        // Packet loss
        StringCchPrintf(temp, TEMP_MAX_LENGTH, _T("%6.2f  "),
            (double)dwPacketsRecvd * 100. / (double)dwPacketsSent);
        safe_strncat(text, temp, TEXT_MAX_LENGTH);
        if (fMarkIt) {
            Perf_MarkAttribute(iPerfMarkId, _T("Packets sent"), dwPacketsSent);
            Perf_MarkAttribute(iPerfMarkId, _T("Packets recvd"), dwPacketsRecvd);
        }
    }

    text[TEXT_MAX_LENGTH - 1] = _T('\0');
    Log(REQUIRED_MSG, text);
    return fLogResult;
}