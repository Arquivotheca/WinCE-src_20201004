//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 *      NK Kernel ppfs client code
 *
 *
 * Module Name:
 *
 *      ppfs.c
 *
 * Abstract:
 *
 *      This file implements the NK kernel ppfs client side interface
 *
 *
 */
#include "relfsd.h"
#include "cefs.h"


/* All strings must be <= 128 bytes including the null termination */

/* Message format: (assume little endians, lower address first>
 * -4      0       4     N-1       N       N+4
 * AA5555AA<header><data><checksum>5AA50A1A
 *
 * Header format:
 * 0       2       4
 * <opcode><length>
 * lsb          msb
 *
 * Length is 16 bit value. It includes all fields (== N)
 * Opcode is 16 bit value.  Current values are:
 * 0x0000 - boot (AA5555AA00000500FA5AA50A1A)
 * 0x0001 - init (AA5555AA01000500F95AA50A1A)
 * 0x0002 - open
 * 0x0003 - close
 * 0x0004 - read
 * 0x0005 - write
 * 0x0006 - seek
 * 0x0007 - delete
 * 0x0008 - findfirst
 * 0x0009 - findnext
 * 0x000A - rRegGet
 * 0x000B - rRegOpen
 * 0x000C - rRegClose
 * 0x000D - rRegEnum
 * 0x0010 - rcreatedir
 * 0x0011 - rremovedir
 * 0x0012 - rfindclose
 * 0x0013 - rgetfileattrib
 * 0x0014 - rsetfileattrib
 * 0x0015 - rmove
 * 0x0016 - rreadseek
 * 0x0017 - rgetdiskfree
 * 0x0018 - rgettime
 * 0x0019 - rsettime
 *
 *
 * Reply to a message will have the same opcode.  Bit15 is set to
 * indicate transmission failure.
 *
 * Data is stream of bytes
 *
 * Checksum is ~(sum of preceding N-1 bytes)
 *
 * Length of data is <length> - 5
 *
 */
 
#define KITL_SVC_PPSH 1

// If PPFS is running over ethernet, buffer up as much data as possible (up to
// KITL_MAX_DATA_SIZE) to send to the other side.  The PpfsEthBuf memory is set
// up through the KITLRegisterDfltClient call.
static BOOL BufferedPPFS = TRUE;
static UCHAR PpfsEthBuf[KITL_MAX_DATA_SIZE];

// Local state variables for handling translation of the packet based ethernet
// commands to the byte read write commands used by the ppfs engine.

static UCHAR *pWritePtr = PpfsEthBuf, *pReadPtr = PpfsEthBuf;

static DWORD dwRemainingBytes = 0;
static DWORD dwBytesWrittenToBuffer = 0;

// Routines for handling the buffering for PPFS over ether.
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#if 0
BOOL EdbgRegisterClient(UCHAR *pId, char *ServiceName, UCHAR Flags, UCHAR WindowSize, UCHAR *pBufferPool)
{
   return CallEdbgRegisterClient(pId, ServiceName, Flags, WindowSize, pBufferPool);
}
BOOL EdbgRegisterDfltClient(UCHAR Service, UCHAR Flags, UCHAR** ppTxBuffer, UCHAR** ppRxBuffer)
{
   return CallEdbgRegisterDfltClient(Service, Flags, ppTxBuffer, ppRxBuffer);
}
void EdbgSetDebug(DWORD ZoneMask)
{
   CallEdbgSetDebug(ZoneMask);
}
BOOL _cdecl EdbgDeregisterClient(UCHAR Id)
{
   return CallEdbgDeregisterClient(Id);
}
#endif

BOOL _cdecl EdbgSend(UCHAR Id, UCHAR *pUserData, DWORD dwUserDataLen)
{
   return CallEdbgSend(Id,pUserData,dwUserDataLen);
}
BOOL  _cdecl EdbgRecv(UCHAR Id, UCHAR *pRecvBuf, DWORD *pdwLen, DWORD Timeout)
{
   return CallEdbgRecv(Id, pRecvBuf, pdwLen, Timeout);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
fill_KITL_buffer()
{
    DWORD dwLen = KITL_MAX_DATA_SIZE;
    if (!EdbgRecv(KITL_SVC_PPSH, PpfsEthBuf,&dwLen, INFINITE)) {
        return FALSE;
    }
    dwRemainingBytes = dwLen;
    pReadPtr = PpfsEthBuf;
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
write_KITL_buffer()
{
    if (dwBytesWrittenToBuffer &&
        !EdbgSend(KITL_SVC_PPSH,PpfsEthBuf,dwBytesWrittenToBuffer)) {
        return FALSE;
    }
    dwBytesWrittenToBuffer = 0;
    pWritePtr = PpfsEthBuf;
    return TRUE;
}


// Functions for lpParallelPortxxxByteFunc, set up by calling
// SetKernelCommDev(KERNEL_SVC_PPSH,KERNEL_COMM_ETHER).


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
ppfs_send_byte_ether(
    BYTE ch
    )
{
    *pWritePtr++ = ch;
    if (++dwBytesWrittenToBuffer == KITL_MAX_DATA_SIZE)
        write_KITL_buffer();
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
ppfs_get_byte_ether()
{
    // Refill buffer if necessary and return next byte
    if (dwRemainingBytes == 0) 
         fill_KITL_buffer();
    dwRemainingBytes--;
    return *pReadPtr++;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
read_value(
    int *chksum
    ) 
{
    int loop;
    int result;
    BYTE ch;

    for (loop = 0; loop < sizeof(int); loop++ ) {
        ch = ppfs_get_byte_ether();
        *chksum += ch;
        ((LPBYTE)&result)[loop] = ch;
    }
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
write_value(
    int val,
    int *chksum
    ) 
{
    int loop;
    BYTE ch;

    for (loop = 0; loop < 4; loop++) {
        ch = ((LPBYTE)&val)[loop];
        *chksum += ch;
        ppfs_send_byte_ether(ch);
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
read_header(
    int *chksum
    ) 
{
    if (BufferedPPFS && !fill_KITL_buffer())
        return -1;
    if (read_value(chksum) != 0xaa5555aa)
        return -1;
    *chksum = 0;
    return read_value(chksum);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
write_header(
    int cmd,
    int *chksum
    ) 
{
    if (BufferedPPFS) {
        dwBytesWrittenToBuffer = 0;
        pWritePtr = PpfsEthBuf;
    }
    write_value(0xaa5555aa,chksum);
    *chksum = 0;
    write_value(cmd,chksum);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
read_data(
    LPBYTE buf,
    int cnt,
    int *chksum
    ) 
{
    BYTE ch;
    while (cnt--) {
        ch = ppfs_get_byte_ether();
        *chksum += ch;
        *buf++ = ch;
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
write_data(
    LPBYTE buf,
    int cnt,
    int *chksum
    ) 
{
    while (cnt--) {
        *chksum += *buf;
        ppfs_send_byte_ether(*buf++);
    }
}



//------------------------------------------------------------------------------
//  returns TRUE if success, FALSE if failure 
//------------------------------------------------------------------------------
BOOL 
read_end(
    int checksum
    ) 
{
    BYTE b;
    int tmp;
    b = ppfs_get_byte_ether();
    if (((checksum & 0xff) != b) ||
        (read_value(&tmp) != 0x1a0aa55a))
        return FALSE;
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
write_end(
    int checksum
    ) 
{
    int tmp;
    BYTE ch = ((checksum & 0xff) ^ 0xff);
    ppfs_send_byte_ether(ch);
    write_value(0x1a0aa55a,&tmp); /* Write out end of message signature */

    // If we're using ethernet, packet is all formatted and ready to go, send now
    if (BufferedPPFS)
        return write_KITL_buffer();
    return TRUE;        
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
PPSHConnect(void) 
{
    int chksum, result;

    write_header(0x00050001, &chksum); /* CMD_INIT */
    if (!write_end(chksum)) {
        return FALSE;
    }    
    if ((result = read_header(&chksum)) != 0x000A0001) {
    }
    result = read_value(&chksum);
    ppfs_get_byte_ether();
    if (!read_end(chksum)) {
    }
    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rlseeknocs(
    int fd,
    int off,
    int mode
    ) 
{
    int chksum, result;

    write_header(0x00110006,&chksum);   /* opcode = 0x0006, length = 17 */
    write_value(fd,&chksum);
    write_value(off,&chksum);
    write_value(mode,&chksum);
    write_end(chksum);
    if (read_header(&chksum) != 0x00090006) { /* opcode = 0x0006, length = 9 */
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rlseeknocs()\n"));
        return -1;
    }
    result = read_value(&chksum);
    if (!read_end(chksum)) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rlseeknocs()\n"));
        return -1;
    }
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rlseek(
    int fd,
    int off,
    int mode
    ) 
{
    int result;

    result = rlseeknocs(fd,off,mode);
    
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rwriteshort(
    int fd,
    char *buf,
    int cnt
    ) 
{
    int chksum, result;

    write_header(0x000d0005+(cnt<<16), &chksum); /* opcode = 0x0005, length = 13 + cnt */
    write_value(fd,&chksum);
    write_value(cnt,&chksum);
    write_data(buf,cnt,&chksum);
    write_end(chksum);
    if (read_header(&chksum) != 0x00090005) { /* opcode = 0x0005, length = 9        */
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rwriteshort()\n"));
        return -1;
    }
    result = read_value(&chksum);
    if (!read_end(chksum)) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rwriteshort()\n"));
        return -1;
    }
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rwrite(
    int fd,
    char *buf,
    int cnt
    ) 
{
    int csize, fullsize;
    int result, result2;
    char *buf2 = buf;
    if (!LockPages (buf, fullsize = cnt, 0, LOCKFLAG_READ))
        return -1;


    result2 = 0;
    while (cnt) {
        csize = ( cnt > 32*1024 ? 32*1024 : cnt);
        if ((result = rwriteshort(fd,buf2,csize)) == -1) {
            result2 = -1;
            break;
        }
        result2 += result;
        cnt -= csize;
        buf2 += csize;
    }
    
    UnlockPages(buf,fullsize);
    return result2;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rreadshort(
    int fd,
    char *buf,
    int cnt
    ) 
{
    int chksum, result, size;
    write_header(0x000d0004, &chksum); /* opcode = 0x0004, length = 13 */
    write_value(fd, &chksum);
    write_value(cnt,&chksum);
    write_end(chksum);
    result = read_header(&chksum);
    if ((result & 0xffff) != 0x0004) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rreadshort()\n"));
        return -1;
    }
    size = ((result >> 16) & 0xffff) - 9; /* subtract header & chksum */
    result = read_value(&chksum);
    read_data(buf,size,&chksum);
    if (!read_end(chksum)) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rreadshort()\n"));
        return -1;
    }
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rreadnocs(
    int fd,
    char *buf,
    int cnt
    ) 
{
    int csize, fullsize;
    int result, result2;
    char *buf2 = buf;

    fullsize = cnt;
    result2 = 0;
    while (cnt) {
        csize = ( cnt > 32*1024 ? 32*1024 : cnt);
        if ((result = rreadshort(fd,buf2,csize)) == -1) {
            result2 = -1;
            break;
        }
        result2 += result;
        cnt -= csize;
        buf2 += csize;
    }
    return result2;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rread(
    int fd,
    char *buf,
    int cnt
    ) 
{
    int result;
    if (!LockPages (buf, cnt, 0, LOCKFLAG_WRITE))
        return -1;


    result = rreadnocs(fd,buf,cnt);
    
    UnlockPages(buf,cnt);
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rreadseek(
    int fd,
    char *buf,
    int cnt,
    int pos
    ) 
{
    int chksum, result;
    if (!LockPages(buf,cnt,0,LOCKFLAG_WRITE))
        return 0;


    write_header(0x00110016,&chksum);   /* opcode = 0x0016, length = 17 */
    write_value(fd,&chksum);
    write_value(cnt,&chksum);
    write_value(pos,&chksum);
    write_end(chksum);
    if ((read_header(&chksum) & 0xffff) != 0x0016) { 
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rreadseek()\n"));
        return -1;
    }
    result = read_value(&chksum);
    read_data (buf, result, &chksum);
    
    if (!read_end(chksum)) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rreadseek()\n"));
        return -1;
    }


#if 0    
    ret = rlseeknocs(fd,pos,0);
    if (ret != pos)
        ret = 0;
    else
        ret = rreadnocs(fd,buf,cnt);
#endif  

    
    UnlockPages(buf,cnt);
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rclose(
    int fd
    ) 
{
    int chksum, result;

    write_header(0x00090003, &chksum); /* opcode = 0x0003, length = 9 */
    write_value(fd,&chksum);
    write_end(chksum);
    if (result = read_header(&chksum) != 0x00090003) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rclose()\n"));
        
        return -1;
    }
    result = read_value(&chksum);
    if (!read_end(chksum)) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rclose()\n"));
        
        return -1;
    }
    
    return result;
}

#define MAX_FILENAME_LEN 256




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
ropen(
    const WCHAR *name,
    int mode
    ) 
{

    int chksum, result, len;
    char fname[MAX_FILENAME_LEN];

    len = strlenW(name)+1;
    KUnicodeToAscii(fname,name,MAX_FILENAME_LEN);
    write_header(0x00090002 + (len<<16), &chksum); /* opcode = 0x0002, length = 9 + strlen + 1 */
    write_value(mode,&chksum);
    write_data(fname,len,&chksum);
    write_end(chksum);

    if (read_header(&chksum) != 0x00090002) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in ropen()\n"));
        
        return -1;
    }

    result = read_value(&chksum);
    if (!read_end(chksum)) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in ropen()\n"));
       
        return -1;
    }
    
    return result;
}

// Registration database access functions for kernel debug support initialization

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rRegOpen(
    DWORD hKey,
    CHAR *szName,
    LPDWORD lphKey
    ) 
{
    int chksum, len;

    len = strlen(szName)+1;

    write_header(0x0009000B + (len<<16), &chksum); /* opcode = 0x000B, length = 9 + strlen + 1 */
    write_value(hKey,&chksum);
    write_data(szName,len,&chksum);
    write_end(chksum);
    if (read_header(&chksum) != 0x0009000B) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rRegOpen()\n"));
        
        return -1;
    }
    *lphKey = read_value(&chksum);
    if (!read_end(chksum)) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rRegOpen()\n"));
        
        return -1;
    }
    
    return 0;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rRegClose(
    DWORD hKey
    ) 
{
    int chksum, result;

    write_header(0x0009000C, &chksum); /* opcode = 0x000C, length = 9 */
    write_value(hKey,&chksum);
    write_end(chksum);
    if (read_header(&chksum) != 0x0009000C) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rRegClose()\n"));
        
        return -1;
    }
    result = read_value(&chksum);
    if (!read_end(chksum)) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rRegClose()\n"));
        
        return -1;
    }
    
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rRegGet(
    DWORD hKey,
    CHAR *szName,
    LPDWORD lpdwType,
    LPBYTE lpbData,
    LPDWORD lpdwSize
    ) 
{
    int chksum, result, len;

    len = strlen(szName)+1;

    write_header(0x0009000A + (len<<16), &chksum); /* opcode = 0x000A, length = 9 + strlen + 1 */
    write_value(hKey,&chksum);
    write_data(szName,len,&chksum);
    write_end(chksum);
    result = read_header(&chksum);
    if ((result & 0xffff) != 0x000A)
    {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rRegGet()\n"));
        
        return 0;
    }       
    len = ((result >> 16) & 0xffff) - 9; /* subtract header & chksum */
    *lpdwType = read_value(&chksum);
    *lpdwSize = len;
    read_data(lpbData,len,&chksum);
    if (!read_end(chksum)) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rRegGet()\n"));
        
        return 0;
    }
    
    return 1;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rRegEnum(
    DWORD hKey,
    DWORD dwIndex,
    LPBYTE lpbData,
    LPDWORD lpdwSize
    ) 
{
    int chksum, result, len;

    write_header(0x000D000D, &chksum); /* opcode = 0x000A, length = 13 */
    write_value(hKey,&chksum);
    write_value(dwIndex,&chksum);
    write_end(chksum);
    result = read_header(&chksum);
    if ((result & 0xffff) != 0x000D)
    {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rRegEnum()\n"));
        
        return -1;
    }       
    len = ((result >> 16) & 0xffff) - 9; /* subtract header & chksum */
    result = read_value(&chksum);
    *lpdwSize = len;
    read_data(lpbData,len,&chksum);
    if (!read_end(chksum)) {
        DEBUGMSG (ZONE_ERROR, (L"ReleaseFSD: Error in rRegEnum()\n"));
        
        return -1;
    }
    
    return result;
}

// Routine for sending debug messages over PPSH.  Note the problem with the PPFS CS here -
// if we are called to put out a debug message while in a sys call, we can't block on the CS,
// but we can't just start sending data, because if someone is called into the loader, the
// other end will get confused (PPFS protocol can't handle packets within other packets).

#define MAX_DEBUGSTRING_LEN 512

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
rcreatedir(     
    const WCHAR* pwsPathName
    )
{
    int chksum, result, len;
    char fname[MAX_FILENAME_LEN];
    len = strlenW(pwsPathName)+1;
    KUnicodeToAscii(fname,pwsPathName,MAX_FILENAME_LEN);
  
    write_header(0x00050010 + (len<<16), &chksum); /* opcode = 0x0010, length = 5 + strlen + 1 */
    write_data(fname,len,&chksum);
    write_end(chksum);
    if (read_header(&chksum) != 0x00090010) {
        
        return -1;
    }
    result = read_value(&chksum);
    if (!read_end(chksum)) {
        
        return -1;
    }
    
    return result;

}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
rremovedir(     
    const WCHAR* pwsPathName
    )
{
    int chksum, result, len;
    char fname[MAX_FILENAME_LEN];
    len = strlenW(pwsPathName)+1;
    KUnicodeToAscii(fname,pwsPathName,MAX_FILENAME_LEN);

    write_header(0x00050011 + (len<<16), &chksum); /* opcode = 0x0011, length = 5 + strlen + 1 */
    write_data(fname,len,&chksum);
    write_end(chksum);
    if (read_header(&chksum) != 0x00090011) {
        
        return -1;
    }
    result = read_value(&chksum);
    if (!read_end(chksum)) {
        
        return -1;
    }
    
    return result;

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

int 
rfindfirst(     
    const WCHAR* pwsPathName,
    PWIN32_FIND_DATAW pfd
    )
{
    WIN32_FIND_DATAW_KITL temp_pfd;
    
    HANDLE hFind;
    int chksum, len, i;
    char fname[MAX_FILENAME_LEN];
    len = strlenW(pwsPathName)+1;
    KUnicodeToAscii(fname,pwsPathName,MAX_FILENAME_LEN);

    write_header(0x00050008 + (len<<16), &chksum); /* opcode = 0x0008, length = 5 + strlen + 1 */
    write_data(fname,len,&chksum);
    write_end(chksum);

    if (read_header(&chksum) != 0x02590008) {
        
        return -1;
    }
    
    hFind = (HANDLE) read_value(&chksum);

    if (pfd) {
        read_data ((LPBYTE)&temp_pfd, sizeof(WIN32_FIND_DATAW_KITL), &chksum);
        pfd->dwFileAttributes = temp_pfd.dwFileAttributes;
        pfd->ftCreationTime.dwLowDateTime = temp_pfd.ftCreationTime.dwLowDateTime;
        pfd->ftCreationTime.dwHighDateTime = temp_pfd.ftCreationTime.dwHighDateTime;
        pfd->ftLastAccessTime.dwLowDateTime = temp_pfd.ftLastAccessTime.dwLowDateTime;
        pfd->ftLastAccessTime.dwHighDateTime = temp_pfd.ftLastAccessTime.dwHighDateTime;
        pfd->ftLastWriteTime.dwLowDateTime = temp_pfd.ftLastWriteTime.dwLowDateTime;
        pfd->ftLastWriteTime.dwHighDateTime = temp_pfd.ftLastWriteTime.dwHighDateTime;
        pfd->nFileSizeHigh = temp_pfd.nFileSizeHigh;
        pfd->nFileSizeLow = temp_pfd.nFileSizeLow;
        for (i = 0; i < MAX_PATH; i++) {
            pfd->cFileName[i] = temp_pfd.cFileName[i];      
            if (temp_pfd.cFileName[i] == L'\0') break;
        }
    }
    
    if (!read_end(chksum)) {
        
        return -1;
    }

    
    return (int) hFind;

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
rfindnext(  
    SearchState *pSearch,
    PWIN32_FIND_DATAW pfd
    )
{
    DWORD dwSizeBuf, dwStrSize;
    DWORD SIZEOF_STATIC_DATA = 36;
    int chksum;
    
    // If there are no more bytes remaining and bFindDone, then there
    // are no more files left
    if (pSearch->dwFindBytesRemaining == 0 && pSearch->bFindDone) {
        pSearch->bFindDone = FALSE;
        return FALSE;
    }

    else if (pSearch->dwFindBytesRemaining == 0)
    {
    
        write_header(0x00090009, &chksum); /* opcode = 0x0009, length = 9 */
        // TO DO: Deal with multiple handles at a time
        write_value(pSearch->fs_Handle,&chksum);
        write_end(chksum);

        read_header(&chksum);      
        pSearch->bFindDone = !read_value(&chksum);
        
        // Get size of buffer
        dwSizeBuf = read_value(&chksum);
        dwSizeBuf -= sizeof(DWORD);

        // Allocate buffer and read whole buffer
        read_data (pSearch->abFindDataBuf, dwSizeBuf, &chksum);

        pSearch->pFindCurrent = pSearch->abFindDataBuf;
        pSearch->dwFindBytesRemaining = dwSizeBuf;

        read_end(chksum);
        
        
    }
    
    if (pSearch->dwFindBytesRemaining == 0) {
        pSearch->bFindDone = FALSE;
        return FALSE;
    }
    
    if (pfd) {

        // Copy WIN32_FIND_DATA fields starting from dwFileAttribues and ending
        // with nFileSizeLow
        memcpy ((BYTE *)pfd, (BYTE *)pSearch->pFindCurrent, SIZEOF_STATIC_DATA);
        pSearch->pFindCurrent += SIZEOF_STATIC_DATA;
        pSearch->dwFindBytesRemaining -= SIZEOF_STATIC_DATA;

        // Copy cFileName string
        wcscpy (pfd->cFileName, (WCHAR*)pSearch->pFindCurrent);
        dwStrSize = (wcslen (pfd->cFileName) + 1) * sizeof(WCHAR);
        pSearch->pFindCurrent += dwStrSize;
        pSearch->dwFindBytesRemaining -= dwStrSize;  
    }
    
    return TRUE;

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rfindclose(
    int fd
    ) 
{
    int chksum, result;

    write_header(0x00090012, &chksum); /* opcode = 0x0012, length = 9 */
    write_value(fd,&chksum);
    write_end(chksum);
    if (read_header(&chksum) != 0x00090012) {
        
        return -1;
    }
    result = read_value(&chksum);
    if (!read_end(chksum)) {
        
        return -1;
    }
    
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
rdelete(    
    const WCHAR* pwsPathName
    )
{
    int chksum, result, len;
    char fname[MAX_FILENAME_LEN];
    len = strlenW(pwsPathName)+1;
    KUnicodeToAscii(fname,pwsPathName,MAX_FILENAME_LEN);

    write_header(0x00050007 + (len<<16), &chksum); /* opcode = 0x0007, length = 5 + strlen + 1 */
    write_data(fname,len,&chksum);
    write_end(chksum);

    if (read_header(&chksum) != 0x00090007) {
        
        return -1;
    }
    
    result = read_value(&chksum);
    if (!read_end(chksum)) {
        
        return -1;
    }
    
    return result;

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rgetfileattrib(     
    const WCHAR* pwsFileName
    )
{
    int chksum, result, len;
    char fname[MAX_FILENAME_LEN];
    len = strlenW(pwsFileName)+1;
    KUnicodeToAscii(fname,pwsFileName,MAX_FILENAME_LEN);

    write_header(0x00050013 + (len<<16), &chksum); /* opcode = 0x0013, length = 5 + strlen + 1 */
    write_data(fname,len,&chksum);
    write_end(chksum);

    if (read_header(&chksum) != 0x00090013) {
        
        return -1;
    }
    
    result = read_value(&chksum);
    if (!read_end(chksum)) {
        
        return -1;
    }
    
    return result;

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
rsetfileattrib(     
    const WCHAR* pwsFileName,
    int dwAttributes
    )
{
    int chksum, result, len;
    char fname[MAX_FILENAME_LEN];
    len = strlenW(pwsFileName)+1;
    KUnicodeToAscii(fname,pwsFileName,MAX_FILENAME_LEN);

    write_header(0x00090014 + (len<<16), &chksum); /* opcode = 0x0014, length = 9 + strlen + 1 */
    write_value (dwAttributes, &chksum);
    write_data(fname,len,&chksum);
    write_end(chksum);

    if (read_header(&chksum) != 0x00090014) {
        
        return -1;
    }
    
    result = read_value(&chksum);
    if (!read_end(chksum)) {
        
        return -1;
    }
    
    return result;

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rgettime(   
    int fd, 
    PFILETIME pCreation, 
    PFILETIME pLastAccess, 
    PFILETIME pLastWrite )
{
    int chksum, result, tempLow, tempHigh;

    write_header(0x00090018, &chksum); /* opcode = 0x0018, length = 9 */
    write_value (fd, &chksum);
    write_end(chksum);

    if (read_header(&chksum) != 0x00210018) {
        
        return -1;
    }
    
    result = read_value(&chksum);

    tempLow = read_value(&chksum);
    tempHigh = read_value(&chksum);
    if (pCreation) {
        pCreation->dwLowDateTime = tempLow;
        pCreation->dwHighDateTime = tempHigh;
    }

    tempLow = read_value(&chksum);
    tempHigh = read_value(&chksum);
    if (pLastAccess) {
        pLastAccess->dwLowDateTime = tempLow;
        pLastAccess->dwHighDateTime = tempHigh;
    }

    tempLow = read_value(&chksum);
    tempHigh = read_value(&chksum);
    if (pLastWrite) {
        pLastWrite->dwLowDateTime = tempLow;
        pLastWrite->dwHighDateTime = tempHigh;
    }  

    if (!read_end(chksum)) {
        
        return -1;
    }
    
    return result;

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
rsettime(   
    int fd, 
    PFILETIME pCreation, 
    PFILETIME pLastAccess, 
    PFILETIME pLastWrite 
    )
{
    int chksum, result;

    write_header(0x00210019, &chksum); /* opcode = 0x0019, length = 0x21 */
    write_value (fd, &chksum);

    if (pCreation) {
        write_value (pCreation->dwLowDateTime, &chksum);
        write_value (pCreation->dwHighDateTime, &chksum);
    } else {
        write_value (0, &chksum);
        write_value (0, &chksum);
    }

    if (pLastAccess) {
        write_value (pLastAccess->dwLowDateTime, &chksum);
        write_value (pLastAccess->dwHighDateTime, &chksum);
    } else {
        write_value (0, &chksum);
        write_value (0, &chksum);
    }

    if (pLastWrite) {
        write_value (pLastWrite->dwLowDateTime, &chksum);
        write_value (pLastWrite->dwHighDateTime, &chksum);
    } else {
        write_value (0, &chksum);
        write_value (0, &chksum);
    }

    write_end(chksum);

    if (read_header(&chksum) != 0x00090019) {
        
        return -1;
    }
    
    result = read_value(&chksum);
  
    if (!read_end(chksum)) {
        
        return -1;
    }
    
    return result;

}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
rmove(  
    const WCHAR* pwsOldFileName,
    const WCHAR* pwsNewFileName
    )
{
    int chksum, result, oldlen, newlen;
    char fOldName[MAX_FILENAME_LEN];
    char fNewName[MAX_FILENAME_LEN];
    oldlen = strlenW(pwsOldFileName)+1;
    KUnicodeToAscii(fOldName,pwsOldFileName,MAX_FILENAME_LEN);
    newlen = strlenW(pwsNewFileName)+1;
    KUnicodeToAscii(fNewName,pwsNewFileName,MAX_FILENAME_LEN);
   
    /* opcode = 0x0015, length = 5 + oldstrlen+1 + newstrlen+1*/
    write_header(0x00050015 + ((oldlen+newlen)<<16), &chksum); 
    write_data(fOldName,oldlen,&chksum);
    write_data(fNewName,newlen,&chksum);
    write_end(chksum);

    if (read_header(&chksum) != 0x00090015) {
        
        return -1;
    }
    
    result = read_value(&chksum);
    if (!read_end(chksum)) {
        
        return -1;
    }
    
    return result;

}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
rgetdiskfree(   
    PCWSTR pwsPathName,
    PDWORD pSectorsPerCluster,
    PDWORD pBytesPerSector,
    PDWORD pFreeClusters,
    PDWORD pClusters
    )
{
    int chksum, result, len;
    char fname[MAX_FILENAME_LEN];
    len = strlenW(pwsPathName)+1;
    KUnicodeToAscii(fname,pwsPathName,MAX_FILENAME_LEN);

    write_header(0x00050017 + (len<<16), &chksum); /* opcode = 0x0017, length = 5 + strlen + 1 */
    write_data(fname,len,&chksum);
    write_end(chksum);

    if (read_header(&chksum) != 0x00190017) {
        
        return -1;
    }

    result = read_value(&chksum);

    if (result) {
        *pSectorsPerCluster = read_value(&chksum);
        *pBytesPerSector = read_value(&chksum);
        *pFreeClusters = read_value(&chksum);
        *pClusters = read_value(&chksum);
    }
    
    if (!read_end(chksum)) {
        
        return -1;
    }
    
    return result;

}

// Routine for sending debug messages over PPSH.  Note the problem with the PPFS CS here -
// if we are called to put out a debug message while in a sys call, we can't block on the CS,
// but we can't just start sending data, because if someone is called into the loader, the
// other end will get confused (PPFS protocol can't handle packets within other packets).

#define MAX_DEBUGSTRING_LEN 512

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PpfsWriteDebugString(
    unsigned short *name
    ) {
    int chksum, len;
    char fname[MAX_DEBUGSTRING_LEN];

    len = strlenW(name)+1;
    KUnicodeToAscii(fname,name,MAX_DEBUGSTRING_LEN);   
    write_header(0x0009000f + (len<<16), &chksum); /* opcode = 0x000f, length = 9 + strlen + 1 */
    write_value(len,&chksum);
    write_data(fname,len,&chksum);
    write_end(chksum);
        
    return;
}

void KUnicodeToAscii(LPCHAR chptr, LPCWSTR wchptr, int maxlen) {
    while ((maxlen-- > 1) && *wchptr) {
        *chptr++ = (CHAR)*wchptr++;
    }
    *chptr = 0;
}

unsigned int strlenW(const wchar_t *wcs)
{
    return wcslen (wcs);
}
