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


// This is allocated whenever an application successfully calls LOC_Open()
// and is used to cleanup resources the process allocated in event it did 
// not clean up after itself.
class serviceHandle_t {
private:
    // Process that created this object.
    HANDLE      m_hOwnerProc;
public:
    serviceHandle_t(HANDLE h) : m_hOwnerProc(h) { ; }
    ~serviceHandle_t();
};

// Allocated for each LocationOpen call - tracks LF's associated resources
class locOpenHandle_t {
private:
    // Associated owner process - parent of this handle
    serviceHandle_t  *m_pServiceHandle;
public:
    locOpenHandle_t(serviceHandle_t *p) : m_pServiceHandle(p) { ; }
    ~locOpenHandle_t();

    BOOL IsParent(serviceHandle_t *pServH) { return (pServH==m_pServiceHandle); }
};

// Tracks handles allocated by direct calls to Plugin IOCTLs
class pluginHandle_t {
private:
    // Associated handle from LocationOpen - parent of this handle
    locOpenHandle_t *m_pLocHandle;
    // GUID of the plugin opened
    plugin_t        *m_pPlugin;
    // Handle that ProviderIoctlOpen or ResolverIoctlOpen returned
    HANDLE           m_basePluginHandle;
public:
    pluginHandle_t(locOpenHandle_t *pLoc, plugin_t *pPlugin, HANDLE plugH);
    ~pluginHandle_t();

    BOOL IsParent(locOpenHandle_t *pLocH) { return (pLocH==m_pLocHandle); }

    plugin_t * GetPlugin(void)     { return m_pPlugin; }
    HANDLE     GetBaseHandle(void) { return m_basePluginHandle; }
};


typedef ce::smart_ptr<serviceHandle_t>       serviceHandleSmartPtr_t;
typedef ce::smart_ptr<locOpenHandle_t>       locOpenHandleSmartPtr_t;
typedef ce::smart_ptr<pluginHandle_t>        pluginHandleSmartPtr_t;

typedef ce::hash_map<HANDLE,serviceHandleSmartPtr_t>          serviceHandles_t;
typedef ce::hash_map<HLOCATION,locOpenHandleSmartPtr_t>       locationHandles_t;
typedef ce::hash_map<HLOCATIONPLUGIN,pluginHandleSmartPtr_t>  pluginHandles_t;

// Manages handles between calling application and location framework service
class handleMgr_t {
private:
    // Tracks which value to assign to next allocated handle
    DWORD             m_nextHandle;
    // Lists all handles allocated via LOC_Open() (services.exe layer)
    serviceHandles_t  m_serviceHandles;
    // Lists all handles allocated via LocationOpen (application)
    locationHandles_t m_locationHandles;
    // Lists all handles allocated via ProviderIoctlOpen and ResolverIoctlOpen
    pluginHandles_t   m_pluginHandles;

    HANDLE GetNextHandle(void);
public:
    friend serviceHandle_t::~serviceHandle_t();
    friend locOpenHandle_t::~locOpenHandle_t();
    
    handleMgr_t();
    ~handleMgr_t();

    BOOL IsHandleMgrInited(void);
    void ResetLocHandles(void);

    HANDLE          AllocServiceHandle(void);
    HLOCATION       AllocLocationHandle(serviceHandle_t *pServiceHandle);
    HLOCATIONPLUGIN AllocPluginHandle(locOpenHandle_t *pLocationHandle, plugin_t *pPlugin, HANDLE pluginHandle);

    DWORD FreeServiceHandle(HANDLE serviceHandle);
    DWORD FreeLocationHandle(HLOCATION locationHandle);
    DWORD FreePluginHandle(HLOCATIONPLUGIN pluginHandle);

    serviceHandle_t * FindServiceHandle(HANDLE serviceHandle);
    locOpenHandle_t * FindLocationHandle(HLOCATION locationHandle);
    pluginHandle_t *  FindPluginHandle(HLOCATIONPLUGIN pluginHandle);
};

extern handleMgr_t *g_pHandleMgr;

