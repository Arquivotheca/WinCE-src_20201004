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
// commprot.cpp

#include "commprot.h"
#include "loglib.h"

#define RECV_TIMEOUT_SEC 360 // Note: Needs to be larger than TEST_TIMEOUT_MS

const TCHAR* SERVICE_ERROR_STRING[] =
{
    TEXT("Test_Success"),
    TEXT("Test_Error"),
    TEXT("Test_Version_Error"),
    TEXT("Test_Comm_Timeout"),
    TEXT("Test_Run_Timeout")
};

#define XMIT_SEND TRUE
#define XMIT_RECV FALSE
INT XmitData(BOOL fsend, SOCKET sock, CHAR* pdata, int data_len, DWORD* pdwWSAError)
{
    int i;

    if (pdwWSAError == NULL) return SOCKET_ERROR;
    *pdwWSAError = 0;
    
    for (i=0; i<data_len;)
    {
        DWORD ret;
        if (fsend)
            ret = send(sock, (CHAR*)(pdata + i), data_len - i, 0);
        else {
            // Impose a limit on how long we'll wait for recv to return.
            timeval timeout = { RECV_TIMEOUT_SEC, 0 };
            fd_set readfsd;

            FD_ZERO(&readfsd);
            FD_SET(sock, &readfsd);

            ret = select(0, &readfsd, NULL, NULL, &timeout);
            if (ret == SOCKET_ERROR)
            { // Panic.
                *pdwWSAError = WSAGetLastError();
                break;
            }
            else if (ret == 0)
            { // Timeout.
                *pdwWSAError = 0;
                break;
            }
            ret = recv(sock, (CHAR*)(pdata + i), data_len - i, 0);
        }

        if (ret == SOCKET_ERROR)
        {
            if (pdwWSAError != NULL) *pdwWSAError = WSAGetLastError();
            break;
        }
        i += ret;
    }

    if (i < data_len)
        return SOCKET_ERROR;
    return 0;
}

INT SendServiceRequest(SOCKET sock, ServiceRequest* preq, DWORD* pdwWSAError)
{
    return XmitData(XMIT_SEND, sock, (CHAR*)preq, sizeof(ServiceRequest), pdwWSAError);
}
INT ReceiveServiceRequest(SOCKET sock, ServiceRequest* preq, DWORD* pdwWSAError)
{
    return XmitData(XMIT_RECV, sock, (CHAR*)preq, sizeof(ServiceRequest), pdwWSAError);
}

INT SendServiceResponse(SOCKET sock, ServiceResponse* presp, DWORD* pdwWSAError)
{
    return XmitData(XMIT_SEND, sock, (CHAR*)presp, sizeof(ServiceResponse), pdwWSAError);
}

INT ReceiveServiceResponse(SOCKET sock, ServiceResponse* presp, DWORD* pdwWSAError)
{
    return XmitData(XMIT_RECV, sock, (CHAR*)presp, sizeof(ServiceResponse), pdwWSAError);
}

INT SendGoMsg(SOCKET sock, DWORD* pdwWSAError)
{
    char temp[] = "GO!";
    INT ret;
    ret = XmitData(XMIT_SEND, sock,
        (CHAR*)temp, sizeof(temp), pdwWSAError);
    return ret;
}

INT ReceiveGoMsg(SOCKET sock, DWORD* pdwWSAError)
{
    char temp[] = "GO!";
    INT ret;
    ret = XmitData(XMIT_RECV, sock,
        (CHAR*)temp, sizeof(temp), pdwWSAError);
    if (ret == SOCKET_ERROR ||
        temp[0] != 'G' ||
        temp[1] != 'O' ||
        temp[2] != '!')
    {
        if (ret != SOCKET_ERROR) pdwWSAError = 0;
        return SOCKET_ERROR;
    }
    return ret;    
}

INT SendTestResponse(SOCKET sock, TestResponse* presp, DWORD* pdwWSAError)
{
    INT ret;
    ret = XmitData(XMIT_SEND, sock,
        (CHAR*)&presp->result->array_size,
        sizeof(presp->result->array_size),
        pdwWSAError);
    if (ret == SOCKET_ERROR) return ret;
    ret = XmitData(XMIT_SEND, sock,
        (CHAR*)presp->result->time_array,
        sizeof(presp->result->time_array[0]) * presp->result->array_size,
        pdwWSAError);
    if (ret == SOCKET_ERROR) return ret;
    ret = XmitData(XMIT_SEND, sock,
        (CHAR*)presp->result->cpuutil_array,
        sizeof(presp->result->cpuutil_array[0]) * presp->result->array_size,
        pdwWSAError);
    if (ret == SOCKET_ERROR) return ret;
    ret = XmitData(XMIT_SEND, sock,
        (CHAR*)presp->result->sent_array,
        sizeof(presp->result->sent_array[0]) * presp->result->array_size,
        pdwWSAError);
    if (ret == SOCKET_ERROR) return ret;
    ret = XmitData(XMIT_SEND, sock,
        (CHAR*)presp->result->recvd_array,
        sizeof(presp->result->recvd_array[0]) * presp->result->array_size,
        pdwWSAError);
    if (ret == SOCKET_ERROR) return ret;

    ret = XmitData(XMIT_SEND, sock,
        (CHAR*)&presp->error,
        sizeof(presp->error),
        pdwWSAError);
    return ret;
}

INT ReceiveTestResponse(SOCKET sock, TestResponse* presp, DWORD* pdwWSAError)
{
    INT ret;
    DWORD array_size;
    ret = XmitData(XMIT_RECV, sock,
        (CHAR*)&array_size, sizeof(array_size), pdwWSAError);
    if (ret == SOCKET_ERROR) return ret;
    if (array_size > presp->result->array_size) { pdwWSAError = 0; return SOCKET_ERROR; }
    ret = XmitData(XMIT_RECV, sock,
        (CHAR*)presp->result->time_array, sizeof(presp->result->time_array[0]) * array_size,
        pdwWSAError);
    if (ret == SOCKET_ERROR) return ret;
    ret = XmitData(XMIT_RECV, sock,
        (CHAR*)presp->result->cpuutil_array, sizeof(presp->result->cpuutil_array[0]) * array_size,
        pdwWSAError);
    if (ret == SOCKET_ERROR) return ret;
    ret = XmitData(XMIT_RECV, sock,
        (CHAR*)presp->result->sent_array, sizeof(presp->result->sent_array[0]) * array_size,
        pdwWSAError);
    if (ret == SOCKET_ERROR) return ret;
    ret = XmitData(XMIT_RECV, sock,
        (CHAR*)presp->result->recvd_array, sizeof(presp->result->recvd_array[0]) * array_size,
        pdwWSAError);
    if (ret == SOCKET_ERROR) return ret;
    
    ret = XmitData(XMIT_RECV, sock,
        (CHAR*)&presp->error, sizeof(presp->error),
        pdwWSAError);
    return ret;
}
