//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <stdlib.h>
#include <stdio.h>
#ifndef UNDER_CE
#include <limits.h>
#include <float.h>
#include <objbase.h>
#endif
#include <windows.h>
#include <tchar.h>
#include <msxml2.h>
#include <stressutils.h>

#undef srand
#undef rand


#define THREAD_WAIT_INTERVAL	(60*1000)
#define ONEKB					1024

HANDLE	g_hTimesUp	= NULL;

CRITICAL_SECTION	g_csResults;

UINT DoStressIteration(void);
DWORD WINAPI TestThreadProc(LPVOID pv);

int
wmain(
	int		argc,
	WCHAR	**argv
	){
	int				retValue = 0;  // Module return value.  You should return 0 unless you had to abort.
	MODULE_PARAMS	mp;            // Cmd line args arranged here.  Used to init the stress utils.
	STRESS_RESULTS	results;       // Cumulative test results for this module.

	int				i, n;
	int				nExited;       // Used to count the number of threads that have exited cleanly

	DWORD			dwEnd;         // End time	 
	DWORD			dwStartTime = GetTickCount();

	///////////////////////////////////////////////////////
	// Initialization
	//

	ZeroMemory((void*)&mp,sizeof(MODULE_PARAMS));
	ZeroMemory((void*)&results,sizeof(STRESS_RESULTS));


	// Read the cmd line args into a module params struct
	// You must call this before InitializeStressUtils()

	// Be sure to use the correct version, depending on your 
	// module entry point.

	if (!ParseCmdLine_wmain(argc,argv,&mp)){
		LogFail (_T("Failure parsing the command line!  exiting ..."));
		return CESTRESS_ABORT; // Use the abort return code
	}


	// You must call this before using any stress utils 

	InitializeStressUtils(
		_T("s2_xmldomxpathxql.exe"),				// Module name to be used in logging
		LOGZONE(SLOG_SPACE_IE,SLOG_XML),			// Logging zones used by default
		&mp											// Module params passed on the cmd line
		);

	// Note on Logging Zones: 
	//
	// Logging is filtered at two different levels: by "logging space" and
	// by "logging zones".  The 16 logging spaces roughly corresponds to the major
	// feature areas in the system (including apps).  Each space has 16 sub-zones
	// for a finer filtering granularity.
	//
	// Use the LOGZONE(space, zones) macro to indicate the logging space
	// that your module will log under (only one allowed) and the zones
	// in that space that you will log under when the space is enabled
	// (may be multiple OR'd values).
	//
	// See \test\stress\stress\inc\logging.h for a list of available
	// logging spaces and the zones that are defined under each space


	LogVerbose (_T("Starting at %d"), dwStartTime);


	// Module-specific parsing of the 'user' portion of the cmd line.
	//
	// In this case, the user info in the cmd line represents the
	// number of threads that we want to run in this test.

	int nThreads = 1;//mp.tszUser ? min(4, max(1, _ttoi(mp.tszUser))) : 1;


	///////////////////////////////////////////////////////
	// Main test loop
	//


	HANDLE	*hThread	= new HANDLE [nThreads];
	DWORD	*dwThreadId	= new DWORD [nThreads];

	if(NULL == hThread || NULL == dwThreadId){
	    LogWarn2(_T("Memory allocation failed.  Aborting."));
	    retValue = CESTRESS_ABORT;
	    goto cleanup;
	}

	InitializeCriticalSection (&g_csResults);


	// Spin one or more worker threads to do the actually testing.
	LogVerbose(_T("Creating %i worker threads"),nThreads);

	g_hTimesUp = CreateEvent (NULL, TRUE, FALSE, NULL);

	for (i = 0; i < nThreads; i++){
		hThread[i] = CreateThread (NULL, 0, TestThreadProc, &results, 0, &dwThreadId[i]);
		if (hThread[i]){
			LogVerbose(_T("Thread %i (0x%x) successfully created"),i,hThread[i]);
		}
		else{
			LogFail (_T("Failed to create thread %i! Error = %d"),i,GetLastError());
		}
	}

	// Main thread is now the timer
	Sleep (1000/*mp.dwDuration*/);

	// Signal the worker threads that they must finish up
	SetEvent (g_hTimesUp);


	///////////////////////////////////////////////////////
	// Clean-up, report results, and exit
	//
	
	
	// Wait for all of the workers to return.
	
	// NOTE: Don't terminate the threads yourself.  Go ahead and let them hang.
	//       The harness will decide whether to let your module hang or
	//       to terminate the process.

	LogComment (_T("Time's up signaled.  Waiting for threads to finish ..."));

	nExited = 0;

	while(nExited != nThreads){
		for (n = 0; n < nThreads; n++){
			if (hThread[n]){
				if (WAIT_OBJECT_0 == WaitForSingleObject (hThread[n], THREAD_WAIT_INTERVAL)){
					LogVerbose (_T("Thread %i (0x%x) finished"), n, hThread[n]);

					CloseHandle(hThread[n]);
					hThread[n] = NULL;
					nExited++;
				}
			}
		}
	}

	DeleteCriticalSection(&g_csResults);



	// Finish
	
	dwEnd = GetTickCount();
	LogVerbose(
		_T("Leaving at %d, duration (ms) = %d, (sec) = %d, (min) = %d"),
		dwEnd,
		dwEnd - dwStartTime,
		(dwEnd - dwStartTime) / 1000,
		(dwEnd - dwStartTime) / 60000
		);

cleanup:

	if(g_hTimesUp)
		CloseHandle(g_hTimesUp);

	if(hThread)
		delete [] hThread;

	if(dwThreadId)
		delete [] dwThreadId;

	// DON'T FORGET:  You must report your results to the harness before exiting
	ReportResults (&results);

	return retValue;
}

typedef struct{
	LPTSTR	lpszDesc;
	LPWSTR	lpwszQuery;
	LPWSTR	lpwszChecksum;
}VARIATION,*LPVARIATION;

VARIATION g_lpXPathVariation[] = {
	{
		TEXT("XPath 1.0: 2.2 Axes - Axis ancestor-or-self::book should return all books."),
		L"book[1]/author/parent::book",
		L"348.34348486798"
	},
	{
		TEXT("XPath 1.0: 2.2 Axes - Axis ancestor::book should return all book nodes."),
		L"book[1]/title/ancestor::book",
		L"348.34348486798"
	},
	{
		TEXT("XPath 1.0: 2.2 Axes - Axis ancestor::* should return all ancestor nodes."),
		L"book[1]/title/ancestor::*",
		L"453.76833223897"
	},
	{
		TEXT("XPath 1.0: 2.2 Axes - Axis ancestor-or-self::book should return all books."),
		L"book[1]/ancestor-or-self::book",
		L"348.34348486798"
	},
	{
		TEXT("XPath 1.0: 2.2 Axes - Axis ancestor-or-self::title should return title."),
		L"book[1]/title/ancestor-or-self::title",
		L"267.61249609090"
	}
};

int	g_nXPathVariation = sizeof(g_lpXPathVariation)/sizeof(VARIATION);

VARIATION g_lpXQLVariation[] = {
	{
		TEXT("XQL: Collections - ./first-name - Find all first-name elements"),
		L"book[1]/author/./first-name",
		L"259.58892054159"
	},
	{
		TEXT("XQL: Children and Descendants - Find all title elements  from current context."),
		L"book[1]/.//title",
		L"264.99281847876"
	},
	{
		TEXT("XQL: Attributes - Return the current book element."),
		L"book[0]/@style",
		L"252.80279109929"
	},
	{
		TEXT("XQL: Comparisons - Return all author elements whose last names greater than &quot;M*&quot;"),
		L"book/author[last-name $gt$ 'M']",
		L"361.64971707202"
	}
};

int	g_nXQLVariation = sizeof(g_lpXQLVariation)/sizeof(VARIATION);

VARIATION g_lpXSLTVariation[] = {
	{
		TEXT("Attribute Value Template, Base Spec Case - every place an Attribute Value Template is allowed"),
		L"<?xml version=\'1.0\'?><xsl:stylesheet xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" version=\"1.0\"> \
		<xsl:output method=\"xml\" omit-xml-declaration=\"yes\" indent=\"yes\"/>\
		<xsl:template match=\"/\"> \
        <xsl:for-each select=\"bookstore/magazine\"> \
        <xsl:value-of  select=\"./title\"/> \
		<xslTITLE name=\"{title}\"></xslTITLE> \
        </xsl:for-each> \
		</xsl:template> \
		</xsl:stylesheet>",
		L"328.51129007756"
	}
};

int	g_nXSLTVariation = sizeof(g_lpXSLTVariation)/sizeof(VARIATION);


BOOL
DOMLoaded(
	IXMLDOMDocument2	*pXML
	){
    IXMLDOMParseError	*pXMLError	= NULL;
	BSTR				bstrError	= NULL;
    LONG				errorCode	= E_FAIL;
	if(pXML){
		if(SUCCEEDED(pXML->get_parseError(&pXMLError)) && pXMLError){
			pXMLError->get_errorCode(&errorCode);
			pXMLError->get_srcText(&bstrError);
			pXMLError->Release();
			pXMLError	= NULL;
		}
#ifdef UNICODE
		LogComment(TEXT("errorCode = %ld, errorDescr = %s"),errorCode,bstrError);
#else
		LogComment(TEXT("errorCode = %ld, errorDescr = %S"),errorCode,bstrError);
#endif
	}
    return (errorCode == 0) ? TRUE : FALSE;
}

BOOL
CoCreateAndLoad(
	IXMLDOMDocument2	**ppXML,
	BSTR				bstrFileToLoad
	){
	HRESULT			hRes;
	VARIANT			varFileToLoad;
	VARIANT_BOOL	isPass = VARIANT_FALSE;
	BOOL			bCreatedAndLoaded;
	bCreatedAndLoaded = FALSE;
	VariantInit(&varFileToLoad);
	varFileToLoad.vt		= VT_BSTR;
	varFileToLoad.bstrVal	= SysAllocString(bstrFileToLoad);

	if(SUCCEEDED(hRes = CoCreateInstance(CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument2, (void**)ppXML))){
		LogComment(TEXT("obtained IXMLDOMDocument2 pointer"));
#ifdef UNICODE
		LogComment(TEXT("Trying to load %s"),bstrFileToLoad);
#else
		LogComment(TEXT("Trying to load %S"),bstrFileToLoad);
#endif
		(*ppXML)->put_async(VARIANT_FALSE);
		hRes = (*ppXML)->load(varFileToLoad,&isPass);
		if(SUCCEEDED(hRes) && (isPass == VARIANT_TRUE) && DOMLoaded(*ppXML)){
			LogComment(_T("Loaded file"));
			bCreatedAndLoaded = TRUE;
		}
		else{
			LogFail(_T("Could not load file, load failed with hRes = 0x%08x"),hRes);
		}
	}
	else{
		LogFail(_T("Could not obtain IXMLDOMDocument2 pointer, CoCreateInstance failed with hRes = 0x%08x"),hRes);
	}

	VariantClear(&varFileToLoad);

	return bCreatedAndLoaded;
}

BOOL
CoCreateAndLoadXMLString(
	IXMLDOMDocument2	**ppXML,
	BSTR				bstrXML
	){
	HRESULT			hRes;
	VARIANT_BOOL	isPass;
	BOOL			bCreatedAndLoaded;
	bCreatedAndLoaded = FALSE;
	if(SUCCEEDED(hRes = CoCreateInstance(CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument2, (void**)ppXML))){
		LogComment(TEXT("obtained IXMLDOMDocument2 pointer"));
		(*ppXML)->put_async(VARIANT_FALSE);
		isPass	= VARIANT_FALSE;
#ifdef UNICODE
		LogComment(TEXT("Trying to load XML %s"),bstrXML);
#else
		LogComment(TEXT("Trying to load XML %S"),bstrXML);
#endif
		hRes = (*ppXML)->loadXML(bstrXML,&isPass);
		if(SUCCEEDED(hRes) && (isPass == VARIANT_TRUE) && DOMLoaded(*ppXML)){
			LogComment(_T("Loaded XML"));
			bCreatedAndLoaded = TRUE;
		}
		else{
			LogFail(_T("Could not load XML, loadXML failed with hRes = 0x%08x"),hRes);
		}
	}
	else{
		LogFail(_T("Could not obtain IXMLDOMDocument2 pointer, CoCreateInstance failed with hRes = 0x%08x"),hRes);
	}
	return bCreatedAndLoaded;
}

BOOL
CoCreateAndLoadRemoteBooks(
	IXMLDOMDocument2	**ppXML
	){
	BSTR	bstrFileToLoad = SysAllocString(L"http://alotweb01/msxml/msxml50/tests/Xpath/XMLFiles/books.xml");
	BOOL	bCreatedAndLoaded = FALSE;
	bCreatedAndLoaded = CoCreateAndLoad(ppXML,bstrFileToLoad);
	SysFreeString(bstrFileToLoad);
	return bCreatedAndLoaded;
}

#ifdef UNDER_CE
extern "C" int U_ropen(const TCHAR *, UINT);
extern "C" int U_rread(int, BYTE *, int);
extern "C" int U_rlseek(int, int, int);
extern "C" int U_rclose(int);
#ifndef _O_RDONLY
#define _O_RDONLY  0x0000
#endif
BOOL
CopyFile(
	LPCTSTR	pszSrcFileName,
	LPTSTR	pszCpyFilePath
	){   
	int     nPPSHFileHandle;
    HANDLE  hDeviceFile;
    BYTE    buffer[512];
    int     nRead;
    DWORD   dwWritten;
    // First, open the file over PPSH/////////////////////////////////////
    nPPSHFileHandle = U_ropen(pszSrcFileName, _O_RDONLY);
    if(nPPSHFileHandle == -1){
		LogFail(_T("Could not open source file %s"),pszSrcFileName);
        return FALSE;
    }
	else{
		LogComment(_T("Opened source file %s"),pszSrcFileName);
	}
    // Open the file on the device for writing////////////////////////////
    hDeviceFile = CreateFile(
        pszCpyFilePath,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if(hDeviceFile == INVALID_HANDLE_VALUE){
        U_rclose(nPPSHFileHandle);
		LogFail(_T("Could not create device file %s"),pszCpyFilePath);
        return FALSE;
    }
	else{
		LogComment(_T("Created device file %s"),pszCpyFilePath);
	}

    // With both files open, transfer data from one to the other//////////
    while((nRead = U_rread(nPPSHFileHandle, buffer, sizeof(buffer))) > 0){
        dwWritten = 0;
        WriteFile(hDeviceFile, buffer, nRead, &dwWritten, NULL);
        if(dwWritten != (DWORD)nRead){
            CloseHandle(hDeviceFile);
            U_rclose(nPPSHFileHandle);
			LogFail(_T("Could not write to device file"));
            return FALSE;
        }
    }
    CloseHandle(hDeviceFile);
    U_rclose(nPPSHFileHandle);
	LogComment(_T("Copied file %s to %s"),pszSrcFileName,pszCpyFilePath);
    return TRUE;
}
#else
BOOL
CopyFile(
	LPCTSTR	pszSrcFileName,
	LPTSTR	pszCpyFilePath
	){
	return CopyFile(pszSrcFileName,pszCpyFilePath,FALSE);
}
#endif

BOOL
CopyFileIfDoesNotExist(
	LPCTSTR	pszSrcFileName,
	LPTSTR	pszCpyFilePath
	){
	HANDLE	hDeviceFile = NULL;
	BOOL	bRet;

	hDeviceFile = CreateFile(
        pszCpyFilePath,
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
	if(INVALID_HANDLE_VALUE != hDeviceFile){
		LogComment(_T("Target file already exist on device, we will not copy"));
		bRet = TRUE;
		CloseHandle(hDeviceFile);
	}
	else{
		LogComment(_T("Target file does not exist on device, we will attempt to copy"));
		bRet = CopyFile(pszSrcFileName,pszCpyFilePath);
	}
	return bRet;
}
BOOL
CoCreateAndLoadLocalBooks(
	IXMLDOMDocument2	**ppXML
	){
	BOOL bCreatedAndLoaded	= FALSE;
	if(CopyFileIfDoesNotExist(TEXT("s2_domxpathxqlxslt.xml"),TEXT("\\windows\\s2_domxpathxqlxslt.xml"))){
		BSTR bstrFileToLoad = SysAllocString(L"\\windows\\s2_domxpathxqlxslt.xml");
		bCreatedAndLoaded = CoCreateAndLoad(ppXML,bstrFileToLoad);
		SysFreeString(bstrFileToLoad);
	}
	return bCreatedAndLoaded;
}

BOOL
CoCreateAndLoadLocalBooks2(
	IXMLDOMDocument2	**ppXML
	){
	HRESULT				hRes;
	VARIANT				vtSpecFileName;
	VARIANT_BOOL		vbSuccess		= VARIANT_FALSE;
	VariantInit(&vtSpecFileName);
	vtSpecFileName.vt		= VT_BSTR;
	vtSpecFileName.bstrVal	= SysAllocString(L"\\windows\\s2_domxpathxqlxslt.xml");


	CopyFileIfDoesNotExist(TEXT("s2_domxpathxqlxslt.xml"),TEXT("\\windows\\s2_domxpathxqlxslt.xml"));


	hRes = CoCreateInstance(
				CLSID_DOMDocument,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IXMLDOMDocument2,
				(void**)ppXML
				);

	if(SUCCEEDED(hRes)){
		//load the xml spec file
		(*ppXML)->put_async(VARIANT_FALSE);
		hRes	= (*ppXML)->load(vtSpecFileName,&vbSuccess);
		VariantClear(&vtSpecFileName);
	}
	return (SUCCEEDED(hRes) && (vbSuccess == VARIANT_TRUE)) ? TRUE : FALSE;
}
BOOL
CoCreateAndLoadBooks(
	IXMLDOMDocument2	**ppXML
	){
	if(!ppXML)
		return FALSE;
	/*
	return CoCreateAndLoadLocalBooks2(ppXML);
	*/
	return CoCreateAndLoadLocalBooks(ppXML) ? TRUE : CoCreateAndLoadRemoteBooks(ppXML);
}

double
CalcCheckSum(
	BSTR	bstrVal
	){
	long	lCtr		= 0,
			lLen		= 0;
	double	dblCheckSum	= 0.0;
	if(!bstrVal)
		return dblCheckSum;
	lLen	= wcslen(bstrVal)*2;
	for(lCtr = 0;lCtr < lLen; lCtr++){
		dblCheckSum	+= ((char *)bstrVal)[lCtr]/(double)(lCtr+1);
	}
	return dblCheckSum;
}

void
ConvertToString(
	IXMLDOMNodeList	*pNodeList,
	BSTR			*pbstrNodeList
	){
	IXMLDOMNode	*pNode			= NULL;
	WCHAR		*pwszNodeList	= NULL;
	WCHAR		*pwszTemp		= NULL;
	size_t		size			= 0;
	BSTR		bstrTemp		= NULL;
	LONG		len				= 0;
	int			nExtra			= 6,
				nNodes			= 0,
				nOldLen			= 0,
				nNodeLen		= 0,
				nTotalLen		= 0,
				nSize			= ONEKB;

	*pbstrNodeList	= NULL;

	if(!pNodeList || !pbstrNodeList){
		LogFail(_T("ConvertToString: got invalid arguments"));
		return;
	}

	pwszNodeList	= (WCHAR *)malloc((size_t)(nSize * sizeof(WCHAR)));
	if(!pwszNodeList){
		LogFail(_T("ConvertToString: alloc fail"));
		return;
	}
	pNodeList->get_length(&len);
    swprintf(pwszNodeList,L"len = %ld\n",len);
	/*
	logInfo(ECHO,TEXT("ConvertToString: len = %ld"),len);
	while(SUCCEEDED(pNodeList->nextNode(&pNode)) && pNode){
		nNodes++;
		pNode->Release();
		pNode	= NULL;
	}
	logInfo(ECHO,TEXT("ConvertToString: nNodes = %ld"),nNodes);
	*/
	pNodeList->reset();
	while(SUCCEEDED(pNodeList->nextNode(&pNode)) && pNode){
		pNode->get_xml(&bstrTemp);
		if(bstrTemp && (wcscmp(bstrTemp,L"") != 0)){
			nOldLen		= wcslen(pwszNodeList);
			nNodeLen	= wcslen(bstrTemp);
			nTotalLen	= nOldLen+nNodeLen+nExtra;
			if(nTotalLen >= nSize){
				pwszTemp	= (WCHAR *)realloc(pwszNodeList,(size_t)((nTotalLen+ONEKB)*sizeof(WCHAR)));
				if(!pwszTemp){
					break;
				}
				else{
					pwszNodeList	= pwszTemp;
					nSize			= nTotalLen+ONEKB;
					wcscat(pwszNodeList,L"Node:");
					wcscat(pwszNodeList,bstrTemp);
					wcscat(pwszNodeList,L"\n");
				}			
			}
			else{
				wcscat(pwszNodeList,L"Node:");
				wcscat(pwszNodeList,bstrTemp);
				wcscat(pwszNodeList,L"\n");
			}
		}
		pNode->Release();
		pNode	= NULL;
		SysFreeString(bstrTemp);
	}

	*pbstrNodeList	= SysAllocString(pwszNodeList);

	free(pwszNodeList);    
}

void
SetSelectionLanguage(
	IXMLDOMDocument2	*pXML,
	WCHAR				*pwszLanguage
	){
	VARIANT	varTemp;
	if(!pXML || !pwszLanguage)
		return;
	VariantInit(&varTemp);
	varTemp.vt		= VT_BSTR;
	varTemp.bstrVal = SysAllocString(pwszLanguage);
	pXML->setProperty(L"SelectionLanguage",varTemp);
	VariantClear(&varTemp);
}

UINT
DoXPathIteration
	(
	void
	){
	int					nCases			= 0;
	int					nPass			= 0;
 	double				dblCheckSum		= 0.0;
	IXMLDOMDocument2	*pXML			= NULL;
    IXMLDOMElement      *pElem			= NULL;
    IXMLDOMNodeList		*pNodeList		= NULL;
	BSTR				bstrQuery		= NULL,
						bstrNodeList	= NULL;
	WCHAR				wszTemp[1024];
	HRESULT				hRes;
	LPVARIATION			lpVariation		= g_lpXPathVariation;

	if(CoCreateAndLoadBooks(&pXML) && (NULL != pXML)){
		SetSelectionLanguage(pXML,L"XPath");
		for(nCases=0;nCases < g_nXPathVariation;nCases++){
			LogComment(_T("Trying to get the documentElement"));
			hRes	= pXML->get_documentElement(&pElem);
			if(SUCCEEDED(hRes) && (NULL != pElem)){
				LogComment(_T("Obtained the documentElement"));
#ifdef UNICODE
				LogComment(_T("Trying to apply the XPath query %s"),lpVariation->lpwszQuery);
#else
				LogComment(_T("Trying to apply the XPath query %S"),lpVariation->lpwszQuery);
#endif
				bstrQuery = SysAllocString(lpVariation->lpwszQuery);
				hRes = pElem->selectNodes(bstrQuery,&pNodeList);
				SysFreeString(bstrQuery);
				pElem->Release();
				pElem	= NULL;
			}
			else{
				LogFail(_T("get_documentElement failed with hRes = 0x%08x"),hRes);
			}
			if(SUCCEEDED(hRes) && (NULL != pNodeList)){
				LogComment(_T("Applied the XPath query"));
				ConvertToString(pNodeList,&bstrNodeList);
				dblCheckSum	= CalcCheckSum(bstrNodeList);
				swprintf(wszTemp,L"%0.11f",dblCheckSum);
				if(wcscmp(wszTemp,lpVariation->lpwszChecksum) == 0){
					nPass++;
					LogComment(_T("We were able to obtain the expected checksum of %s"),lpVariation->lpwszChecksum);
				}
				else{
					LogFail(_T("Could not obtain the expected checksum"));
#ifdef UNICODE
					LogFail(_T("Computed CheckSum = %s, Expected Checksum = %s"),wszTemp,lpVariation->lpwszChecksum);
#else
					LogFail(_T("Computed CheckSum = %S, Expected Checksum = %S"),wszTemp,lpVariation->lpwszChecksum);
#endif
				}
				SysFreeString(bstrNodeList);
			}
			else{
				LogFail(_T("selectNodes failed with hRes = 0x%08x"),hRes);
			}
			if(pNodeList){
				pNodeList->Release();
				pNodeList	= NULL;
			}
			lpVariation++;
		}
		pXML->Release();
		pXML = NULL;
	}
	else{
		LogFail(_T("CoCreateAndLoadBooks returned FALSE"));
	}
	return (nPass == g_nXPathVariation) ? CESTRESS_PASS : CESTRESS_FAIL;
}


UINT
DoXQLIteration
	(
	void
	){
	int					nCases			= 0;
	int					nPass			= 0;
 	double				dblCheckSum		= 0.0;
	IXMLDOMDocument2	*pXML			= NULL;
    IXMLDOMElement      *pElem			= NULL;
    IXMLDOMNodeList		*pNodeList		= NULL;
	BSTR				bstrQuery		= NULL,
						bstrNodeList	= NULL;
	WCHAR				wszTemp[1024];
	HRESULT				hRes;
	LPVARIATION			lpVariation		= g_lpXQLVariation;

	if(CoCreateAndLoadBooks(&pXML) && (NULL != pXML)){
		for(nCases=0;nCases < g_nXQLVariation;nCases++){
			//Apply the query
			LogComment(_T("Trying to get the documentElement"));
			hRes	= pXML->get_documentElement(&pElem);
			if(SUCCEEDED(hRes) && (NULL != pElem)){
				LogComment(_T("Obtained the documentElement"));
#ifdef UNICODE
				LogComment(_T("Trying to apply the XQL query %s"),lpVariation->lpwszQuery);
#else
				LogComment(_T("Trying to apply the XQL query %S"),lpVariation->lpwszQuery);
#endif
				bstrQuery = SysAllocString(lpVariation->lpwszQuery);
				hRes = pElem->selectNodes(bstrQuery,&pNodeList);
				SysFreeString(bstrQuery);
				pElem->Release();
				pElem	= NULL;
			}
			else{
				LogFail(_T("get_documentElement failed with hRes = 0x%08x"),hRes);
			}
			if(SUCCEEDED(hRes) && (NULL != pNodeList)){
				LogComment(_T("Applied the XPath query"));
				ConvertToString(pNodeList,&bstrNodeList);
				dblCheckSum	= CalcCheckSum(bstrNodeList);
				swprintf(wszTemp,L"%0.11f",dblCheckSum);
				if(wcscmp(wszTemp,lpVariation->lpwszChecksum) == 0){
					nPass++;
					LogComment(_T("We were able to obtain the expected checksum of %s"),lpVariation->lpwszChecksum);
				}
				else{
					LogFail(_T("Could not obtain the expected checksum"));
#ifdef UNICODE
					LogFail(_T("Computed CheckSum = %s, Expected Checksum = %s"),wszTemp,lpVariation->lpwszChecksum);
#else
					LogFail(_T("Computed CheckSum = %S, Expected Checksum = %S"),wszTemp,lpVariation->lpwszChecksum);
#endif
				}
				SysFreeString(bstrNodeList);
			}
			else{
				LogFail(_T("selectNodes failed with hRes = 0x%08x"),hRes);
			}
			if(pNodeList){
				pNodeList->Release();
				pNodeList	= NULL;
			}
			lpVariation++;
		}
		pXML->Release();
		pXML = NULL;
	}
	else{
		LogFail(_T("CoCreateAndLoadBooks returned FALSE"));
	}
	return (nPass == g_nXQLVariation) ? CESTRESS_PASS : CESTRESS_FAIL;
}

UINT
DoXSLTIteration
	(
	void
	){
	int					nCases			= 0;
	int					nPass			= 0;
 	double				dblCheckSum		= 0.0;
	IXMLDOMDocument2	*pXML			= NULL;
	IXMLDOMDocument2	*pXSL			= NULL;
    IXMLDOMElement      *pElem			= NULL;
    IXMLDOMNodeList		*pNodeList		= NULL;
	BSTR				bstrQuery		= NULL,
						bstrNodeList	= NULL;
	WCHAR				wszTemp[1024];
	HRESULT				hRes;
	LPVARIATION			lpVariation		= g_lpXSLTVariation;

	for(nCases=0;nCases < g_nXSLTVariation;nCases++){
		if(CoCreateAndLoadBooks(&pXML) && (NULL != pXML)){
			if(CoCreateAndLoadXMLString(&pXSL,lpVariation->lpwszQuery) && (NULL != pXSL)){
#ifdef UNICODE
				LogComment(_T("Trying to apply the XSLT %s"),lpVariation->lpwszQuery);
#else
				LogComment(_T("Trying to apply the XSLT %S"),lpVariation->lpwszQuery);
#endif
				hRes = pXML->transformNode(pXSL,&bstrNodeList);
				if(SUCCEEDED(hRes)){
					LogComment(_T("Applied the XSLT"));
					dblCheckSum	= CalcCheckSum(bstrNodeList);
					swprintf(wszTemp,L"%0.11f",dblCheckSum);
					if(wcscmp(wszTemp,lpVariation->lpwszChecksum) == 0){
						nPass++;
						LogComment(_T("We were able to obtain the expected checksum of %s"),lpVariation->lpwszChecksum);
					}
					else{
						LogFail(_T("Could not obtain the expected checksum"));
	#ifdef UNICODE
						LogFail(_T("Computed CheckSum = %s, Expected Checksum = %s"),wszTemp,lpVariation->lpwszChecksum);
	#else
						LogFail(_T("Computed CheckSum = %S, Expected Checksum = %S"),wszTemp,lpVariation->lpwszChecksum);
	#endif
					}
					SysFreeString(bstrNodeList);
				}
				else{
					LogFail(_T("transformNode failed with hRes = 0x%08x"),hRes);
				}
				pXSL->Release();
				pXSL = NULL;
			}
			else{
				LogFail(_T("CoCreateAndLoadXMLString returned FALSE"));
			}
			pXML->Release();
			pXML = NULL;
		}
		else{
			LogFail(_T("CoCreateAndLoadBooks returned FALSE"));
		}
		lpVariation++;
	}
	return (nPass == g_nXSLTVariation) ? CESTRESS_PASS : CESTRESS_FAIL;
}

///////////////////////////////////////////////
//
// Worker thread to do the acutal testing
//
DWORD
WINAPI
TestThreadProc
	(
	LPVOID	pv
	){
	HRESULT			hRes;
	UINT			ret;
	int				nIterations = 0;
	STRESS_RESULTS	res;
	STRESS_RESULTS	*pRes = (STRESS_RESULTS*)pv; // get a ptr to the module results struct

	ZeroMemory((void*)&res,sizeof(STRESS_RESULTS));

	if(FAILED(hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED))){
		LogFail (_T("CoInitializeEx = 0x%08x"),hRes);
	}

	// Do our testing until time's up is signaled
	while(WAIT_OBJECT_0 != WaitForSingleObject(g_hTimesUp,0)){

		nIterations++;

		switch(nIterations%3){
		case 0:
			ret = DoXPathIteration();
			break;
		case 1:
			ret = DoXQLIteration();
			break;
		case 2:
			ret = DoXSLTIteration();
			break;
		default:
			break;
		}

		if (ret == CESTRESS_ABORT)
			return CESTRESS_ABORT;

		RecordIterationResults(&res,ret);
	}

	// Add this thread's results to the module tally
	EnterCriticalSection(&g_csResults);

	AddResults(pRes,&res);

	LeaveCriticalSection(&g_csResults);

	CoUninitialize();

	return 0;
}


