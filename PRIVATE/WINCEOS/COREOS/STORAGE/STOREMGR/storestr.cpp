//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <fsdmgrp.h>

const TCHAR *g_szSTORAGE_PATH = L"System\\StorageManager";
const TCHAR *g_szPROFILE_PATH = L"System\\StorageManager\\Profiles";
const TCHAR *g_szAUTOLOAD_PATH = L"System\\StorageManager\\AutoLoad";
const TCHAR *g_szReloadTimeOut= L"PNPUnloadDelay";
const TCHAR *g_szWaitDelay=L"PNPWaitIODelay";
const TCHAR *g_szPNPThreadPrio=L"PNPThreadPrio256";
const TCHAR *g_szAUTO_PART_STRING = L"AutoPart";
const TCHAR *g_szAUTO_MOUNT_STRING = L"AutoMount";
const TCHAR *g_szAUTO_FORMAT_STRING = L"AutoFormat";
const TCHAR *g_szPART_DRIVER_STRING = L"PartitionDriver";
const TCHAR *g_szPART_DRIVER_MODULE_STRING = L"Dll";
const TCHAR *g_szPART_DRIVER_NAME_STRING = L"PartitionDriverName";
const TCHAR *g_szPART_TABLE_STRING = L"PartitionTable";
const TCHAR *g_szFILE_SYSTEM_STRING = L"DefaultFileSystem";
const TCHAR *g_szFILE_SYSTEM_MODULE_STRING = L"Dll";
const TCHAR *g_szFSD_BOOTPHASE_STRING = L"BootPhase";
const TCHAR *g_szFILE_SYSTEM_DRIVER_STRING = L"DriverPath";
const TCHAR *g_szFSD_ORDER_STRING = L"Order";
const TCHAR *g_szFSD_LOADFLAG_STRING = L"LoadFlags";
const TCHAR *g_szSTORE_NAME_STRING =L"Name";
const TCHAR *g_szFOLDER_NAME_STRING =L"Folder";
const TCHAR *g_szMOUNT_FLAGS_STRING =L"MountFlags";
const TCHAR *g_szATTRIB_STRING =L"Attrib";
const TCHAR *g_szACTIVITY_TIMER_STRING = L"ActivityEvent";
const TCHAR *g_szACTIVITY_TIMER_ENABLE_STRING = L"EnableActivityEvent";
const TCHAR *g_szCHECKFORFORMAT = L"CheckForFormat";

const TCHAR *g_szMOUNT_FLAG_MOUNTASROOT_STRING =L"MountAsRoot";
const TCHAR *g_szMOUNT_FLAG_MOUNTHIDDEN_STRING =L"MountHidden";
const TCHAR *g_szMOUNT_FLAG_MOUNTASBOOT_STRING =L"MountAsBootable";
const TCHAR *g_szMOUNT_FLAG_MOUNTASROM_STRING =L"MountAsROM";
const TCHAR *g_szMOUNT_FLAG_MOUNTSYSTEM_STRING =L"MountSystem";
const TCHAR *g_szMOUNT_FLAG_MOUNTPERMANENT_STRING =L"MountPermanent";

#ifdef UNDER_CE
const TCHAR *g_szDEFAULT_PARTITION_DRIVER = L"mspart.dll";
#else
const TCHAR *g_szDEFAULT_PARTITION_DRIVER = L"mspart_nt.dll";
#endif
const TCHAR *g_szDEFAULT_FILESYSTEM     = L"FATFS";
const TCHAR *g_szDEFAULT_STORAGE_NAME = L"External Storage";
const TCHAR *g_szDEFAULT_FOLDER_NAME = L"Mounted Volume";
const TCHAR *g_szDEFAULT_ACTIVITY_NAME = L""; // No activity timer
