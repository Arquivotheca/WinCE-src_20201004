#include <windows.h>
#include "main.h"
#include "globals.h"
#include "ProgressPackage.h"

ProgressPackage::ProgressPackage(DWORD _dwCopyFlags, CancelType _cancel, DWORD _dwRequestedReturn)
	: cancel(_cancel), dwCopyFlags(_dwCopyFlags), dwRequestedReturn(_dwRequestedReturn), dwLastReturned(PROGRESS_CONTINUE), bRet(true)
{}


void	ProgressPackage::StoreParams(
									LARGE_INTEGER _TotalFileSize, 
									LARGE_INTEGER _TotalBytesTransferred, 
									LARGE_INTEGER _StreamSize, 
									LARGE_INTEGER _StreamBytesTransferred, 
									DWORD _dwStreamNumber, 
									DWORD _dwCallbackReason, 
									HANDLE _hSourceFile, 
									HANDLE _hDestinationFile
									)
{
	TotalBytesTransferred = _TotalBytesTransferred;
	TotalFileSize = _TotalFileSize;
	StreamSize = _StreamSize;
	StreamBytesTransferred = _StreamBytesTransferred;
	dwStreamNumber = _dwStreamNumber;
	dwCallbackReason = _dwCallbackReason;
	hSourceFile = _hSourceFile;
	hDestinationFile = _hDestinationFile;
}

void ProgressPackage::Print(void)
{
    g_pKato->Log(LOG_COMMENT, TEXT("\nParams in progress routine:"));
	g_pKato->Log(LOG_COMMENT, TEXT("\tTotalBytesTransferred:[%d]"),  TotalBytesTransferred);
	g_pKato->Log(LOG_COMMENT, TEXT("\tTotalFileSize:[%d]"),  TotalFileSize);
	g_pKato->Log(LOG_COMMENT, TEXT("\tStreamSize:[%d]"),  StreamSize);
	g_pKato->Log(LOG_COMMENT, TEXT("\tStreamBytesTransferred:[%d]"),  StreamBytesTransferred);
	g_pKato->Log(LOG_COMMENT, TEXT("\tdwStreamNumber:[%d]"),  dwStreamNumber);
	g_pKato->Log(LOG_COMMENT, TEXT("\tdwCallbackReason:[%s]"),  
		dwCallbackReason == CALLBACK_CHUNK_FINISHED ? _T("CALLBACK_CHUNK_FINISHED") : 
		dwCallbackReason == CALLBACK_STREAM_SWITCH ? _T("CALLBACK_STREAM_SWITCH") : _T("UNKNOWN REASON"));
	g_pKato->Log(LOG_COMMENT, TEXT("\thSourceFile:[0x%x]"),  hSourceFile);
	g_pKato->Log(LOG_COMMENT, TEXT("\thDestinationFile:[0x%x]"),  hDestinationFile);
}


bool	ProgressPackage::Evaluate(
									LARGE_INTEGER _TotalFileSize, 
									LARGE_INTEGER _TotalBytesTransferred, 
									LARGE_INTEGER _StreamSize, 
									LARGE_INTEGER _StreamBytesTransferred, 
									DWORD _dwStreamNumber, 
									DWORD _dwCallbackReason, 
									HANDLE _hSourceFile, 
									HANDLE _hDestinationFile
									)
{
	if(_TotalBytesTransferred.QuadPart > _TotalFileSize.QuadPart)
	{
		bRet = false; 
		return false; 
	}

	if(_StreamBytesTransferred.QuadPart > _StreamSize.QuadPart)
	{
		bRet = false; 
		return false; 
	}

	if(dwLastReturned == PROGRESS_CANCEL || dwLastReturned == PROGRESS_STOP || dwLastReturned == PROGRESS_QUIET)
	{
		bRet = false; 
		return false; 
	}

	return true; 
}
