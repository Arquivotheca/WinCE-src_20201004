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

//
// Details of the UPnP AV toolkit's implementation
//

#ifndef __AV_UPNP_INTERNAL_H
#define __AV_UPNP_INTERNAL_H

#ifndef UNDER_CE
// XP doesn't support IDispatch::Invoke for UPnP actions
#define NO_INVOKE_FOR_UPNP_ACTIONS

#define DEBUGMSG(x, y)
#endif

namespace av_upnp
{

namespace details
{


//
// Namespace for LastChange event schema for AVTransport and RenderingControl service
//
template <typename T>
inline LPCWSTR get_event_namespace();

template <>
inline LPCWSTR get_event_namespace<IAVTransport>()
    {return L"urn:schemas-upnp-org:metadata-1-0/AVT/"; }

template <>
inline LPCWSTR get_event_namespace<IRenderingControl>()
    {return L"urn:schemas-upnp-org:metadata-1-0/RCS/"; }


//
// IUPnPEventSinkSupport allows inheritance from multiple support classes needing to store
// IUPnPEventSinks to share a common IUPnPEventSink, by virtually inheriting from this class
//
class IUPnPEventSinkSupport
{
public:
    IUPnPEventSinkSupport();
    virtual ~IUPnPEventSinkSupport();

protected:
    ce::critical_section    m_csPunkSubscriber;
    CComPtr<IUPnPEventSink> m_punkSubscriber;
};


//
// ITimedEventCallee interface
//
class ITimedEventCallee
{
public:
    virtual void TimedEventCall() = 0;
};


// TimedEventCaller
class TimedEventCaller
{
public:
    TimedEventCaller()
    {
        assert(m_mapCalleeGroups.empty());
    }
    
    // Call pCallee::TimedEventCall() with a minimum of nTimeout ms between each call
    // nTimeout must be less than 0x7FFFFFFF
    DWORD RegisterCallee(DWORD nTimeout, ITimedEventCallee* pCallee);
    DWORD UnregisterCallee(DWORD nTimeout, ITimedEventCallee* pCallee);

    // While the number of refs for the given nTimeout is zero,
    // timed event calling for events with this timing is disabled
    void AddRef(DWORD nTimeout);
    void Release(DWORD nTimeout);

private:
    class CalleeGroup
    {
    public:
        CalleeGroup(DWORD nTimeout);
        ~CalleeGroup();
        
        DWORD RegisterCallee(ITimedEventCallee* pCallee);
        BOOL UnregisterCallee(ITimedEventCallee* pCallee);
        
        void AddRef();
        void Release();

    protected:
        static DWORD WINAPI EventCallerThread(LPVOID lpvthis);

    protected:
        typedef ce::hash_set<ITimedEventCallee*> CalleeSet;

        ce::critical_section_with_copy  m_cs;
        CalleeSet                       m_setCallees;
        long                            m_nRefs;
        ce::smart_handle                m_hEventRun; // signaled while m_nRefs > 0
        ce::smart_handle                m_hCallerThread;
        ce::smart_handle                m_hEventExit;
        const DWORD                     m_nTimeout;
    };

    typedef ce::hash_map<DWORD, CalleeGroup> CalleeGroupMap;

    ce::critical_section_with_copy  m_csMapCalleeGroups;
    CalleeGroupMap                  m_mapCalleeGroups; // map from minSleepPeriod -> CalleeGroup
};

typedef ce::singleton<TimedEventCaller> TimedEventCallerSingleton;


// wstring_buffer is useful when one wants to use a single wstring many times without
// incurring the allocation costs with allocating a new wstring for each use.
//
// Usage: use strBuffer, call reset_buffer() when done.
// wstring_buffer will then be ready for use again, not lowering strBuffer's internal allocation
// below the max used over the past nSizeCheckPeriod uses.
class wstring_buffer
{
public:
    wstring_buffer(unsigned int nSizeCheckPeriod = 1000);

    wstring                 strBuffer;

    void ResetBuffer(); // reset the buffer to use again

private:
    size_t                  m_nMaxSize;            // max size of strBuffer during the current count period
    unsigned int            m_nUsesSinceSizeCheck; // number of uses since last shrink check
    const unsigned int      m_nSizeCheckPeriod;    // number of uses to wait between shrink checks
};


// HashMapTraits
template <typename K, typename T>
struct HashMapTraits
{
    size_t hash_function(typename const ce::hash_map<K,T>::const_iterator& Key) const
    {
        /* This hash function uses the knowledge that all iterators are, under the hood, pointers to
        * the corresponding object to generate the iterator's hash value.
        * The hash value is computed by using the value of this pointer, shifting it right to remove the lower
        * bits which do not change because of alignment, and masking out the high bits.
        */
        typedef const ce::hash_map<K,T>::const_iterator KeyType;

        assert(sizeof(Key) == sizeof(DWORD));

        const DWORD  HighMask   = 0x400;
        const DWORD* pdwKey     = reinterpret_cast<const DWORD*>(&Key);
        const DWORD  HashedKey  = HighMask & ( *pdwKey >> __alignof(KeyType) );

        return HashedKey;
    }
};


// VirtualEventSink
class VirtualEventSink : public IEventSink
{
public:
    VirtualEventSink();
    VirtualEventSink(const VirtualEventSink &ves);

    // Append state variable data to *pLastChange.
    // If GetAllVariables append data for all variables,
    // else only variables which have changed since last call to GetLastChange
    DWORD GetLastChange(
            bool bUpdatesOnly,
            wstring* pLastChange);

    bool  HasChanged();

// IEventSink
public:
    DWORD OnStateChanged(
            LPCWSTR pszStateVariableName,
            LPCWSTR pszValue);
    DWORD OnStateChanged(
            LPCWSTR pszStateVariableName,
            long nValue);
    DWORD OnStateChanged(
            LPCWSTR pszStateVariableName,
            LPCWSTR pszChannel,
            long nValue);

private:
    // Forward declare for class Variable
    struct VariableHashTraits;

    class Variable
    {
    friend VariableHashTraits;
    public:
        Variable(const wstring &strName);
        Variable(const wstring &strName, const wstring &strChannel);

        DWORD AppendLastChange(wstring* pstrLastChange, const wstring &strVarValue) const;

        bool operator==(const Variable &v) const;

    private:
        const wstring m_strName;

        // channel attribute
        const bool      m_fChannel;
        const LPCWSTR   m_pszChannelAttributeName;
        const wstring   m_strChannel;
    };

    struct VariableHashTraits
          // privately inherit from hash_traits<wstring> to use its hash_function(),
          // inherit instead of have as a data member to still take zero bytes
        : private ce::hash_traits<wstring>
    {
        size_t hash_function(const Variable& Key) const;
    };


    typedef ce::hash_map<Variable, wstring, VariableHashTraits>                     VarMap; // map from variable -> value
    typedef ce::hash_set<VarMap::const_iterator, HashMapTraits<Variable, wstring> > ChangedVarsSet;


    ce::critical_section    m_csDataMembers;
    VarMap                  m_mapVars;
    ChangedVarsSet          m_setChangedVars;

private:
    DWORD generic_OnStateChanged(const Variable &var, LPCWSTR pszValue);
};

namespace Direction
{
    const LPCWSTR Input     = L"Input",
                  Output    = L"Output";
};

} // av_upnp::details

} // av_upnp

#define AV_TEXT(string) TEXT("AV: ") TEXT(string) TEXT(" (") TEXT(__FUNCTION__) TEXT(")")

#endif // __AV_UPNP_INTERNAL_H
