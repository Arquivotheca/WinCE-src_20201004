#ifndef __CNFYDATA__
#define __CNFYDATA__

class CNfyData
{
public:
	CNfyData();
	~CNfyData();

	void ClearData();
	int AppendData(TCHAR *cmdline, SYSTEMTIME *st);
	NOTIFTESTDATA *GetNotificationTestData();

private:
	CMMFile mmFile;		//This creates the shared block of memory

	HANDLE mutex1;		//For interprocess sync
};

#endif
