//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//
//      PerfTestUtils.cpp
//      
//      
//


#include <XmlDocument.h>
#include <PerfTestUtils.h>

extern CKato* g_pKato;

static XmlDocument g_doc;
static XmlElement *g_pTestCaseElement = NULL;

#define XML_TEST_CASE L"TC"
#define XML_ELEMENT_NAME L"ElementName"
#define XML_ID L"id"

#define RET_FAILED_HR(func_call, text)       \
    {                                        \
        hr = func_call;                      \
        if(FAILED(hr))                       \
        {                                    \
            g_pKato->Log(LOG_COMMENT, text); \
            LOG_LOCATION();                  \
            return hr;                       \
        }                                    \
    }

#define EXIT_FAILED_HR(func_call, text)       \
    {                                        \
        hr = func_call;                      \
        if(FAILED(hr))                       \
        {                                    \
            g_pKato->Log(LOG_COMMENT, text); \
            LOG_LOCATION();                  \
            goto Exit;                       \
        }                                    \
    }

extern HMODULE g_hInstance;

static HRESULT GetMemberType(WCHAR szXmlAttrName[], IID& riidMemberType, WCHAR szMemberType[], bool& overrideFail);
static bool SplitString(WCHAR szInput[], WCHAR* szOutput1[], WCHAR* szOutput2[]);

HRESULT GetTargetElement(IXRVisualHost *pHost, WCHAR *pszName, IXRDependencyObject **ppDO)
{
    HRESULT    hr = E_FAIL;
    IXRFrameworkElement    *pFrameworkElement = NULL;

    if (ppDO == NULL)
        goto Exit;

    hr = pHost->GetRootElement(&pFrameworkElement);
    if (FAILED(hr) || (pFrameworkElement == NULL))
        goto Exit;

    hr = pFrameworkElement->FindName(pszName, ppDO);
    if (FAILED(hr) || (*ppDO == NULL))
        goto Exit;

    hr = S_OK;

Exit:
    SAFE_RELEASE(pFrameworkElement);
    return hr;

}

/* called once when dll is loaded */
bool LoadDataFile(SPS_SHELL_INFO *spParam)
{
    if (FAILED(g_doc.Load((PTSTR)spParam->szDllCmdLine))) {  //L"SampleTests.xml"
        g_pKato->Log(LOG_COMMENT,L"Failed to load xml file: %s", spParam->szDllCmdLine);
        return false;
    }

    return true;
}

bool LoadDataFile(wchar_t* spParam)
{
    if (FAILED(g_doc.Load((PTSTR)spParam))) {  //L"SampleTests.xml"
        g_pKato->Log(LOG_COMMENT,L"Failed to load xml file: %s", spParam);
        return false;
    }

    return true;
}

/* get attribute value from test case */
HRESULT GetAttributeStringValue(WCHAR* pAttrName, WCHAR* pszAttrValue, size_t AttrValueSize)
{
    XmlElement *pTestCase = g_pTestCaseElement;

    HRESULT hr = E_FAIL;
    XmlAttribute Attribute;
    XmlAttributeCollection* pAttributes = NULL;
    LONG AttrCount = 0;
    LONG i = 0;
    WCHAR szName[MAX_PATH] = {0};
    
    if(!pTestCase)
    {
        g_pKato->Log(LOG_COMMENT, L"pTestCase is NULL");
        return E_FAIL;
    }

    EXIT_FAILED_HR(pTestCase->GetAttributes(&pAttributes), L"Failed to get attributes.");
    EXIT_FAILED_HR(pAttributes->GetLength(&AttrCount), L"Failed to get attributes count.");

    for(i=0; i<AttrCount; i++)
    {
        EXIT_FAILED_HR(pAttributes->GetItem(&Attribute, i), L"Failed to get attribute.");
        EXIT_FAILED_HR(Attribute.GetName(szName, MAX_PATH), L"Failed to get attribute name.");
        if(!wcsicmp(szName, pAttrName))
        {
            break;
        }
    }
    if(i>=AttrCount)
    {
        g_pKato->Log(LOG_COMMENT, L"Failed to find attribute %s.", pAttrName);
        hr = E_FAIL;
        goto Exit;
    }
    EXIT_FAILED_HR(Attribute.GetValue(pszAttrValue, AttrValueSize), L"Failed to get attribute value.");

    hr = S_OK;

Exit:
    CLEAN_PTR(pAttributes);
    return hr;
}

/* called once before each test case */
bool SetTestCaseData(SPS_BEGIN_TEST *spParam)
{
    DWORD    dwTestCaseId = spParam->lpFTE->dwUserData;
    XmlElement        *pDocElement = NULL;
    XmlElementList    *pDocChildren = NULL;
    XmlElement        *pTestCasesElement = NULL;
    XmlElementList    *pTestCasesChildren = NULL;
    XmlElement        *pTestCase = NULL;
    LONG    ChildrenCount = 0,i;
    WCHAR    szStr[MAX_PATH], szNumber[MAX_PATH];
    bool    Ret = false;

    if (g_pTestCaseElement)
    {
        g_pKato->Log(LOG_COMMENT,L"Failed to set test case data for test case #%d. Call CleanTestCaseData first.", dwTestCaseId);
        goto Error;
    }

    if (FAILED(g_doc.GetDocumentElement(&pDocElement)))
    {
        g_pKato->Log(LOG_COMMENT,L"Failed to get Document element for test case #%d", dwTestCaseId);
        goto Error;
    }
    if (FAILED(pDocElement->GetChildren(&pDocChildren)))
    {
        g_pKato->Log(LOG_COMMENT,L"Failed to get Document element children for test case #%d", dwTestCaseId);
        goto Error;
    }
    if (FAILED(pDocChildren->GetLength(&ChildrenCount)))
    {
        g_pKato->Log(LOG_COMMENT,L"Failed to get Document element children's count  for test case #%d", dwTestCaseId);
        goto Error;
    }

    _ultow_s((ulong)dwTestCaseId,szNumber, 10);

    for (i=0; i<ChildrenCount;i++)
    {
        CLEAN_PTR(pTestCase); //clean previous interation

        if (FAILED(pDocChildren->GetItem(&pTestCase, i)))
        {
            g_pKato->Log(LOG_COMMENT,L"Failed to get TC element #%d for test case #%d", i,dwTestCaseId);
            goto Error;
        }
        if (FAILED(pTestCase->GetTagName(szStr, MAX_PATH)))
        {
            g_pKato->Log(LOG_COMMENT,L"Failed to get TestCases element child tag for test case #%d", dwTestCaseId);
            goto Error;
        }
        if (wcsicmp(XML_TEST_CASE,szStr)) //not TC
        {
            continue;
        }

        g_pTestCaseElement = pTestCase; //GetAttributeStringValue is using g_pTestCaseElement
        HRESULT hr = GetAttributeStringValue(XML_ID, szStr, MAX_PATH);
        g_pTestCaseElement = NULL;

        if (FAILED(hr))
        {
            g_pKato->Log(LOG_COMMENT,L"Failed to get TestCase id for for test case #%d (GetAttributeStringValue failed).",dwTestCaseId);
            goto Error;
        }
        if (!wcsicmp(szNumber,szStr)) //found
            break;
    }
    if (i >= ChildrenCount)
    {
        g_pKato->Log(LOG_COMMENT,L"Failed to find test case data for test case #%d", dwTestCaseId);
        goto Error;
    }

    g_pTestCaseElement = pTestCase;
    Ret = true;
    goto Exit;

Error:
    CLEAN_PTR(pTestCase);
Exit:
    CLEAN_PTR(pDocChildren);
    CLEAN_PTR(pDocElement);
    
    return Ret;
}

/* called once after each test case */
void CleanTestCaseData()
{
    CLEAN_PTR(g_pTestCaseElement);
}

/* get name of target element from test case data */
HRESULT GetTargetElementName(WCHAR *pszName, size_t NameSize)
{
    return GetAttributeStringValue(XML_ELEMENT_NAME, pszName, MAX_PATH);
}

HRESULT Getint(WCHAR *pszAttrName, int *pValue)
{
    int        Value;
    WCHAR    szInt[MAX_PATH];
    int        count;
    HRESULT    hr = E_FAIL;

    RET_FAILED_HR(GetAttributeStringValue(pszAttrName, szInt, MAX_PATH),L"GetAttributeStringValue failed.");

    // get the int value, count should be 1.
    count = swscanf_s(szInt, L"%i", &Value);
    if (count != 1)
    {
        g_pKato->Log(LOG_COMMENT,L"Attribute %s value %s cannot be converted to int.",pszAttrName, szInt); 
        return E_FAIL;
    }
    *pValue = Value;
    return S_OK;
}

HRESULT Getbool(WCHAR *pszAttrName, bool *pValue)
{
    WCHAR    szBool[MAX_PATH];
    HRESULT    hr = E_FAIL;

    RET_FAILED_HR(GetAttributeStringValue(pszAttrName, szBool, MAX_PATH),L"GetAttributeStringValue failed.");

    // compare the passed in value to wide char "True" and "False" strings, set pValue accordingly.
    if (!wcsicmp(szBool,L"True"))
        *pValue = true;
    else if (!wcsicmp(szBool,L"False"))
        *pValue = false;
    else
    {
        g_pKato->Log(LOG_COMMENT,L"Attribute %s value %s cannot be converted to bool.",pszAttrName, szBool); 
        return E_FAIL;
    }
    return S_OK;
}

HRESULT GetWCHAR(WCHAR* pszAttrName, WCHAR* pValue, size_t maxLen)
{
    HRESULT hr = E_FAIL;

    RET_FAILED_HR(GetAttributeStringValue(pszAttrName, pValue, maxLen), L"GetAttributeStringValue failed.");
    return S_OK;
}
