//

#include <windows.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------
BOOL WINAPI 
DllMain(
	HANDLE hInstance, 
	ULONG dwReason, 
	LPVOID lpReserved ) 
// --------------------------------------------------------------------    
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(lpReserved);
	
	switch( dwReason )
	{
		case DLL_PROCESS_ATTACH:
			return TRUE;

		case DLL_PROCESS_DETACH:
			return TRUE;
	}
	return FALSE;
}

#ifdef __cplusplus
} // end extern "C"
#endif

