#ifndef __CE_PTR_CPP_INCLUDED__
#define __CE_PTR_CPP_INCLUDED__

#include <CePtr.hpp>






bool
CePtrBase_t::
IsMappedAssert(
	void
	) const
{
	bool	PtrIsMapped = IsMapped();

	if ( !PtrIsMapped )
		{
//		DebugBreak();
		ASSERT(0);
		}

	return PtrIsMapped;
}




void
CePtrBase_t::
MapToProcess(
	HPROCESS	hProcessToMapTo
	)
{
	if ( IsMapped() )
		{
		//	Pointer is already mapped.  Leave it alone.
		}
	else
		{
		//	Remember if this is a dll address.
		if ( m_ProcessIndex == 1 )
			{
			m_IsDllAddress = 1;
			}

		if ( hProcessToMapTo == 0 )
			{
			m_ProcessIndex = GetCurrentProcessIndex() + 1;
			}
		else
			{
			m_ProcessIndex = GetProcessIndexFromID(hProcessToMapTo) + 1;
			}
		}

	return;
}



void
CePtrBase_t::
Unpack(
	void**		pp,
	HPROCESS*	phProcess
	) const
{
	void*		p = 0;
	HPROCESS	hProcess = 0;

	if ( m_Ptr )
		{
		//	Need to make a copy so that we don't destroy the original.
		CePtrBase_t	CePtr(m_Ptr);

		//	Get the process that this pointer originally came from.
		hProcess = GetProcessIDFromIndex(CePtr.m_ProcessIndex - 1);

		//	If the address was a dll address,
		//	set it back to slot 1,
		//	otherwise, set it back to slot 0.
		if ( CePtr.m_IsDllAddress )
			{
			CePtr.m_ProcessIndex = 1;
			CePtr.m_IsDllAddress = 0;
			}
		else
			{
			CePtr.m_ProcessIndex = 0;
			}

		p = CePtr.m_Ptr;
		}

	if ( phProcess )
		{
		*phProcess = hProcess;
		}

	if ( pp )
		{
		*pp = p;
		}

	return;
	
}






#endif

