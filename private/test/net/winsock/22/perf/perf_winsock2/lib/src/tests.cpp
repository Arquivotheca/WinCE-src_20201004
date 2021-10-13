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
// tests.cpp

#include "tests.h"
#include "loglib.h"
#include "conntrk.h"
#include "ntcompat.h"

///////////////////////////////////////////////////////////////////////////
//
// Globals
//
BOOL g_MinPauseInited = FALSE;
const INT g_CompressionWindow = 4096;

///////////////////////////////////////////////////////////////////////////
//
// Test result memory manipulation functions
//
BOOL AllocTestResult(DWORD num, TestResult* result)
{
    if (result == NULL) return FALSE;
    
    result->array_size = num;
    if ( (result->time_array = new DWORD[num]) == NULL) goto error;
    if ( (result->cpuutil_array = new DWORD[num]) == NULL) goto error;
    if ( (result->sent_array = new DWORD[num]) == NULL) goto error;
    if ( (result->recvd_array = new DWORD[num]) == NULL) goto error;

    ClearTestResult(result);
    return TRUE;
    
error:
    FreeTestResult(result);
    return FALSE;    
}

BOOL ClearTestResult(TestResult* result)
{
    if (result == NULL ||
        result->time_array == NULL ||
        result->cpuutil_array == NULL ||
        result->sent_array == NULL ||
        result->recvd_array == NULL) return FALSE;
    
    DWORD num = result->array_size;
    for (DWORD i=0; i<num; i+=1)
        result->time_array[i]
        = result->cpuutil_array[i]
        = result->sent_array[i]
        = result->recvd_array[i]
        = 0;
    return TRUE;
}

BOOL CopyTestResult(TestResult* result_dest, TestResult* result_src)
{
    if (result_dest == NULL || result_src == NULL) return FALSE;
    DWORD num = result_src->array_size;
    if (num > result_dest->array_size) return FALSE;
    for (DWORD i=0; i<num; i+=1)
    {
        result_dest->time_array[i] = result_src->time_array[i];
        result_dest->cpuutil_array[i] = result_src->cpuutil_array[i];
        result_dest->sent_array[i] = result_src->sent_array[i];
        result_dest->recvd_array[i] = result_src->recvd_array[i];
    }
    return TRUE;
}

BOOL FreeTestResult(TestResult* result)
{
    if (result == NULL) return FALSE;

    if (result->time_array != NULL) delete [] result->time_array;
    if (result->cpuutil_array != NULL) delete [] result->cpuutil_array;
    if (result->sent_array != NULL) delete [] result->sent_array;
    if (result->recvd_array != NULL) delete [] result->recvd_array;

    result->time_array = result->cpuutil_array = NULL;
    result->sent_array = result->recvd_array = NULL;

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////
//
// Internal memory allocation routines
//
BOOL AllocBufferChain(INT packet_size, CHAR*** pppbuffer)
{
    if (pppbuffer == NULL) return FALSE;

    // TODO: calculate number of buffers to compensate for the compression window

    *pppbuffer = new PCHAR[1];
    if (*pppbuffer == NULL) return FALSE;

    (*pppbuffer)[0] = new CHAR[packet_size];
    if ((*pppbuffer)[0] == NULL) {
        delete [] (*pppbuffer);
        *pppbuffer = NULL;
        return FALSE;
    }
    return TRUE;
}

BOOL FreeBufferChain(INT packet_size, CHAR*** pppbuffer)
{
    if (pppbuffer == NULL) return FALSE;

    delete [] (*pppbuffer)[0];
    delete [] *pppbuffer;
    *pppbuffer = NULL;
    
    return TRUE;
}

BOOL AllocBuffer(INT packet_size, CHAR** ppbuffer)
{
    if (ppbuffer == NULL) return FALSE;

    *ppbuffer = new CHAR[packet_size];
    if (*ppbuffer == NULL) return FALSE;

    return TRUE;
}

INT FreeBuffer(INT packet_size, CHAR** ppbuffer)
{
    if (ppbuffer == NULL) return FALSE;

    delete [] *ppbuffer;
    *ppbuffer = NULL;
    
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////
//
// Implementation of test initialization, cleanup and the actual test
//
class AltCpuMon {
    friend DWORD WINAPI AltCpuMonThrd(LPVOID lpParam);
    HANDLE m_Thrd;
    DWORD m_Count;
    DWORD m_MaxCountPerMs;
    BOOL  m_Terminate;
public:
    AltCpuMon()
    {
        m_Terminate = FALSE;
        m_Thrd = CreateThread(NULL, 0, AltCpuMonThrd, this, 0, NULL);
    }
    ~AltCpuMon()
    {
        if (m_Thrd == NULL) return;
        m_Terminate = TRUE;
        WaitForSingleObject(m_Thrd, 500);
        CloseHandle(m_Thrd);
    }
    BOOL Calibrate()
    {
        m_MaxCountPerMs = 0;
        for (int i=0; i<10; i+=1)
        {
            DWORD time, count;

            time = GetTickCount();
            count = m_Count;

            Sleep(100);

            count = m_Count - count;
            time = GetTickCount() - time;

            if (count == 0) count = 1;
            if (count / time > m_MaxCountPerMs)
            {
                m_MaxCountPerMs = count / time;
                if (m_MaxCountPerMs == 0) m_MaxCountPerMs = 1;
            }
        }
        if (m_MaxCountPerMs < 10) return FALSE; // didn't calibrate properly
        return TRUE;
    }
    DWORD GetIdleTime()
    {
        if (m_MaxCountPerMs < 10) return 0;
        return m_Count / m_MaxCountPerMs;
    }
};

DWORD WINAPI AltCpuMonThrd(LPVOID lpParam)
{
    AltCpuMon* obj_ptr = (AltCpuMon*)lpParam;
#ifdef UNDER_CE
    CeSetThreadPriority(GetCurrentThread(), 255);
#endif

    DWORD &count = obj_ptr->m_Count;
    BOOL  &terminate = obj_ptr->m_Terminate;

    for (;;)
    {
        ++count;
        if (terminate == TRUE) break;
    }
    return 0;
}

struct TestForm
{
    TestType test_type;
    INT protocol;
    DWORD num_iterations;
    DWORD send_total_size;    // total size of bytes to send per iteration
    DWORD send_packet_size;   //  packet/buffer to use to send
    DWORD recv_total_size;    // total size of bytes to receive per iteration
    DWORD recv_packet_size;   //  packets/buffer to use to receive
    CHAR** send_buf_chain;
    INT num_send_bufs_in_chain;
    CHAR* recv_buf;
    AltCpuMon* cpu_mon_ptr;   // alternative CPU monitor

    TestForm(): send_buf_chain(NULL), recv_buf(NULL), cpu_mon_ptr(NULL) {}
};

INT TestInit(
    TestType test_type,
    INT protocol,
    DWORD num_iterations,
    DWORD send_total_size,    // total size of bytes to send per iteration
    DWORD send_packet_size,   //  packet/buffer to use to send
    DWORD recv_total_size,    // total size of bytes to receive per iteration
    DWORD recv_packet_size,   //  packets/buffer to use to receive
    BOOL  alt_cpu_mon,        // alternative CPU monitor when GetIdleTime is not implemented
    PTestForm* pptest_form
) {
    if (pptest_form == NULL) {
        ASSERT(FALSE);
        return 0;
    }
    *pptest_form = new TestForm;
    if (*pptest_form == NULL) {
        ASSERT(FALSE);
        return 0;
    }
    (*pptest_form)->test_type = test_type;
    (*pptest_form)->protocol  = protocol;
    (*pptest_form)->num_iterations = num_iterations;
    (*pptest_form)->send_total_size = send_total_size;
    (*pptest_form)->send_packet_size = send_packet_size;
    (*pptest_form)->recv_total_size = recv_total_size;
    (*pptest_form)->recv_packet_size = recv_packet_size;

    if (alt_cpu_mon)
    {
        (*pptest_form)->cpu_mon_ptr = new AltCpuMon;
        if ((*pptest_form)->cpu_mon_ptr != NULL)
            if ((*pptest_form)->cpu_mon_ptr->Calibrate() == FALSE)
            {
                delete (*pptest_form)->cpu_mon_ptr;
                (*pptest_form)->cpu_mon_ptr = NULL;
                Log(ERROR_MSG, TEXT("TestInit: failed to properly calibrate AltCpuMon"));
            }
                
    }
    else
        (*pptest_form)->cpu_mon_ptr = NULL;
    
    (*pptest_form)->num_send_bufs_in_chain =
        AllocBufferChain(
            send_packet_size,
            &(*pptest_form)->send_buf_chain);
    AllocBuffer(recv_packet_size, &(*pptest_form)->recv_buf);

    return 0;
}

INT TestDeinit(
    PTestForm* pptest_form
) {
    if (pptest_form == NULL ||
        *pptest_form == NULL) return 1;

    FreeBufferChain(
        (*pptest_form)->send_packet_size,
        &(*pptest_form)->send_buf_chain
    );

    FreeBuffer(
        (*pptest_form)->recv_packet_size,
        &(*pptest_form)->recv_buf);

    if ( (*pptest_form)->cpu_mon_ptr != NULL )
        delete (*pptest_form)->cpu_mon_ptr;
      
    delete *pptest_form;
    *pptest_form = NULL;
    return 0;
}

#ifndef UNDER_CE
DWORD GetIdleTime()
{
    return 0;
}
#endif

INT Test(
    PTestForm ptest_form,   // TestForm pointer from TestInit
    SOCKET sock,            // Socket for the established connection
    DWORD send_req_rate,    // Requested send rate (bytes/second)
    DWORD recv_req_rate,    // Requested recv rate (bytes/second)
    CONNTRACK_HANDLE contrack_handle,
    TestResult* presult     // result structure that will be populated with result of the test
) {
    // per-iteration definitions
    DWORD num_packets_to_send = 0;
    Log(DEBUG_MSG, TEXT("Test Started"));
    if (ptest_form->send_packet_size > 0 && ptest_form->send_total_size > 0)
        num_packets_to_send =
            (ptest_form->send_total_size / ptest_form->send_packet_size) +
            (ptest_form->send_total_size % ptest_form->send_packet_size == 0 ? 0 : 1);

    DWORD num_packets_to_receive = 0;
    if (ptest_form->recv_packet_size > 0 && ptest_form->recv_total_size > 0)
        num_packets_to_receive =
            (ptest_form->recv_total_size / ptest_form->recv_packet_size) +
            (ptest_form->recv_total_size % ptest_form->recv_packet_size == 0 ? 0 : 1);

    Log(DEBUG_MSG, TEXT("Requested rate = %ld bytes/s"), send_req_rate);

    INT ret = 0;
    BOOL send_next = ptest_form->test_type == Send ? TRUE : FALSE;

    DWORD start_time = 0, end_time = 0;
    DWORD start_cpu_idle = 0, end_cpu_idle = 0;

    LARGE_INTEGER l1, l2, freq;
    LISet32(l1, 0);
    LISet32(l2, 0);
    LISet32(freq, 0);

    QueryPerformanceFrequency(&freq);       
    Log(DEBUG_MSG, TEXT("Perf Counter Freq. = %I64u ticks/s"), freq.QuadPart);

    DWORD i1 = 0, i2 = 0;

    presult->total_time = GetTickCount();
    if ( ptest_form->cpu_mon_ptr != NULL )
        presult->total_cputil = ptest_form->cpu_mon_ptr->GetIdleTime();
    else
        presult->total_cputil = GetIdleTime();

    for (DWORD i = 0;
         i < ptest_form->num_iterations * 2 && ret != SOCKET_ERROR;
         i += 1)
    {
        DWORD packets_remaining = send_next ?
            num_packets_to_send : num_packets_to_receive;

        if (i % 2 == 0)
        {
            start_time = GetTickCount();
#ifdef UNDER_CE
            if ( ptest_form->cpu_mon_ptr != NULL )
                start_cpu_idle = ptest_form->cpu_mon_ptr->GetIdleTime();
            else
                start_cpu_idle = GetIdleTime();
#else
            start_cpu_idle = 0;
#endif
        }

        if (send_req_rate != 0 && send_next) QueryPerformanceCounter(&l1);
        if (recv_req_rate != 0 && !send_next) i1 = GetTickCount();

        while (packets_remaining > 0)
        {
            DWORD bytes_to_transmit;
            DWORD bytes_transmitted;

            bytes_to_transmit = send_next ?
                ptest_form->send_packet_size : ptest_form->recv_packet_size;
            bytes_transmitted = 0;
             
            // this applies only for sending
            DWORD chain_buf_to_use =
                packets_remaining % ptest_form->num_send_bufs_in_chain;

            while (bytes_transmitted < bytes_to_transmit)
            {
                if (send_next)
                    ret = send(sock, ptest_form->send_buf_chain[chain_buf_to_use] + bytes_transmitted,
                        bytes_to_transmit - bytes_transmitted, 0);
                else
                    ret = recv(sock, ptest_form->recv_buf + bytes_transmitted,
                        bytes_to_transmit - bytes_transmitted, 0);

                if (ret == SOCKET_ERROR)
                {
                    DWORD WSAErr = WSAGetLastError();
                    Log(DEBUG_MSG, TEXT("Xmit data failed, WSAError=%ld"), WSAErr);
                    i = 1;  //to obtain an end time even if a timeout occured
                    goto NextBuffer;
                }
                else if (ret == 0)
                {
                    Log(DEBUG_MSG, TEXT("Xmit data failed, remote-end closed connection"));
                    ret = SOCKET_ERROR; // We treat this the same as a socket error.
                    i = 1;  //to obtain an end time even if test failed.
                    goto NextBuffer;
                }
                bytes_transmitted += ret;

                if (contrack_handle != NULL) ConnTrack_Touch(contrack_handle);
            }

            packets_remaining -= 1;
            
            // Update result array
            if (send_next)
                presult->sent_array[i/2] += 1;
            else
                presult->recvd_array[i/2] += 1;
            
            //
            // Send/recv rate throtteling code
            //
            if (packets_remaining > 0)
            {
                if (send_req_rate != 0 && send_next)
                {
                    // Check what time is it
                    QueryPerformanceCounter(&l2);

                    // Check whether we have not transmitted more data that
                    // we are allowed to transmit given the bandwidth constraint.
                    // Note: Using 1/100 second precision.
                    if ( send_req_rate / 100 * (l2.QuadPart - l1.QuadPart) / (freq.QuadPart / 100)   // Data transfer constraint
                        < (num_packets_to_send - packets_remaining) * ptest_form->send_packet_size ) // Num bytes sent so far
                    {
                        // Determine for how long to stall
                        LARGE_INTEGER stall_until;
                        stall_until.QuadPart =
                            (num_packets_to_send - packets_remaining) * ptest_form->send_packet_size // num bytes sent
                            / (send_req_rate / 100)
                            * (freq.QuadPart / 100)
                            + l1.QuadPart;

                        // Apply the pause
                        while ( l2.QuadPart < stall_until.QuadPart)
                        {
#if UNDER_CE
                            Sleep(0);
#else
                            SwitchToThread();
#endif
                            QueryPerformanceCounter(&l2);
                        }
                    }
                }
                else if (recv_req_rate != 0 && !send_next)
                {
                    // Check what time is it
                    i2 = GetTickCount();

                    // Check whether we have not transmitted more data that
                    // we are allowed to transmit given the bandwidth constraint.
                    // Note: Using 1/100 second precision.
                    if ( recv_req_rate / 100 * (i2 - i1)                                          // Data transfer constraint
                        < (num_packets_to_receive - packets_remaining) * ptest_form->recv_packet_size * 10 ) // Num bytes recv'd so far
                    {
                        // Determine for how long to stall
                        DWORD wait_dur;
                        wait_dur =
                            ((num_packets_to_receive - packets_remaining) * ptest_form->recv_packet_size) * 10 // num bytes recv'd
                            / (recv_req_rate / 100) - (i2 - i1);

                        // Apply the pause
                        Sleep(wait_dur);
                    }
                }
            } // if (packets_remaining > 0)
        }
NextBuffer:
        if (i % 2 == 1)
        {
            end_time = GetTickCount();
            if ( ptest_form->cpu_mon_ptr != NULL )
                end_cpu_idle = ptest_form->cpu_mon_ptr->GetIdleTime();
            else
                end_cpu_idle = GetIdleTime();

            // record result in array
            presult->time_array[i/2] = end_time - start_time;
            presult->cpuutil_array[i/2] = end_cpu_idle - start_cpu_idle;
        }
        send_next = !send_next;
    }

    presult->total_time = GetTickCount() - presult->total_time;
    if ( ptest_form->cpu_mon_ptr != NULL )
        presult->total_cputil = ptest_form->cpu_mon_ptr->GetIdleTime() - presult->total_cputil;
    else
        presult->total_cputil = GetIdleTime() - presult->total_cputil;

    // return error code
    Log(DEBUG_MSG, TEXT("Test Ended - Ret Code: %d "),ret);
    return ret;
}

INT TestSendRecvRate(
    PTestForm ptest_form,   // TestForm pointer from TestInit
    SOCKET sock,            // Socket for the established connection
    DWORD req_cpu_util,    // Requested cpu utilization
    CONNTRACK_HANDLE contrack_handle,
    TestResult* presult     // result structure that will be populated with result of the test
) {
    // per-iteration definitions
    DWORD num_packets_to_send = 0;
    if (ptest_form->send_packet_size > 0 && ptest_form->send_total_size > 0)
        num_packets_to_send =
            (ptest_form->send_total_size / ptest_form->send_packet_size) +
            (ptest_form->send_total_size % ptest_form->send_packet_size == 0 ? 0 : 1);

    DWORD num_packets_to_receive = 0;
    if (ptest_form->recv_packet_size > 0 && ptest_form->recv_total_size > 0)
        num_packets_to_receive =
            (ptest_form->recv_total_size / ptest_form->recv_packet_size) +
            (ptest_form->recv_total_size % ptest_form->recv_packet_size == 0 ? 0 : 1);

    INT ret = 0;
    BOOL send_next = ptest_form->test_type == Send ? TRUE : FALSE;

    DWORD start_time = 0, end_time = 0;
    DWORD start_cpu_idle = 0, end_cpu_idle = 0;

    presult->total_time = GetTickCount();
    if ( ptest_form->cpu_mon_ptr != NULL )
        presult->total_cputil = ptest_form->cpu_mon_ptr->GetIdleTime();
    else
        presult->total_cputil = GetIdleTime();

    for (DWORD i = 0;
         i < ptest_form->num_iterations * 2 && ret != SOCKET_ERROR;
         i += 1)
    {
        DWORD packets_remaining = send_next ?
            num_packets_to_send : num_packets_to_receive;

        if (i % 2 == 0)
        {
            start_time = GetTickCount();
#ifdef UNDER_CE
            if ( ptest_form->cpu_mon_ptr != NULL )
                start_cpu_idle = ptest_form->cpu_mon_ptr->GetIdleTime();
            else
                start_cpu_idle = GetIdleTime();
#else
            start_cpu_idle = 0;
#endif
        }


        while (packets_remaining > 0)
        {
            DWORD bytes_to_transmit;
            DWORD bytes_transmitted;

            bytes_to_transmit = send_next ?
                ptest_form->send_packet_size : ptest_form->recv_packet_size;
            bytes_transmitted = 0;
             
            // this applies only for sending
            DWORD chain_buf_to_use =
                packets_remaining % ptest_form->num_send_bufs_in_chain;
            while (bytes_transmitted < bytes_to_transmit)
            {    
                if (send_next)
                    ret = send(sock, ptest_form->send_buf_chain[chain_buf_to_use] + bytes_transmitted,
                        bytes_to_transmit - bytes_transmitted, 0);
                else
                    ret = recv(sock, ptest_form->recv_buf + bytes_transmitted,
                        bytes_to_transmit - bytes_transmitted, 0);

                if (ret == SOCKET_ERROR)
                {
                    DWORD WSAErr = WSAGetLastError();
                    Log(DEBUG_MSG, TEXT("Xmit data failed, WSAError=%ld"), WSAErr);
                    i = 1;  //to obtain an end time even if a timeout occured
                    goto NextBuffer;
                }
                else if (ret == 0)
                {
                    Log(DEBUG_MSG, TEXT("Xmit data failed, remote-end closed connection"));
                    ret = SOCKET_ERROR; // We treat this the same as a socket error.
                    i = 1;  //to obtain an end time even if test failed.
                    goto NextBuffer;
                }
                bytes_transmitted += ret;

                if (contrack_handle != NULL) ConnTrack_Touch(contrack_handle);
            }

            packets_remaining -= 1;
            
            // Update result array
            if (send_next)
                presult->sent_array[i/2] += 1;
            else
                presult->recvd_array[i/2] += 1;
        } // while (packets_remaining > 0)

NextBuffer:
        if (i % 2 == 1)
        {
            end_time = GetTickCount();   

            if ( ptest_form->cpu_mon_ptr != NULL )
                end_cpu_idle = ptest_form->cpu_mon_ptr->GetIdleTime();
            else
                end_cpu_idle = GetIdleTime();
            
            if(end_cpu_idle - start_cpu_idle > 0)
            {
                Log(DEBUG_MSG, TEXT("CPU idle %ums during send/recv."), end_cpu_idle - start_cpu_idle);
                Log(DEBUG_MSG, TEXT("CPU usage is calculated using the max CPU utilization reached."));
            }

            // sleep to allow GetIdleTime() to increment
            float sleep_factor = (float)(100-req_cpu_util)/req_cpu_util;
            Sleep( (DWORD)((end_time - start_time) * sleep_factor));  

            end_time = GetTickCount();
            if ( ptest_form->cpu_mon_ptr != NULL )
                end_cpu_idle = ptest_form->cpu_mon_ptr->GetIdleTime();
            else
                end_cpu_idle = GetIdleTime();
            // record result in array
            presult->time_array[i/2] = end_time - start_time;
            presult->cpuutil_array[i/2] = end_cpu_idle - start_cpu_idle;
        }
        send_next = !send_next;
    }

    presult->total_time = GetTickCount() - presult->total_time;
    if ( ptest_form->cpu_mon_ptr != NULL )
        presult->total_cputil = ptest_form->cpu_mon_ptr->GetIdleTime() - presult->total_cputil;
    else
        presult->total_cputil = GetIdleTime() - presult->total_cputil;
    // return error code
    return ret;
}
