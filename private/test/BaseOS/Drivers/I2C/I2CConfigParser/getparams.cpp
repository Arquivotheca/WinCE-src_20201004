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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//
//  Module: test.cpp
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <kato.h>
#include "I2CConfigParserCommon.h"
#include "GetParams.h"
#include "XmlDocument.h"


////////////////////////////////////////////////////////////////////////////////
// ConvertWCharToByteArray
//convert the input text that is hex bytes, into byte array that driver can process
//
// Parameters:
//  
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 

static DWORD ConvertWCharToByteArray(
   __in TCHAR *pwszInputString, 
   __out BYTE    *ByteArray, 
   __in DWORD dwNumberOfWchars,
   __in __out DWORD    dwNumberOfBytes)
{
   ASSERT(pwszInputString &&  ByteArray);
   memset(ByteArray, 0, dwNumberOfBytes * sizeof(BYTE));
   
   for (DWORD index=0,ByteArrayIndex = 0; ByteArrayIndex < dwNumberOfBytes && index< dwNumberOfWchars; ByteArrayIndex++)
   {
      UCHAR Byte = 0;
      WCHAR wCh = 0;
      for (DWORD ByteIndex = 0; ByteIndex < 3; ByteIndex++)
      {
         
         wCh = pwszInputString[index++];
         if (L' ' == wCh )
         {
            if (ByteIndex > 0)
            {
               ByteArray[ByteArrayIndex] = Byte;
               break;
            }
            continue;      // ignore extra spaces
         }
         else if (0== wCh)
         {
            if (ByteIndex > 0)
            {
               ByteArray[ByteArrayIndex] = Byte;
               ByteArrayIndex++;
            }
            return ByteArrayIndex;
         }
         else if (ByteIndex == 2)
         {
            return 0;
         }

        
         Byte <<= 4;
         if ((wCh >= L'0') && (wCh <= L'9'))
         {
            Byte = Byte + (UCHAR)(wCh - L'0');
         }
         else if ((wCh >= L'A') && (wCh <= L'F'))
         {
            Byte = Byte + (UCHAR)(wCh - L'A' + 10);
         }
         else if ((wCh >= L'a') && (wCh <= L'f'))
         {
            Byte = Byte + (UCHAR)(wCh - L'a' + 10);
         }
         else
         {
            return 0;
         }
      }
   }
   return dwNumberOfBytes;
}

////////////////////////////////////////////////////////////////////////////////
// PrintI2CDataList
//Prints the data scaned from xml by parser
//
// Parameters:
//  
//
// Return value:
//  Void
//////////////////////////////////////////////////////////////////////////////// 
void PrintI2CDataList()
{
    for(long BusIndex=0;BusIndex<gI2CDataSize;BusIndex++)
    {
        
        NKDbgPrintfW( L"I2CData[%d].dwTransactionID=%d",BusIndex, gI2CData[BusIndex].dwTransactionID);
        NKDbgPrintfW( L"I2CData[%d].pwszDeviceName=%s",BusIndex, gI2CData[BusIndex].pwszDeviceName);
        NKDbgPrintfW( L"I2CData[%d].Operation=%s",BusIndex, g_astrI2COp[gI2CData[BusIndex].Operation] );
        NKDbgPrintfW( L"I2CData[%d].dwSubordinateAddress=%x", BusIndex,gI2CData[BusIndex].dwSubordinateAddress );
        NKDbgPrintfW( L"I2CData[%d].dwRegisterAddress=%x",BusIndex,gI2CData[BusIndex].dwRegisterAddress );
        NKDbgPrintfW( L"I2CData[%d].dwLoopbackAddress=%x", BusIndex, gI2CData[BusIndex].dwLoopbackAddress );
        NKDbgPrintfW( L"I2CData[%d].dwNumOfBytes=%d",BusIndex,gI2CData[BusIndex].dwNumOfBytes );
        NKDbgPrintfW( L"I2CData[%d].BufferPtr=%p: %x",BusIndex, gI2CData[BusIndex].BufferPtr, *(gI2CData[BusIndex].BufferPtr));
        NKDbgPrintfW( L"I2CData[%d].dwTimeOut=%d",BusIndex, gI2CData[BusIndex].dwTimeOut);
        NKDbgPrintfW( L"I2CData[%d].dwSpeedOfTransaction=%d", BusIndex,gI2CData[BusIndex].dwSpeedOfTransaction );
        NKDbgPrintfW( L"I2CData[%d].dwSubordinateAddressSize=%d",BusIndex,gI2CData[BusIndex].dwSubordinateAddressSize );
        NKDbgPrintfW( L"I2CData[%d].dwRegisterAddressSize=%d",BusIndex,gI2CData[BusIndex].dwRegisterAddressSize );
        
        
    }
}

////////////////////////////////////////////////////////////////////////////////
// AllocateI2CTransList
//Allocates on heap three lists:
//pI2CTRANSNode: main list of structs that has all transactions
//BufferPtr: Pointer to list of buffers for all transactions
//DeviceNamePtr: Pointer to list of Device names for all transactions
// Parameters:
//  
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD AllocateI2CTransList(LONG Length, I2CTRANS **pI2CTRANSNode )
{
    DWORD dwRet=FAILURE;
    AcquireI2CTestDataLock();  
    BYTE *BPtr=NULL;
    *pI2CTRANSNode=(I2CTRANS *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ((sizeof(I2CTRANS))*Length));
    BufferPtr=(BYTE *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ((I2C_MAX_LENGTH*sizeof(BYTE))*Length));
    DeviceNamePtr=(BYTE *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ((MAX_PATH*sizeof(TCHAR))*Length));
    if (pI2CTRANSNode!=NULL)
    {     
     //just allocate we only fix up addresses for transactions that are valid after scan, this is done later in
        dwRet=SUCCESS;
    }
    ReleaseI2CTestDataLock();  
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//InitI2CTransList
//Initializes any transaction node with default values
// Parameters:
//  
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD InitI2CTransList(LONG Length, I2CTRANS *pI2CTRANSNode )
{
    DWORD dwRet=FAILURE;
    AcquireI2CTestDataLock();  
    if (NULL != pI2CTRANSNode)
    {
        for(int Index=0;Index<Length;Index++)
        {
             if(&pI2CTRANSNode[Index]!=NULL)
             {
                
                pI2CTRANSNode[Index].HandleId=-1;
                pI2CTRANSNode[Index].Operation = DEFAULT_I2C_OPERATION;
                pI2CTRANSNode[Index].dwSubordinateAddress=0;          
                pI2CTRANSNode[Index].dwRegisterAddress = DEFAULT_REGISTER_ADDRESS;
                pI2CTRANSNode[Index].dwLoopbackAddress = pI2CTRANSNode[Index].dwRegisterAddress;
                pI2CTRANSNode[Index].dwNumOfBytes=0;
                pI2CTRANSNode[Index].dwSpeedOfTransaction = DEFAULT_I2C_SPEED;
                pI2CTRANSNode[Index].dwTimeOut = DEFAULT_TIMEOUT;
                pI2CTRANSNode[Index].dwSubordinateAddressSize = DEFAULT_SUBORDINATE_ADDRESS_SIZE;
                pI2CTRANSNode[Index].dwRegisterAddressSize = DEFAULT_REG_ADDRESS_SIZE;
                memset(pI2CTRANSNode[Index].BufferPtr, 0, I2C_MAX_LENGTH);   
             }
        }
        dwRet=SUCCESS;
    }
    ReleaseI2CTestDataLock();  
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////

//FreeParams
//Free all three lists that were allocated on the process heap
//once they 
// Parameters:
//  
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
DWORD FreeParams()
{
    DWORD dwRet=SUCCESS;
    AcquireI2CTestDataLock();  
    if(NULL!=DeviceNamePtr)
    {
        if (!HeapFree(GetProcessHeap(), 0, DeviceNamePtr))
        {
            dwRet=FAILURE;
        }
    }
    if(NULL!=BufferPtr)
    {
        if (!HeapFree(GetProcessHeap(), 0, BufferPtr))
        {
            dwRet=FAILURE;
        }
    }
    
     if(NULL!=gI2CData)
    {
        if(!HeapFree(GetProcessHeap(), 0, gI2CData))
       {
            dwRet=FAILURE;
       }
    
    }
    
    ReleaseI2CTestDataLock();  
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//getElementList
//Gets the list of elements of all elements in the xml document by particular name
// Parameters:
//note the release of the params is responsibility of caller
//  
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getElementList(XmlDocument *pdoc,TCHAR* szElementName, XmlElementList** pElementList, LONG* plSizeOfList)
{
    HRESULT hr=0;
    DWORD dwRet=FAILURE;
    ASSERT(pdoc && szElementName);
    hr = pdoc->GetElementsByTagName(
            szElementName, 
            pElementList);
    if(S_OK==hr)
    {
        hr=(*pElementList)->GetLength(plSizeOfList);
        NKDbgPrintfW( TEXT("Got Element List %s, size of list is %d"), szElementName, *plSizeOfList);
        dwRet=SUCCESS;
    }
    else
    {
        NKDbgPrintfW( TEXT("Failure getting element: %s"), szElementName);     
       
    }
    
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//getElementDeviceName
//Gets the Device name elements value within a particular transaction element
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getElementDeviceName(XmlElement* pElement, I2CTRANS *pI2CTRANS)
{
    DWORD dwRet=FAILURE;
    HRESULT hr=NULL;
    ASSERT(pElement && pI2CTRANS);
    XmlElement* pDeviceName=NULL;
    TCHAR DeviceName[MAX_PATH]={0};
    hr=pElement->GetSingleElementByTagName(I2C_CONFIG_FILE_TAG_BUS_NAME, &pDeviceName);
    if(S_OK==hr && pDeviceName !=NULL)
    {
        hr= pDeviceName->GetText(DeviceName,MAX_PATH);
        if(S_OK==hr && NULL != DeviceName )
        {
            DeviceName[MAX_PATH-1]=L'\0';
            if(!StringCbCopyN(pI2CTRANS->pwszDeviceName,MAX_PATH,DeviceName, MAX_PATH))
            {
                //pI2CTRANS->pwszDeviceName[MAX_PATH-1]=L'\0';         
                NKDbgPrintfW( TEXT("Bus Name=%s"), pI2CTRANS->pwszDeviceName);
                dwRet=SUCCESS;
            }
        }
    }
    MANUAL_RELEASE(pDeviceName);
        
       
    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
//getElementDeviceName
//Gets the Buffer elements value within a particular transaction element
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
//
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getElementBuffer(XmlElement* pElement, I2CTRANS *pI2CTRANS)
{
    ASSERT(pElement && pI2CTRANS);
    DWORD dwRet=FAILURE;
    HRESULT hr=NULL;
    XmlElement* pI2CTransElement_Buffer=NULL;
    DWORD dwNumberOfBytes=0 ;
    //note we allocate more for the tchar array, then we compress the bytes into byte array
    //the input is space seperated and list of bytes=> xx xx xx xx
    //the test scans and converts each entry into a byte and stores in a byte array that it can pass to driver
    TCHAR TempBuffer[I2C_MAX_LENGTH*3-1]={0};
    DWORD sizeofTempBuffer=I2C_MAX_LENGTH*3-1;
    if(pI2CTRANS!=NULL && pI2CTRANS->dwNumOfBytes!=NULL)
    {
        
            hr=pElement->GetSingleElementByTagName(I2C_CONFIG_FILE_TAG_DATA, &pI2CTransElement_Buffer);
            if(S_OK==hr && pI2CTransElement_Buffer !=NULL)
            { 
                    hr= pI2CTransElement_Buffer->GetText((TCHAR *)TempBuffer,(sizeofTempBuffer));
                    
                    
                    if(S_OK==hr && TempBuffer)
                    {  
                        TempBuffer[sizeofTempBuffer-1]=L'\0';
                        dwNumberOfBytes=ConvertWCharToByteArray(TempBuffer, pI2CTRANS->BufferPtr,(sizeofTempBuffer),I2C_MAX_LENGTH );
                        if(dwNumberOfBytes>=1 && dwNumberOfBytes<=I2C_MAX_LENGTH)
                        {
                            if(pI2CTRANS->dwNumOfBytes!=dwNumberOfBytes)
                            {
                                NKDbgPrintfW( TEXT("Number of bytes scanned=>%d is not same as specified in config file=>%d, check the data"), dwNumberOfBytes,pI2CTRANS->dwNumOfBytes );
                            }
                             NKDbgPrintfW( TEXT("Buffer Start=>%x"), pI2CTRANS->BufferPtr[0]);
                             dwRet=SUCCESS;
                        }
                        else
                        {
                            NKDbgPrintfW( TEXT("Data is not specified in proper format. Transaction ignored"));
                        }
                    }
                    else
                    {
                        NKDbgPrintfW( TEXT("Data not specified. Transaction ignored"));
                    }
                
            }

    }
    MANUAL_RELEASE(pI2CTransElement_Buffer);
    
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//getElementDeviceName
//Gets the Operation elements value within a transaction element
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getElementOperation(XmlElement* pElement, I2CTRANS *pI2CTRANS)
{
    ASSERT(pElement && pI2CTRANS);
    DWORD dwRet=FAILURE;
    HRESULT hr=NULL;
    XmlElement* pI2CTransElement_Operation=NULL;
    TCHAR OpValue[MAX_PATH]={0};
    hr=pElement->GetSingleElementByTagName(I2C_CONFIG_FILE_TAG_OPERATION, &pI2CTransElement_Operation);
    if(S_OK==hr && pI2CTransElement_Operation !=NULL)
    {
        hr=pI2CTransElement_Operation->GetText(OpValue,MAX_PATH);
        
        if(S_OK==hr && OpValue !=NULL)
        {
            OpValue[MAX_PATH-1]=L'\0';
            if(!_wcsnicmp(OpValue,I2C_CONFIG_FILE_TAG_READ ,COUNTOF(I2C_CONFIG_FILE_TAG_READ )))
            {
                
                pI2CTRANS->Operation=I2C_OP_READ;
                
                NKDbgPrintfW( TEXT("Operation=%d"), pI2CTRANS->Operation);
                dwRet=SUCCESS;
            }
            else if(!_wcsnicmp(OpValue,I2C_CONFIG_FILE_TAG_WRITE,COUNTOF(I2C_CONFIG_FILE_TAG_WRITE)))
            {
                
                pI2CTRANS->Operation=I2C_OP_WRITE;
                NKDbgPrintfW( TEXT("Operation=%d"), pI2CTRANS->Operation);
                dwRet=SUCCESS;
            }
            else if(!_wcsnicmp(OpValue,I2C_CONFIG_FILE_TAG_LOOPBACK,COUNTOF(I2C_CONFIG_FILE_TAG_LOOPBACK)))
            {
                
                pI2CTRANS->Operation=I2C_OP_LOOPBACK;
                NKDbgPrintfW( TEXT("Operation=%d"), pI2CTRANS->Operation);
                dwRet=SUCCESS;
            }
        }
    }
    MANUAL_RELEASE( pI2CTransElement_Operation);
    
    return dwRet;
}


////////////////////////////////////////////////////////////////////////////////
//getElementSubordinateAddress
//Gets the Subordinate address element value within a transaction element
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getElementSubordinateAddress(XmlElement* pElement, I2CTRANS *pI2CTRANS)
{
    ASSERT(pElement && pI2CTRANS);
    DWORD dwRet=FAILURE;
    HRESULT hr=NULL;
    XmlElement* pSubordinateAddress=NULL;
    TCHAR  pszAddressValue[MAX_PATH]={0};
    DWORD dwAddressValue=0;
    hr=pElement->GetSingleElementByTagName(I2C_CONFIG_FILE_TAG_SUBORDINATE_ADDRESS, &pSubordinateAddress);
   
    if(S_OK==hr && pSubordinateAddress !=NULL)
    {
            hr=pSubordinateAddress->GetText(pszAddressValue,MAX_PATH);
           
            if(S_OK==hr && pszAddressValue !=NULL)
            {
                pszAddressValue[MAX_PATH-1]='\0';
                dwAddressValue=wcstoul(pszAddressValue, NULL, 16);
                if(dwAddressValue)
                {
                    
                    pI2CTRANS->dwSubordinateAddress=dwAddressValue;
                    NKDbgPrintfW( TEXT("Subordinate Address=%d"), dwAddressValue);
                    dwRet=SUCCESS;
                }
            }
    }
    
    MANUAL_RELEASE(pSubordinateAddress);
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//getElementSubordinateAddressSize
//Gets the Subordinate address size element value within a transaction element
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getElementSubordinateAddressSize(XmlElement* pElement, I2CTRANS *pI2CTRANS)
{
    ASSERT(pElement && pI2CTRANS);
    DWORD dwRet=FAILURE;
    HRESULT hr=NULL;
    XmlElement* pSubordinateAddressSize=NULL;
    TCHAR  pszAddressSizeValue[MAX_PATH]={0};
    DWORD dwAddressSizeValue=0;
    hr=pElement->GetSingleElementByTagName(I2C_CONFIG_FILE_TAG_SUBORDINATE_ADDRESS_SIZE, &pSubordinateAddressSize);
   if(S_OK==hr && pSubordinateAddressSize !=NULL)
    {
            hr=pSubordinateAddressSize->GetText(pszAddressSizeValue,MAX_PATH);
            if(S_OK==hr && pszAddressSizeValue !=NULL)
            {
                pszAddressSizeValue[MAX_PATH-1]='\0';
                dwAddressSizeValue=_wtoi(pszAddressSizeValue);
                if(dwAddressSizeValue)
                {
                    
                    pI2CTRANS->dwSubordinateAddressSize=dwAddressSizeValue;
                    NKDbgPrintfW( TEXT("Subordinate Address size=%d"), pI2CTRANS->dwSubordinateAddressSize);
                    dwRet=SUCCESS;
                }
            }
    }
    
    MANUAL_RELEASE(pSubordinateAddressSize);
    
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//getElementRegisterAddress
//Gets the Register address element value within a transaction element
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getElementRegisterAddress(XmlElement* pElement, I2CTRANS *pI2CTRANS)
{
    ASSERT(pElement && pI2CTRANS);
    DWORD dwRet=FAILURE;
    HRESULT hr=NULL;
    XmlElement* pI2CDeviceElement_Register=NULL;
    TCHAR  pszAddressValue[MAX_PATH]={}, pszAddressSizeValue[MAX_PATH]={};
    DWORD dwAddressValue=0;
    hr=pElement->GetSingleElementByTagName(I2C_CONFIG_FILE_TAG_REGISTER_ADDRESS, &pI2CDeviceElement_Register);
    
    if(S_OK==hr && pI2CDeviceElement_Register !=NULL)
    {
            hr=pI2CDeviceElement_Register->GetText(pszAddressValue, MAX_PATH);
            if(S_OK==hr && pszAddressValue !=NULL)
            {
                pszAddressValue[MAX_PATH-1]=L'\0';
                dwAddressValue=wcstoul(pszAddressValue, NULL, 16);
                if(dwAddressValue)
                {
                    pI2CTRANS->dwRegisterAddress=dwAddressValue;
                    NKDbgPrintfW( TEXT("Register Address=%d"), dwAddressValue);
                    dwRet=SUCCESS;
                 }
                               
            }
    }
    
    MANUAL_RELEASE(pI2CDeviceElement_Register);
    
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//getElementRegisterAddressSize
//Gets the Register address size element value within a transaction element
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getElementRegisterAddressSize(XmlElement* pElement, I2CTRANS *pI2CTRANS)
{
    ASSERT(pElement && pI2CTRANS);
    DWORD dwRet=FAILURE;
    HRESULT hr=NULL;
    XmlElement* pRegisterAddressSize=NULL;
    TCHAR  pszAddressSizeValue[MAX_PATH]={0};
    DWORD dwAddressSizeValue=0;
    hr=pElement->GetSingleElementByTagName(I2C_CONFIG_FILE_TAG_REGISTER_ADDRESS_SIZE, &pRegisterAddressSize);
   
    if(S_OK==hr && pRegisterAddressSize !=NULL)
    {
            hr=pRegisterAddressSize->GetText(pszAddressSizeValue,MAX_PATH);
            if(S_OK==hr && pszAddressSizeValue !=NULL)
            {
                pszAddressSizeValue[MAX_PATH-1]=L'\0';
                dwAddressSizeValue=_wtoi(pszAddressSizeValue);
                if(dwAddressSizeValue)
                {
                    
                    pI2CTRANS->dwRegisterAddressSize = dwAddressSizeValue;
                    NKDbgPrintfW( TEXT("Register Address size=%d"), pI2CTRANS->dwRegisterAddressSize);
                    dwRet=SUCCESS;
                }
            }
    }
    MANUAL_RELEASE(pRegisterAddressSize);
    
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//getElementLoopbackAddress
//Gets the Loopback address element value within a transaction element
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getElementLoopbackAddress(XmlElement* pElement, I2CTRANS *pI2CTRANS)
{
    ASSERT(pElement && pI2CTRANS);
    DWORD dwRet=FAILURE;
    HRESULT hr=NULL;
    XmlElement* pI2CDeviceElement_Loopback=NULL;
    TCHAR  pszAddressValue[MAX_PATH]={}, pszAddressSizeValue[MAX_PATH]={};
    DWORD dwAddressValue=0;
    hr=pElement->GetSingleElementByTagName(I2C_CONFIG_FILE_TAG_LOOPBACK_ADDRESS, &pI2CDeviceElement_Loopback);
   
    if(S_OK==hr && pI2CDeviceElement_Loopback !=NULL)
    {
            hr=pI2CDeviceElement_Loopback->GetText(pszAddressValue,MAX_PATH);
            if(S_OK==hr && pszAddressValue !=NULL)
            {
                pszAddressValue[MAX_PATH-1]=L'\0';
                dwAddressValue=wcstoul(pszAddressValue, NULL, 16);
                if(dwAddressValue)
                {
                    pI2CTRANS->dwLoopbackAddress=dwAddressValue;
                    NKDbgPrintfW( TEXT("Loopback Address size=%d"), dwAddressValue);
                    dwRet=SUCCESS;
                 }
                
                
            }
    }
       
    MANUAL_RELEASE(pI2CDeviceElement_Loopback);
    
    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
//getElementNumOfBytes
//Gets the number of bytes element value within a transaction element
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getElementNumOfBytes(XmlElement* pElement, I2CTRANS *pI2CTRANS)
{
    ASSERT(pElement && pI2CTRANS);
    DWORD dwRet=FAILURE;
    HRESULT hr=NULL;
    XmlElement* pI2CTransElement_NumOfBytes=NULL;
    TCHAR pszNumberOfBytesValue[MAX_PATH]={0};
    DWORD dwNumOfBytes=0;
    hr=pElement->GetSingleElementByTagName(I2C_CONFIG_FILE_TAG_NUMBER_OF_BYTES, &pI2CTransElement_NumOfBytes);
    if(S_OK==hr && pI2CTransElement_NumOfBytes !=NULL)
    {
        hr=pI2CTransElement_NumOfBytes->GetText(pszNumberOfBytesValue, MAX_PATH);
        if(S_OK==hr && pszNumberOfBytesValue !=NULL)
        {
            pszNumberOfBytesValue[MAX_PATH-1]=L'\0';
            dwNumOfBytes=_wtoi(pszNumberOfBytesValue);
            if(dwNumOfBytes)
            {
                
                pI2CTRANS->dwNumOfBytes=dwNumOfBytes;
                if(pI2CTRANS->dwNumOfBytes>MAX_I2C_BUFFER_SIZE)
                {
                    dwRet=FAILURE;
                }
                else
                {
                    NKDbgPrintfW( TEXT("Number of Bytes for Operation=%d"), dwNumOfBytes);
                    dwRet=SUCCESS;
                }
            }
        }
    }
    if(NULL!=pI2CTransElement_NumOfBytes)
    {
        delete pI2CTransElement_NumOfBytes;
        pI2CTransElement_NumOfBytes=NULL;
    }
    
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//getElementSpeed
//Gets speed element value within a transaction element
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getElementSpeed(XmlElement* pElement, I2CTRANS *pI2CTRANS)
{
    ASSERT(pElement && pI2CTRANS);
    DWORD dwRet=FAILURE;
    HRESULT hr=NULL;
    XmlElement* pSpeed=NULL;
    TCHAR pszSpeedValue[MAX_PATH]={};
    DWORD dwSpeedValue=0;
      
    hr=pElement->GetSingleElementByTagName(I2C_CONFIG_FILE_TAG_SPEED, &pSpeed);
    if(S_OK==hr && pSpeed !=NULL)
    {
        hr=pSpeed->GetText(pszSpeedValue,MAX_PATH);
        if(S_OK==hr && pszSpeedValue !=NULL)
        {
            pszSpeedValue[MAX_PATH-1]=L'\0';
            dwSpeedValue= _wtoi(pszSpeedValue);
            if(dwSpeedValue)
            {
                
                pI2CTRANS->dwSpeedOfTransaction=dwSpeedValue;
                NKDbgPrintfW( TEXT("Speed of transaction= %d"), pI2CTRANS->dwSpeedOfTransaction=dwSpeedValue);
                dwRet=SUCCESS;
            }
        }
    }
    MANUAL_RELEASE(pSpeed);
    
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//getElementTimeOut
//Gets timout element value within a transaction element
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getElementTimeOut(XmlElement* pElement, I2CTRANS *pI2CTRANS)
{
    ASSERT(pElement && pI2CTRANS);
    DWORD dwRet=FAILURE;
    HRESULT hr=NULL;
    XmlElement* pTimeOut=NULL;
    TCHAR pszTimeOutValue[MAX_PATH]={};
    DWORD dwTimeOutValue=0;
      
    hr=pElement->GetSingleElementByTagName(I2C_CONFIG_FILE_TAG_TIMEOUT, &pTimeOut);
    if(S_OK==hr && pTimeOut !=NULL)
    {
        hr=pTimeOut->GetText(pszTimeOutValue,MAX_PATH);
        if(S_OK==hr && pszTimeOutValue !=NULL)
        {
            pszTimeOutValue[MAX_PATH-1]=L'\0';
            dwTimeOutValue= _wtoi(pszTimeOutValue);
            if(dwTimeOutValue)
            {
                
                pI2CTRANS->dwTimeOut=dwTimeOutValue;
                NKDbgPrintfW( TEXT("TimeOut for transaction= %d"), dwTimeOutValue);
                dwRet=SUCCESS;
            }
        }
    }
    MANUAL_RELEASE( pTimeOut);
    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
//getAttributeTransactionID
//Gets the attribute ID within a transaction element
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD getAttributeTransactionID(XmlElement* pElement, I2CTRANS *pI2CTRANS)
{
    ASSERT(pElement && pI2CTRANS);
    DWORD dwRet=FAILURE;
    HRESULT hr=NULL;
    TCHAR pszTransID[MAX_PATH]={};
    DWORD dwTransIDValue = 0;
      
    hr=pElement->GetAttributeValue((PTSTR)pszIDAttribute, pszTransID, MAX_PATH);
    if(S_OK==hr && pszTransID !=NULL)
    {
        pszTransID[MAX_PATH-1]=L'\0';
        dwTransIDValue= _wtoi(pszTransID);
        if(dwTransIDValue)
        {
            
            pI2CTRANS->dwTransactionID = dwTransIDValue;
            NKDbgPrintfW( TEXT("Transaction ID = %d"), dwTransIDValue);
            dwRet=SUCCESS;
        }
    }

    return dwRet;
}


////////////////////////////////////////////////////////////////////////////////
//ScanAndGetI2CTransactionNode
//Scans a transaction element for all the elements and values within it, if a 
// mandatory value is missing it returns failure, so that node doesnt make the 
//list of transactions
// Parameters:
// XmlElement* pElement XML Element
// I2CTRANS *pI2CTRANS  Transaction structure
// long index               Transaction index
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD ScanAndGetI2CTransactionNode(XmlElement* pI2CTransElement, I2CTRANS *pI2CTRANS, long index)
{
    ASSERT(pI2CTransElement && pI2CTRANS);
    DWORD dwRet=SUCCESS;
    do{ 
        //return failure if any of the mandatory fields is not present
            NKDbgPrintfW( TEXT("*******************"));
            NKDbgPrintfW( TEXT("Scanning Element %d"), index + 1);
            NKDbgPrintfW( TEXT("*******************"));

            if(FAILURE== getAttributeTransactionID(pI2CTransElement, pI2CTRANS))
            {
                NKDbgPrintfW( TEXT("No Transaction ID. Transaction ignored"));
                dwRet=FAILURE;
                break;
            }

            if(FAILURE== getElementDeviceName(pI2CTransElement, pI2CTRANS))
            {
                NKDbgPrintfW( TEXT("No Bus Name. Transaction ignored"));
                dwRet=FAILURE;
                break;
            }
            
            if(FAILURE== getElementSubordinateAddress(pI2CTransElement, pI2CTRANS))
            {
                NKDbgPrintfW( TEXT("No Device Subordinate Address. Transaction ignored"));
                dwRet=FAILURE;
                break;
            }
                           
            if(FAILURE== getElementOperation(pI2CTransElement, pI2CTRANS))
            {
                NKDbgPrintfW( TEXT("No Operation specified. Transaction ignored"));
                dwRet=FAILURE;
                break;
            }
            if(FAILURE== getElementNumOfBytes(pI2CTransElement, pI2CTRANS))
            {
                NKDbgPrintfW( TEXT("Number of bytes not specified. Transaction ignored"));
                dwRet=FAILURE;
                break;
                                   
            }
            if((pI2CTRANS->Operation==I2C_OP_WRITE) || (pI2CTRANS->Operation==I2C_OP_LOOPBACK) )
            {
                if(FAILURE==getElementBuffer(pI2CTransElement, pI2CTRANS))
                {
                    dwRet=FAILURE;
                    break;    
                }                
            }
            else
            {
                if(FAILURE==getElementBuffer(pI2CTransElement, pI2CTRANS))
                {
                    NKDbgPrintfW( TEXT("No verification data for read"));  
                }

            }
            //do not return failure for any of the optional fields just print the field is missing
            if(FAILURE== getElementRegisterAddress(pI2CTransElement, pI2CTRANS))
            {
                NKDbgPrintfW( TEXT("No Register Address"));
                                   
            }
           
            if(FAILURE== getElementLoopbackAddress(pI2CTransElement, pI2CTRANS))
            {
                NKDbgPrintfW( TEXT("No Loopback Register Address"));
                NKDbgPrintfW( TEXT("Will default to Register Address"));

                pI2CTRANS->dwLoopbackAddress = pI2CTRANS->dwRegisterAddress;
                                   
            }
              if(FAILURE== getElementSpeed(pI2CTransElement, pI2CTRANS))
            {
                NKDbgPrintfW( TEXT("No Speed specified"));
                                   
            }
            if(FAILURE== getElementTimeOut(pI2CTransElement, pI2CTRANS))
            {
                NKDbgPrintfW( TEXT("No Time Out specified Address"));
                                   
            }
            if(FAILURE== getElementSubordinateAddressSize(pI2CTransElement, pI2CTRANS))
            {
                NKDbgPrintfW( TEXT("No Subordinate Address Size"));
            }
            if(FAILURE== getElementRegisterAddressSize(pI2CTransElement, pI2CTRANS))
            {
                NKDbgPrintfW( TEXT("No Register Address Size"));
                                   
            }  
           
    }while(0);
return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//PopulateI2CTransList
//For each valid transaction node, it populates the global trans list struct 
//with the data.
// Parameters:
//  XmlElementList* pElementList        Pointer to XML Element List
//  long *Length                    Number of transaction elements
//  I2CTRANS *pI2CTrans,            POinter to transaction structure
//  long *ActualLength              Actual number of transaction 
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
static DWORD PopulateI2CTransList(XmlElementList* pElementList, long *Length, I2CTRANS *pI2CTrans, long *ActualLength)
{
    ASSERT(pElementList && pI2CTrans && Length && ActualLength);
    HRESULT hr=0;
    DWORD dwRet=FAILURE;
    XmlElement* pI2CTransElement=NULL;
    
    I2CTRANS *I2CTransTemp=(I2CTRANS *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (sizeof(I2CTRANS)));
    I2CTransTemp->BufferPtr=(BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (I2C_MAX_LENGTH));
    I2CTransTemp->pwszDeviceName=(TCHAR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (MAX_PATH));
    
    long index=0, i=0;
    BYTE *APtr=NULL,*BPtr=NULL;
    
    for(i=0,index=0; i<(*Length);i++)
        {  
            if (SUCCEEDED(hr = pElementList->GetNext(&pI2CTransElement))
              && pI2CTransElement !=NULL)
            {
                InitI2CTransList(1,I2CTransTemp);
                
                if(SUCCESS == ScanAndGetI2CTransactionNode(pI2CTransElement, I2CTransTemp, i))
                {
                    //only enter here if the node is valid I2c Transaction node
                    AcquireI2CTestDataLock();   
                    //fixing the embedded pointers only for the node that is valid. 
                    //inorder to avoid, data misalingment between user mode process and kernel mode test 
                    if(0==index)
                    {
                        pI2CTrans[index].BufferPtr= reinterpret_cast<BYTE *> (BufferPtr);
                        BPtr=reinterpret_cast<BYTE *>(BufferPtr);
                        pI2CTrans[index].pwszDeviceName=reinterpret_cast<TCHAR *>(DeviceNamePtr);
                        APtr=reinterpret_cast<BYTE *>(DeviceNamePtr);
                    }
                    else
                    {
                       BPtr=reinterpret_cast<BYTE *>(BPtr+I2C_MAX_LENGTH);
                       pI2CTrans[index].BufferPtr= reinterpret_cast<BYTE *> (BPtr);
                       APtr=reinterpret_cast<BYTE *>(APtr+MAX_PATH);
                       pI2CTrans[index].pwszDeviceName=reinterpret_cast<TCHAR *> (APtr);
                    }
                    
                    pI2CTrans[index].dwSubordinateAddress = I2CTransTemp->dwSubordinateAddress;  
                    StringCbCatN(I2CTransTemp->pwszDeviceName, MAX_PATH, L"\0", 2);
                    StringCbCopyN(pI2CTrans[index].pwszDeviceName, MAX_PATH, I2CTransTemp->pwszDeviceName, MAX_PATH);
                    pI2CTrans[index].Operation = I2CTransTemp->Operation;
                    pI2CTrans[index].dwRegisterAddress = I2CTransTemp->dwRegisterAddress;
                    pI2CTrans[index].dwRegisterAddressSize = I2CTransTemp->dwRegisterAddressSize;
                    pI2CTrans[index].dwLoopbackAddress = I2CTransTemp->dwLoopbackAddress;
                    pI2CTrans[index].dwNumOfBytes = I2CTransTemp->dwNumOfBytes;
                    I2CTransTemp->BufferPtr[I2C_MAX_LENGTH-1]='\0';
                    memcpy(pI2CTrans[index].BufferPtr, I2CTransTemp->BufferPtr, I2C_MAX_LENGTH); 
                    pI2CTrans[index].dwTimeOut = I2CTransTemp->dwTimeOut;
                    pI2CTrans[index].dwSpeedOfTransaction = I2CTransTemp->dwSpeedOfTransaction;
                    pI2CTrans[index].dwSubordinateAddressSize = I2CTransTemp->dwSubordinateAddressSize;
                    pI2CTrans[index].dwTransactionID = I2CTransTemp->dwTransactionID;
                    index++;
                    ReleaseI2CTestDataLock();
                    //atleast one transaction valid in the list for this device, so return success for the function
                    dwRet=SUCCESS;
                }
                
                      
            } 
            MANUAL_RELEASE(pI2CTransElement);  
        }//for
    (*ActualLength)=index; 
    //free the temp struct that we allocated on heap
    if(!HeapFree(GetProcessHeap(), 0, I2CTransTemp->BufferPtr))
           {
                dwRet=FAILURE;
           }
    if(!HeapFree(GetProcessHeap(), 0, I2CTransTemp->pwszDeviceName))
           {
                dwRet=FAILURE;
           }
    if(!HeapFree(GetProcessHeap(), 0, I2CTransTemp))
           {
                dwRet=FAILURE;
           }
    return dwRet;  
       
}
////////////////////////////////////////////////////////////////////////////////
//getParams
//Gets the i2c transaction list from an xml file
// Parameters:
//  TCHAR* szFileName   Config File Name
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
//////////////////////////////////////////////////////////////////////////////// 
DWORD getParams(TCHAR* szFileName )
{
    ASSERT(szFileName);
    DWORD dwRet=FAILURE;
    HRESULT hr = S_OK;
    XmlDocument doc;
    XmlElementList* pElementList = NULL ;
    LONG lSizeOfI2CList=0;

    hr= doc.Load(szFileName);
    if (SUCCEEDED(hr))
    {
        NKDbgPrintfW( TEXT("Opened document"));
        
        // Get all the xml elements with the tag 
        if (SUCCESS==getElementList(&doc, I2C_CONFIG_FILE_TAG_TRANSACTION, &pElementList, &lSizeOfI2CList))
        {   //allocate for the device list
            if(SUCCESS==AllocateI2CTransList(lSizeOfI2CList, &gI2CData))
            {
                if(SUCCESS== PopulateI2CTransList(pElementList, &lSizeOfI2CList, gI2CData, &gI2CDataSize ))
                {
                    dwRet=SUCCESS;
                }
            }
            
        }
        
    }
    else
    {
        NKDbgPrintfW( TEXT("Failed to open document %s"), szFileName);
    }
    MANUAL_RELEASE( pElementList);
    return dwRet;
}


