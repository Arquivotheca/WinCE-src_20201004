//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    scpacket.hxx

Abstract:

    Small client packet class 


--*/

#if ! defined (__scpacket_HXX__)
#define __scpacket_HXX__	1

#include <svsutil.hxx>

#include <ph.h>
#include <phintr.h>

class ScPacketImage {
public:
	void	*pvBinary;
	void	*pvExt;

	struct {
		CBaseHeader		       *pBaseHeader;
		CUserHeader		       *pUserHeader;
		CXactHeader		       *pXactHeader;
		CSecurityHeader	       *pSecurityHeader;
		CPropertyHeader	       *pPropHeader;
		CDebugSection	       *pDebugSect;
		CSrmpEnvelopeHeader    *pSrmpEnvelopeHeader;
		CCompoundMessageHeader *pCompoundMessageHeader;
		CEodHeader             *pEodHeader;
		CEodAckHeader          *pEodAckHeader;
		CSoapSection           *pSoapHeaderSection;
		CSoapSection           *pSoapBodySection;
		CDataHeader            *pFwdViaHeader;
		CDataHeader            *pRevViaHeader;
		CSoapExtSection        *pSoapExtHeaderSection;
	} sect;

	SVSCKey				hkOrderKey;
	union {
		struct {
			unsigned int	fSecureSession       : 1;
			unsigned int	fFwdIncluded         : 1;
			unsigned int    fRevIncluded         : 1;
			unsigned int    fSoapExtIncluded     : 1;
			unsigned int    fSRMPGenerated       : 1;
			unsigned int	fHaveIpv4Addr        : 1;
		} flags;
		int allflags;
	};

	union {
#if defined (INADDR_ANY)
		in_addr	ipSourceAddr;
#endif
		unsigned char ucSourceAddr[32];
	};

	int PopulateSections (void);

	int BinarySize (void) {
		return sect.pBaseHeader->GetPacketSize ();
	}

	int ExtensionsSize (void) {
		void *pExtImage = pvExt;
		void *pExtBaseImage = pExtImage;

		if (flags.fFwdIncluded) {
			CDataHeader *pHdr = (CDataHeader *)pExtImage;
			pExtImage = pHdr->GetNextSection ();
		}

		if (flags.fRevIncluded) {
			CDataHeader *pHdr = (CDataHeader *)pExtImage;
			pExtImage = pHdr->GetNextSection ();
		}

		if (flags.fSoapExtIncluded) {
			CSoapExtSection *pHdr = (CSoapExtSection *)pExtImage;
			pExtImage = pHdr->GetNextSection ();
		}

		return (unsigned char *)pExtImage - (unsigned char *)pExtBaseImage;
	}

	int PersistedSize (void) {
		return ExtensionsSize () + BinarySize () + sizeof(ScPacketImage) - offsetof (ScPacketImage, hkOrderKey);
	}

	int AllocSize (void) {
		return PersistedSize () + offsetof (ScPacketImage, hkOrderKey);
	}

	void *PersistedStart (void) {
		return (void *)&hkOrderKey;
	}

	static int PersistedOffset(void) {
		return offsetof (ScPacketImage, hkOrderKey);
	}
};

//
//	1. Packets are sorted for retrieval by priority and arrival time
//
//	2. Packets are numbered by origination QM guid and unique number within
//	this QM. Therefore the number within a queue is guaranteed to be unique
//	only for outgoing queues. Fast lookup is necessary to ensure that the
//	message that has already arrived does not get stored twice.
//
//
// There are 7 priority levels for message within a queue

#define SCPACKET_ORD_TIMEBITS		29
#define SCPACKET_PACKETS_PER_BLOCK	50

#define SCPACKET_STATE_ALIVE		0
#define SCPACKET_STATE_DEAD			1
#define SCPACKET_STATE_WAITORDERACK	2

class ScQueue;

class ScPacket : public SVSRefObj {
public:
	unsigned int	uiPacketState;

	SVSTNode		*pNode;
	SVSTNode		*pTimeoutNode;
	ScQueue			*pQueue;

	ScPacketImage	*pImage;

	unsigned int	tCreationTime;		// Always absolute time

	unsigned int	tExpirationTime;	// Always absolute time

	unsigned int	uiAckType;
	unsigned int	uiAuditType;

	int				iDirEntry;
	//int				iSize;

	SVSCKey			hkOrderKey;
	unsigned int	uiSeqN;

#if defined (SC_VERBOSE) || defined (SC_INCLUDE_CONSOLE)
	unsigned int	uiMessageID;
	GUID			guidSourceQM;
#endif

private:
	ScPacket (void) {
		uiPacketState = SCPACKET_STATE_ALIVE;

		pNode			= NULL;
		pTimeoutNode    = NULL;
		pQueue          = NULL;

		pImage			= NULL;

		tCreationTime	= 0;

		tExpirationTime = 0;

		uiAckType       = 0;
		uiAuditType     = 0;

		//iSize			= 0;
		iDirEntry		= -1;

		hkOrderKey      = 0;

		uiSeqN          = 0;

#if defined (SC_VERBOSE) || defined (SC_INCLUDE_CONSOLE)
		uiMessageID		= 0;
		memset (&guidSourceQM, 0, sizeof(guidSourceQM));
#endif
	}

public:

	friend class	ScQueue;
	friend class	ScFile;
	friend DWORD WINAPI scapi_UserControlThread (LPVOID lpParameter);
};

#endif /* __scpacket_HXX__ */

