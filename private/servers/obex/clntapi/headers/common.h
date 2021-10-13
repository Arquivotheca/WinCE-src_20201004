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
#ifndef _COMMON_H_
#define _COMMON_H_

#include <windows.h>
#include <winsock2.h>
#include <strsafe.h>
#undef AF_IRDA
#include <af_irda.h>
#include <Ocidl.h>
#include <oleauto.h>
#include <ws2bth.h>
#include <svsutil.hxx>
#include <obexerr.h>

//note: if your reading this a good bit after 4/2/2002
//  you can remove the ..\..\idl\obex.h entry
//  and just use obex.h (this was to fix a potential build
//  break due to the idl file moving)
#ifndef SDK_BUILD
	#include "..\..\idl\obex.h"
#else
	#include "obex.h"
#endif

#include "obexp.h"


//for Pocket-PC
#define WSAGetLastError GetLastError

#include <svsutil.hxx>
class CSynch:public SVSSynch
{

};

extern CSynch *gpSynch;

#define CONSTRUCT_ZONE					DEBUGZONE(0)
#define OBEX_TRANSPORTSOCKET_ZONE		DEBUGZONE(1)
#define OBEX_IRDATRANSPORT_ZONE			DEBUGZONE(2)
#define OBEX_BTHTRANSPORT_ZONE			DEBUGZONE(3)
#define OBEX_TRASPORTCONNECTION_ZONE	DEBUGZONE(4)
#define OBEX_NETWORK_ZONE				DEBUGZONE(5)
#define OBEX_DEVICEENUM_ZONE			DEBUGZONE(6)
#define OBEX_HEADERCOLLECTION_ZONE		DEBUGZONE(7)
#define OBEX_HEADERENUM_ZONE			DEBUGZONE(8)
#define OBEX_COBEX_ZONE					DEBUGZONE(9)
#define OBEX_OBEXDEVICE_ZONE			DEBUGZONE(10)
#define OBEX_OBEXSTREAM_ZONE			DEBUGZONE(11)
#define OBEX_PROPERTYBAG_ZONE			DEBUGZONE(12)
#define OBEX_ADDREFREL_ZONE			    DEBUGZONE(13)
#define OBEX_DUMP_PACKETS_ZONE          DEBUGZONE(14)
//										DEBUGZONE(15)


#define FOUND_NAME      1 
#define FOUND_PORT      2
#define FOUND_TRANSPORT 4
#define CAN_CONNECT     8

/*
#if defined (DEBUG) || defined (_DEBUG)
	void *operator new(UINT size);
	void operator delete(void *pVoid);
#endif
*/

class CCritSection
{
	public:
		CCritSection(LPCRITICAL_SECTION _lpCrit)
		{
			lpCrit = _lpCrit;
			fLocked = FALSE;
		}

		void Lock()
		{
			ASSERT(!fLocked);
			
			if(!fLocked)
				EnterCriticalSection(lpCrit);

			fLocked = TRUE;
		}

		void UnLock()
		{
			ASSERT(fLocked);

			if(fLocked)
				LeaveCriticalSection(lpCrit);

			fLocked = FALSE;
		}
		~CCritSection()
		{
			if(fLocked)
				LeaveCriticalSection(lpCrit);
		}
	private:
		BOOL fLocked;
		LPCRITICAL_SECTION lpCrit;
};


#endif
