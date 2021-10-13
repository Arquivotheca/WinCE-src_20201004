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


#include "PerfListTests.h"
#include "Globals.h"


typedef XRObservableCollection<WCHAR*> ObservableCollectionString;
class _declspec(uuid("{7BE5824D-F2D2-4561-A1B2-035742749009}")) EnumerableTProp : public TPropertyBag<EnumerableTProp>
{
public:
    TBoundPointerProperty<IXREnumerable> m_pEnumerable;
};


////////////////////////////////////////////////////////////////////////////////
//
//  InitListBoxDataBindingStringCollection
//
//   Uses the DataBinding (simple) method to fill a listbox with strings.
//
////////////////////////////////////////////////////////////////////////////////
HRESULT InitListBoxDataBindingStringCollection( IXRListBox *plstBox, int NumItems )
{
    HRESULT hr = S_OK;
    DWORD dwRet = TPR_FAIL;

    XRValue valueValue;
    XRPtr<EnumerableTProp> valueProp;
    ObservableCollectionString* pObsCollection = NULL;

    //////////////////////////Data Binding/////////////////////////////
    //Create the collection
    CHK_HR( XRObject<ObservableCollectionString>::CreateInstance(&pObsCollection), L"ObservableCollectionString::CreateInstance", dwRet);
    CHK_PTR(pObsCollection, L"ObservableCollectionString* pObsCollection", dwRet);

    //Fill the collection, this can also be done after displaying the listbox (a data binding feature)
    for(int i = 1; i < NumItems; i++)
    {
        WCHAR Buffer[100] = L"";
        StringCchPrintf(Buffer, _countof(Buffer), L"ListItem%d", i);

        CHK_HR(pObsCollection->Add(Buffer), L"pObsCollection->Add", dwRet);
    }

    //Attach the collection to the EnumerableTProp
    CHK_HR(EnumerableTProp::CreateInstance(&valueProp), L"EnumerableTProp::CreateInstance", dwRet);

    valueValue.vType = VTYPE_ENUMERABLE;
    valueValue.pEnumerableVal = pObsCollection;

    //Attach the EnumerableTProp to the word "foostrDBind" which is in the xaml
    CHK_HR(valueProp->BeginRegisterProperties(), L"BeginRegisterProperties", dwRet);
    CHK_HR(valueProp->RegisterBoundProperty(L"foostrDBind", valueProp->m_pEnumerable), L"RegisterBoundProperty - Enumerable", dwRet);    
    CHK_HR(valueProp->EndRegisterProperties(), L"EndRegisterProperties", dwRet);
    CHK_HR(valueProp->SetValue(L"foostrDBind", &valueValue), L"EnumerableTProp::SetValue", dwRet);

    //Attach the word "foostrDBind" the listbox control
    CHK_HR(plstBox->SetDataContext(valueProp), L"EnumerableTProp::SetDataContext", dwRet);

Exit:
    SAFE_FREEXRVALUE( &valueValue );

    return hr;
}

////////////////////////////////////////////////////////////////////////////////
//
//  DataTemplate objects
//
//   These objects are needed for DataTemplate binding
//
////////////////////////////////////////////////////////////////////////////////

class _declspec(uuid("{94646A5F-B97D-416b-AFE1-A76C1DBD5E16}")) PeopleInfoBlock : public TPropertyBag<PeopleInfoBlock>
{
public:
    PeopleInfoBlock()
    {
    }

    ~PeopleInfoBlock()
    {
    }

    HRESULT InitializeProperties()
    {
        HRESULT hr = S_OK;
        DWORD dwRet = TPR_FAIL;

        CHK_HR(BeginRegisterProperties(), L"BeginRegisterProperties", dwRet);
        
        CHK_HR(RegisterBoundProperty(L"People", m_pPeople), L"RegisterBoundProperty People", dwRet);
        
        CHK_HR(EndRegisterProperties(), L"EndRegisterProperties", dwRet);

    Exit:
        return hr;
    }

    TBoundPointerProperty<IXREnumerable> m_pPeople;

};

class _declspec(uuid("{B1FC5CA3-EB83-4b1c-AD5F-6CA5519D41B5}")) Person : public TPropertyBag<Person>
{
public:
    Person()
    {
    }

    ~Person()
    {
    }

    HRESULT InitializeProperties()
    {
        HRESULT hr = S_OK;
        DWORD dwRet = TPR_FAIL;

        CHK_HR(BeginRegisterProperties(), L"BeginRegisterProperties", dwRet);
        
        CHK_HR(RegisterBoundProperty(L"FirstName", m_pFirstName), L"RegisterBoundProperty FirstName", dwRet);
        CHK_HR(RegisterBoundProperty(L"LastName", m_pLastName), L"RegisterBoundProperty LastName", dwRet);
        CHK_HR(RegisterBoundProperty(L"Location", m_pLocation), L"RegisterBoundProperty Location", dwRet);
        CHK_HR(RegisterBoundProperty(L"StartDate", m_pStartDate), L"RegisterBoundProperty StartDate", dwRet);
        CHK_HR(RegisterBoundProperty(L"PictureFileName", m_pPictureFileName), L"RegisterBoundProperty PictureFileName", dwRet);
        CHK_HR(RegisterBoundProperty(L"BGColor", m_pBGColor), L"RegisterBoundProperty BGColor", dwRet);

        CHK_HR(EndRegisterProperties(), L"EndRegisterProperties", dwRet);

        m_pFirstName = NULL;
        m_pLastName = NULL;
        m_pLocation = NULL;
        m_pStartDate = NULL;
        m_pPictureFileName = NULL;
        m_pBGColor = NULL;

    Exit:
        return hr;
    }

    static HRESULT CreatePerson(__in const wchar_t *pFirstName, __in const wchar_t *pLastName, __in const wchar_t *pLocation, __in const wchar_t *pStartDate, __in const wchar_t *pPictureFileName, __in const wchar_t *pBGColor, __out Person **ppPerson)
    {
        HRESULT hr = S_OK;
        DWORD dwRet = TPR_FAIL;
    
        Person *pPerson = NULL;
    
        Person::CreateInstance(&pPerson);
        CHK_PTR(pPerson, L"pPerson", dwRet );
        CHK_HR(pPerson->InitializeProperties(), L"pPerson->InitializeProperties()", dwRet );
    
        pPerson->m_pFirstName = SysAllocString(pFirstName);
        pPerson->m_pLastName = SysAllocString(pLastName);
        pPerson->m_pLocation = SysAllocString(pLocation);
        pPerson->m_pStartDate = SysAllocString(pStartDate);
        pPerson->m_pPictureFileName = SysAllocString(pPictureFileName);
        pPerson->m_pBGColor = SysAllocString(pBGColor);
    
        //tx ownership
        *ppPerson = pPerson;
        pPerson = NULL;
    
    Exit:
        SAFE_RELEASE(pPerson);
        return hr;
    }

    TBoundProperty<BSTR> m_pFirstName;
    TBoundProperty<BSTR> m_pLastName;
    TBoundProperty<BSTR> m_pLocation;
    TBoundProperty<BSTR> m_pStartDate;
    TBoundProperty<BSTR> m_pPictureFileName;
    TBoundProperty<BSTR> m_pBGColor;
};

////////////////////////////////////////////////////////////////////////////////
//
//  InitListBoxDataBindingTemplate
//
//   Fill a listbox according to a datatemplate
//
////////////////////////////////////////////////////////////////////////////////
HRESULT InitListBoxDataBindingTemplate( IXRListBox *plstBox, int NumItems, bool bLoadImagesFromFiles )
{
    HRESULT hr = S_OK;
    DWORD dwRet = TPR_FAIL;

    WCHAR szBuffer[20] = {0};
    XRPtr<PeopleInfoBlock> pPeopleInfoBlock;
    XRPtr<XRObservableCollection<IXRPropertyBag *>,IXREnumerable> pPersonCollection;
    XRPtr<Person> pPerson;
    XRValue value;
    int i;

    PeopleInfoBlock::CreateInstance(&pPeopleInfoBlock);
    CHK_PTR(pPeopleInfoBlock, L"pPeopleInfoBlock", dwRet);
    CHK_HR(pPeopleInfoBlock->InitializeProperties(), L"pPeopleInfoBlock->InitializeProperties", dwRet);  //Registers for the Binding Path in the xaml

    XRObject<XRObservableCollection<IXRPropertyBag *>>::CreateInstance(&pPersonCollection);
    CHK_PTR(pPersonCollection, L"pPersonCollection", dwRet);
    //Add people Collection to the Property Bag
    pPeopleInfoBlock->m_pPeople = pPersonCollection;

    //set the propertybag
    value.vType = VTYPE_PROPERTYBAG;
    value.pPropertyBagVal = pPeopleInfoBlock;
    CHK_HR(plstBox->SetDataContext(&value), L"plstBox->SetDataContext", dwRet);

    //Create the people and add them to the collection
    for( i = 0; i < NumItems; i++ )
    {
        if( bLoadImagesFromFiles )
        {
            StringCchPrintfW( szBuffer, 20, L"XRPix%02d.jpg", 1+i );
        }
        else
        {
            StringCchPrintfW( szBuffer, 20, L"Pix%02d.png", 1+(i%20) );
        }

        switch( i % 4 )
        {
        case 0:
            Person::CreatePerson( L"Joe", L"Smith", L"Studio H", L"1/15/1998", szBuffer, L"#FFB0FFF0", &pPerson );
            break;
        case 1:
            Person::CreatePerson( L"Lana", L"Jones", L"Redmond", L"2/25/2000", szBuffer, L"#FFB0DDF0", &pPerson );
            break;
        case 2:
            Person::CreatePerson( L"Allan", L"Woods", L"Oregon", L"5/5/2002", szBuffer, L"#FFB0FFF0", &pPerson );
            break;
        default:
            Person::CreatePerson( L"Lisa", L"Martz", L"Building 8", L"3/6/1997", szBuffer, L"#FFB0DDF0", &pPerson );
            break;
        }
        pPersonCollection->Add( pPerson );
    }

Exit:
    return hr;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//  DataBinding Template for Images ONLY
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
//  DataTemplate objects
//
//   These objects are needed for DataTemplate binding
//
////////////////////////////////////////////////////////////////////////////////


class _declspec(uuid("{BE9B56C3-723D-4efd-8FA1-3BF7494E0584}")) PersonImageOnly : public TPropertyBag<PersonImageOnly>
{
public:
    PersonImageOnly()
    {
    }

    ~PersonImageOnly()
    {
    }

    HRESULT InitializeProperties()
    {
        HRESULT hr = S_OK;
        DWORD dwRet = TPR_FAIL;

        CHK_HR(BeginRegisterProperties(), L"BeginRegisterProperties", dwRet);
        
        CHK_HR(RegisterBoundProperty(L"BGColor", m_pBGColor), L"RegisterBoundProperty BGColor", dwRet);
        CHK_HR(RegisterBoundProperty(L"PictureFileName", m_pPictureFileName), L"RegisterBoundProperty PictureFileName", dwRet);

        CHK_HR(EndRegisterProperties(), L"EndRegisterProperties", dwRet);

        m_pBGColor = NULL;
        m_pPictureFileName = NULL;

    Exit:
        return hr;
    }

    static HRESULT CreatePerson(__in const wchar_t *pPictureFileName, __in const wchar_t *pBGColor, __out PersonImageOnly **ppPerson)
    {
        HRESULT hr = S_OK;
        DWORD dwRet = TPR_FAIL;
    
        PersonImageOnly *pPerson = NULL;
    
        PersonImageOnly::CreateInstance(&pPerson);
        CHK_PTR(pPerson, L"pPerson", dwRet );
        CHK_HR(pPerson->InitializeProperties(), L"pPerson->InitializeProperties()", dwRet );
    
        pPerson->m_pBGColor = SysAllocString(pBGColor);
        pPerson->m_pPictureFileName = SysAllocString(pPictureFileName);
    
        //tx ownership
        *ppPerson = pPerson;
        pPerson = NULL;
    
    Exit:
        SAFE_RELEASE(pPerson);
        return hr;
    }

    TBoundProperty<BSTR> m_pBGColor;
    TBoundProperty<BSTR> m_pPictureFileName;
};

////////////////////////////////////////////////////////////////////////////////
//
//  InitListBoxDataBindingTemplate
//
//   Fill a listbox according to a datatemplate
//
////////////////////////////////////////////////////////////////////////////////
HRESULT InitListBoxDataBindingTemplateImageOnly( IXRListBox *plstBox, int NumItems )
{
    HRESULT hr = S_OK;
    DWORD dwRet = TPR_FAIL;

    WCHAR szBuffer[20] = {0};
    XRPtr<PeopleInfoBlock> pPeopleInfoBlock;
    XRPtr<XRObservableCollection<IXRPropertyBag *>,IXREnumerable> pPersonCollection;
    XRPtr<PersonImageOnly> pPerson;
    XRValue value;
    int i;

    PeopleInfoBlock::CreateInstance(&pPeopleInfoBlock);
    CHK_PTR(pPeopleInfoBlock, L"pPeopleInfoBlock", dwRet);
    CHK_HR(pPeopleInfoBlock->InitializeProperties(), L"pPeopleInfoBlock->InitializeProperties", dwRet);  //Registers for the Binding Path in the xaml

    XRObject<XRObservableCollection<IXRPropertyBag *>>::CreateInstance(&pPersonCollection);
    CHK_PTR(pPersonCollection, L"pPersonCollection", dwRet);
    //Add people Collection to the Property Bag
    pPeopleInfoBlock->m_pPeople = pPersonCollection;

    //set the propertybag
    value.vType = VTYPE_PROPERTYBAG;
    value.pPropertyBagVal = pPeopleInfoBlock;
    CHK_HR(plstBox->SetDataContext(&value), L"plstBox->SetDataContext", dwRet);

    //Create the people and add them to the collection
    for( i = 0; i < NumItems; i++ )
    {
        StringCchPrintfW( szBuffer, 20, L"XRPix%02d.jpg", i+1 );
        PersonImageOnly::CreatePerson( szBuffer, i % 2 ? L"#FFB0DDF0" : L"#FFB0BBF0", &pPerson );
        pPersonCollection->Add( pPerson );
    }

Exit:
    return hr;
}
