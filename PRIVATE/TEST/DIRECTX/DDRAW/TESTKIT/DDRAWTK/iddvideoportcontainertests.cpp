//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//

#include "main.h"

#include <DDrawUty.h>
#include <QATestUty/WinApp.h>
#include <list>

#include "DDrawTK.h"

////////////////////////////////////////////////////////////////////////////////
// Macros
//  
#if CFG_TEST_INVALID_PARAMETERS
    #define INVALID_PARAM_TEST_RET(__CALL, __HREXPECTED) \
        hr = __CALL; \
        QueryForHRESULT(hr, __HREXPECTED, TEXT(#__CALL), tr |= trFail)
    #define INVALID_PARAM_TEST(__CALL) \
        INVALID_PARAM_TEST_RET(__CALL, DDERR_INVALIDPARAMS)
#endif

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;

namespace Test_DDVideoPortContainer
{
    eTestResult CTest_SimpleDDVideoPortContainer::TestIDirectDraw()
    {
        eTestResult tr = trPass;
        HRESULT hr;

        if ((DDCAPS2_VIDEOPORT & g_DDConfig.HALCaps().dwCaps2) ||
            (DDCAPS2_VIDEOPORT & g_DDConfig.HELCaps().dwCaps2))
        {
            hr = m_piDD->QueryInterface(IID_IDDVideoPortContainer, (void**)m_piDDVPC.AsTestedOutParam());
            CheckHRESULT(hr, "QI for IDDVideoPortContainer", trFail);
            CheckCondition(m_piDDVPC.InvalidOutParam(), "QI Succeeded, but didn't fill out param", trFail);

            tr |= RunVideoPortContainerTest();
        }
        else
        {
            dbgout << "VideoPorts not supported.  Skipping..." << endl;
            tr = trSkip;
        }
            
        return tr;
    }
}


namespace Test_IDDVideoPortContainer
{
    namespace 
    {
        ////////////////////////////////////////////////////////////////////////////////
        // CompareVideoPortCompatability
        //  
        //
        bool CompareVideoPortCompatability(DDVIDEOPORTCAPS &ddvpcapsRequested, DDVIDEOPORTCAPS &ddvpcapsActual)
        {
            bool result = true;
            DWORD flags = ddvpcapsRequested.dwFlags;

            if ((flags & DDVPD_WIDTH) && (ddvpcapsRequested.dwMaxWidth != ddvpcapsActual.dwMaxWidth))
            {
                Debug(LOG_FAIL,
                      TEXT("\tThe actual width (%ud) != the requested width (%ud) %s\n"),
                      ddvpcapsActual.dwMaxWidth,
                      ddvpcapsRequested.dwMaxWidth,
                      FILELOC);
                result = false;
            }

            if ((flags & DDVPD_HEIGHT) && (ddvpcapsRequested.dwMaxHeight != ddvpcapsActual.dwMaxHeight ))
            {
                Debug(LOG_FAIL,
                      TEXT("\tThe actual height (%ud) != the requested height (%ud) %s\n"),
                      ddvpcapsActual.dwMaxHeight,
                      ddvpcapsRequested.dwMaxHeight,
                      FILELOC);
                result = false;
            }

            if ((flags & DDVPD_ID) && (ddvpcapsRequested.dwVideoPortID != ddvpcapsActual.dwVideoPortID))
            {
                Debug(LOG_FAIL,
                      TEXT("\tThe actual VideoPortID (%ud) != the requested VideoPortID (%ud) %s\n"),
                      ddvpcapsActual.dwVideoPortID,
                      ddvpcapsRequested.dwVideoPortID,
                      FILELOC);
                result = false;
            }

            if ((flags & DDVPD_CAPS) && ((ddvpcapsRequested.dwCaps & ddvpcapsActual.dwCaps) != ddvpcapsRequested.dwCaps))
            {
                Debug(LOG_FAIL,
                      TEXT("\tThe actual port capabilities (%ud) != the requested port capabilities (%ud) %s\n"),
                      ddvpcapsActual.dwCaps,
                      ddvpcapsRequested.dwCaps,
                      FILELOC);
                result = false;
            }

            if ((flags & DDVPD_FX) && ((ddvpcapsRequested.dwFX & ddvpcapsActual.dwFX) != ddvpcapsRequested.dwFX ))
            {
                Debug(LOG_FAIL,
                      TEXT("\tThe actual port FX capabilities (%ud) != the requested port FX capabilities (%ud) %s\n"),
                      ddvpcapsActual.dwCaps,
                      ddvpcapsRequested.dwCaps,
                      FILELOC);
                result = false;
            }

            return result;
        }

        
        namespace privGetVideoPorts
        {
            struct CContext
            {
                eTestResult m_tr;
                std::vector<DWORD>& m_vectVideoPortIds;
                
                CContext(std::vector<DWORD>& vectVideoPortIds) :
                    m_tr(trFail),
                    m_vectVideoPortIds(vectVideoPortIds) { };
            };

            // Callback (callback for first enumeration test)
            HRESULT CALLBACK Callback(LPDDVIDEOPORTCAPS lpDDVideoPortCaps, LPVOID pvContext)
            {
                eTestResult tr = trPass;

                CheckCondition(NULL == pvContext, "NULL Context", DDENUMRET_CANCEL);
                
                CContext &Context = *reinterpret_cast<CContext*>(pvContext);
                
                QueryCondition(NULL == lpDDVideoPortCaps, "lpDDVideoPortCaps is NULL", tr |= trFail)
                else
                    Context.m_vectVideoPortIds.push_back(lpDDVideoPortCaps->dwVideoPortID);

                Context.m_tr = tr;
                return DDENUMRET_OK;
            }
        }

        eTestResult GetVideoPorts(IN  IDDVideoPortContainer* piVideoPortContainer, 
                                  OUT std::vector<DWORD> &vectVideoPortIds)
        {
            using namespace privGetVideoPorts;

            HRESULT hr;
            
            vectVideoPortIds.clear();
            CContext Context(vectVideoPortIds);
            hr = piVideoPortContainer->EnumVideoPorts(0, NULL, &Context, Callback);
            CheckHRESULT(hr, "EnumVideoPorts", trFail);
            CheckCondition(trPass != Context.m_tr, "GetVideoPorts Failure", Context.m_tr);
            CheckCondition(0 == vectVideoPortIds.size(), "No Video Ports Found", trFail);

            dbgout << "Found " << vectVideoPortIds.size() << " Video Ports" << endl;
            
            return trPass;
        }

    } // unnamed namespace 


    ////////////////////////////////////////////////////////////////////////////////
    // CTest_CreateVideoPort::TestIDDVideoPortContainer
    //  Tests the CreateVideoPort Method.
    //
    eTestResult CTest_CreateVideoPort::TestIDDVideoPortContainer()
    {
        CComPointer<IDirectDrawVideoPort> piDDVP;
        std::vector<DWORD> vectVideoPortIds;
        std::vector<DWORD>::iterator itr;
        CDDVideoPortDesc cddvpd;
        eTestResult tr = trPass;
        HRESULT hr;
        int i = 0,
            j = 0;
        
        tr |= GetVideoPorts(m_piDDVPC.AsInParam(), vectVideoPortIds);
        CheckCondition(trPass != tr, "GetVideoPorts Failure", tr);

#       if CFG_TEST_INVALID_PARAMETERS
        {
            dbgout << "Testing invalid args" << endl;
            INVALID_PARAM_TEST(m_piDDVPC->CreateVideoPort(0, NULL, piDDVP.AsTestedOutParam(), NULL));
            INVALID_PARAM_TEST(m_piDDVPC->CreateVideoPort(0, &cddvpd, NULL, NULL));
            INVALID_PARAM_TEST_RET(m_piDDVPC->CreateVideoPort(0, &cddvpd, piDDVP.AsTestedOutParam(), (IUnknown*)0xBAADF00D), CLASS_E_NOAGGREGATION);
        }
#       endif

        for (itr  = vectVideoPortIds.begin();
             itr != vectVideoPortIds.end();
             itr++)
        {
            dbgout << "Testing Video Port #" << i++ << indent;
            
            DWORD dwEntries = 0;
            hr = m_piDDVPC->GetVideoPortConnectInfo(*itr, &dwEntries, NULL);
            QueryHRESULT(hr, "GetVideoPortConnectInfo with NULL lpConnectInfo", tr |= trFail);
            dbgout << "Port has " << dwEntries << " connections" << endl;

            CDDVideoPortConnect *pddvpc = new CDDVideoPortConnect[dwEntries];
            QueryCondition(NULL == pddvpc, "Out of Memory", trAbort);

            hr = m_piDDVPC->GetVideoPortConnectInfo(*itr, &dwEntries, pddvpc);
            QueryHRESULT(hr, "GetVideoPortConnectInfo", tr |= trFail);

            for (j = 0; j < (int)dwEntries && tr == trPass; j++)
            {
                dbgout << "Testing Connection #" << j << endl;
                
                cddvpd.dwFieldWidth = 640;
                cddvpd.dwVBIWidth = 0;
                cddvpd.dwFieldHeight = 240;
                cddvpd.dwMicrosecondsPerField = 1000 * 1000 / 60; // ???
                cddvpd.dwMaxPixelsPerSecond = 0xFFFFFFF7; // ????
                cddvpd.dwVideoPortID = *itr;
                cddvpd.VideoPortType.dwPortWidth = pddvpc[j].dwPortWidth;
                cddvpd.VideoPortType.guidTypeID = pddvpc[j].guidTypeID;
                cddvpd.VideoPortType.dwFlags = pddvpc[j].dwFlags & (DDVPCONNECT_VACT | DDVPCONNECT_INTERLACED);

                // Try with NULL creation flags
                dbgout << "Testing Flags = NULL" << endl;
                hr = m_piDDVPC->CreateVideoPort(NULL, &cddvpd, piDDVP.AsTestedOutParam(), NULL);
                QueryHRESULT(hr, "CreateVideoPort", tr |= trFail);
                QueryCondition(piDDVP.InvalidOutParam(), "CreateVideoPort didn't fill out param", tr |= trFail);

                // If the VREF data is not automatically discarded
                if (!(pddvpc[j].dwFlags & DDVPCONNECT_DISCARDSVREFDATA))
                {   
                    // Try to get only the VBI data
                    dbgout << "Testing Flags = DDVPCREATE_VBIONLY" << endl;
                    hr = m_piDDVPC->CreateVideoPort(DDVPCREATE_VBIONLY, &cddvpd, piDDVP.AsTestedOutParam(), NULL);
                    QueryHRESULT(hr, "CreateVideoPort", tr |= trFail);
                    QueryCondition(piDDVP.InvalidOutParam(), "CreateVideoPort didn't fill out param", tr |= trFail);
                }

                // Try to get only the video data
                dbgout << "Testing Flags = DDVPCREATE_VIDEOONLY" << endl;
                hr = m_piDDVPC->CreateVideoPort(DDVPCREATE_VIDEOONLY, &cddvpd, piDDVP.AsTestedOutParam(), NULL);
                QueryHRESULT(hr, "CreateVideoPort", tr |= trFail);
                QueryCondition(piDDVP.InvalidOutParam(), "CreateVideoPort didn't fill out param", tr |= trFail);

                piDDVP.ReleaseInterface();
            }

            dbgout << unindent << "Done Video Port" << endl;

            delete[] pddvpc;
        }
        
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // privTestEnumVideoPorts_All
    //  Enumerates all video ports.
    //
    namespace privTestEnumVideoPorts_All
    {
        // CContext (context class for first enumeration test)
        struct CContext
        {
            int m_nPortCount;
            enumTestResult m_tr;
            std::list<DDVIDEOPORTCAPS> &m_lstVideoPortCapabilities;

            CContext(std::list<DDVIDEOPORTCAPS> &lstVideoPortCapabilities) :
                m_nPortCount(0),
                m_tr(trFail),
                m_lstVideoPortCapabilities(lstVideoPortCapabilities) { };
        };
        
        // Callback (callback for first enumeration test)
        HRESULT CALLBACK Callback(LPDDVIDEOPORTCAPS lpDDVideoPortCaps, LPVOID pvContext)
        {
            eTestResult tr = trPass;

            CheckCondition(NULL == pvContext, "pvContext is NULL", DDENUMRET_CANCEL);
            
            CContext &Context = *reinterpret_cast<CContext*>(pvContext);
            
            QueryCondition(NULL == lpDDVideoPortCaps, "lpDDVideoPortCaps is NULL", tr |= trFail)
            else
            {
                // display the device's capabilities
                dbgout  << "VideoPort #" << Context.m_nPortCount
                        << ": " << *lpDDVideoPortCaps << endl;

                // add the capabilities to the list of capabilities
                Context.m_lstVideoPortCapabilities.push_back(*lpDDVideoPortCaps);

                Context.m_nPortCount++;
            }

            Context.m_tr = tr;

            return DDENUMRET_OK;
        }

        // Test
        enumTestResult Test(IN  IDDVideoPortContainer* piVideoPortContainer,
                            OUT std::list<DDVIDEOPORTCAPS> &lstVideoPortCapabilities)
        {
            HRESULT hr;
            CContext Context(lstVideoPortCapabilities);

            dbgout << "Enumerating All Video Ports" << indent;

            hr = piVideoPortContainer->EnumVideoPorts(0, NULL, &Context, Callback);
            CheckHRESULT(hr, "EnumVideoPorts", trFail);
            CheckCondition(trPass != Context.m_tr, "Failure during callback", Context.m_tr);

            dbgout << unindent << "Done All Video Ports" << endl;

            return trPass;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // privTestEnumVideoPorts_Specific
    //  Enumerates specific video ports.
    //
    namespace privTestEnumVideoPorts_Specific
    {
        // Context (Context for second enumeration test)
        struct CContext
        {
            int m_nPortCount;
            enumTestResult m_tr;
            DDVIDEOPORTCAPS m_ddvpcaps;

            CContext(DDVIDEOPORTCAPS ddvpcaps) :
                m_nPortCount(0),
                m_tr(trFail),
                m_ddvpcaps(ddvpcaps) { };
        };

        // Callback (Callback for second enumeration test)
        HRESULT CALLBACK Callback(LPDDVIDEOPORTCAPS lpDDVideoPortCaps, LPVOID pvContext)
        {
            eTestResult tr = trPass;

            CheckCondition(NULL == pvContext, "pvContext is NULL", DDENUMRET_CANCEL);
            
            CContext &Context = *reinterpret_cast<CContext*>(pvContext);
            
            QueryCondition(NULL == lpDDVideoPortCaps, "lpDDVideoPortCaps is NULL", tr |= trFail)
            else
            {
                dbgout << "Comparing Expected vs. Recieved" << endl;
                // compare the context capabilities to the port's in capabilities.
                if (!CompareVideoPortCompatability(Context.m_ddvpcaps, *lpDDVideoPortCaps))
                    tr |= trFail;

                Context.m_nPortCount++;
            }
        
            Context.m_tr = tr;
            
            return DDENUMRET_OK;
        }

        // Test 
        enumTestResult Test(IN  IDDVideoPortContainer* piVideoPortContainer, 
                            OUT std::list<DDVIDEOPORTCAPS> &lstVideoPortCapabilities)
        {
            HRESULT hr;
            std::list<DDVIDEOPORTCAPS>::iterator itrVPCaps;

            dbgout << "Enumerating Video Ports with specific capabilities" << indent;

            for (itrVPCaps  = lstVideoPortCapabilities.begin();
                 itrVPCaps != lstVideoPortCapabilities.end();
                 itrVPCaps++)
            {
                CContext Context(*itrVPCaps);

                hr = piVideoPortContainer->EnumVideoPorts(0, &(*itrVPCaps), &Context, Callback);
                if (FAILED(hr))
                {
                    QueryHRESULT(hr, "EnumVideoPorts", dbgout << "Caps = " << *itrVPCaps << endl);
                    return trFail;
                }

                CheckCondition(trPass != Context.m_tr, "Failure during enumeration", Context.m_tr);
                QueryCondition(Context.m_nPortCount == 0, "No ports enumerated", dbgout << "Caps = " << *itrVPCaps << endl; return trFail);
            }

            dbgout << unindent << "Done specific Video Ports" << endl;
            
            return trPass;
        }
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // privTestEnumVideoPorts_Dummy
    //  Dummy callback for invalid parameters.
    //
    namespace privTestEnumVideoPorts_Dummy
    {
        // Callback (Callback for dummy enumeration test)
        HRESULT CALLBACK Callback(LPDDVIDEOPORTCAPS lpDDVideoPortCaps, LPVOID pvContext)
        {
            dbgout(LOG_FAIL) << "Dummy Callback called" << endl;
            return DDENUMRET_OK;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_EnumVideoPorts::TestIDDVideoPortContainer
    //  Tests the CreateVideoPort Method.
    //
    eTestResult CTest_EnumVideoPorts::TestIDDVideoPortContainer()
    {
        std::list<DDVIDEOPORTCAPS> lstVideoPortCapabilities;
        eTestResult tr = trPass;

#       if CFG_TEST_INVALID_PARAMETERS
        {
            HRESULT hr;
            dbgout << "Testing invalid args" << endl;
            INVALID_PARAM_TEST(m_piDDVPC->EnumVideoPorts(0, NULL, NULL, NULL));
            INVALID_PARAM_TEST(m_piDDVPC->EnumVideoPorts(0xBAADF00D, NULL, NULL, NULL));
            hr = m_piDDVPC->EnumVideoPorts(0xBAADF00D, NULL, NULL, privTestEnumVideoPorts_Dummy::Callback);
            QueryForHRESULT(hr, DDERR_INVALIDPARAMS, "EnumVideoPorts with garbage flags", tr |= trFail)
            
            CDDVideoPortCaps cddvpc;
            cddvpc.dwSize = 0xBAADF00D;
            INVALID_PARAM_TEST(m_piDDVPC->EnumVideoPorts(0, &cddvpc, NULL, NULL));
            INVALID_PARAM_TEST(m_piDDVPC->EnumVideoPorts(0, &cddvpc, NULL, privTestEnumVideoPorts_Dummy::Callback));
            INVALID_PARAM_TEST(m_piDDVPC->EnumVideoPorts(0xBAADF00D, &cddvpc, NULL, NULL));
            INVALID_PARAM_TEST(m_piDDVPC->EnumVideoPorts(0xBAADF00D, &cddvpc, NULL, privTestEnumVideoPorts_Dummy::Callback));
            cddvpc.dwSize = 0;
            INVALID_PARAM_TEST(m_piDDVPC->EnumVideoPorts(0, &cddvpc, NULL, NULL));
            INVALID_PARAM_TEST(m_piDDVPC->EnumVideoPorts(0, &cddvpc, NULL, privTestEnumVideoPorts_Dummy::Callback));
            INVALID_PARAM_TEST(m_piDDVPC->EnumVideoPorts(0xBAADF00D, &cddvpc, NULL, NULL));
            INVALID_PARAM_TEST(m_piDDVPC->EnumVideoPorts(0xBAADF00D, &cddvpc, NULL, privTestEnumVideoPorts_Dummy::Callback));
        }
#       endif

        // Enumerate ALL video ports
        tr |= privTestEnumVideoPorts_All::Test(m_piDDVPC.AsInParam(), lstVideoPortCapabilities);
        QueryCondition(trPass != tr, "Enumerate All Ports Failed", 0);
        
        // Enumerating Video Ports with specific capabilities
        tr |= privTestEnumVideoPorts_Specific::Test(m_piDDVPC.AsInParam(), lstVideoPortCapabilities);
        QueryCondition(trPass != tr, "Enumerate Specific Ports Failed", 0);

        // Todo: implement test to enumerate for ports with unsupported capabilities 
        // (enumeration should not occur)
        
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetVideoPortConnectInfo::TestIDDVideoPortContainer
    //  Tests the CreateVideoPort Method.
    //
    eTestResult CTest_GetVideoPortConnectInfo::TestIDDVideoPortContainer()
    {
        std::vector<DWORD>::iterator itr;
        std::vector<DWORD> vectVideoPortIds;
        eTestResult tr = trPass;
        DWORD dwEntries,
              dwTemp;
        HRESULT hr;
        int i = 0,
            j = 0;
        
        tr |= GetVideoPorts(m_piDDVPC.AsInParam(), vectVideoPortIds);
        CheckCondition(trPass != tr, "GetVideoPorts Failure", trAbort);

#       if CFG_TEST_INVALID_PARAMETERS
        {
            CDDVideoPortConnect rgcddvpc[10];
            
            dbgout << "Testing invalid args" << endl;
            dwEntries = countof(rgcddvpc);
            INVALID_PARAM_TEST(m_piDDVPC->GetVideoPortConnectInfo(-1, &dwEntries, rgcddvpc));
            INVALID_PARAM_TEST(m_piDDVPC->GetVideoPortConnectInfo(vectVideoPortIds.size(), &dwEntries, rgcddvpc));
            dwEntries = 0;
            INVALID_PARAM_TEST(m_piDDVPC->GetVideoPortConnectInfo(0, &dwEntries, rgcddvpc));
        }
#       endif

        for (itr  = vectVideoPortIds.begin();
             itr != vectVideoPortIds.end();
             itr++)
        {
            dbgout << "Testing Video Port #" << i++ << endl;

            // Get the count of entries
            dwEntries = 0;
            hr = m_piDDVPC->GetVideoPortConnectInfo(*itr, &dwEntries, NULL);
            QueryHRESULT(hr, "GetVideoPortConnectInfo with NULL lpConnectInfo", tr |= trFail);
            dbgout << "Port has " << dwEntries << " connections" << endl;

            CDDVideoPortConnect *pddvpc1 = new CDDVideoPortConnect[dwEntries+1],
                                *pddvpc2 = new CDDVideoPortConnect[dwEntries+1];
            QueryCondition(NULL == pddvpc1 || NULL == pddvpc2, "Out of Memory", delete[] pddvpc1; delete[] pddvpc2; return trAbort);

            // Set the final (unused) entry to known data
            // HACKHACK:
            // GetVideoPortConnectInfo appears to zero out the dwReserved 
            // member for all elements in the passed array, regardless of
            // whether or not it's going to fill in that particular elements.
            // This doesn't seem like a .
            CDDVideoPortConnect cddvpcTemp;
            cddvpcTemp.Preset();
            cddvpcTemp.dwReserved1 = 0;
            pddvpc1[dwEntries] = cddvpcTemp;

            // Get the connection info, offering one more slot than required
            dwTemp = dwEntries + 1;
            hr = m_piDDVPC->GetVideoPortConnectInfo(*itr, &dwTemp, pddvpc1);
            QueryHRESULT(hr, "GetVideoPortConnectInfo", tr |= trFail);

            // Verify elements past end are not modified
            QueryCondition(pddvpc1[dwEntries] != cddvpcTemp, "GetVideoPortConnectionInfo overstepped array", tr |= trFail);

            // If there are multiple entries, test an array that's too small
            if (dwEntries > 1)
            {
                DWORD dwLower = dwEntries - 1;
                
                dbgout << "Testing asking for only " << dwLower << " entries" << endl;

                // Set the unused arrays entries to known data
                for (j = dwLower; j < (int)dwEntries + 1; j++)
                    pddvpc2[j].Preset();
                    
                // Get the connection info, offering one less entry than required
                dwTemp = dwLower;
                hr = m_piDDVPC->GetVideoPortConnectInfo(*itr, &dwTemp, pddvpc2);
                QueryForHRESULT(hr, DDERR_MOREDATA, "GetVideoPortConnectInfo didn't return DDERR_MOREDATA", tr |= trFail);
                QueryCondition(dwTemp != dwEntries, "GetVideoPortConnectInfo didn't correct count", tr |= trFail);
                
                // Verify elements past end are not modified
                for (j = dwLower; j < (int)dwEntries + 1; j++)
                    QueryCondition(!pddvpc2[j].IsPreset(), "GetVideoPortConnectionInfo overstepped array", tr |= trFail);
            }

            // Get the connection info offering as many entries as required
            dwTemp = dwEntries;
            hr = m_piDDVPC->GetVideoPortConnectInfo(*itr, &dwTemp, pddvpc2);
            QueryHRESULT(hr, "GetVideoPortConnectInfo", tr |= trFail);

            // Verify that the two arrays from the two calls match
            for (j = 0; j < (int)dwEntries; j++)
            {
                QueryCondition(0 != memcmp(&pddvpc1[j], &pddvpc2[j], sizeof(pddvpc1[j])), "Info doesn't match", tr |= trFail);
                dbgout << "Connection #" << j << ": " << pddvpc1[j] << endl;
            }

            delete[] pddvpc2;
            delete[] pddvpc1;

        }
        
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_QueryVideoPortStatus::TestIDDVideoPortContainer
    //  Tests the CreateVideoPort Method.
    //
    eTestResult CTest_QueryVideoPortStatus::TestIDDVideoPortContainer()
    {
        std::vector<DWORD>::iterator itr;
        std::vector<DWORD> vectVideoPortIds;
        eTestResult tr = trPass;
        HRESULT hr;
        int i = 0;
        
        tr |= GetVideoPorts(m_piDDVPC.AsInParam(), vectVideoPortIds);
        CheckCondition(trPass != tr, "GetVideoPorts Failure", trAbort);

#       if CFG_TEST_INVALID_PARAMETERS
        {
            CDDVideoPortStatus cddvps;
            
            dbgout << "Testing invalid args" << endl;
            INVALID_PARAM_TEST(m_piDDVPC->QueryVideoPortStatus(-1, &cddvps));
            INVALID_PARAM_TEST(m_piDDVPC->QueryVideoPortStatus(vectVideoPortIds.size(), &cddvps));
            // REVIEW: These should probably return DDERR_INVALIDPARAMS, instead
            // of DDERR_INVALIDOBJECT for consistancy's sake, but...
            INVALID_PARAM_TEST_RET(m_piDDVPC->QueryVideoPortStatus(0, NULL), DDERR_INVALIDOBJECT);
            cddvps.dwSize = 0xBAADF00D;
            INVALID_PARAM_TEST_RET(m_piDDVPC->QueryVideoPortStatus(-1, &cddvps), DDERR_INVALIDOBJECT);
            INVALID_PARAM_TEST_RET(m_piDDVPC->QueryVideoPortStatus(vectVideoPortIds.size(), &cddvps), DDERR_INVALIDOBJECT);
            cddvps.dwSize = 0;
            INVALID_PARAM_TEST_RET(m_piDDVPC->QueryVideoPortStatus(-1, &cddvps), DDERR_INVALIDOBJECT);
            INVALID_PARAM_TEST_RET(m_piDDVPC->QueryVideoPortStatus(vectVideoPortIds.size(), &cddvps), DDERR_INVALIDOBJECT);
        }
#       endif

        for (itr  = vectVideoPortIds.begin();
             itr != vectVideoPortIds.end();
             itr++)
        {
            dbgout << "Testing Video Port #" << i++ << endl;

            CDDVideoPortStatus cddvps1,
                               cddvps2;
            cddvps1.Preset();
            hr = m_piDDVPC->QueryVideoPortStatus(*itr, &cddvps1);
            CheckHRESULT(hr, "QueryVideoPortStatus", trFail);
            CheckCondition(cddvps1.IsPreset(), "QueryVideoPortStatus didn't fill status", trFail);
            
            cddvps2.Preset();
            hr = m_piDDVPC->QueryVideoPortStatus(*itr, &cddvps2);
            CheckHRESULT(hr, "QueryVideoPortStatus", trFail);
            CheckCondition(cddvps2.IsPreset(), "QueryVideoPortStatus didn't fill status", trFail);

            CheckCondition(cddvps1 != cddvps2, "Video PortStatus not consistant", trFail);

            dbgout << "Found Status : " << cddvps1 << endl;
        }
        
        return tr;
    }
}
