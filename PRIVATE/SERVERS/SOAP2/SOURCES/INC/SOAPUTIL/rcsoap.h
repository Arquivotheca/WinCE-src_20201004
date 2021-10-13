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
//      rcsoap.h
//
// Contents:
//
//      Resource strings definition file
//
//----------------------------------------------------------------------------------

#ifndef __RCSOAP_H_INCLUDED__
#define __RCSOAP_H_INCLUDED__

// These string values do not need to be localized, because they are language constructs
#define    SOAP_IDS_SERVER		200
#define    SOAP_IDS_CLIENT		201
#define    SOAP_IDS_MUSTUNDERSTAND	202
#define    SOAP_IDS_VERSIONMISMATCH	203

// Put localized resource string values here, and the strings themselves into mssoapres.rc

#define     SOAP_LOCALIZABLE_STRING_BEGIN	600
#define     SOAP_IDS_SERVER_ERR	600
#define     SOAP_IDS_CLIENT_ERR       601
#define     SOAP_IDS_MUSTUNDERSTAND_ERR	602
#define     SOAP_IDS_VERSIONMISM	603
#define     SOAP_IDS_UNSPECIFIED_ERR   604
#define     SOAP_IDS_OUTOFMEMORY    605
#define     SOAP_IDS_INVALID_PARAM  606
#define     SOAP_IDS_INVALID_PARAM_ARG 607
#define     SOAP_CLIENT_NOT_INITED  608
#define     SOAP_UNKNOWN_PROPERTY   609
#define     CLIENT_IDS_DISPGETPARAMFAILED           610
#define     SOAP_IDS_SERVER_COULDNOTLOADREQUEST  611
#define     SOAP_IDS_SENDMESSAGEFAILED      612
#define     SOAP_IDS_READMESSAGEFAILED      613
#define     SOAP_IDS_SERVER_NOTINITIALIZED  614
#define     SOAP_IDS_COULDNOTFINDSERVICE    615
#define     SOAP_IDS_COULDNOTFINDPORT       616
#define     SOAP_IDS_NODEFAULTSERVICE       617
#define     SOAP_IDS_NODEFAULTPORT          618
#define     CLIENT_IDS_INSUFFICIENTPARAMETERINMESSAGE 619
#define     CLIENT_IDS_UNKNOWNPARAMETERINMESSAGE 620
#define     CLIENT_IDS_INCORRECTNUMBEROFPARAMETERS  622
#define     SOAP_IDS_COULDNOTCREATECONNECTOR        623
#define     SOAP_IDS_COULDNOTCREATESERIALIZER       624
#define     SOAP_CLIENT_ALREADY_INITED              625
#define     CLIENT_IDS_ARRAYRESULTBYREF 626
#define     CLIENT_IDS_HEADERNOTUNDERSTOOD      627

#define     SOAP_CONN_AMBIGUOUS             1000

#define     SOAP_CONN_BAD_REQUEST           1050
#define     SOAP_CONN_ACCESS_DENIED         1051
#define     SOAP_CONN_FORBIDDEN             1052
#define     SOAP_CONN_NOT_FOUND             1053
#define     SOAP_CONN_BAD_METHOD            1054
#define     SOAP_CONN_REQ_TIMEOUT           1055
#define     SOAP_CONN_CONFLICT              1056
#define     SOAP_CONN_GONE                  1057
#define     SOAP_CONN_TOO_LARGE             1058
#define     SOAP_CONN_ADDRESS               1059

#define     SOAP_CONN_SERVER_ERROR          1100
#define     SOAP_CONN_SRV_NOT_SUPPORTED     1101
#define     SOAP_CONN_BAD_GATEWAY           1102
#define     SOAP_CONN_NOT_AVAILABLE         1103
#define     SOAP_CONN_SRV_TIMEOUT           1104
#define     SOAP_CONN_VER_NOT_SUPPORTED     1105

#define     SOAP_CONN_BAD_CONTENT           1200

#define     SOAP_CONN_CONNECTION_ERROR      1300
#define     SOAP_CONN_BAD_CERTIFICATE_NAME  1301

#define     SOAP_CONN_HTTP_UNSPECIFIED      1400
#define     SOAP_CONN_HTTP_SENDRECV         1401
#define     SOAP_CONN_HTTP_OUTOFMEMORY      1402
#define     SOAP_CONN_HTTP_BAD_REQUEST      1403
#define     SOAP_CONN_HTTP_BAD_RESPONSE     1404
#define     SOAP_CONN_HTTP_BAD_URL          1405
#define     SOAP_CONN_HTTP_DNS_FAILURE      1406
#define     SOAP_CONN_HTTP_CONNECT_FAILED   1407
#define     SOAP_CONN_HTTP_SEND_FAILED      1408
#define     SOAP_CONN_HTTP_RECV_FAILED      1409
#define     SOAP_CONN_HTTP_HOST_NOT_FOUND   1410
#define     SOAP_CONN_HTTP_OVERLOADED       1411
#define     SOAP_CONN_HTTP_FORCED_ABORT     1412
#define     SOAP_CONN_HTTP_NO_RESPONSE      1413
#define     SOAP_CONN_HTTP_BAD_CHUNK        1414
#define     SOAP_CONN_HTTP_PARSE_RESPONSE   1415

#define     SOAP_CONN_HTTP_TIMEOUT          1416
#define     SOAP_CONN_HTTP_CANNOT_USE_PROXY 1417
#define     SOAP_CONN_HTTP_BAD_CERTIFICATE  1418
#define     SOAP_CONN_HTTP_BAD_CERT_AUTHORITY 1419
#define     SOAP_CONN_HTTP_SSL_ERROR        1420

// WSDLREADER Resources
#define     WSDL_IDS_READER                 2000
#define     WSML_IDS_LOADFAILED             2001
#define     WSDL_IDS_LOADFAILED             2002
#define     WSDL_IDS_NOSERVICE              2003
#define     WSDL_IDS_INITSERVICEFAILED      2004
#define     WSDL_IDS_SERVICENONAME          2005
#define     WSDL_IDS_SERVICE                2006
#define     WSDL_IDS_SERVICENOPORTDEF       2007
#define     WSDL_IDS_SERVICEPORTINITFAILED  2009
#define     WSDL_IDS_SERVICENOPORTSDEFINED  2010
#define     WSDL_IDS_PORTNONAME             2011
#define     WSDL_IDS_PORT                   2012
#define     WSDL_IDS_PORTNOBINDING          2013
#define     WSDL_IDS_NOSOAPADDRESS          2014
#define     WSDL_IDS_NOLOCATIONATTR         2015
#define     WSDL_IDS_BINDINGSNOTFOUND       2016
#define     WSDL_IDS_BINDINGSNONAME         2017
#define     WSDL_IDS_ANALYZEBINDINGFAILED   2018
#define     WSDL_IDS_BINDINGNOTYPE          2019
#define     WSDL_IDS_PORTTYPENOBINDING      2020
#define     WSDL_IDS_PORTNOSTYLE            2021
#define     WSDL_IDS_PORTNOTRANSPORT        2022
#define     WSDL_IDS_PORTNOOPERATIONS       2023
#define     WSDL_IDS_PORTOPERATIONINITFAILED 2024
#define     WSDL_IDS_OPERNONAME             2025
#define     WSDL_IDS_OPERATION              2026
#define     WSDL_IDS_OPERNOSOAPOPER         2027
#define     WSDL_IDS_OPERNOSOAPACTION       2028
#define     WSDL_IDS_OPERINITINPUTFA        2029
#define     WSDL_IDS_OPERINITOUTPUTFA       2030
#define     WSDL_IDS_OPERNOBODYHEADER       2031
#define     WSDL_IDS_OPERNOUSEATTR          2032
#define     WSDL_IDS_OPERNOTFOUNDINPORT     2033
#define     WSDL_IDS_OPERMSGPARTINPORTNF    2034
#define     WSDL_IDS_OPERMSGPARTNF          2035
#define     WSDL_IDS_INITMAPPERFAILED       2036
#define     WSDL_IDS_MAPPER                 2037
#define     WSDL_IDS_MAPPERNONAME           2038
#define     WSDL_IDS_MAPPERNOELEMENTORTYPE  2039
#define     WSDL_IDS_MAPPERNOSCHEMA         2040
#define     WSDL_IDS_MAPPERNODEFINITION     2051
#define     WSDL_IDS_MAPPERNOTYPE           2052
#define     WSDL_IDS_MAPPERNOTCREATED       2053
#define     WSDL_IDS_ANALYZEWSDLFAILED      2054
#define     WSDL_IDS_OPERNODISPIDFOUND      2055
#define     WSDL_IDS_OPERCAUSEDEXECPTION    2056
#define     WSDL_IDS_EXECFAILED             2057
#define     WSDL_IDS_EXECBADPARAMCOUNT      2058
#define     WSDL_IDS_EXECBADVARTYPE         2059
#define     WSDL_IDS_EXECMEMBERNOTFOUND     2060
#define     WSDL_IDS_EXECUNKNOWNERROR       2061
#define     WSDL_IDS_EXECBADTYPE            2062
#define     WSDL_IDS_OPERNOOBJECT           2063
#define     WSDL_IDS_PARSEREQUESTFAILED     2064
#define     WSDL_IDS_XMLDOCUMENTLOADFAILED  2065
#define     WSDL_IDS_INVALIDSCHEMA          2066
#define     WSDL_IDS_MAPPERDUPLICATENAME    2067
#define     WSDL_IDS_URIFORQNAMENF          2068
#define     WSDL_IDS_MAPPERNOSCHEMAELEMENT  2069
#define     WSDL_IDS_TYPEINWRONGSCHEMA      2070
#define     WSDL_IDS_WRONGENCODINGSTYLE     2071
#define     WSDL_IDS_NOENCODINGSTYLE        2072
#define     WSDL_IDS_NONONAMSPACE           2073
#define     WSDL_IDS_CREATINGPARAMETERS     2074
#define     WSDL_IDS_OPERPARAORDERINVALID       2075
#define     WSDL_IDS_INITCALLEDTWICE         2076
#define     WSDL_IDS_UNSUPPORTEDMODE       2077
#define     WSDL_IDS_OPERMULTIPLEPARTSWITHTYPE  2078
#define     WSDL_IDS_ENGLISHNOTINSTALLED        2079
#define     WSDL_IDS_DOCLITERALTOOMANY          2080
#define     WSDL_IDS_RCPENCODEDWITHELMENT       2081
#define     WSML_IDS_COULDNOTFINDHREF           2082
#define     WSDL_IDS_VTRECORDNOTSUPPORTED      2083
#define     WSDL_IDS_LOADREQUESTFAILED          2084
#define     WSDL_IDS_OPERNOHEADERHANDLER           2085
#define     WSDL_IDS_OPERREADHEADERSFAILED          2086
#define     WSDL_IDS_CUSTOMMAPPERFREETHREADED      2087

#define     WSML_IDS_WSMLANALYZEFAILED      2100
#define     WSML_IDS_WSMLLOADFAILED         2101
#define     WSML_IDS_SERVICENOTFOUND        2102
#define     WSML_IDS_INITSERVICEFAILED      2103
#define     WSML_IDS_SERVICENONAME          2104
#define     WSML_IDS_SERVICENOUSING         2105
#define     WSML_IDS_SERVICENOPROGID        2106
#define     WSML_IDS_SERVICENOID            2107
#define     WSML_IDS_PORTINITFAILED         2108
#define     WSML_IDS_OPERATIONINITFAILED    2109
#define     WSML_IDS_OPERATIONNOEXECUTE     2110
#define     WSML_IDS_OPERATIONNOMETHODNAME  2111
#define     WSML_IDS_OPERATIONNOUSESATTR    2112
#define     WSML_IDS_OPERATIONINVALIDDISP   2113
#define     WSML_IDS_MAPPERINITFAILED       2114
#define     WSML_IDS_MAPPERNOELEMENTNAME    2115
#define     WSML_IDS_MAPPERNOCALLINDEX      2116
#define     WSML_IDS_CUSTOMMAPERNOUSE       2117
#define     WSML_IDS_CUSTOMMAPPERNOOBJECT   2118
#define     WSML_IDS_CUSTOMMAPPERNOINTERFACE 2119
#define     WSML_IDS_TYPEMAPPERINITFAILED   2120
#define     WSML_IDS_TYPEMAPPERCONVERTFAILED 2121
#define     WSML_IDS_TYPEMAPPERPUTVALUE      2122
#define     WSML_IDS_TYPEMAPPERPUTSOAPVALUE  2123
#define     WSML_IDS_TYPEMAPPERSAVE          2124
#define     WSML_IDS_NOPORTTYPE              2125
#define     WSML_IDS_SERVICEINVALIDPROGID    2126
#define     WSML_IDS_TYPEMAPPERCLSID  2127

#define     SOAP_IDS_MAPPERARRAYCREATEELEMENT   2200
#define     SOAP_IDS_MAPPERARRAYDIMENSIONS 2201
#define     SOAP_IDS_MAPPERARRAYNULL 2202
#define     SOAP_IDS_MAPPERINVALIDANYTYPE 2203
#define     SOAP_IDS_MAPPERANYTYPECREATEELEMENT   2204

#define     SOAP_IDS_SOAPHEADERS                  3000

#define     IDS_SOAPHEADER_WRITEFAILED            3001
#define     IDS_SOAPHEADER_READFAILED                3002

#endif  // __RCSOAP_H_INCLUDED__
