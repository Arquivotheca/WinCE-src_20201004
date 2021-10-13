//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      soapglo.h
//
// Contents:
//
//      Microsoft SOAP Listener ISAPI Implementation - SOAPMSG DLL globals
//
//----------------------------------------------------------------------------------

#ifndef _SOAPGLO_H_INCLUDED
#define _SOAPGLO_H_INCLUDED

#ifdef INIT_SOAP_GLOBALS

    const WCHAR * g_pwstrSelNamespaces = L"SelectionNamespaces";
    const WCHAR * g_pwstrSelLanguage = L"SelectionLanguage";
    const WCHAR * g_pwstrXpathLanguage = L"XPath";
    const WCHAR * g_pwstrXpathEnv = L"xmlns:env='http://schemas.xmlsoap.org/soap/envelope/'";
    const WCHAR * g_pwstrXpathEnc = L"xmlns:enc='http://schemas.xmlsoap.org/soap/encoding/'";
    const WCHAR * g_pwstrXpathDef = L"xmlns:def='http://schemas.xmlsoap.org/wsdl/'";
    const WCHAR * g_pwstrTKData = L"http://schemas.microsoft.com/soap-toolkit/data-types";
    const WCHAR * g_pwstrXpathXSIanyType = L"./@xsi:type";

    const WCHAR * g_pwstrEnvNS = L"http://schemas.xmlsoap.org/soap/envelope/";
    const WCHAR * g_pwstrEncStyleNS = L"http://schemas.xmlsoap.org/soap/encoding/";
    const WCHAR * g_pwstrMSErrorNS = L"http://schemas.microsoft.com/soap-toolkit/faultdetail/error/";


    const WCHAR * g_pwstrMSExtension = L"http://schemas.microsoft.com/soap-toolkit/wsdl-extension";
    const WCHAR * g_pwstrSOAP = L"http://schemas.xmlsoap.org/wsdl/soap/";
    const WCHAR * g_pwstrWSDL = L"http://schemas.xmlsoap.org/wsdl/";    


    const WCHAR * g_pwstrUTF8 = L"UTF-8";
    const WCHAR * g_pwstrUTF16 = L"UTF-16";
    const WCHAR * g_pwstrEnvPrefix = L"SOAP-ENV";
    const WCHAR * g_pwstrEnv = L"Envelope";
    const WCHAR * g_pwstrHeader = L"Header";
    const WCHAR * g_pwstrBody = L"Body";
    const WCHAR * g_pwstrEncStyle = L"encodingStyle";
    const WCHAR * g_pwstrMustUnderstand = L"mustUnderstand";
    const WCHAR * g_pwstrCMustUnderstand = L"MustUnderstand";
    const WCHAR * g_pwstrActor = L"actor";
    const WCHAR * g_pwstrXmlns = L"xmlns";
    const WCHAR * g_pwstrFault = L"Fault";
    const WCHAR * g_pwstrFaultcode = L"faultcode";
    const WCHAR * g_pwstrFaultstring = L"faultstring";
    const WCHAR * g_pwstrFaultactor = L"faultactor";
    const WCHAR * g_pwstrDetail = L"detail";
    const WCHAR * g_pwstrEmpty = L"";
    const WCHAR  *g_pwstrStandard = L"STANDARD";
    const WCHAR  *g_pwstrNone = L"NONE";
    const WCHAR  *g_pwstrOpenHresult = L"<HRESULT>";
    const WCHAR  *g_pwstrCloseHresult = L"</HRESULT>";
    const WCHAR  *g_pwstrServer = L"Server";
    const WCHAR  *g_pwstrClient = L"Client";
    const WCHAR  *g_pwstrVersionMismatch = L"VersionMismatch";
    const WCHAR * g_pwstrDocument = L"document";
    const WCHAR * g_pwstrRPC = L"rpc";
    const WCHAR * g_pwstrEncodingAttribute = L"preferredEncoding";
    const WCHAR * g_pwstrCreateHREFs = L"createHrefs";
    const WCHAR * g_pwstrOpenCDATA = L"<![CDATA[";
    const WCHAR * g_pwstrCloseCDATA = L"]]>";
    const WCHAR * g_pwstrClosing = L">";
    const WCHAR * g_pwstrMessageNSPrefix = L"m";
    const WCHAR * g_pwstrMessageSchemaNSPrefix = L"stns";
    const WCHAR * g_pwstrHref = L"href";
    const WCHAR * g_pwstrEmptyType = L"empty";


    const WCHAR * g_pwstrName = L"name";
    const WCHAR * g_pwstrXSDtype = L"type";
    const WCHAR * g_pwstrAnyType = L"anyType";
    const WCHAR * g_pwstrarrayType = L"arrayType";
    const WCHAR * g_pwstrArray = L"Array";
    const WCHAR * g_pwstrSOAPtrue = L"true";
    const WCHAR * g_pwstrSOAPfalse = L"false";
    const WCHAR * g_pwstrMinOccurs = L"minOccurs";
    const WCHAR * g_pwstrMaxOccurs = L"maxOccurs";
    const WCHAR * g_pwstrUnbounded = L"unbounded";    
    const WCHAR * g_pwstrStar = L"*";
    const WCHAR * g_pwstrSBase64ECType = L"base64";
    const WCHAR * g_pwstrBinaryType = L"binary";


    // strings to create the errorinformation
    const WCHAR *g_pwstrErrorNSPrefix = L"mserror";
    const WCHAR *g_pwstrErrorInfoElement = L"errorInfo";
    const WCHAR *g_pwstrErrorReturnHR = L"returnCode";
    const WCHAR *g_pwstrErrorHelpFile = L"helpFile";
    const WCHAR *g_pwstrErrorDescription = L"description";
    const WCHAR *g_pwstrErrorSource = L"source";
    const WCHAR *g_pwstrErrorHelpContext = L"helpContext";
    const WCHAR *g_pwstrErrorCallElement = L"callElement";
    const WCHAR *g_pwstrErrorComponent = L"component";
    const WCHAR *g_pwstrErrorCallstack= L"callStack";
    const WCHAR *g_pwstrErrorServerElement = L"serverErrorInfo";

    const WCHAR *g_pwstrHeaderHandlerAttribute = L"headerHandler";

    const WCHAR *g_pwstrConnectorProgID = L"ConnectorProgID";
    const WCHAR *g_szStatusInternalError = L"500 Internal Server Error";   

#else   //INIT_SOAP_GLOBALS

//    extern DWORD            g_dwPlatformId;

    extern const WCHAR * g_pwstrXpathLanguage;
    extern const WCHAR * g_pwstrXpathEnv;
    extern const WCHAR * g_pwstrXpathEnc;
    extern const WCHAR * g_pwstrXpathDef;
    extern const WCHAR * g_pwstrXpathXSIanyType;
    extern const WCHAR * g_pwstrSelNamespaces;
    extern const WCHAR * g_pwstrSelLanguage;
    extern const WCHAR * g_pwstrTKData;


    extern const WCHAR * g_pwstrEnvNS;
    extern const WCHAR * g_pwstrEncStyleNS;
    extern const WCHAR * g_pwstrMSErrorNS;

    extern const WCHAR * g_pwstrMSExtension;
    extern const WCHAR * g_pwstrWSDL;
    extern const WCHAR * g_pwstrSOAP;

    extern const WCHAR * g_pwstrUTF8;
    extern const WCHAR * g_pwstrUTF16;
    extern const WCHAR * g_pwstrEnvPrefix;
    extern const WCHAR * g_pwstrEnv;
    extern const WCHAR * g_pwstrHeader;
    extern const WCHAR * g_pwstrBody;
    extern const WCHAR * g_pwstrEncStyle;
    extern const WCHAR * g_pwstrMustUnderstand;    
    extern const WCHAR * g_pwstrCMustUnderstand;
    extern const WCHAR * g_pwstrActor;
    extern const WCHAR * g_pwstrXmlns;
    extern const WCHAR * g_pwstrFault;
    extern const WCHAR * g_pwstrFaultcode;
    extern const WCHAR * g_pwstrFaultstring;
    extern const WCHAR * g_pwstrFaultactor;
    extern const WCHAR * g_pwstrDetail;
    extern const WCHAR * g_pwstrEmpty;
    extern const WCHAR  *g_pwstrStandard;
    extern const WCHAR  *g_pwstrNone;
    extern const WCHAR  *g_pwstrOpenHresult;
    extern const WCHAR  *g_pwstrCloseHresult;
    extern const WCHAR  *g_pwstrServer;
    extern const WCHAR  *g_pwstrClient;
    extern const WCHAR  *g_pwstrVersionMismatch;
    extern const WCHAR * g_pwstrDocument;
    extern const WCHAR * g_pwstrRPC;
    extern const WCHAR * g_pwstrEncodingAttribute;
    extern const WCHAR * g_pwstrCreateHREFs;
    extern const WCHAR * g_pwstrOpenCDATA;
    extern const WCHAR * g_pwstrCloseCDATA;
    extern const WCHAR * g_pwstrClosing;
    extern const WCHAR * g_pwstrMessageNSPrefix;
    extern const WCHAR * g_pwstrMessageSchemaNSPrefix;
    extern const WCHAR * g_pwstrHref;
    extern const WCHAR * g_pwstrEmptyType;

    extern const WCHAR * g_pwstrName;
    extern const WCHAR * g_pwstrXSDtype;
    extern const WCHAR * g_pwstrAnyType;
    extern const WCHAR * g_pwstrarrayType;
    extern const WCHAR * g_pwstrArray;
    extern const WCHAR * g_pwstrSOAPtrue;
    extern const WCHAR * g_pwstrSOAPfalse;
    extern const WCHAR * g_pwstrMinOccurs;
    extern const WCHAR * g_pwstrMaxOccurs;
    extern const WCHAR * g_pwstrUnbounded;
    extern const WCHAR * g_pwstrStar;
    extern const WCHAR * g_pwstrSBase64ECType;
    extern const WCHAR * g_pwstrBinaryType;

    extern const WCHAR *g_pwstrErrorNSPrefix;
    extern const WCHAR *g_pwstrErrorInfoElement;
    extern const WCHAR *g_pwstrErrorReturnHR;
    extern const WCHAR *g_pwstrErrorHelpFile;
    extern const WCHAR *g_pwstrErrorDescription;
    extern const WCHAR *g_pwstrErrorSource;
    extern const WCHAR *g_pwstrErrorHelpContext;
    extern const WCHAR *g_pwstrErrorCallElement;
    extern const WCHAR *g_pwstrErrorComponent;
    extern const WCHAR *g_pwstrErrorCallstack;
    extern const WCHAR *g_pwstrErrorServerElement ;

    extern const WCHAR *g_pwstrHeaderHandlerAttribute;

    extern const WCHAR *g_pwstrConnectorProgID;
    extern const WCHAR *g_szStatusInternalError;   

#endif  //INIT_SOAP_GLOBALS

#define MAX_RES_STRING_SIZE		1024

#endif  //_SOAPGLO_H_INCLUDED


