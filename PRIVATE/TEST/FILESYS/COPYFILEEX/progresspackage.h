#ifndef _PROGRESSPACKAGE_H_
#define _PROGRESSPACKAGE_H_

//#include <winnt.h>	//LARGE_INTEGER

typedef	enum CancelType
{
	CancelNull = 0, 
	True,
	False
};

class ProgressPackage
{
public:
	ProgressPackage(DWORD _dwCopyFlags = 0, CancelType _cancel = CancelNull, DWORD _dwRequestedReturn = PROGRESS_CONTINUE);
	virtual ~ProgressPackage(){};
	void	StoreParams(LARGE_INTEGER, LARGE_INTEGER, LARGE_INTEGER, LARGE_INTEGER, DWORD, DWORD, HANDLE, HANDLE);
	void	Print(void);
	bool	Evaluate(LARGE_INTEGER, LARGE_INTEGER, LARGE_INTEGER, LARGE_INTEGER, DWORD, DWORD, HANDLE, HANDLE);

public:
	LARGE_INTEGER TotalFileSize;
	LARGE_INTEGER TotalBytesTransferred;
	LARGE_INTEGER StreamSize;
	LARGE_INTEGER StreamBytesTransferred;
	DWORD dwStreamNumber;
	DWORD dwCallbackReason;
	HANDLE hSourceFile;
	HANDLE hDestinationFile;

	DWORD	dwCopyFlags;
	CancelType	cancel;

	DWORD	dwRequestedReturn;		//what the suite wants us to pass back
	DWORD	dwLastReturned;			//cache the value we last returned
	bool	bRet;					//test pass or fail?

	unsigned ulIter;
};

#endif