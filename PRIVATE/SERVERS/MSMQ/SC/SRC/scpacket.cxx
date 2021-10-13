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
