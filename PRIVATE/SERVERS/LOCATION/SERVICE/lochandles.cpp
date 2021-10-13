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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

// Abstract: Location app handle manager

#include <locCore.hpp>

serviceHandle_t::~serviceHandle_t() {
    locationHandles_t::iterator it    = g_pHandleMgr->m_locationHandles.begin();
    locationHandles_t::iterator itEnd = g_pHandleMgr->m_locationHandles.end();

    for (; it != itEnd; ) {
        locOpenHandle_t *pLocHandle = it->second;
        
        if (pLocHandle->IsParent(this))
            g_pHandleMgr->m_locationHandles.erase(it++);
        else
            ++it;
    }
}

locOpenHandle_t::~locOpenHandle_t() {
    // Remove all associated plugin handles
    pluginHandles_t::iterator it    = g_pHandleMgr->m_pluginHandles.begin();
    pluginHandles_t::iterator itEnd = g_pHandleMgr->m_pluginHandles.end();

    for (; it != itEnd; ) {
        pluginHandle_t *pPluginHandle = it->second;
        if (pPluginHandle->IsParent(this)) {
            // Explicitly call plugin's Close routine so that it 
            // can clean up after itself.
            plugin_t *pPlugin = pPluginHandle->GetPlugin();
            pPlugin->CallIoctlClose(pPluginHandle->GetBaseHandle());
            g_pHandleMgr->m_pluginHandles.erase(it++);
        }
        else
           ++it;
    }

    g_pPluginMgr->UnRegisterEventsFromHandle(this);
}

pluginHandle_t::pluginHandle_t(locOpenHandle_t *pLoc, plugin_t *pPlugin, HANDLE plugH) {
    m_pLocHandle = pLoc;
    m_pPlugin = pPlugin;
    m_basePluginHandle = plugH;
}

pluginHandle_t::~pluginHandle_t() {
    ;
}

handleMgr_t::handleMgr_t() {
    m_nextHandle = 0;
}

// If dynamic memory allocations that could fail are ever added
// to handleMgr_t constructor, then check they are valid in this function.
handleMgr_t::IsHandleMgrInited(void) {
    return TRUE;
}

handleMgr_t::~handleMgr_t() {
    ;
}

// This is called when stopping or refreshing the location framework service.
// It will remove ALL LocationOpen() and LocationPluginOpen returned handles,
// causing appropriate cleanup to be performed.  It will not however
// invalid service handles (LOC_Open via services.exe), since otherwise
// existing applications that were using the API would no longer work
// after a REFRESH came in.
void handleMgr_t::ResetLocHandles(void) {
    DEBUGCHK(g_pLocLock->IsLocked());

    // Just free location handles from this function.  Plugin handles
    // will be freed in location handles cleanup.
    m_locationHandles.clear();
}

// Returns handle value that will be returned to the application.
HANDLE handleMgr_t::GetNextHandle() {
    m_nextHandle++;
    // 0 is error code, so we can never return that.
    if (m_nextHandle == 0)
        m_nextHandle++;

    return (HANDLE)m_nextHandle;
}

HANDLE handleMgr_t::AllocServiceHandle(void) {
    DEBUGCHK(g_pLocLock->IsLocked());

    serviceHandleSmartPtr_t servH = new serviceHandle_t(GetOwnerProcess());
    if (! servH.valid()) {
        DEBUGMSG_OOM();
        return 0;
    }

    HANDLE h = GetNextHandle();

    if (m_serviceHandles.end() == m_serviceHandles.insert(h,servH)) {
        DEBUGMSG_OOM();
        return 0;
    }
   
    return h;
}

HLOCATION handleMgr_t::AllocLocationHandle(serviceHandle_t *pServiceHandle) {
    DEBUGCHK(g_pLocLock->IsLocked());

    locOpenHandleSmartPtr_t locH = new locOpenHandle_t(pServiceHandle);
    if (! locH.valid()) {
        DEBUGMSG_OOM();
        return 0;
    }

    HLOCATION h = (HLOCATION)GetNextHandle();

    if (m_locationHandles.end() == m_locationHandles.insert(h,locH)) {
        DEBUGMSG_OOM();
        return 0;
    }
    
    return h;
}

HLOCATIONPLUGIN handleMgr_t::AllocPluginHandle(locOpenHandle_t *pLocationHandle, plugin_t *pPlugin, HANDLE pluginHandle) {   
    DEBUGCHK(g_pLocLock->IsLocked());

    pluginHandleSmartPtr_t plugH = new pluginHandle_t(pLocationHandle,pPlugin,pluginHandle);
    if (! plugH.valid()) {
        DEBUGMSG_OOM();
        return 0;
    }
    
    HLOCATIONPLUGIN h = (HLOCATIONPLUGIN)GetNextHandle();

    if (m_pluginHandles.end() == m_pluginHandles.insert(h,plugH)) {
        DEBUGMSG_OOM();
        return 0;
    }

    return h;
}


DWORD handleMgr_t::FreeServiceHandle(HANDLE serviceHandle) {
    DEBUGCHK(g_pLocLock->IsLocked());

    if (! m_serviceHandles.erase(serviceHandle)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Service handle <0x%08x> is invalid and cannot be deleted\r\n",serviceHandle));
        return ERROR_INVALID_PARAMETER;
    }

    return ERROR_SUCCESS;
}

DWORD handleMgr_t::FreeLocationHandle(HLOCATION locationHandle) {
    DEBUGCHK(g_pLocLock->IsLocked());

    if (! m_locationHandles.erase(locationHandle)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Location handle <0x%08x> is invalid and cannot be deleted\r\n",locationHandle));
        return ERROR_INVALID_PARAMETER;
    }

    return ERROR_SUCCESS;
}

DWORD handleMgr_t::FreePluginHandle(HLOCATIONPLUGIN pluginHandle) {
    DEBUGCHK(g_pLocLock->IsLocked());

    if (! m_pluginHandles.erase(pluginHandle)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Plugin handle <0x%08x> is invalid and cannot be deleted\r\n",pluginHandle));
        return ERROR_INVALID_PARAMETER;
    }

    return ERROR_SUCCESS;
}

serviceHandle_t *handleMgr_t::FindServiceHandle(HANDLE serviceHandle) {
    DEBUGCHK(g_pLocLock->IsLocked());
    serviceHandles_t::iterator it = m_serviceHandles.find(serviceHandle);

    if (it == m_serviceHandles.end()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Service Handle <0x%08x> not found\r\n",serviceHandle));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    return it->second;
}

locOpenHandle_t *handleMgr_t::FindLocationHandle(HLOCATION locationHandle) {
    DEBUGCHK(g_pLocLock->IsLocked());

    locationHandles_t::iterator it = m_locationHandles.find(locationHandle);

    if (it == m_locationHandles.end()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Location Handle <0x%08x> not found\r\n",locationHandle));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    return it->second;
}

pluginHandle_t *handleMgr_t::FindPluginHandle(HLOCATIONPLUGIN pluginHandle) {
    DEBUGCHK(g_pLocLock->IsLocked());

    pluginHandles_t::iterator it = m_pluginHandles.find(pluginHandle);

    if (it == m_pluginHandles.end()) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Plugin Handle <0x%08x> not found\r\n",pluginHandle));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    return it->second;
}

