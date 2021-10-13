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

#include "av_upnp.h"

using namespace av_upnp;
using namespace av_upnp::details;

/////////////////////////////////////////////////////////////////////////////
// VirtualEventSink

VirtualEventSink::VirtualEventSink()
{
}


// This class is designed to allow copying only when m_mapVars and m_setChangedVars
// are empty, which allows an empty VirtualEventSink to be inserted into a container.
// Only empty copies are allowed because otherwise m_setChangedVars, which would
// contain iterators from m_mapVars, would need special code to remap the contained iterators.
VirtualEventSink::VirtualEventSink(const VirtualEventSink &ves)
{
    assert(ves.m_mapVars.empty());
    assert(ves.m_setChangedVars.empty());
}



//
// IEventSink
//

// OnStateChanged
DWORD VirtualEventSink::OnStateChanged(
                            LPCWSTR pszStateVariableName,
                            LPCWSTR pszValue)
{
    if(!pszStateVariableName)
    {
        return ERROR_AV_POINTER;
    }

    const Variable var(pszStateVariableName);

    return generic_OnStateChanged(var, pszValue);
}


// OnStateChanged
DWORD VirtualEventSink::OnStateChanged(
                            LPCWSTR pszStateVariableName,
                            long nValue)
{
    WCHAR pszValue[33];
    
    _ltow(nValue, pszValue, 10);

    return OnStateChanged(pszStateVariableName, pszValue);
}


// OnStateChanged
DWORD VirtualEventSink::OnStateChanged(
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszChannel,
        /*[in]*/  long nValue)
{
    if(!pszStateVariableName)
    {
        return ERROR_AV_POINTER;
    }

    const Variable  var(pszStateVariableName, pszChannel);
    WCHAR           pszValue[33];
    
    _ltow(nValue, pszValue, 10);

    return generic_OnStateChanged(var, pszValue);
}


// GetLastChange
DWORD VirtualEventSink::GetLastChange(
                            bool     bUpdatesOnly,
                            wstring* pLastChange)
{
    if(!pLastChange)
    {
        return ERROR_AV_POINTER;
    }

    ce::gate<ce::critical_section> gate(m_csDataMembers);

    if(bUpdatesOnly)
    {
        // Retrieve variables that have changed since last get_LastChange() call
        for(ChangedVarsSet::const_iterator it = m_setChangedVars.begin(), itEnd = m_setChangedVars.end(); itEnd != it; ++it)
        {
            DWORD retALC = (*it)->first.AppendLastChange(pLastChange, (*it)->second);
            
            if(SUCCESS_AV != retALC)
            {
                return retALC;
            }
        }
    }
    else
    {
        // Retrieve all variables
        for(VarMap::const_iterator it = m_mapVars.begin(), itEnd = m_mapVars.end(); itEnd != it; ++it)
        {
            DWORD retALC = it->first.AppendLastChange(pLastChange, it->second);
            
            if(SUCCESS_AV != retALC)
            {
                return retALC;
            }
        }
    }

    // Now that we've sent all variables that have changed, clear this set
    m_setChangedVars.clear();

    return SUCCESS_AV;
}


// HasChanged
bool VirtualEventSink::HasChanged()
{
    ce::gate<ce::critical_section> gate(m_csDataMembers);

    return !m_setChangedVars.empty();
}



//
// Private
//

DWORD VirtualEventSink::generic_OnStateChanged(const Variable &var, LPCWSTR pszValue)
{
    if(!pszValue)
    {
        return ERROR_AV_POINTER;
    }

    ce::gate<ce::critical_section> gate(m_csDataMembers);

    // Insert new/update existing variable value
    VarMap::iterator itVars = m_mapVars.find(var);
    
    if(m_mapVars.end() != itVars)
    {
        itVars->second = pszValue;
    }
    else
    {
        itVars = m_mapVars.insert(var, pszValue);
        
        if(m_mapVars.end() == itVars)
        {
            return ERROR_AV_OOM;
        }
    }

    // Insert iterator into changed iterators set if not already there
    if(m_setChangedVars.end() == m_setChangedVars.insert(itVars))
    {
        return ERROR_AV_OOM;
    }

    return SUCCESS_AV;
}




/////////////////////////////////////////////////////////////////////////////
// VirtualEventSink::Variable

VirtualEventSink::Variable::Variable(const wstring &strName)
    : m_fChannel(false),
      m_pszChannelAttributeName(L"channel"),
      m_strName(strName)
{
}


VirtualEventSink::Variable::Variable(const wstring &strName, const wstring &strChannel)
    : m_fChannel(true),
      m_pszChannelAttributeName(L"channel"),
      m_strChannel(strChannel),
      m_strName(strName)
{
}


DWORD VirtualEventSink::Variable::AppendLastChange(wstring* pstrLastChange, const wstring &strVarValue) const
{
    if(!pstrLastChange)
    {
        return ERROR_AV_POINTER;
    }

    // Start element
    if(!pstrLastChange->append(L"<") ||
       !pstrLastChange->append(m_strName) ||
       !pstrLastChange->append(L" "))
    {
       return ERROR_AV_OOM;
    }

    // Add channel attribute
    if(m_fChannel)
    {
        if(!pstrLastChange->append(m_pszChannelAttributeName) ||
           !pstrLastChange->append(L"=\"") ||
           !pstrLastChange->append(m_strChannel) ||
           !pstrLastChange->append(L"\" "))
        {
           return ERROR_AV_OOM;
        }
    }

    // Add value attribute and close element
    if(!pstrLastChange->append(L"val=\"") ||
       !pstrLastChange->append(strVarValue) ||
       !pstrLastChange->append(L"\"/>"))
    {
        return ERROR_AV_OOM;
    }

    return SUCCESS_AV;
}


bool VirtualEventSink::Variable::operator==(const Variable &v) const
{
    return ((m_strName  == v.m_strName) &&
            (m_fChannel == v.m_fChannel) &&
            (m_fChannel ? (m_strChannel == v.m_strChannel) : true));
}




/////////////////////////////////////////////////////////////////////////////
// VirtualEventSink::VariableHashTraits

size_t VirtualEventSink::VariableHashTraits::hash_function(const Variable& Key) const
{
    size_t HashedKey;

    HashedKey = ce::hash_traits<wstring>::hash_function(Key.m_strName);

    if(Key.m_fChannel)
    {
        HashedKey += ce::hash_traits<wstring>::hash_function(Key.m_strChannel);
    }

    return HashedKey;
}
