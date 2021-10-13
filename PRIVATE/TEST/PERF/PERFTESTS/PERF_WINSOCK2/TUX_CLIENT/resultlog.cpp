//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include "resultlog.h"
#include "loglib.h"
#include "PerfLoggerApi.h"

#define MIN(a,b)  ((a) < (b) ? (a) : (b))

inline TCHAR* safe_strncat(
    TCHAR* szOutStr,
    TCHAR* szInStr,
    int    iOutStrMaxLen)
{
    // Make sure that the string is null-terminated
    szOutStr[iOutStrMaxLen - 1] = _T('\0');
    return _tcsncat(szOutStr, szInStr, iOutStrMaxLen - 1 - _tcslen(szOutStr));
}

ResultLog::ResultLog(
    TCHAR *szTestName,
    DWORD dwTestOpt
    ) {

    _tcsncpy(m_szTestName, szTestName, MIN(MAX_TEST_NAME_LEN, _tcslen(szTestName) + 1));
    m_dwTestOpt = dwTestOpt;
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
        safe_strncat(text1, _T("CPU Util  "), iMaxTextLen);
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

void ResultLog::LogResult(
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
    BOOL fMarkIt
    ) {

    const int iTextMaxLen = 160;
    TCHAR text[iTextMaxLen];
    const int iTempMaxLen = 40;
    TCHAR temp[iTempMaxLen];
    TCHAR szPerfMark[MAX_MARKERNAME_LEN];
    const MARKER_ID iPerfMarkId = 1;

    text[0] = TCHAR('\0');
    temp[0] = TCHAR('\0');

    if (fMarkIt)
        safe_strncat(text, _T("* "), iTextMaxLen);
    else
        safe_strncat(text, _T("  "), iTextMaxLen);

    _tcsncpy(szPerfMark, m_szTestName, MAX_MARKERNAME_LEN);
    szPerfMark[MAX_MARKERNAME_LEN - 1] = _T('\0');

    // Create buffer size at each end

    if (APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) & m_dwTestOpt) {
        // Send packet size
        _stprintf(temp, _T("%11ld  "), dwSendPacketSize);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            _stprintf(temp, _T(" %ld"), dwSendPacketSize);
            safe_strncat(szPerfMark, temp, MAX_MARKERNAME_LEN);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_RECV_PKT_SIZE) & m_dwTestOpt) {
        // Recv packet size
        _stprintf(temp, _T("%11ld  "), dwRecvPacketSize);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            _stprintf(temp, _T("%c%ld"),
                APPLY_TO_CLIENT(SHOW_SEND_PKT_SIZE) & m_dwTestOpt ? _T('/') : _T(' '),
                dwRecvPacketSize);
            safe_strncat(szPerfMark, temp, MAX_MARKERNAME_LEN);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_BYTES_SENT) & m_dwTestOpt) {
        // Number of bytes sent
        _stprintf(temp, _T("%11ld  "), dwPacketsSent * dwSendPacketSize);
        safe_strncat(text, temp, iTextMaxLen);
    }

    if (!(APPLY_TO_CLIENT(HIGHLIGHT_SEND_RATE) & m_dwTestOpt) && fMarkIt) {
        if (!Perf_RegisterMark(iPerfMarkId, szPerfMark))
            fMarkIt = FALSE;
    }

    if (APPLY_TO_CLIENT(SHOW_SEND_RATE) & m_dwTestOpt) {
        // Send rate (Kbps)
        _stprintf(temp, _T("%11.2f  "), (double)dwPacketsSent * dwSendPacketSize * 8. / (double)dwSendTime);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            if ((APPLY_TO_CLIENT(HIGHLIGHT_SEND_RATE) & m_dwTestOpt)) {
                _stprintf(temp, _T(" sent@ %d Mbps"),
                    (int)( (double)dwPacketsSent * dwSendPacketSize * 8. / (double)dwSendTime / 1000. + 0.5));
                safe_strncat(szPerfMark, temp, MAX_MARKERNAME_LEN);
            } else {
                Perf_MarkAttributeDecimal(iPerfMarkId, _T("Send Rate (Kbps)"),
                    (double)dwPacketsSent * dwSendPacketSize * 8. / (double)dwSendTime);
            }
        }
    }
    if (APPLY_TO_CLIENT(SHOW_BYTES_RECVD) & m_dwTestOpt) {
        // Number of bytes recv'd
        _stprintf(temp, _T("%11ld  "), dwPacketsRecvd * dwRecvPacketSize);
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
        _stprintf(temp, _T("%11.2f  "), (double)dwPacketsRecvd * dwRecvPacketSize * 8. / (double)dwRecvTime);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            if ((APPLY_TO_CLIENT(HIGHLIGHT_RECV_RATE) & m_dwTestOpt)) {
                _stprintf(temp, _T(" recvd@ %d Mbps"),
                    (int)( (double)dwPacketsRecvd * dwRecvPacketSize * 8. / (double)dwRecvTime / 1000. + 0.5));
                safe_strncat(szPerfMark, temp, MAX_MARKERNAME_LEN);
            } else {
                Perf_MarkAttributeDecimal(iPerfMarkId, _T("Recv Rate (Kbps)"),
                    (double)dwPacketsRecvd * dwRecvPacketSize * 8. / (double)dwRecvTime);
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
        _stprintf(temp, _T("%11.2f  "), (double)dwLatencyUs / 1000.);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            Perf_MarkAttributeDecimal(iPerfMarkId, _T("Latency (ms)"),
                ((double)dwLatencyUs) / 1000.);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_JITTER) & m_dwTestOpt) {
        // Jitter
        _stprintf(temp, _T("%11.2f  "), (double)dwJitterUs / 1000.);
        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            Perf_MarkAttributeDecimal(iPerfMarkId, _T("Jitter (ms)"),
                (double)dwJitterUs / 1000.);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_CPU_UTIL) & m_dwTestOpt) {
        // CPU utilization
        _stprintf(temp, _T("%8.2f  "), (double)(dwTotalTime - dwCpuIdle) * 100. / (double)dwTotalTime);

        safe_strncat(text, temp, iTextMaxLen);
        if (fMarkIt) {
            Perf_MarkAttributeDecimal(iPerfMarkId, _T("CPU util (%)"),
                (double)(dwTotalTime - dwCpuIdle) * 100. / (double)dwTotalTime);
        }
    }
    if (APPLY_TO_CLIENT(SHOW_PKT_LOSS) & m_dwTestOpt) {
        // Packet loss
        _stprintf(temp, _T("%6.2f %ld/%ld"),
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
}