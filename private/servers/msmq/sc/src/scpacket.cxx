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
/*++


Module Name:

    scpacket.cxx

Abstract:

    Small client packet class support


--*/
#include <sc.hxx>

#include <scpacket.hxx>

int ScPacketImage::PopulateSections (void) {
	memset (&sect, 0, sizeof(sect));

	pvBinary = (void *)(this + 1);
	pvExt = NULL;

	sect.pBaseHeader = (CBaseHeader *)pvBinary;

	if (! sect.pBaseHeader->VersionIsValid())
		return FALSE;

	if (! sect.pBaseHeader->SignatureIsValid())
		return FALSE;

	if (sect.pBaseHeader->GetType() != FALCON_USER_PACKET)
		return FALSE;

	sect.pUserHeader = (CUserHeader*) sect.pBaseHeader->GetNextSection();
	void *pSection = sect.pUserHeader->GetNextSection();

	if (sect.pUserHeader->IsOrdered()) {
		sect.pXactHeader = (CXactHeader *)pSection;
		pSection = sect.pXactHeader->GetNextSection();
	}

	if (sect.pUserHeader->SecurityIsIncluded()) {
		sect.pSecurityHeader = (CSecurityHeader *)pSection;
		pSection = sect.pSecurityHeader->GetNextSection();
	}

	if (sect.pUserHeader->PropertyIsIncluded()) {
		sect.pPropHeader = (CPropertyHeader *)pSection;
		pSection = sect.pPropHeader->GetNextSection ();
	}

	if (sect.pBaseHeader->DebugIsIncluded()) {
		sect.pDebugSect = (CDebugSection *)pSection;
		pSection = sect.pDebugSect->GetNextSection();
	}

	if (sect.pUserHeader->SrmpIsIncluded()) {
		sect.pSrmpEnvelopeHeader = (CSrmpEnvelopeHeader*)pSection;
		pSection = sect.pSrmpEnvelopeHeader->GetNextSection();

		sect.pCompoundMessageHeader = (CCompoundMessageHeader *)pSection;
		pSection = sect.pCompoundMessageHeader->GetNextSection();
	}

	if (sect.pUserHeader->EodIsIncluded()) {
		sect.pEodHeader = (CEodHeader*)pSection;
		pSection = sect.pEodHeader->GetNextSection();
	}

	if (sect.pUserHeader->EodAckIsIncluded()) {
		sect.pEodAckHeader = (CEodAckHeader*)pSection;
		pSection = sect.pEodAckHeader->GetNextSection();
	}

	if (sect.pUserHeader->SoapIsIncluded()) {
		sect.pSoapHeaderSection = (CSoapSection*)pSection;
		pSection = sect.pSoapHeaderSection->GetNextSection();

		sect.pSoapBodySection = (CSoapSection*)pSection;
		pSection = sect.pSoapBodySection->GetNextSection();
	}

	if (flags.fFwdIncluded || flags.fRevIncluded || flags.fSoapExtIncluded)
		pvExt = pSection;

	if (flags.fFwdIncluded) {
		sect.pFwdViaHeader = (CDataHeader *)pSection;
		pSection = sect.pFwdViaHeader->GetNextSection ();
	}

	if (flags.fRevIncluded) {
		sect.pRevViaHeader = (CDataHeader *)pSection;
		pSection = sect.pRevViaHeader->GetNextSection ();
	}

	if (flags.fSoapExtIncluded) {
		sect.pSoapExtHeaderSection = (CSoapExtSection *)pSection;
		pSection = sect.pSoapExtHeaderSection->GetNextSection();
	}

	return TRUE;
}
