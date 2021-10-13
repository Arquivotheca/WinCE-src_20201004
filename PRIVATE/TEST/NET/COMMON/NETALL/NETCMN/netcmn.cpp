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

/*
      @doc LIBRARY

Module Name:

  netcmn.cpp

 @module NetCommon - Contains generic helper functions. |

  This file is meant to contain a variety of generic helper functions to aid the
  networking tests with mundane or oft repeated tasks.

   <nl>Link:    netcmn.lib
   <nl>Include: netcmn.h

*/

#include "netcmn.h"


////////////////////////////////////////////////////////////////////////////////////////
//				Functions for registering WATT variables
////////////////////////////////////////////////////////////////////////////////////////

LPTSTR g_ptszFilterVars[MAX_FILTER_VARS];

void RegisterWattFilter(LPTSTR ptszFilter)
{
	TCHAR *p;
	DWORD i;

	for(i = 0; i < MAX_FILTER_VARS; i++)
		g_ptszFilterVars[i] = NULL;

	// Parse through the registered string and change ',' chars to NULLs
	// Simultaneously setup the array of filter variable string pointers
	if(ptszFilter)
	{
		i = 0;
		g_ptszFilterVars[i] = ptszFilter;
		for(p = ptszFilter; *p != _T('\0'); p++)
		{
			if(*p == _T(','))
			{
				*p = _T('\0');	// Change comma to NULL char
				// If this isn't the end of the string, set next pointer
				if(*(p + 1) != ('\0'))
					g_ptszFilterVars[++i] = p + 1;
			}
		}
	}
}

void PrintWattVar(LPCTSTR ptszVarName, LPCTSTR ptszVarValue)
{
	TCHAR tszString[MAX_PATH];
	DWORD i;
	
	// Check to see if we're supposed to print this variable
	if(g_ptszFilterVars[0])
	{
		for(i = 0; i < MAX_FILTER_VARS; i++)
		{
			if(g_ptszFilterVars[i] && _tcsicmp(g_ptszFilterVars[i], ptszVarName) == 0)
				break;	// Match found
		}

		if(i == MAX_FILTER_VARS)
			return;		// Not found in filter list, so don't print it
	}

	_stprintf(tszString, _T("SET_WATT_VAR %s=\"%s\"\n"), ptszVarName, ptszVarValue);
	OutputDebugString(tszString);
}

////////////////////////////////////////////////////////////////////////////////////////
//				Random Number Function - NT/CE
////////////////////////////////////////////////////////////////////////////////////////
#ifndef UNDER_CE
// For random number generation
#include <stdlib.h>
#include <time.h>
#endif

BOOL bRandSeeded = FALSE;

DWORD GetRandomNumber(DWORD dwRandMax)
{
#define NT_RAND_BIT_MASK	0x000000FF

	if(dwRandMax == 0)
		return 0;

#ifdef UNDER_CE
	if(dwRandMax == 0xffffffff)
		return Random();
	else
		return( Random() % (dwRandMax + 1) );
#else
	DWORD dwRet = 0;

	if(!bRandSeeded)
	{
		srand( (unsigned)time( NULL ) );
		bRandSeeded = TRUE;
	}

	// maximum rand() is 0x7fff, here we make it 0xffffffff
	for(int i = 3; i >= 0; i--)
	{
		dwRet |= ((rand() & NT_RAND_BIT_MASK) << (8 * i));
	}

	if(dwRandMax == 0xffffffff)
		return( dwRet );
	else
		return( dwRet % (dwRandMax + 1) );
#endif
}

DWORD GetRandomRange(DWORD dwRandMin, DWORD dwRandMax)
{
	if(dwRandMax < dwRandMin)
		return 0;

	return( GetRandomNumber(dwRandMax - dwRandMin) + dwRandMin );
}

////////////////////////////////////////////////////////////////////////////////////////
//				Error Code Translation
////////////////////////////////////////////////////////////////////////////////////////

TCHAR* GetLastErrorText()
{
	return ErrorToText(GetLastError());
}

TCHAR* ErrorToText(DWORD dwErrorCode)
{
	switch(dwErrorCode)
	{
	case 0:
		return(TEXT("No Error"));
	case ERROR_INVALID_FUNCTION:
		return(TEXT("ERROR_INVALID_FUNCTION"));
	case ERROR_FILE_NOT_FOUND:
		return(TEXT("ERROR_FILE_NOT_FOUND"));
	case ERROR_PATH_NOT_FOUND:
		return(TEXT("ERROR_PATH_NOT_FOUND"));
	case ERROR_TOO_MANY_OPEN_FILES:
		return(TEXT("ERROR_TOO_MANY_OPEN_FILES"));
	case ERROR_ACCESS_DENIED:
		return(TEXT("ERROR_ACCESS_DENIED"));
	case ERROR_INVALID_HANDLE:
		return(TEXT("ERROR_INVALID_HANDLE"));
	case ERROR_ARENA_TRASHED:
		return(TEXT("ERROR_ARENA_TRASHED"));
	case ERROR_NOT_ENOUGH_MEMORY:
		return(TEXT("ERROR_NOT_ENOUGH_MEMORY"));
	case ERROR_INVALID_BLOCK:
		return(TEXT("ERROR_INVALID_BLOCK"));
	case ERROR_BAD_ENVIRONMENT:
		return(TEXT("ERROR_BAD_ENVIRONMENT"));
	case ERROR_BAD_FORMAT:
		return(TEXT("ERROR_BAD_FORMAT"));
	case ERROR_INVALID_ACCESS:
		return(TEXT("ERROR_INVALID_ACCESS"));
	case ERROR_INVALID_DATA:
		return(TEXT("ERROR_INVALID_DATA"));
	case ERROR_OUTOFMEMORY:
		return(TEXT("ERROR_OUTOFMEMORY"));
	case ERROR_INVALID_DRIVE:
		return(TEXT("ERROR_INVALID_DRIVE"));
	case ERROR_CURRENT_DIRECTORY :
		return(TEXT("ERROR_CURRENT_DIRECTORY "));
	case ERROR_NOT_SAME_DEVICE:
		return(TEXT("ERROR_NOT_SAME_DEVICE"));
	case ERROR_NO_MORE_FILES:
		return(TEXT("ERROR_NO_MORE_FILES"));
	case ERROR_WRITE_PROTECT:
		return(TEXT("ERROR_WRITE_PROTECT"));
	case ERROR_BAD_UNIT:
		return(TEXT("ERROR_BAD_UNIT"));
	case ERROR_NOT_READY:
		return(TEXT("ERROR_NOT_READY"));
	case ERROR_BAD_COMMAND:
		return(TEXT("ERROR_BAD_COMMAND"));
	case ERROR_CRC:
		return(TEXT("ERROR_CRC"));
	case ERROR_BAD_LENGTH:
		return(TEXT("ERROR_BAD_LENGTH"));
	case ERROR_SEEK:
		return(TEXT("ERROR_SEEK"));
	case ERROR_NOT_DOS_DISK:
		return(TEXT("ERROR_NOT_DOS_DISK"));
	case ERROR_SECTOR_NOT_FOUND:
		return(TEXT("ERROR_SECTOR_NOT_FOUND"));
	case ERROR_OUT_OF_PAPER:
		return(TEXT("ERROR_OUT_OF_PAPER"));
	case ERROR_WRITE_FAULT:
		return(TEXT("ERROR_WRITE_FAULT"));
	case ERROR_READ_FAULT:
		return(TEXT("ERROR_READ_FAULT"));
	case ERROR_GEN_FAILURE:
		return(TEXT("ERROR_GEN_FAILURE"));
	case ERROR_SHARING_VIOLATION:
		return(TEXT("ERROR_SHARING_VIOLATION"));
	case ERROR_LOCK_VIOLATION:
		return(TEXT("ERROR_LOCK_VIOLATION"));
	case ERROR_WRONG_DISK:
		return(TEXT("ERROR_WRONG_DISK"));
	case ERROR_SHARING_BUFFER_EXCEEDED:
		return(TEXT("ERROR_SHARING_BUFFER_EXCEEDED"));
	case ERROR_HANDLE_EOF:
		return(TEXT("ERROR_HANDLE_EOF"));
	case ERROR_HANDLE_DISK_FULL:
		return(TEXT("ERROR_HANDLE_DISK_FULL"));
	case ERROR_NOT_SUPPORTED:
		return(TEXT("ERROR_NOT_SUPPORTED"));
	case ERROR_REM_NOT_LIST:
		return(TEXT("ERROR_REM_NOT_LIST"));
	case ERROR_DUP_NAME:
		return(TEXT("ERROR_DUP_NAME"));
	case ERROR_BAD_NETPATH:
		return(TEXT("ERROR_BAD_NETPATH"));
	case ERROR_NETWORK_BUSY:
		return(TEXT("ERROR_NETWORK_BUSY"));
	case ERROR_DEV_NOT_EXIST:
		return(TEXT("ERROR_DEV_NOT_EXIST"));
	case ERROR_TOO_MANY_CMDS:
		return(TEXT("ERROR_TOO_MANY_CMDS"));
	case ERROR_ADAP_HDW_ERR:
		return(TEXT("ERROR_ADAP_HDW_ERR"));
	case ERROR_BAD_NET_RESP:
		return(TEXT("ERROR_BAD_NET_RESP"));
	case ERROR_UNEXP_NET_ERR:
		return(TEXT("ERROR_UNEXP_NET_ERR"));
	case ERROR_BAD_REM_ADAP:
		return(TEXT("ERROR_BAD_REM_ADAP"));
	case ERROR_PRINTQ_FULL:
		return(TEXT("ERROR_PRINTQ_FULL"));
	case ERROR_NO_SPOOL_SPACE:
		return(TEXT("ERROR_NO_SPOOL_SPACE"));
	case ERROR_PRINT_CANCELLED:
		return(TEXT("ERROR_PRINT_CANCELLED"));
	case ERROR_NETNAME_DELETED:
		return(TEXT("ERROR_NETNAME_DELETED"));
	case ERROR_NETWORK_ACCESS_DENIED:
		return(TEXT("ERROR_NETWORK_ACCESS_DENIED"));
	case ERROR_BAD_DEV_TYPE:
		return(TEXT("ERROR_BAD_DEV_TYPE"));
	case ERROR_BAD_NET_NAME:
		return(TEXT("ERROR_BAD_NET_NAME"));
	case ERROR_TOO_MANY_NAMES:
		return(TEXT("ERROR_TOO_MANY_NAMES"));
	case ERROR_TOO_MANY_SESS:
		return(TEXT("ERROR_TOO_MANY_SESS"));
	case ERROR_SHARING_PAUSED:
		return(TEXT("ERROR_SHARING_PAUSED"));
	case ERROR_REQ_NOT_ACCEP:
		return(TEXT("ERROR_REQ_NOT_ACCEP"));
	case ERROR_REDIR_PAUSED:
		return(TEXT("ERROR_REDIR_PAUSED"));
	case ERROR_FILE_EXISTS:
		return(TEXT("ERROR_FILE_EXISTS"));
	case ERROR_CANNOT_MAKE:
		return(TEXT("ERROR_CANNOT_MAKE"));
	case ERROR_FAIL_I24:
		return(TEXT("ERROR_FAIL_I24"));
	case ERROR_OUT_OF_STRUCTURES:
		return(TEXT("ERROR_OUT_OF_STRUCTURES"));
	case ERROR_ALREADY_ASSIGNED:
		return(TEXT("ERROR_ALREADY_ASSIGNED"));
	case ERROR_INVALID_PASSWORD:
		return(TEXT("ERROR_INVALID_PASSWORD"));
	case ERROR_INVALID_PARAMETER:
		return(TEXT("ERROR_INVALID_PARAMETER"));
	case ERROR_NET_WRITE_FAULT:
		return(TEXT("ERROR_NET_WRITE_FAULT"));
	case ERROR_NO_PROC_SLOTS:
		return(TEXT("ERROR_NO_PROC_SLOTS"));
	case ERROR_TOO_MANY_SEMAPHORES:
		return(TEXT("ERROR_TOO_MANY_SEMAPHORES"));
	case ERROR_EXCL_SEM_ALREADY_OWNED:
		return(TEXT("ERROR_EXCL_SEM_ALREADY_OWNED"));
	case ERROR_SEM_IS_SET:
		return(TEXT("ERROR_SEM_IS_SET"));
	case ERROR_TOO_MANY_SEM_REQUESTS:
		return(TEXT("ERROR_TOO_MANY_SEM_REQUESTS"));
	case ERROR_INVALID_AT_INTERRUPT_TIME:
		return(TEXT("ERROR_INVALID_AT_INTERRUPT_TIME"));
	case ERROR_SEM_OWNER_DIED:
		return(TEXT("ERROR_SEM_OWNER_DIED"));
	case ERROR_SEM_USER_LIMIT:
		return(TEXT("ERROR_SEM_USER_LIMIT"));
	case ERROR_DISK_CHANGE:
		return(TEXT("ERROR_DISK_CHANGE"));
	case ERROR_DRIVE_LOCKED:
		return(TEXT("ERROR_DRIVE_LOCKED"));
	case ERROR_BROKEN_PIPE:
		return(TEXT("ERROR_BROKEN_PIPE"));
	case ERROR_OPEN_FAILED:
		return(TEXT("ERROR_OPEN_FAILED"));
	case ERROR_BUFFER_OVERFLOW:
		return(TEXT("ERROR_BUFFER_OVERFLOW"));
	case ERROR_DISK_FULL:
		return(TEXT("ERROR_DISK_FULL"));
	case ERROR_NO_MORE_SEARCH_HANDLES:
		return(TEXT("ERROR_NO_MORE_SEARCH_HANDLES"));
	case ERROR_INVALID_TARGET_HANDLE:
		return(TEXT("ERROR_INVALID_TARGET_HANDLE"));
	case ERROR_INVALID_CATEGORY:
		return(TEXT("ERROR_INVALID_CATEGORY"));
	case ERROR_INVALID_VERIFY_SWITCH:
		return(TEXT("ERROR_INVALID_VERIFY_SWITCH"));
	case ERROR_BAD_DRIVER_LEVEL:
		return(TEXT("ERROR_BAD_DRIVER_LEVEL"));
	case ERROR_CALL_NOT_IMPLEMENTED:
		return(TEXT("ERROR_CALL_NOT_IMPLEMENTED"));
	case ERROR_SEM_TIMEOUT:
		return(TEXT("ERROR_SEM_TIMEOUT"));
	case ERROR_INSUFFICIENT_BUFFER:
		return(TEXT("ERROR_INSUFFICIENT_BUFFER"));
	case ERROR_INVALID_NAME:
		return(TEXT("ERROR_INVALID_NAME"));
	case ERROR_INVALID_LEVEL:
		return(TEXT("ERROR_INVALID_LEVEL"));
	case ERROR_NO_VOLUME_LABEL:
		return(TEXT("ERROR_NO_VOLUME_LABEL"));
	case ERROR_MOD_NOT_FOUND:
		return(TEXT("ERROR_MOD_NOT_FOUND"));
	case ERROR_PROC_NOT_FOUND:
		return(TEXT("ERROR_PROC_NOT_FOUND"));
	case ERROR_WAIT_NO_CHILDREN:
		return(TEXT("ERROR_WAIT_NO_CHILDREN"));
	case ERROR_CHILD_NOT_COMPLETE:
		return(TEXT("ERROR_CHILD_NOT_COMPLETE"));
	case ERROR_DIRECT_ACCESS_HANDLE:
		return(TEXT("ERROR_DIRECT_ACCESS_HANDLE"));
	case ERROR_NEGATIVE_SEEK:
		return(TEXT("ERROR_NEGATIVE_SEEK"));
	case ERROR_SEEK_ON_DEVICE:
		return(TEXT("ERROR_SEEK_ON_DEVICE"));
	case ERROR_IS_JOIN_TARGET:
		return(TEXT("ERROR_IS_JOIN_TARGET"));
	case ERROR_IS_JOINED:
		return(TEXT("ERROR_IS_JOINED"));
	case ERROR_IS_SUBSTED:
		return(TEXT("ERROR_IS_SUBSTED"));
	case ERROR_NOT_JOINED:
		return(TEXT("ERROR_NOT_JOINED"));
	case ERROR_NOT_SUBSTED:
		return(TEXT("ERROR_NOT_SUBSTED"));
	case ERROR_JOIN_TO_JOIN:
		return(TEXT("ERROR_JOIN_TO_JOIN"));
	case ERROR_SUBST_TO_SUBST:
		return(TEXT("ERROR_SUBST_TO_SUBST"));
	case ERROR_JOIN_TO_SUBST:
		return(TEXT("ERROR_JOIN_TO_SUBST"));
	case ERROR_SUBST_TO_JOIN:
		return(TEXT("ERROR_SUBST_TO_JOIN"));
	case ERROR_BUSY_DRIVE:
		return(TEXT("ERROR_BUSY_DRIVE"));
	case ERROR_SAME_DRIVE:
		return(TEXT("ERROR_SAME_DRIVE"));
	case ERROR_DIR_NOT_ROOT:
		return(TEXT("ERROR_DIR_NOT_ROOT"));
	case ERROR_DIR_NOT_EMPTY:
		return(TEXT("ERROR_DIR_NOT_EMPTY"));
	case ERROR_IS_SUBST_PATH:
		return(TEXT("ERROR_IS_SUBST_PATH"));
	case ERROR_IS_JOIN_PATH:
		return(TEXT("ERROR_IS_JOIN_PATH"));
	case ERROR_PATH_BUSY:
		return(TEXT("ERROR_PATH_BUSY"));
	case ERROR_IS_SUBST_TARGET:
		return(TEXT("ERROR_IS_SUBST_TARGET"));
	case ERROR_SYSTEM_TRACE:
		return(TEXT("ERROR_SYSTEM_TRACE"));
	case ERROR_INVALID_EVENT_COUNT:
		return(TEXT("ERROR_INVALID_EVENT_COUNT"));
	case ERROR_TOO_MANY_MUXWAITERS:
		return(TEXT("ERROR_TOO_MANY_MUXWAITERS"));
	case ERROR_INVALID_LIST_FORMAT:
		return(TEXT("ERROR_INVALID_LIST_FORMAT"));
	case ERROR_LABEL_TOO_LONG:
		return(TEXT("ERROR_LABEL_TOO_LONG"));
	case ERROR_TOO_MANY_TCBS:
		return(TEXT("ERROR_TOO_MANY_TCBS"));
	case ERROR_SIGNAL_REFUSED:
		return(TEXT("ERROR_SIGNAL_REFUSED"));
	case ERROR_DISCARDED:
		return(TEXT("ERROR_DISCARDED"));
	case ERROR_NOT_LOCKED:
		return(TEXT("ERROR_NOT_LOCKED"));
	case ERROR_BAD_THREADID_ADDR:
		return(TEXT("ERROR_BAD_THREADID_ADDR"));
	case ERROR_BAD_ARGUMENTS:
		return(TEXT("ERROR_BAD_ARGUMENTS"));
	case ERROR_BAD_PATHNAME:
		return(TEXT("ERROR_BAD_PATHNAME"));
	case ERROR_SIGNAL_PENDING:
		return(TEXT("ERROR_SIGNAL_PENDING"));
	case ERROR_MAX_THRDS_REACHED:
		return(TEXT("ERROR_MAX_THRDS_REACHED"));
	case ERROR_LOCK_FAILED:
		return(TEXT("ERROR_LOCK_FAILED"));
	case ERROR_BUSY:
		return(TEXT("ERROR_BUSY"));
	case ERROR_CANCEL_VIOLATION:
		return(TEXT("ERROR_CANCEL_VIOLATION"));
	case ERROR_ATOMIC_LOCKS_NOT_SUPPORTED:
		return(TEXT("ERROR_ATOMIC_LOCKS_NOT_SUPPORTED"));
	case ERROR_INVALID_SEGMENT_NUMBER:
		return(TEXT("ERROR_INVALID_SEGMENT_NUMBER"));
	case ERROR_INVALID_ORDINAL:
		return(TEXT("ERROR_INVALID_ORDINAL"));
	case ERROR_ALREADY_EXISTS:
		return(TEXT("ERROR_ALREADY_EXISTS"));
	case ERROR_INVALID_FLAG_NUMBER:
		return(TEXT("ERROR_INVALID_FLAG_NUMBER"));
	case ERROR_SEM_NOT_FOUND:
		return(TEXT("ERROR_SEM_NOT_FOUND"));
	case ERROR_INVALID_STARTING_CODESEG:
		return(TEXT("ERROR_INVALID_STARTING_CODESEG"));
	case ERROR_INVALID_STACKSEG:
		return(TEXT("ERROR_INVALID_STACKSEG"));
	case ERROR_INVALID_MODULETYPE:
		return(TEXT("ERROR_INVALID_MODULETYPE"));
	case ERROR_INVALID_EXE_SIGNATURE:
		return(TEXT("ERROR_INVALID_EXE_SIGNATURE"));
	case ERROR_EXE_MARKED_INVALID:
		return(TEXT("ERROR_EXE_MARKED_INVALID"));
	case ERROR_BAD_EXE_FORMAT:
		return(TEXT("ERROR_BAD_EXE_FORMAT"));
	case ERROR_ITERATED_DATA_EXCEEDS_64k:
		return(TEXT("ERROR_ITERATED_DATA_EXCEEDS_64k"));
	case ERROR_INVALID_MINALLOCSIZE:
		return(TEXT("ERROR_INVALID_MINALLOCSIZE"));
	case ERROR_DYNLINK_FROM_INVALID_RING:
		return(TEXT("ERROR_DYNLINK_FROM_INVALID_RING"));
	case ERROR_IOPL_NOT_ENABLED:
		return(TEXT("ERROR_IOPL_NOT_ENABLED"));
	case ERROR_INVALID_SEGDPL:
		return(TEXT("ERROR_INVALID_SEGDPL"));
	case ERROR_AUTODATASEG_EXCEEDS_64k:
		return(TEXT("ERROR_AUTODATASEG_EXCEEDS_64k"));
	case ERROR_RING2SEG_MUST_BE_MOVABLE:
		return(TEXT("ERROR_RING2SEG_MUST_BE_MOVABLE"));
	case ERROR_RELOC_CHAIN_XEEDS_SEGLIM:
		return(TEXT("ERROR_RELOC_CHAIN_XEEDS_SEGLIM"));
	case ERROR_INFLOOP_IN_RELOC_CHAIN:
		return(TEXT("ERROR_INFLOOP_IN_RELOC_CHAIN"));
	case ERROR_ENVVAR_NOT_FOUND:
		return(TEXT("ERROR_ENVVAR_NOT_FOUND"));
	case ERROR_NO_SIGNAL_SENT:
		return(TEXT("ERROR_NO_SIGNAL_SENT"));
	case ERROR_FILENAME_EXCED_RANGE:
		return(TEXT("ERROR_FILENAME_EXCED_RANGE"));
	case ERROR_RING2_STACK_IN_USE:
		return(TEXT("ERROR_RING2_STACK_IN_USE"));
	case ERROR_META_EXPANSION_TOO_LONG:
		return(TEXT("ERROR_META_EXPANSION_TOO_LONG"));
	case ERROR_INVALID_SIGNAL_NUMBER:
		return(TEXT("ERROR_INVALID_SIGNAL_NUMBER"));
	case ERROR_THREAD_1_INACTIVE:
		return(TEXT("ERROR_THREAD_1_INACTIVE"));
	case ERROR_LOCKED:
		return(TEXT("ERROR_LOCKED"));
	case ERROR_TOO_MANY_MODULES:
		return(TEXT("ERROR_TOO_MANY_MODULES"));
	case ERROR_NESTING_NOT_ALLOWED:
		return(TEXT("ERROR_NESTING_NOT_ALLOWED"));
	case ERROR_EXE_MACHINE_TYPE_MISMATCH:
		return(TEXT("ERROR_EXE_MACHINE_TYPE_MISMATCH"));
	case ERROR_BAD_PIPE:
		return(TEXT("ERROR_BAD_PIPE"));
	case ERROR_PIPE_BUSY:
		return(TEXT("ERROR_PIPE_BUSY"));
	case ERROR_NO_DATA:
		return(TEXT("ERROR_NO_DATA"));
	case ERROR_PIPE_NOT_CONNECTED:
		return(TEXT("ERROR_PIPE_NOT_CONNECTED"));
	case ERROR_MORE_DATA:
		return(TEXT("ERROR_MORE_DATA"));
	case ERROR_VC_DISCONNECTED:
		return(TEXT("ERROR_VC_DISCONNECTED"));
	case ERROR_INVALID_EA_NAME:
		return(TEXT("ERROR_INVALID_EA_NAME"));
	case ERROR_EA_LIST_INCONSISTENT:
		return(TEXT("ERROR_EA_LIST_INCONSISTENT"));
	case WAIT_TIMEOUT:
		return(TEXT("WAIT_TIMEOUT"));
	case ERROR_NO_MORE_ITEMS:
		return(TEXT("ERROR_NO_MORE_ITEMS"));
	case ERROR_CANNOT_COPY:
		return(TEXT("ERROR_CANNOT_COPY"));
	case ERROR_DIRECTORY:
		return(TEXT("ERROR_DIRECTORY"));
	case ERROR_EAS_DIDNT_FIT:
		return(TEXT("ERROR_EAS_DIDNT_FIT"));
	case ERROR_EA_FILE_CORRUPT:
		return(TEXT("ERROR_EA_FILE_CORRUPT"));
	case ERROR_EA_TABLE_FULL:
		return(TEXT("ERROR_EA_TABLE_FULL"));
	case ERROR_INVALID_EA_HANDLE:
		return(TEXT("ERROR_INVALID_EA_HANDLE"));
	case ERROR_EAS_NOT_SUPPORTED:
		return(TEXT("ERROR_EAS_NOT_SUPPORTED"));
	case ERROR_NOT_OWNER:
		return(TEXT("ERROR_NOT_OWNER"));
	case ERROR_TOO_MANY_POSTS:
		return(TEXT("ERROR_TOO_MANY_POSTS"));
	case ERROR_PARTIAL_COPY:
		return(TEXT("ERROR_PARTIAL_COPY"));
	case ERROR_OPLOCK_NOT_GRANTED:
		return(TEXT("ERROR_OPLOCK_NOT_GRANTED"));
	case ERROR_INVALID_OPLOCK_PROTOCOL:
		return(TEXT("ERROR_INVALID_OPLOCK_PROTOCOL"));
	case ERROR_DISK_TOO_FRAGMENTED:
		return(TEXT("ERROR_DISK_TOO_FRAGMENTED"));
	case ERROR_DELETE_PENDING:
		return(TEXT("ERROR_DELETE_PENDING"));
	case ERROR_MR_MID_NOT_FOUND:
		return(TEXT("ERROR_MR_MID_NOT_FOUND"));
	case ERROR_INVALID_ADDRESS:
		return(TEXT("ERROR_INVALID_ADDRESS"));
	case ERROR_ARITHMETIC_OVERFLOW:
		return(TEXT("ERROR_ARITHMETIC_OVERFLOW"));
	case ERROR_PIPE_CONNECTED:
		return(TEXT("ERROR_PIPE_CONNECTED"));
	case ERROR_PIPE_LISTENING:
		return(TEXT("ERROR_PIPE_LISTENING"));
	case ERROR_EA_ACCESS_DENIED:
		return(TEXT("ERROR_EA_ACCESS_DENIED"));
	case ERROR_OPERATION_ABORTED:
		return(TEXT("ERROR_OPERATION_ABORTED"));
	case ERROR_IO_INCOMPLETE:
		return(TEXT("ERROR_IO_INCOMPLETE"));
	case ERROR_IO_PENDING:
		return(TEXT("ERROR_IO_PENDING"));
	case ERROR_NOACCESS:
		return(TEXT("ERROR_NOACCESS"));
	case ERROR_SWAPERROR:
		return(TEXT("ERROR_SWAPERROR"));
	case WSAEINTR:
		return(TEXT("WSAEINTR"));
	case WSAEBADF:
		return(TEXT("WSAEBADF"));
	case WSAEACCES:
		return(TEXT("WSAEACCES"));
	case WSAEFAULT:
		return(TEXT("WSAEFAULT"));
	case WSAEINVAL:
		return(TEXT("WSAEINVAL"));
	case WSAEMFILE:
		return(TEXT("WSAEMFILE"));
	case WSAEWOULDBLOCK:
		return(TEXT("WSAEWOULDBLOCK"));
	case WSAEINPROGRESS:
		return(TEXT("WSAEINPROGRESS"));
	case WSAEALREADY:
		return(TEXT("WSAEALREADY"));
	case WSAENOTSOCK:
		return(TEXT("WSAENOTSOCK"));
	case WSAEDESTADDRREQ:
		return(TEXT("WSAEDESTADDRREQ"));
	case WSAEMSGSIZE:
		return(TEXT("WSAEMSGSIZE"));
	case WSAEPROTOTYPE:
		return(TEXT("WSAEPROTOTYPE"));
	case WSAENOPROTOOPT:
		return(TEXT("WSAENOPROTOOPT"));
	case WSAEPROTONOSUPPORT:
		return(TEXT("WSAEPROTONOSUPPORT"));
	case WSAESOCKTNOSUPPORT:
		return(TEXT("WSAESOCKTNOSUPPORT"));
	case WSAEOPNOTSUPP:
		return(TEXT("WSAEOPNOTSUPP"));
	case WSAEPFNOSUPPORT:
		return(TEXT("WSAEPFNOSUPPORT"));
	case WSAEAFNOSUPPORT:
		return(TEXT("WSAEAFNOSUPPORT"));
	case WSAEADDRINUSE:
		return(TEXT("WSAEADDRINUSE"));
	case WSAEADDRNOTAVAIL:
		return(TEXT("WSAEADDRNOTAVAIL"));
	case WSAENETDOWN:
		return(TEXT("WSAENETDOWN"));
	case WSAENETUNREACH:
		return(TEXT("WSAENETUNREACH"));
	case WSAENETRESET:
		return(TEXT("WSAENETRESET"));
	case WSAECONNABORTED:
		return(TEXT("WSAECONNABORTED"));
	case WSAECONNRESET:
		return(TEXT("WSAECONNRESET"));
	case WSAENOBUFS:
		return(TEXT("WSAENOBUFS"));
	case WSAEISCONN:
		return(TEXT("WSAEISCONN"));
	case WSAENOTCONN:
		return(TEXT("WSAENOTCONN"));
	case WSAESHUTDOWN:
		return(TEXT("WSAESHUTDOWN"));
	case WSAETOOMANYREFS:
		return(TEXT("WSAETOOMANYREFS"));
	case WSAETIMEDOUT:
		return(TEXT("WSAETIMEDOUT"));
	case WSAECONNREFUSED:
		return(TEXT("WSAECONNREFUSED"));
	case WSAELOOP:
		return(TEXT("WSAELOOP"));
	case WSAENAMETOOLONG:
		return(TEXT("WSAENAMETOOLONG"));
	case WSAEHOSTDOWN:
		return(TEXT("WSAEHOSTDOWN"));
	case WSAEHOSTUNREACH:
		return(TEXT("WSAEHOSTUNREACH"));
	case WSAENOTEMPTY:
		return(TEXT("WSAENOTEMPTY"));
	case WSAEPROCLIM:
		return(TEXT("WSAEPROCLIM"));
	case WSAEUSERS:
		return(TEXT("WSAEUSERS"));
	case WSAEDQUOT:
		return(TEXT("WSAEDQUOT"));
	case WSAESTALE:
		return(TEXT("WSAESTALE"));
	case WSAEREMOTE:
		return(TEXT("WSAEREMOTE"));
	case WSAEDISCON:
		return(TEXT("WSAEDISCON"));
	case WSASYSNOTREADY:
		return(TEXT("WSASYSNOTREADY"));
	case WSAVERNOTSUPPORTED:
		return(TEXT("WSAVERNOTSUPPORTED"));
	case WSANOTINITIALISED:
		return(TEXT("WSANOTINITIALISED"));
	case WSAHOST_NOT_FOUND:
		return(TEXT("WSAHOST_NOT_FOUND"));
	case WSATRY_AGAIN:
		return(TEXT("WSATRY_AGAIN"));
	case WSANO_RECOVERY:
		return(TEXT("WSANO_RECOVERY"));
	case WSANO_DATA:
		return(TEXT("WSANO_DATA"));
	default:
		return(TEXT("Unknown Error"));
	}
}