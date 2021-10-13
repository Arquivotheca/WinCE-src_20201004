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
// @doc
// PipelineManagers.h : Base classes for capture and playback pipeline
// managers.
//

#pragma once
#define PIPELINEMANAGERS_H

#include "..\\PipelineInterfaces.h"
#include "..\\SampleProducer\\ProducerConsumerUtilities.h"

namespace MSDvr
{

class CCommandQueue;

// @struct SPipelineElement |
// Type of component list element
struct SPipelineElement
{
	IPipelineComponent*		m_piComponent;
	bool					m_fContainerIsOwner;
};

// @type t_PipelineComponents |
// Linked list of pipeline components
typedef std::list<SPipelineElement> t_PipelineComponents;

//////////////////////////////////////////////////////////////////////////////
// @class CPipelineRouter |
// Instances of this class are returned by IPipelineManager::GetRouter and
// are used to distribute calls to several components in the pipeline.
// Note: this class is safe for copy construction and assignment.
//////////////////////////////////////////////////////////////////////////////
class CPipelineRouter :			public ICapturePipelineComponent,
								public IPlaybackPipelineComponent
{
// @access Private Members
	// @cmember Constructor for use by the base pipeline manager only
	CPipelineRouter(
								// @parm component list
								const t_PipelineComponents& rcComponentList,
								// @parm asynchronous command queue, NULL for
								// synchronous routing
								CCommandQueue*		pcQueuedCommands,
								// @parm component at which to start
								// distribution; NULL: head or tail,
								// depending on fUpstream value
								IPipelineComponent*	piStart,
								// @parm upstream or downstream distribution
								bool				fUpstream,
								bool				fQueueAtHead = false);

// @access Public Members
public:
	// @cmember Default constructor
	// Note: when using this constructor, the resulting object must not be used
	//		 until properly initialized using operator =
	CPipelineRouter();

// IPipelineComponent
	virtual void	RemoveFromPipeline();
	virtual	ROUTE	GetPrivateInterface(
						REFIID			riid,
						void*&			rpInterface);
	virtual ROUTE	NotifyFilterStateChange(
						FILTER_STATE	eState);
	virtual ROUTE	ConfigurePipeline(
						UCHAR			iNumPins,
						CMediaType		cMediaTypes[],
						UINT			iSizeCustom,
						BYTE			Custom[]);
	virtual ROUTE DispatchExtension(
						// @parm The extension to be dispatched.
						CExtendedRequest &rcExtendedRequest);

// ICapturePipelineComponent
	virtual unsigned char AddToPipeline(
						ICapturePipelineManager& rcManager);
	virtual ROUTE	ProcessInputSample(
						IMediaSample&	riSample,
						CDVRInputPin&	rcInputPin);
	virtual ROUTE	NotifyBeginFlush(
						CDVRInputPin&	rcInputPin);
	virtual ROUTE	NotifyEndFlush(
						CDVRInputPin&	rcInputPin);
	virtual ROUTE	GetAllocatorRequirements(
						ALLOCATOR_PROPERTIES& rProperties,
						CDVRInputPin&	rcInputPin);
	virtual ROUTE	NotifyAllocator(
						IMemAllocator&	riAllocator,
						bool			fReadOnly,
						CDVRInputPin&	rcInputPin);
	virtual ROUTE	CheckMediaType(
						const CMediaType& rcMediaType,
						CDVRInputPin&	rcInputPin,
						HRESULT&		rhResult);
	virtual ROUTE	CompleteConnect(
						IPin&			riReceivePin,
						CDVRInputPin&	rcInputPin);
	virtual ROUTE	NotifyNewSegment(
						CDVRInputPin&	rcInputPin,
						REFERENCE_TIME rtStart,
						REFERENCE_TIME rtEnd,
						double dblRate);


// IPlaybackPipelineComponent
	virtual unsigned char AddToPipeline(
						IPlaybackPipelineManager& rcManager);
	virtual ROUTE	ProcessOutputSample(
						IMediaSample&	riSample,
						CDVROutputPin&	rcOutputPin);
	virtual ROUTE	DecideBufferSize(
						IMemAllocator&	riAllocator,
						ALLOCATOR_PROPERTIES& rProperties,
						CDVROutputPin&	rcOutputPin);
	virtual ROUTE	CheckMediaType(
						const CMediaType& rcMediaType,
						CDVROutputPin&	rcOutputPin,
						HRESULT&		rhResult);
	virtual ROUTE	EndOfStream();

	// @struct SInvoker |
	// Function object used to invoke a pipeline interface method across
	// pipeline components
	struct SInvoker
	{
		virtual				~SInvoker() {}
		virtual ROUTE		operator () (
								IPipelineComponent&	riPipelineComponent) = 0;
		virtual bool		DiscardWhileFlushing() const { return false; }
	};

// @access Private Members
private:
	// @mfunc SInvoker& | CPipelineRouter | AllocateInvoker |
	// This helper template arranges for passing the SInvoker-derived parameter
	// to PipelineInvoke on the stack in case of synchronous invocation, and on
	// the heap when invoking asynchronously
	template <class TSInvoker>
	TSInvoker&				AllocateInvoker(
								TSInvoker&			rsInvoker) const
	{
		// When used asynchronously, the invoker is allocated on the heap, and
		// bound ultimately for the m_psRouterInvoker auto_ptr of
		// CQueuedPipelineCommands
		if (m_pcQueuedCommands)
			return *new TSInvoker(rsInvoker);

		return rsInvoker;
	}

	ROUTE					PipelineInvoke(
								SInvoker&			rsInvoker,
								bool				fForceSync = false) const;

	friend class CBasePipelineManager;
	friend class CQueuedPipelineCommand;

	const t_PipelineComponents* m_pcComponentList;
	CCommandQueue*			m_pcQueuedCommands;
	t_PipelineComponents::const_iterator m_ForwardIterator;
	t_PipelineComponents::const_reverse_iterator m_ReverseIterator;
	bool					m_fUpstream;
};

//////////////////////////////////////////////////////////////////////////////
// @class SGuidCompare |
// A binary predicate class for establishing a strict weak ordering among
// GUID values.
//////////////////////////////////////////////////////////////////////////////
struct SGuidCompare : std::binary_function<REFGUID, REFGUID, bool>
{
	bool operator () (REFGUID arg1, REFGUID arg2) const;
};

//////////////////////////////////////////////////////////////////////////////
// @class SPinGuidCompare |
// A binary predicate class for establishing a strict weak ordering among
// <Pin,GUID> values.
//////////////////////////////////////////////////////////////////////////////

struct SInputPinInterface {
	SInputPinInterface(GUID guid, CDVRInputPin &rcDVRInputPin)
		: iid(guid), riPin(rcDVRInputPin) {};

	CDVRInputPin &riPin;
	GUID         iid;
};

struct SPinGuidCompare : std::binary_function<SInputPinInterface, SInputPinInterface, bool>
{
	bool operator () (SInputPinInterface arg1, SInputPinInterface arg2) const;
};


#include "CommandQueue.h"

//////////////////////////////////////////////////////////////////////////////
// @class CBasePipelineManager |
// Implementation of base functinality for all pipeline managers, capture
// and playback.
//////////////////////////////////////////////////////////////////////////////
class CBasePipelineManager
{
// @access Public Interface
public:
	// @cmember Constructor
	CBasePipelineManager(CBaseFilter& rcFilter);

	// @cmember Destructor
	~CBasePipelineManager();

	// @method CBaseFilter& | IPipelineManager | GetFilter |
	// Access to the sink or source filter object
	CBaseFilter&			GetFilter();

	// @method IUnknown* | IPipelineManager | RegisterCOMInterface |
	// Register a COM interface on the sink or source filter;
	// see IPipelineManager
	IUnknown*				RegisterCOMInterface(
								// @parm COM interface implementation
								CPipelineUnknown&	rcPipUnk);

	// @method void | IPipelineManager | NonDelegatingQueryInterface |
	// Retrieve the fist element in the registration chain for the interface
	// with the given id
	void					NonDelegatingQueryInterface(
								// @parm interface id
								REFIID				riid,
								// @parm ref-counted interface when
								// registration is present, NULL otherwise
								LPVOID&				rpv);

	// @method CPipelineRouter | IPipelineManager | GetRouter |
	// Routers are used to distribute capture and playback pipeline
	// component interface methods through a pipeline
	CPipelineRouter			GetRouter(
								// @parm component at which to start
								// distribution; NULL: head or tail,
								// depending on fUpstream value
								IPipelineComponent*	piStart,
								// @parm upstream or downstream distribution
								bool				fUpstream,
								// @parm immediate distribution, or
								// queuing and streaming thread
								// processing
								bool				fAsync);

	// @method HANDLE | IPipelineManager | GetFlushEvent |
	// Retrieve a handle to the event that will be set when flushing begins
	HANDLE					GetFlushEvent();

	// @method bool | IPipelineManager | WaitEndFlush |
	// Once the flush event is set, call this method; terminate the thread
	// when false is returned
	bool					WaitEndFlush();

	// @method void | IPipelineManager | StartSync |
	// Enter the thread-synchronized flushing state
	void					StartSync();

	// @method void | IPipelineManager | EndSync |
	// Leave the thread-synchronized flushing state
	void					EndSync();

	// @mfunc Append a new pipeline component to the list of components
	// managed by this object
	void					AppendPipelineComponent(
								// @parm component to add
								IPipelineComponent&	riComponent,
								// @parm object ownership transfer flag
								bool				fContainerToOwn);

	// @mfunc Take ownership of the head element; this supports two-phase
	// pipeline addition, then acquisition
	void					TakeHeadOwnership();

	// @mfunc Notify all pipeline components of having been added to the
	// pipeline
	void					NotifyAllAddedComponents(
								// @parm true if this is a capture
								// pipeline, false if a playback pipeline
								bool				fCapture,
								// @parm capture or playback pipeline
								// manager
								IPipelineManager&	riPipelineManager);

	// @mfunc Notify those pipeline components that have already been
	// successfully been notified of pipeline addition of their
	// impending removal, remove all components, and delete them
	void					ClearPipeline();

	// @mfunc Launch the streaming thread to begin processing of methods on
	// asynchronous pipeline routers
	void					StartStreamingThread();

	// @mfunc Wait for the streaming thread to exit, and then close its thread
	// handle
	void					WaitStreamingThread();

// @access Private Members
private:
	// @mfunc Streaming thread entry point
	static DWORD WINAPI GlobalStreamingThreadEntry(LPVOID pvBasePipelineManager);

	// @mfunc Non-static streaming thread processing function
	unsigned int StreamingThreadEntry();

	// @type t_COMRegistrants |
	// Per-IID map of COM interface implementing pipeline components
	typedef std::map<GUID, CPipelineUnknown*, SGuidCompare> t_COMRegistrants;

	// @mdata CBaseFilter | CBasePipelineManager | m_rcFilter |
	// Reference to the owning filter
	CBaseFilter&			m_rcFilter;

	// @mdata map | CBasePipelineManager | m_cCOMRegistrants |
	// Per-interface map of most recently registered implementer
	t_COMRegistrants		m_cCOMRegistrants;


	// @mdata list | CBasePipelineManager | m_cPipelineComponents |
	// List of managed pipeline components
	t_PipelineComponents	m_cPipelineComponents;

	// @mdata unsigned char | CBasePipelineManager | m_bLastNotifiedComponent |
	// The last component in the list that has successfully been notified of
	// having been added to the pipeline
	unsigned char			m_bLastNotifiedComponent;

	// @mdata HANDLE | CBasePipelineManager | m_hStreamingThread |
	// Streaming thread handle; NULL if and only if the streaming thread
	// is not running
	HANDLE					m_hStreamingThread;

	// @mdata HANDLE | CBasePipelineManager | m_hBeginFlushEvent |
	// Signaled while flushing
	CEventHandle			m_hBeginFlushEvent;

	// @mdata HANDLE | CBasePipelineManager | m_hEndFlushEvent |
	// Signaled while streaming
	CEventHandle			m_hEndFlushEvent;

	// @mdata unsigned short | CBasePipelineManager | m_wMacStreamingThreads |
	// The number of streaming threads registered with this pipeline manager
	unsigned short			m_wMacStreamingThreads;

	// @mdata LONG | CBasePipelineManager | m_lThreadsToSync |
	// While syncing, the number of threads WaitEndFlush is still waiting for
	LONG					m_lThreadsToSync;

	// @mdata HANDLE | CBasePipelineManager | m_hThreadSyncEvent |
	// Set when all threads expected to sync have called WaitEndFlush
	CEventHandle			m_hThreadSyncEvent;

	// @mdata queue | CBasePipelineManager | m_cQueuedCommands |
	// List of commands queued for asynchronous execution
	CCommandQueue			m_cQueuedCommands;

	CBasePipelineManager(const CBasePipelineManager&);				// not implemented
	CBasePipelineManager& operator = (const CBasePipelineManager&);	// not implemented
};

//////////////////////////////////////////////////////////////////////////////
// @class CCapturePipelineManager |
// Implementation of base functinality for all capture pipeline managers.
//////////////////////////////////////////////////////////////////////////////
class CCapturePipelineManager : public ICapturePipelineManager
{
// @access Public Interface
public:
	// @cmember Constructor
	CCapturePipelineManager(CDVRSinkFilter& rcFilter);

	// @cmember Access to the sink or source filter object
	virtual CBaseFilter&	GetFilter();

	// @method IUnknown* | IPipelineManager | RegisterCOMInterface |
	// Register a COM interface on the sink or source filter;
	// de-registration is not supported; when a single pipeline
	// component is removed, all will be removed and unregistered
	// Returns: if the returned interface is non-null, then it
	//			should be chained to by the interface implementer
	virtual IUnknown*		RegisterCOMInterface(
								// @parm COM interface implementation
								CPipelineUnknown&	rcPipUnk);

	// @method void | IPipelineManager | NonDelegatingQueryInterface |
	// Retrieve the fist element in the registration chain for the interface
	// with the given id
	virtual void			NonDelegatingQueryInterface(
								// @parm interface id
								REFIID				riid,
								// @parm ref-counted interface when
								// registration is present, NULL otherwise
								LPVOID&				rpv);

	// @method void | IPipelineManager | NonDelegatingQueryInterface |
	// Retrieve the fist element in the registration chain for the interface
	// with the given id and for the given Pin
	virtual void			NonDelegatingQueryPinInterface(
								// @parm input pin
								CDVRInputPin&       rcDVRInputPin,
								// @parm interface id
								REFIID				riid,
								// @parm ref-counted interface when
								// registration is present, NULL otherwise
								LPVOID&				rpv);

	// @method CPipelineRouter | IPipelineManager | GetRouter |
	// Routers are used to distribute capture and playback pipeline
	// component interface methods through a pipeline
	virtual CPipelineRouter	GetRouter(
								// @parm Component at which to start
								// distribution; NULL: head or tail,
								// depending on fUpstream value
								IPipelineComponent*	piStart,
								// @parm Upstream or downstream distribution
								bool				fUpstream,
								// @parm Immediate distribution, or
								// queuing and streaming thread
								// processing
								bool				fAsync);

	// @method HANDLE | IPipelineManager | GetFlushEvent |
	// Retrieve a handle to the event that will be set when flushing begins
	virtual HANDLE			GetFlushEvent();

	// @method bool | IPipelineManager | WaitEndFlush |
	// Once the flush event is set, call this method; terminate the thread
	// when false is returned
	virtual bool			WaitEndFlush();

	// @method void | IPipelineManager | StartSync |
	// Enter the thread-synchronized flushing state
	virtual void			StartSync();

	// @method void | IPipelineManager | EndSync |
	// Leave the thread-synchronized flushing state
	virtual void			EndSync();

	// @cmember Access to the sink filter object
	virtual CDVRSinkFilter&	GetSinkFilter();

	// @method IUnknown* | ICapturePipelineManager | RegisterCOMInterface |
	// Register a COM interface on an input pin; see also the
	// corresponding filter method
	virtual IUnknown*		RegisterCOMInterface(
								// @parm COM interface implementation
								CPipelineUnknown&	rcPipUnk,
								// @parm Exposing COM pin object
								CDVRInputPin&		rcInputPin);

// @access Protected Members
protected:
	// @type t_COMRegistrants |
	// Per-IID map of COM interface implementing pipeline components
	typedef std::map<SInputPinInterface, CPipelineUnknown*, SPinGuidCompare> t_COMPinRegistrants;

	CBasePipelineManager	m_cBasePipelineManager;

	// @mdata map | CBasePipelineManager | m_cCOMRegistrants |
	// Per-interface map of most recently registered implementer
	t_COMPinRegistrants		m_cCOMPinRegistrants;

};

//////////////////////////////////////////////////////////////////////////////
// @class CPlaybackPipelineManager |
// Implementation of base functinality for all playback pipeline managers.
//////////////////////////////////////////////////////////////////////////////
class CPlaybackPipelineManager : public IPlaybackPipelineManager
{
// @access Public Interface
public:
	// @cmember Constructor
	CPlaybackPipelineManager(CDVRSourceFilter& rcFilter);

	// @cmember Access to the sink or source filter object
	virtual CBaseFilter&	GetFilter();

	// @method IUnknown* | IPipelineManager | RegisterCOMInterface |
	// Register a COM interface on the sink or source filter;
	// de-registration is not supported; when a single pipeline
	// component is removed, all will be removed and unregistered
	// Returns: if the returned interface is non-null, then it
	//			should be chained to by the interface implementer
	virtual IUnknown*		RegisterCOMInterface(
								// @parm COM interface implementation
								CPipelineUnknown&	rcPipUnk);

	// @method void | IPipelineManager | NonDelegatingQueryInterface |
	// Retrieve the fist element in the registration chain for the interface
	// with the given id
	virtual void			NonDelegatingQueryInterface(
								// @parm interface id
								REFIID				riid,
								// @parm ref-counted interface when
								// registration is present, NULL otherwise
								LPVOID&				rpv);

	// @method CPipelineRouter | IPipelineManager | GetRouter |
	// Routers are used to distribute capture and playback pipeline
	// component interface methods through a pipeline
	virtual CPipelineRouter	GetRouter(
								// @parm Component at which to start
								// distribution; NULL: head or tail,
								// depending on fUpstream value
								IPipelineComponent*	piStart,
								// @parm Upstream or downstream distribution
								bool				fUpstream,
								// @parm Immediate distribution, or
								// queuing and streaming thread
								// processing
								bool				fAsync);

	// @method HANDLE | IPipelineManager | GetFlushEvent |
	// Retrieve a handle to the event that will be set when flushing begins
	virtual HANDLE			GetFlushEvent();

	// @method bool | IPipelineManager | WaitEndFlush |
	// Once the flush event is set, call this method; terminate the thread
	// when false is returned
	virtual bool			WaitEndFlush();

	// @method void | IPipelineManager | StartSync |
	// Enter the thread-synchronized flushing state
	virtual void			StartSync();

	// @method void | IPipelineManager | EndSync |
	// Leave the thread-synchronized flushing state
	virtual void			EndSync();

	// @cmember Access to the source filter object
	virtual CDVRSourceFilter& GetSourceFilter();

	// @method IUnknown* | IPlaybackPipelineManager | RegisterCOMInterface |
	// Register a COM interface on an output pin; see also the
	// corresponding filter method
	virtual IUnknown*		RegisterCOMInterface(
								// @parm COM interface implementation
								CPipelineUnknown&	rcPipUnk,
								// @parm Exposing COM pin object
								CDVROutputPin&		rcOutputPin);

	// @method HRESULT | IPlaybackPipelineManager | GetMediaType |
	// Per-output pin media type(s) access: default implementation acts as does
	// CBasePin
	virtual HRESULT			GetMediaType(
								// @parm type index
								int					iPosition,
								// @parm media type object to be copied into
								CMediaType*			pMediaType,
								// @parm output pin
								CDVROutputPin&		rcOutputPin);

// @access Protected Members
protected:
	CBasePipelineManager	m_cBasePipelineManager;
};

}
