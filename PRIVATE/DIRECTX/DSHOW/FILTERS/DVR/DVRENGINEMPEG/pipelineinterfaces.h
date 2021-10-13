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
// PipelineInterfaces.h : Internal interfaces exposed by sink and source filter
// pipeline components.
//

#pragma once

namespace MSDvr
{

interface IPipelineComponent;
class		CPipelineRouter;
class		CDVRSinkFilter;
class		CDVRSourceFilter;
class		CDVRInputPin;
class		CDVROutputPin;
class		CPauseBufferData;
interface	ISampleProducer;

EXTERN_C const IID IID_IWriter;
EXTERN_C const IID IID_IReader;
EXTERN_C const IID IID_IPauseBufferMgr;
EXTERN_C const IID IID_ISampleProducer;
EXTERN_C const IID IID_ISampleConsumer;
EXTERN_C const IID IID_IDecoderDriver;

//////////////////////////////////////////////////////////////////////////////
// CPipelineUnknownBase, CPipelineUnknown and TRegisterableCOMInterface
// form the basis of types	used to implement COM interfaces provided by
// pipeline components. They make IUnknown interface methods invisible to
// the implementing components.
//////////////////////////////////////////////////////////////////////////////

// @interface CPipelineUnknownBase | TODO - Fill in the class description
class CPipelineUnknownBase
{
protected:
	// @cmember Constructor
	CPipelineUnknownBase();

	// @cmember Implementation of the filter or pin with which the single
	// or multiple COM interfaces that this object implements are registered;
	// there is a single instance of this member, regardless of how often
	// TRegisterableCOMInterface appears in the inheritance
	// hierarchy
	CUnknown*				m_pcUnknown;

private:
	friend class CBasePipelineManager;
	friend class CCapturePipelineManager;

	CPipelineUnknownBase(const CPipelineUnknownBase&);					// not implemented
	CPipelineUnknownBase& operator = (const CPipelineUnknownBase&);		// not implemented
};

// @interface CPipelineUnknown | TODO - Fill in the class description
// @base virtual | CPipelineUnknownBase
struct CPipelineUnknown :			virtual CPipelineUnknownBase
{
	// @cmember The pipeline manager retrieves the COM interface to be
	// registered via this method
	virtual IUnknown&		GetRegistrationInterface() = 0;

	// @cmember The pipeline manager retrieves the IID of the COM interface
	// to be registered via this method
	virtual const IID&		GetRegistrationIID() = 0;
};

// @interface TRegisterableCOMInterface | Templatized class that a pipeline
// component can derive from to expose a COM interface from the filter.
// @tcarg class | T | COM interface that pipeline component exposes.
// @base public | T
// @base public | CPipelineUnknown
template <class T>
class TRegisterableCOMInterface :	public T,
									public CPipelineUnknown
{
public:
	// @cmember Automagic QI Implementation for pipeline components
	STDMETHOD( QueryInterface(REFIID riid, void** ppv) );
	// @cmember Automagic AddRef Implementation for pipeline components
	STDMETHOD_( ULONG, AddRef() );
	// @cmember Automagic Release Implementation for pipeline components
	STDMETHOD_( ULONG, Release() );

private:
	virtual IUnknown&		GetRegistrationInterface();
	virtual const IID&		GetRegistrationIID();
};

//////////////////////////////////////////////////////////////////////////////
// @interface CExtendedRequest |
// Parent classes of objects to hold non-standard requests to be
// propagated down the pipeline.
//////////////////////////////////////////////////////////////////////////////
class CExtendedRequest
{
public:
	enum FlushAndStopBehavior {
		DISCARD_ON_FLUSH,
		RETAIN_ON_FLUSH,
		EXECUTE_ON_FLUSH
	};

	enum ExtendedRequestType
	{
		SAMPLE_PRODUCER_SEND_EVENT_WITH_POSITION,
		DECODER_DRIVER_ENACT_RATE,
		DECODER_DRIVER_SEEK_COMPLETE,
		DECODER_DRIVER_SEND_NOTIFICATION,
		DECODER_DRIVER_END_SEGMENT,
		DECODER_DRIVER_CALLBACK_AT_POSITION
	};

	FlushAndStopBehavior m_eFlushAndStopBehavior;
	IPipelineComponent *m_iPipelineComponentPrimaryTarget;
	ExtendedRequestType m_eExtendedRequestType;

protected:
	CExtendedRequest(IPipelineComponent *piPipelineComponent,
					ExtendedRequestType eExtendedRequestType,
					FlushAndStopBehavior eFlushAndStopBehavior
					= DISCARD_ON_FLUSH)
				: m_lRefs(1)
				, m_iPipelineComponentPrimaryTarget(piPipelineComponent)
				, m_eExtendedRequestType(eExtendedRequestType)
				, m_eFlushAndStopBehavior(eFlushAndStopBehavior)
				{}

	LONG m_lRefs;

public:
	virtual ~CExtendedRequest() {}

	LONG AddRef()
			{
				return InterlockedIncrement(&m_lRefs);
			}
	LONG Release()
			{
				LONG lRefs = InterlockedDecrement(&m_lRefs);
				if (!lRefs)
					delete this;
				return lRefs;
			}

private:  // not implemented deliberately
	CExtendedRequest(CExtendedRequest &);
	CExtendedRequest & operator = (CExtendedRequest);
};

//////////////////////////////////////////////////////////////////////////////
// @interface IPipelineManager |
// All pipeline managers, capture and playback, implement this interface.
// Note: access the base types returned by this interface with caution;
//		 in particular, non-virtual modifiers might need to be called via
//		 the derived type.
//////////////////////////////////////////////////////////////////////////////
interface IPipelineManager
{
	// @cmember Destruction
	virtual ~IPipelineManager();

	// @cmember Access to the sink or source filter object
	virtual CBaseFilter&	GetFilter() = 0;

	// @method IUnknown* | IPipelineManager | RegisterCOMInterface |
	// Register a COM interface on the sink or source filter;
	// de-registration is not supported; when a single pipeline
	// component is removed, all will be removed and unregistered
	// Returns: if the returned interface is non-null, then it
	//			should be chained to by the interface implementer
	virtual IUnknown*		RegisterCOMInterface(
								// @parm COM interface implementation
								CPipelineUnknown&	rcPipUnk) = 0;

	// @method void | IPipelineManager | NonDelegatingQueryInterface |
	// Retrieve the fist element in the registration chain for the interface
	// with the given id
	virtual void			NonDelegatingQueryInterface(
								// @parm interface id
								REFIID				riid,
								// @parm ref-counted interface when
								// registration is present, NULL otherwise
								LPVOID&				rpv) = 0;

	// @cmember Identification of the primary stream
	virtual CBasePin&		GetPrimary() = 0;

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
								bool				fAsync) = 0;

	// @method HANDLE | IPipelineManager | GetFlushEvent |
	// Retrieve a handle to the event that will be set when flushing begins
	virtual HANDLE			GetFlushEvent() = 0;

	// @method bool | IPipelineManager | WaitEndFlush |
	// Once the flush event is set, call this method; terminate the thread
	// when false is returned
	virtual bool			WaitEndFlush() = 0;

	// @method void | IPipelineManager | StartSync |
	// Enter the thread-synchronized flushing state
	virtual void			StartSync() = 0;

	// @method void | IPipelineManager | EndSync |
	// Leave the thread-synchronized flushing state
	virtual void			EndSync() = 0;
};

//////////////////////////////////////////////////////////////////////////////
// @interface ICapturePipelineManager |
// All capture pipeline managers implement this interface.
// @base | IPipelineManager
//////////////////////////////////////////////////////////////////////////////
interface ICapturePipelineManager		: IPipelineManager
{
	// @cmember Access to the sink filter object
	virtual CDVRSinkFilter&	GetSinkFilter() = 0;

	// @method IUnknown* | ICapturePipelineManager | RegisterCOMInterface |
	// Register a COM interface on an input pin; see also the
	// corresponding filter method
	virtual IUnknown*		RegisterCOMInterface(
								// @parm COM interface implementation
								CPipelineUnknown&	rcPipUnk,
								// @parm Exposing COM pin object
								CDVRInputPin&		rcInputPin) = 0;

	// @mfunc Type-safe identification of the primary stream
	virtual CDVRInputPin&	GetPrimaryInput() = 0;

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
								LPVOID&				rpv) = 0;
};

//////////////////////////////////////////////////////////////////////////////
// @interface IPlaybackPipelineManager |
// All playback pipeline managers implement this interface.
// @base | IPipelineManager
//////////////////////////////////////////////////////////////////////////////
interface IPlaybackPipelineManager		: IPipelineManager
{
	// @cmember Access to the source filter object
	virtual CDVRSourceFilter&	GetSourceFilter() = 0;

	// @method IUnknown* | IPlaybackPipelineManager | RegisterCOMInterface |
	// Register a COM interface on an output pin; see also the
	// corresponding filter method
	virtual IUnknown*		RegisterCOMInterface(
								// @parm COM interface implementation
								CPipelineUnknown&	rcPipUnk,
								// @parm Exposing COM pin object
								CDVROutputPin&		rcOutputPin) = 0;

	// @mfunc Type-safe identification of the primary stream
	virtual CDVROutputPin&	GetPrimaryOutput() = 0;

	// @mfunc Per-output pin media type(s) access
	virtual HRESULT			GetMediaType(
								// @parm type index
								int					iPosition,
								// @parm media type object to be copied into
								CMediaType*			pMediaType,
								// @parm output pin
								CDVROutputPin&		rcOutputPin) = 0;
};

// @enum Modes in which pipeline components can handle routing functions.
enum ROUTE
{
	UNHANDLED_CONTINUE,	// @emem not handled, continue routing
						// (default implementation for non-pure virtuals)
	HANDLED_CONTINUE,	// @emem handled, continue routing
	UNHANDLED_STOP,		// @emem not handled, stop routing
	HANDLED_STOP		// @emem handled, stop routing
};

//////////////////////////////////////////////////////////////////////////////
// @interface IPipelineComponent |
// All capture and playback pipeline components must expose the
// IPipelineComponent interface. Implementations must be provided for all
// pure virtual functions.
// Functions that are distributed throughout the pipeline by the router
// are typed to return a value from the ROUTE enumeration.
//////////////////////////////////////////////////////////////////////////////
interface IPipelineComponent
{
	// @cmember Destruction
	virtual ~IPipelineComponent();

	// @cmember Invoked at time of pipeline removal
	virtual void	RemoveFromPipeline() = 0;

	// @method ROUTE | IPipelineComponent | GetPrivateInterface |
	// Pipeline components can implement private (non-COM) interfaces and
	// expose them to one another irrespective of their relative
	// in-pipeline positioning
	virtual	ROUTE	GetPrivateInterface(
						// @parm ID of the private interface
						REFIID			riid,
						// @parm C++ interface or C function pointer
						void*&			rpInterface);

	// @method ROUTE | IPipelineComponent | NotifyFilterStateChange |
	// Pipeline components are notified of filter state changes
	virtual ROUTE	NotifyFilterStateChange(
						// State being put into effect
						FILTER_STATE	eState);

	// @method ROUTE | IPipelineComponent | ConfigurePipeline |
	// Pipeline configuration <nl>
	// Capture: sent by capture pipeline manager before any media
	//			samples are queued and when changes are detected;
	//			handled by serializer/writer <nl>
	// Playback: sent by reader/deserializer after being added to the
	//			pipeline; queued by reader/deserializer in-line with
	//			media samples for mid-stream changes; handled by
	//			playback pipeline manager
	virtual ROUTE	ConfigurePipeline(
						// @parm Number of streams
						UCHAR			iNumPins,
						// @parm Ordered array of media types, of the
						// number indicated
						CMediaType		cMediaTypes[],
						// @parm Size of custom pipeline manager
						// information
						UINT			iSizeCustom,
						// @parm Custom pipeline manager information,
						// of the size indicated
						BYTE			Custom[]);

	// @method ROUTE | IPipelineComponent | DispatchExtension |
	// Invoked when an extended dispatch request needs to be processed.
	virtual ROUTE DispatchExtension(
						// @parm The extension to be dispatched.
						CExtendedRequest &rcExtendedRequest);
};

//////////////////////////////////////////////////////////////////////////////
// @interface ICapturePipelineComponent |
// All capture pipeline components must expose this interface.
// Implementations must be provided for all pure virtual functions.
// @base | IPipelineComponent
//////////////////////////////////////////////////////////////////////////////
interface ICapturePipelineComponent		: IPipelineComponent
{
	// @method unsigned char | ICapturePipelineComponent | AddToPipeline |
	// Invoked at time of pipeline adding; returns number of streaming
	// threads to manage
	virtual unsigned char AddToPipeline(
						// @parm Gives pipeline component access to the pipeline
						// manager, and thus the entire filter infrastructure
						ICapturePipelineManager& rcManager) = 0;

	// @method ROUTE | ICapturePipelineComponent | ProcessInputSample |
	// Sample processing <nl>
	// Invoker:	input pin - asynchronous
	virtual ROUTE	ProcessInputSample(
						// @parm Sample received from upstream pin
						IMediaSample&	riSample,
						// @parm Receiving pin
						CDVRInputPin&	rcInputPin);

	// @method ROUTE | ICapturePipelineComponent | NotifyBeginFlush |
	// Pipeline flushing - start <nl>
	// Invoker: input pin - synchronous
	// Handler:	input pin, pre routing; all media samples are removed from
	//			the queue, regardless of their pin affiliation
	virtual ROUTE	NotifyBeginFlush(
						// @parm Receiving pin
						CDVRInputPin&	rcInputPin);

	// @method ROUTE | ICapturePipelineComponent | NotifyEndFlush |
	// Pipeline flushing - end <nl>
	// Invoker: input pin - synchronous <nl>
	virtual ROUTE	NotifyEndFlush(
						// @parm Receiving pin
						CDVRInputPin&	rcInputPin);

	// @method ROUTE | ICapturePipelineComponent | GetAllocatorRequirements |
	// Allocator negotiation <nl>
	// Invoker: input pin - synchronous <nl>
	// Handler: sample consumer
	virtual ROUTE	GetAllocatorRequirements(
						// @parm Properties to be used for allocator requirement
						// communication
						ALLOCATOR_PROPERTIES& rProperties,
						// @parm Receiving pin
						CDVRInputPin&	rcInputPin);

	// @method ROUTE | ICapturePipelineComponent | NotifyAllocator |
	// Allocator negotiation outcome notification <nl>
	// Invoker: input pin - synchronous
	virtual ROUTE	NotifyAllocator(
						// @parm Pointer to chosen allocator
						IMemAllocator&	riAllocator,
						// @parm Read-only sample indicator
						bool			fReadOnly,
						// @parm Receiving pin
						CDVRInputPin&	rcInputPin);

	// @method ROUTE | ICapturePipelineComponent | CheckMediaType |
	// Pin type negotiation <nl>
	// Invoker:	input pin - synchronous <nl>
	// Handler: pipeline manager, post routing, and if handled flag remains
	//			set to false
	virtual ROUTE	CheckMediaType(
						// @parm Proposed media type
						const CMediaType& rcMediaType,
						// @parm Receiving pin
						CDVRInputPin&	rcInputPin,
						// @parm Format acceptance indicator
						HRESULT&		rhResult);

	// @method ROUTE | ICapturePipelineComponent | CompleteConnect |
	// Pin connection establishment <nl>
	// Invoker: input pin - synchronous <nl>
	// Handler: pipeline manager, post routing
	virtual ROUTE	CompleteConnect(
						// @parm Pointer to the output pin's interface
						IPin&			riReceivePin,
						// @parm Receiving pin
						CDVRInputPin&	rcInputPin);

	// @method ROUTE | ICapturePipelineComponent | NotifyNewSegment |
	// Pipeline segment start <nl>
	// Invoker: input pin - asynchronous
	virtual ROUTE	NotifyNewSegment(
						// @parm Receiving pin
						CDVRInputPin&	rcInputPin,
						REFERENCE_TIME rtStart,
						REFERENCE_TIME rtEnd,
						double dblRate);

};

//////////////////////////////////////////////////////////////////////////////
// @interface IWriter |
//	Writers implement this interface.
// @base | ICapturePipelineComponent
//////////////////////////////////////////////////////////////////////////////
interface IWriter						: ICapturePipelineComponent
{
	// @cmember
	// The pipeline manager will call this method before a writer will be
	// added to the pipeline; some time after being added, but before being
	// asked to capture input samples, the pipeline configuration message
	// will be routed to the writer
	// Returns: true if the writer can catpure to the indicated file at the
	//			indicated location, false otherwise
	virtual bool CanWriteFile(LPCTSTR sFullPathName) = 0;

	// @cmember
	// Return the guaranteed maximum latency between when the writer accepts a
	// media sample and when a reader attached to the same file can seek to that
	// same sample.  This is essentially how big the sample window is that the
	// sample producer makes available to playback graphs.
	virtual LONGLONG GetMaxSampleLatency() = 0;

	// @method LONGLONG | IWriter | GetLatestSampleTime |
	// Debug/sanity check method for verifying producer / writer gap <nl>
	virtual LONGLONG GetLatestSampleTime() = 0;
};

//////////////////////////////////////////////////////////////////////////////
// @interface IPlaybackPipelineComponent |
//	All playback pipeline components must expose this interface.
//	Implementations must be provided for all pure virtual functions.
// @base | IPipelineComponent
//////////////////////////////////////////////////////////////////////////////
interface IPlaybackPipelineComponent	: IPipelineComponent
{
	// @method unsigned char | IPlaybackPipelineComponent | AddToPipeline |
	// Invoked at time of pipeline adding; returns number of streaming
	// threads to manage
	virtual unsigned char AddToPipeline(
						// @parm Gives pipeline component access to the pipeline
						// manager, and thus the entire filter infrastructure
						IPlaybackPipelineManager& rcManager) = 0;

	// @method ROUTE | IPlaybackPipelineComponent | ProcessOutputSample |
	// Sample processing <nl>
	// Handler:	output pin - may block
	virtual ROUTE	ProcessOutputSample(
						// @parm Media sample to be delivered; must have been
						// acquired from target pin's allocator
						IMediaSample&	riSample,
						// @parm Sending pin
						CDVROutputPin&	rcOutputPin);

	// @method ROUTE | IPlaybackPipelineComponent | DecideBufferSize |
	// Allocator negotiation <nl>
	// Invoker:	output pin - synchronous <nl>
	// Handler:	sample producer
	virtual ROUTE	DecideBufferSize(
						// @parm Configuration target
						IMemAllocator&	riAllocator,
						// @parm Configuration
						ALLOCATOR_PROPERTIES& rProperties,
						// @parm Allocator's affiliation
						CDVROutputPin&	rcOutputPin);

	// @method ROUTE | IPlaybackPipelineComponent | CheckMediaType |
	// Pin type negotiation <nl>
	// Invoker:	output pin - synchronous <nl>
	// Handler: pipeline manager, post routing, and if handled flag remains
	//			set to false
	virtual ROUTE	CheckMediaType(
						// @parm Proposed media type
						const CMediaType& rcMediaType,
						// @parm Sending pin
						CDVROutputPin&	rcOutputPin,
						// @parm Format acceptance indicator
						HRESULT&		rhResult);

	// @method ROUTE | IPlaybackPipelineComponent | EndOfStream |
	// Deliver end of stream notifications via all output pins <nl>
	// Invocation: asynchronous <nl>
	// Handler: pipeline manager, post routing
	virtual ROUTE	EndOfStream();
};

// @enum
// The reader supports multiple read modes for playback rate support.  The
// 'normal' mode will be to read every sample of every stream.  In the 'fast'
// mode, only samples of the primary stream are read whose syncPoint flag
// is set, ie keyframes.  We can also support even faster modes where we only
// deliver every nth key frame - or, alternatively, key frames spaced every n
// seconds apart.  At this point it's unknown whether we'll need to support
// these faster modes and what the value of n would be for these faster modes.
// This will need to be determined once we have real hardware.  In the 'reverse
// fast' mode, key frames are read progress backwards from the current position.
// Finally, there exist multiple extended fast and reverse-fast modes for
// implementing super fast rates (ie +/- 100x) where not even all key frames are
// read, but rather key frames separated by the specified interval.  The values
// of these intervals will be tweeked once we have real hardware.
enum FRAME_SKIP_MODE
{
	SKIP_MODE_NORMAL		   = 0x100, // @emem Read every sample of all streams.
	SKIP_MODE_FAST			   = 0x101, // @emem Only keyframes (KF) of primary stream.
	SKIP_MODE_FAST_NTH		   = 0x102, // @emem Only KFs on N sec intervals.
	SKIP_MODE_REVERSE_FAST	   = 0x201, // @emem Only KFs in reverse direction.
	SKIP_MODE_REVERSE_FAST_NTH = 0x202, // @emem Only KFs in reverse on N sec intervals.
	SKIP_MODE_FORWARD_FLAG     = 0x100, // @emem Flag for testing if a mode is forward.
	SKIP_MODE_REVERSE_FLAG     = 0x200, // @emem Flag for testing if a mode is forward.
};

//////////////////////////////////////////////////////////////////////////////
// @interface IReader |
// Readers implement this interface.
// @base | IPlaybackPipelineComponent
//////////////////////////////////////////////////////////////////////////////
interface IReader						: IPlaybackPipelineComponent
{
	// @cmember
	// The pipeline manager will call this method before a reader will be
	// added to the pipeline; immediately after being added, the reader should
	// synchronously route the pipeline configuration message through the
	// entire pipeline (that has been built up to that point)
	// Returns:	true if the reader can stream from the indicated permanent or
	//			temporary recording, false otherwise
	virtual bool CanReadFile(LPCOLESTR wszFullPathName) = 0;

	// @cmember
	// Get the minimum seek position.  If the current playback transition policy
	// is STRMBUF_STAY_WITH_SINK then the position returned may actually be in a
	// file other than the one specified by IFileSourceFilter::Load depending on
	// the current arrangement of files within the 'Pause Buffer'.
	virtual LONGLONG GetMinSeekPosition() = 0;

	// @cmember Get the maximum seek position.
	virtual LONGLONG GetMaxSeekPosition() = 0;

	// @cmember Get the reader's current seek position.
	virtual LONGLONG GetCurrentPosition() = 0;

	// @cmember Sets the current seek position. <nl>
	// If fSeekToKeyframe is true, then the reader will seek the key frame nearest
	// newPos and the Discontinuity flag on the next sample will be set. <nl>
	// If fSeekToKeyframe is false, then the reader attempts to seeks to the exact
	// media sample specified by newPos and does NOT set the discontinuity flag on
	// the next sample.  If a media sample with that position exact does not exist,
	// then ERROR_SEEK is returned. <nl>
	// Returns ERROR_SUCCESS (0) on success. <nl>
	// Returns ERROR_NEGATIVE_SEEK (131) if newPos is less than the min position. <nl>
	// Returns ERROR_SEEK (25) if newPos is greater than the max position or
	// fSeekToKeyframe is false and the  seek position cannot be found. <nl>
	// Returns -1 for any other error.
	virtual DWORD SetCurrentPosition(LONGLONG newPos, bool fSeekToKeyframe) = 0;

	// @cmember Gets the current frame skip mode and, for every Nth second modes,
	// the seconds.
	virtual FRAME_SKIP_MODE GetFrameSkipMode() = 0;
	virtual DWORD GetFrameSkipModeSeconds() = 0;

	// @cmember Retrieves custom data buffer for currently loaded recording.
	virtual BYTE *GetCustomData() = 0;

	// @cmember Retrieves custom data buffer size for currently loaded recording.
	virtual DWORD GetCustomDataSize() = 0;

	// @cmember
	// Sets the current frame skip mode.  Returns ERROR_SUCCESS (0) on success.
	// Any other return value indicates that the mode is not supported or an
	// error occurred in setting the mode.  If the skip mode is one of the
	// every Nth second flavors, the second argument supplies the N.
	virtual DWORD SetFrameSkipMode(FRAME_SKIP_MODE skipMode, DWORD skipModeSeconds) = 0;

	// @cmember
	// Enables or disables the reader's streaming thread.  It is enabled on
	// true and disabled on false.  This is the way to 'pause' the reader's
	// streaming thread.  The streaming thread is not paused immediately, but
	// will finish delivering the sample it is currently reading and then pause.
	// This function returns the previous enabled state of the streaming thread.
	// While streaming is paused, the seek position and frame skip rate of the
	// reader can still be set.
	virtual bool SetIsStreaming(bool bEnabled) = 0;

	// @cmember Returns the state of the streaming thread.  True if the
	// streaming thread is enabled, false if it is disabled/paused.
	virtual bool GetIsStreaming()	 = 0;

	// @cmember Alerts the Reader to cut back the priority of its threads
	// if in background mode (e.g., a DVR source filter used to burn a DVD
	// or do some other background action needs to take a back seat to DVR
	// playback or other time-critical activity).
	virtual void SetBackgroundPriorityMode(BOOL fUseBackgroundPriority) = 0;
};


//////////////////////////////////////////////////////////////////////////////
//	Pause buffer interfaces													//
//////////////////////////////////////////////////////////////////////////////


// @interface IPauseBufferCallback |
// Interface implemented by component which is interested in receiving
// notifications when the pause buffer is updated.
interface IPauseBufferCallback
{
	// @cmember Called by the pause buffer mgr when the pause buffer is updated.
	// The pause buffer manager hold into a reference to pData prior during the
	// call to this function. If the implementor wants to hold into the pause
	// buffer beyond the scope of the call, the implementor must call AddRef()
	// during the call and Release() when done with the pause buffer. The
	// argument pData can be NULL. The value NULL indicates that the source of
	// previous call buffers is going away so any retained pause buffers
	// from that source must be released immediately.
    virtual void PauseBufferUpdated(CPauseBufferData *pData) = 0;
};

// @interface IPauseBufferMgr |
// Interface implemented by components which need to alert other components
// of changes to the pause buffer.  This is intended to be implemented by the
// writer and sample consumer.
interface IPauseBufferMgr
{
	// @cmember Allows some other component to register for pause buffer
	// notifications.
	virtual void RegisterForNotifications(IPauseBufferCallback *pCallback) = 0;

	// @cmember Allows some other component to no longer receive pause buffer
	// notifications.
	virtual void CancelNotifications(IPauseBufferCallback *pCallback) = 0;

	// @cmember Retrieve the current pause buffer.  Returns NULL on error.
	virtual CPauseBufferData* GetPauseBuffer() = 0;
};

// @interface CPauseBufferMgrImpl |
// Class which provides an almost complete implementation of IPauseBufferMgr.
// Objects which derive from this class must still implement GetPauseBuffer.
// @base public | IPauseBufferMgr
class CPauseBufferMgrImpl : public IPauseBufferMgr
{
public:
	// @cmember Clean up the CPauseBufferMgrImpl
	~CPauseBufferMgrImpl();

	// @cmember Clear out the list of callback functions
	void ClearPauseBufferCallbacks();

	// @cmember Call the PauseBufferUpdated method on each of the registered
	// callback pointers.  This function automatically increments the ref
	// count of pData before making any callback and decrementing it after
	// the last callback completes.
	void FirePauseBufferUpdateEvent(CPauseBufferData *pData);

	// IPauseBufferMgr
	void CancelNotifications(IPauseBufferCallback *pCallback);
	void RegisterForNotifications(IPauseBufferCallback *pCallback);

private:
	std::list<IPauseBufferCallback*> m_lPauseBufferCallbacks;
};

//////////////////////////////////////////////////////////////////////////////
//	Sample Consumers implement this interface.								//
//////////////////////////////////////////////////////////////////////////////

typedef
// @enum
// The enumeration PRODUCER_STATE reports the state of the producer's
// filter graph (i.e., the state of the capture graph).
enum PRODUCER_STATE {
    PRODUCER_IS_RUNNING,	// @emem The capture graph is in State_Running
    PRODUCER_IS_PAUSED,		// @emem The capture graph is in State_Paused
    PRODUCER_IS_STOPPED,	// @emem The capture graph is in State_Stopped
    PRODUCER_IS_DEAD		// @emem The capture graph is being torn down.
} PRODUCER_STATE;

// @interface ISampleConsumer |
// The interface ISampleConsumer must be implemented by sample consumer components. This
// interface defines the APIs needed to support the notify the consumer of the producer's
// activities and to support the indirect hand-shakes between other pairs of sink/source
// components. For example, the API PauseBufferUpdated() supports Reader/Writer sync-up
// on the content of the pause buffer.
interface ISampleConsumer
{
	// @cmember
	// NotifyBound() is called whenever a Sample Producer is bound to the
	// Sample Consumer. This notification allows the Sample Consumer to
	// updates its own state to account for the binding.
    virtual void NotifyBound(ISampleProducer *piSampleProducer) = 0;

	// @cmember
	// SinkStateChanged() is called whenever the filter graph state
	// of the sink (capture) filter changes between running, paused,
	// and/or stopped. It is also called with the Sample Producer is
	// removed from the sink filter's internal pipeline. This
	// notification allows the Sample Consumer to disconnect from
	// the producer (e.g., because the producer is being removed from
	// a pipeline) or recompute whether the Reader or Producer is the
	// preferred source of media samples. <nl> <nl>
	// The sample producer is passed in to minimize the locking
	// requirements associated with unbinding from one sample producer
	// and rebinding to another.
    virtual void SinkStateChanged(ISampleProducer *piSampleProducer,
								  PRODUCER_STATE eProducerState) = 0;

	// @cmember
	// SinkFlushNotify() is called whenever the producer's filter graph
	// starts or ends a flush. This notification lets the Sample Consumer
	// know that it should release any producer media samples it is holding
	// onto and that it should discard any additional media samples it
	// receives while the producer's filter graph is flushing. <nl> <nl>
	// The sample producer is passed in to minimize the locking
	// requirements associated with unbinding from one sample producer
	// and rebinding to another.
	virtual void SinkFlushNotify(ISampleProducer *piSampleProducer,
								 UCHAR bPinIndex, bool fIsFlushing) = 0;

	// @cmember
	// ProcessProducerSample() is called whenever the producer has a
	// media sample for the sample consumer. The arguments consist of
	// a reference to the media sample and the producer's index for
	// the pin. The sample consumer must copy the sample's data into
	// a new sample allocated from its own filter's memory allocator
	// and map the sink's input pin to the analogous source filter's
	// output pin. <nl> <nl>
	// The sample producer is passed in to minimize the locking
	// requirements associated with unbinding from one sample producer
	// and rebinding to another.
    virtual void ProcessProducerSample(
        ISampleProducer *piSampleProducer,
		LONGLONG hyMediaSampleID,
		ULONG uModeVersionCount,
		LONGLONG hyMediaStartPos, LONGLONG hyMediaEndPos,
        UCHAR bPinIndex) = 0;

	// @cmember
	// ProcessProducerTune() is called to indicate where tune complete
	// events happen relative to media samples. The pin is implicitly
	// the primary input pin of the sink. <nl> <nl>
	// The sample producer is passed in to minimize the locking
	// requirements associated with unbinding from one sample producer
	// and rebinding to another.
	virtual void ProcessProducerTune(ISampleProducer *piSampleProducer,
		LONGLONG hyTuneSampleStartPos) = 0;

    // @cmember
	// NotifyProducerCacheDone() is called when the sample producer has
	// finished sending all requested media samples from its cache. This
	// notification allows the Sample Consumer to transition to the reader
	// or adjust the playback speed to no more than 1.0 so that media samples
	// are not consumed faster than they can be generated.
	virtual void NotifyProducerCacheDone(ISampleProducer *piSampleProducer,
				ULONG uModeVersionCount) = 0;

	// @cmember
	// PauseBufferUpdated() is called when the Sample Producer receives
	// notification from the IPauseBufferMgr of the sink filter that the
	// pause buffer has changed. The argument is the modified pause buffer.
	// If the caller wants to hold onto the pause buffer, the caller must
	// call AddRef() on the pause buffer. <nl> <nl>
	// The sample producer is passed in to minimize the locking
	// requirements associated with unbinding from one sample producer
	// and rebinding to another.
    virtual void PauseBufferUpdated(ISampleProducer *piSampleProducer,
		CPauseBufferData *pcPauseBufferData) = 0;

	// @cmember
	// SetPositionFlushComplete() is called by the DecoderDriver once
	// the flush needed for setting the position is done.
	virtual void SetPositionFlushComplete(LONGLONG hyPosition, bool fSeekToKeyFrame) = 0;

	// @cmember
	// NotifyCachePruned() is called by the SampleProducer when it
	// trims down its cache of media samples.
	virtual void NotifyCachePruned(ISampleProducer *piSampleProducer,
		LONGLONG hyFirstSampleIDRetained) = 0;

	// @cmember
	// SeekToTunePosition() is called by the Decoder Driver from its
	// application thread while holding the application lock. This
	// gives the consumer a chance to do a flushing seek on an app
	// thread if needed.
	virtual void SeekToTunePosition(LONGLONG hyChannelStartPos) = 0;

	// @cmember
	// NotifyRecordingWillStart() is called by the SampleProducer
	// when it knows a recording is about to start and it
	// sees a key frame. (The producer knows that the Writer will
	// start the recording with that key frame). If the consumer is
	// bound to a permanent recording that is in progress, it will
	// use the supplied position to ensure it does not play forward
	// into the new recording.
	virtual void NotifyRecordingWillStart(ISampleProducer *piSampleProducer,
		LONGLONG hyRecordingStartPosition,
		LONGLONG hyPriorRecordingEnd,
		const std::wstring pwszCompletedRecordingName) = 0;

	// @cmember
	// GetMostRecentSampleTime() returns the media start time of the most
	// recent sample in the bound producer's buffer.  If not bound to
	// a producer, then -1 is returned.
	virtual LONGLONG GetMostRecentSampleTime() const = 0;

	// Returns the load incarnation number -- with no locking:
	virtual DWORD GetLoadIncarnation() const = 0;

	// @cmember Do whatever needs to be done if the given recording
	// is an orphaned permanent recording in order to ensure that
	// playback ceases to contain the recording in its pause buffer:
	virtual HRESULT ExitRecordingIfOrphan(LPCOLESTR pszRecordingName) = 0;

	// @cmember For use by other components to issue a notification of a
	// glitch event. This operation may require doing a QI on the filter
	// graph manager, which means it may try to graph a filter graph lock:
	virtual void NotifyEvent(long lEvent, long lParam1, long lParam2) = 0;
};

//////////////////////////////////////////////////////////////////////////////
//	Sample Producers implement this interface.								//
//////////////////////////////////////////////////////////////////////////////

typedef
// @enum
// The enumeration SAMPLE_PRODUCER_MODE tells a Sample Producer which
// media samples are of interest to a particular Sample Consumer.
enum SAMPLE_PRODUCER_MODE {
    PRODUCER_SEND_SAMPLES_FORWARD,	// @emem Send the cached samples oldest to newest, then live samples as they arrive.
    PRODUCER_SEND_SAMPLES_BACKWARD,	// @emem Send the cached samples newest to oldest, then don't send any more
    PRODUCER_SEND_SAMPLES_LIVE,		// @emem Send live samples only, i.e., send samples as they arrive.
    PRODUCER_SUPPRESS_SAMPLES		// @emem Don't send any media samples
} SAMPLE_PRODUCER_MODE;

namespace SampleProducer
{
	class CPauseBufferHistory;
}

// Extended events supported by sample producers:

class CEventWithAVPosition : public CExtendedRequest
{
public:
	CEventWithAVPosition(IPipelineComponent *piSampleProducer,
						long lEventId,
						long lEventParam1)
		: CExtendedRequest(piSampleProducer,
			SAMPLE_PRODUCER_SEND_EVENT_WITH_POSITION,
			RETAIN_ON_FLUSH)
		, m_lEventId(lEventId)
		, m_lParam1(lEventParam1)
	{}

	long m_lEventId, m_lParam1;
};

// @interface ISampleProducer |
// The interface ISampleProducer must be implemented by all sample producers.
// The interface defines those APIs that are needed to support the Sample
// Consumer and the helper class that acts as a matchmaker, binding Sample
// Consumers to Sample Producers.
interface  ISampleProducer
{
	// @cmember
	// IsHandlingFile() returns true if and only if the Sample Producer's
	// capture graph is currently making the recording named pszFileName
	// or has as its live TV token pszFileName. This API is used by the
	// matchmaker class.
    virtual bool IsHandlingFile(
        /* in */ LPCOLESTR pszFileName) = 0;

	// @cmember
	// IsLiveTvToken() returns true if and only if the Sample Producer's
	// capture graph is currently making the recording named pszFileName.
	// This API is used by the matchmaker class.
    virtual bool IsLiveTvToken(
        /* in */ LPCOLESTR pszLiveTvToken) const = 0;

	// @cmember
	// BindConsumer() must update the producer's own data structures to
	// record that the Sample Consumer piSampleConsumer is bound to the
	// producer. If the producer succeeds in setting up its own state,
	// it must call ISampleConsumer::NotifyBound(). Note that before
	// binding the consumer, the producer must re-confirm that it is
	// still making the recording-in-progress pszRecordingName. The lack
	// of locking between an initial query to IsHandlingFile() and a
	// subsequent call to BindConsumer() opens a window during which
	// another thread might have told the capture graph to start a new
	// recording.
    virtual HRESULT BindConsumer(
        /* in */ ISampleConsumer *piSampleConsumer,
        /* in */ LPCOLESTR pszRecordingName) = 0;

	// @cmember
	// UnbindConsumer() must update the producer's data structures to
	// clear out all memory of the Sample Consumer piSampleConsumer.
    virtual HRESULT UnbindConsumer(ISampleConsumer *piSampleConsumer) = 0;

	// @cmember
	// UpdateMode() must update the producer's data structures as to
	// which media samples the Sample Consumer wants. If the consumer
	// wants cached samples, those must be sent before any live ones.
    virtual HRESULT UpdateMode(
        SAMPLE_PRODUCER_MODE eSampleProducerMode,
		ULONG uModeVersionCount,
		LONGLONG hyDesiredPosition,
        ISampleConsumer *piSampleConsumer) = 0;

	// @cmember
	// GetPauseBuffer() must return the pause buffer as computed by the
	// sink pipeline's IPauseBufferMgr. This query is initiated by the
	// Sample Consumer when first bound to a producer so that the Sample
	// Consumer can notify its registerered parties. The query will also
	// be made if anyone asks the Sample Consumer for the current (Writer)
	// pause buffer.
    virtual CPauseBufferData* GetPauseBuffer() = 0;

    // @cmember
	// QueryNumInputPins() must return the number of input pins for
	// the producer's sink filter.
    virtual UCHAR QueryNumInputPins() = 0;

    // @cmember
	// QueryInputPinMediaType() must return the media type for the
	// bPinIndex-th input pin of the producer's sink filter. The
	// pin order must be consistent from call to call and with the
	// pin index reported by the producer.
	virtual CMediaType QueryInputPinMediaType(UCHAR bPinIndex) = 0;

    // @cmember
	// GetSinkState() must return the running/paused/stopped/tearing-down
	// state for the producer's sink filter.
	virtual PRODUCER_STATE GetSinkState() = 0;

    // @cmember
	// GetPositionRange() must return the media sample positions
	// for the earliest sample in the producer's cache (which must
	// always be a key frame on the primary pin) and the latest
	// sample received on the primary pin.
	virtual void GetPositionRange(LONGLONG &rhyMinPosition,
								  LONGLONG &rhyMaxPosition) const = 0;

	// @cmember
	// GetSample() maps a pin index and media sample identifier back to
	// a media sample. If the mapping works, the sample is copied to
	// riMediaSample and true is returned. If the mapping fails, riMediaSample
	// is not touched and false is returned.
	virtual bool GetSample(UCHAR bPinIndex, LONGLONG hyMediaSampleID,
		IMediaSample &riMediaSample) = 0;

	virtual LONGLONG GetReaderWriterGapIn100Ns() const = 0;

	// @cmember
	// GetModeForConsumer() is called by the sample consumer to obtain
	// the current producer mode after resuming from a stopped state.
	// The consumer may have lost messages related to the producer
	// as a result of being without its streaming thread.
	virtual void GetModeForConsumer(ISampleConsumer *piSampleConsumer,
		SAMPLE_PRODUCER_MODE &reSampleProducerMode, ULONG &ruModeVersionCount) = 0;

	// @cmember
	// Returns the clock of the graph in which this sample producer exists.
	// The interface has been AddRef'ed before being returned and must
	// be released eventually by the caller.
	virtual HRESULT GetGraphClock(IReferenceClock** ppIReferenceClock) const = 0;

	// @cmember
	// Returns what media time would be stamped into a sample as the
	// media time-stamp if a sample were to arrive right now.
	virtual LONGLONG GetProducerTime() const = 0;

	// @cmember
	// Converts an ISampleProducer to an IPipelineComponent:
	virtual IPipelineComponent *GetPipelineComponent() = 0;

	// @cmember
	// Asks whether a post-flush sample indicates a tune:
	virtual bool IndicatesFlushWasTune(IMediaSample &riMediaSample, CDVRInputPin&	rcInputPin) = 0;

	// @cmember
	// Asks whether a post-new-segment sample indicates a tune:
	virtual bool IndicatesNewSegmentWasTune(IMediaSample &riMediaSample, CDVRInputPin &rcInputPin) = 0;
};

//////////////////////////////////////////////////////////////////////////////
// @interface CSampleProducerLocator |
// This class implements the glue that allows Sample Consumers to bind to
// the correct Sample Producer. Each Sample Producer registers itself via
// a static member function. When a Sample Consumer wants to bind to the
// producer for a particular recording, it calls a different static method
// of CSampleProducerLocator to invoke a matchmaking service.
//////////////////////////////////////////////////////////////////////////////
class CSampleProducerLocator
{
public:
	// @cmember
	// BindToSampleProducer() iterates over the registered sample producers
	// to see if any are handling the recording pszRecordingName. If such
	// a sample producer is found, it is asked to bind to the consumer.
	// The returned HRESULT is S_OK if a binding was established. This
	// method was designed for use by Sample Consumers when
	// IFileSourceFilter::Load() is called.
    static HRESULT BindToSampleProducer(
        /* [in] */ ISampleConsumer *piSampleConsumer,
        /* [in] */ LPCOLESTR pszRecordingName);

	// @cmember
	// Returns the clock of the graph to which the supplied live TV token
	// belongs.
	static HRESULT GetSourceGraphClock(LPCOLESTR pszLiveTvToken, IReferenceClock** ppIReferenceClock);

	// @cmember
	// RegisterSampleProducer() adds the given sample producer to the
	// registry of known sample producers available for binding to
	// consumers. This method was designed for use by Sample Producers
	// when added to a pipeline.
    static HRESULT RegisterSampleProducer(
        ISampleProducer *piSampleProducer);

	// @cmember
	// UnregisterSampleProducer() removes the given sample producer
	// from the registry of sample producers available for binding.
	// This method was designed for use by Sample Producers when
	// removed from a pipeline.
    static void UnregisterSampleProducer(
        const ISampleProducer *piSampleProducer);

	// @cmember
	// IsLiveTvToken() returns true if and only if the given string
	// is in fact the live tv token for some sink filter.
	static bool IsLiveTvToken(LPCOLESTR pszLiveTvToken);
};

//////////////////////////////////////////////////////////////////////////////
//	Decoder Drivers implement this interface.								//
//////////////////////////////////////////////////////////////////////////////
typedef
// @enum
// The enumeration PLAYBACK_END_MODE specifies why the decoder driver
// should stop the playback. The choice of reason affects how the
// decoder driver response, for example, what notifications are sent
// and exactly what it does to the filter graph.
enum PLAYBACK_END_MODE
{
	PLAYBACK_AT_STOP_POSITION,  			// @emem Playback reached the stop position. No special stop behaviors were requested.
	PLAYBACK_AT_STOP_POSITION_NEW_SEGMENT,	// @emem Playback reached the stop position. The AM_SEEKING_NewSegment stop behavior was requested.
	PLAYBACK_AT_BEGINNING,					// @emem Playback reached the beginning of the pause buffer or bound recording.
	PLAYBACK_AT_END							// @emem Playback reached the end of a bound recording or there is nothing in the pause buffer.
} PLAYBACK_END_MODE;

struct CAppThreadAction
{
	virtual ~CAppThreadAction() {}
	virtual void Do() = 0;
};

//////////////////////////////////////////////////////////////////////////////
// @interface IDecoderDriver |
// This interface must be implemented by all Decoder Driver components.
// Most of the APIs are designed to provide Sample Consumers with the support
// they need to carry out their rate and position control responsibilities.
// A couple of APIs are supplied just so that the Decoder Driver can be the
// sole point of responsibility for notifying the playback filter graph of
// other interesting events that are known by the Sample Consumer.
//////////////////////////////////////////////////////////////////////////////

class CDecoderDriverEnactRateChange: public CExtendedRequest
{
public:
	CDecoderDriverEnactRateChange(IPipelineComponent *piDecoderDriver, double dblRate)
		: CExtendedRequest(piDecoderDriver, DECODER_DRIVER_ENACT_RATE, RETAIN_ON_FLUSH)
		, m_dblRate(dblRate)
	{
	}

	double m_dblRate;
};

class CDecoderDriverEndSegment: public CExtendedRequest
{
public:
	CDecoderDriverEndSegment(IPipelineComponent *piDecoderDriver)
		: CExtendedRequest(piDecoderDriver,
					DECODER_DRIVER_END_SEGMENT,
					RETAIN_ON_FLUSH)
		{}
};

class CDecoderDriverSeekComplete: public CExtendedRequest
{
public:
	CDecoderDriverSeekComplete(IPipelineComponent *piDecoderDriver)
		: CExtendedRequest(piDecoderDriver,
					DECODER_DRIVER_SEEK_COMPLETE)
		{}
};

class CDecoderDriverSendNotification: public CExtendedRequest
{
public:
	CDecoderDriverSendNotification(IPipelineComponent *piDecoderDriver,
			long lEventId, long lParam1, long lParam2, bool fDiscardOnFlush)
		: CExtendedRequest(piDecoderDriver,
					DECODER_DRIVER_SEND_NOTIFICATION,
					fDiscardOnFlush ? DISCARD_ON_FLUSH : RETAIN_ON_FLUSH)
		, m_lEventId(lEventId)
		, m_lParam1(lParam1)
		, m_lParam2(lParam2)
		{}

	long m_lEventId, m_lParam1, m_lParam2;
};

class CDecoderDriverNotifyOnPosition: public CExtendedRequest
{
public:
	CDecoderDriverNotifyOnPosition(IPipelineComponent *piDecoderDriver, LONGLONG hyPosition)
		: CExtendedRequest(piDecoderDriver, DECODER_DRIVER_CALLBACK_AT_POSITION, DISCARD_ON_FLUSH)
		, m_hyTargetPosition(hyPosition)
	{
	}

	virtual void OnPosition() = 0;

	LONGLONG m_hyTargetPosition;
};

//////////////////////////////////////////////////////////////////////////////
// @interface IPipelineComponentIdleAction |
// This interface must be implemented by a pipeline component that wants
// the decoder driver to invoke a periodic 'idle' action on its application
// thread.
//////////////////////////////////////////////////////////////////////////////

interface IPipelineComponentIdleAction
{
public:
	// @cmember
	// DoAction() carries out the action
	virtual void DoAction() = 0;
};
typedef IPipelineComponentIdleAction *PIPPipelineComponentIdleAction;

#define DECODER_DRIVER_PLAYBACK_POSITION_UNKNOWN		-1LL
#define DECODER_DRIVER_PLAYBACK_POSITION_END_OF_MEDIA	-2LL
#define DECODER_DRIVER_PLAYBACK_POSITION_START_OF_MEDIA	-3LL

interface IDecoderDriver
{
public:
	// @cmember
	// GetPreroll() normally sets rhyPreroll to the preroll requirement
	// of the media type and returns S_OK. If there is a problem, an
	// appropriate error HRESULT will be returned instead.
	virtual HRESULT GetPreroll(LONGLONG &rhyPreroll) = 0;

	// @cmember
	// IsRateSupported() returns true if the given rate can be rendered
	// and false if not.
	virtual bool IsRateSupported(double dblRate) = 0;

	// @cmember
	// SetNewRate() tells the Decoder Driver that it is necessary to
	// initiate a change in rate.  This will result in a call to
	// ImplementNewRate() once the Sample Consumer has accepted the
	// new rate.
	virtual void SetNewRate(double dblRate) = 0;

	// @cmember
	// ImplementNewRate() tells the Decoder Driver that it is time to
	// start playing back at the given rate. The Decoder Driver is
	// responsible for issuing a STREAMBUFFER_EC_RATE_CHANGE notification.
	// The decoder driver is also responsible for telling the reader
	// of any change to the policy on what frames to discard. If that
	// policy changes, the decoder driver must issue an EC_QUALITY_CHANGE
	// notification. The decoder driver may also need to initiate a flush
	// or inform a downstream filter of the rate change.
	virtual void ImplementNewRate(IPipelineComponent *piPipelineComponentOrigin, double dblRate, BOOL fFlushInProgress) = 0;

	// @cmember
	// ImplementNewPosition() tells the Decoder Driver that the position
	// has jumped to hyPosition and further informs the Decoder Driver
	// whether the Sample Consumer believes that there should be no flush
	// unless the downstream filter graphs simply cannot handle the new
	// position without a flush. The boolean fSkippingTimeHole is true
	// if and only if the Sample Consumer initiated the change of position
	// to skip over a gap between the old position and the next available
	// position for media samples (e.g., after an overnight pause sitting
	// in a permanent recording). If true, the Decoder Driver should issue
	// a STREAMBUFFER_EC_TIMEHOLE notification.
	virtual void ImplementNewPosition(IPipelineComponent *piPipelineComponentOrigin, LONGLONG hyPosition,
		bool fNoFlushRequested, bool fSkippingTimeHole,
		bool fSeekToKeyFrame, bool fOnAppThread) = 0;

	// @cmember
	// ImplementEndPlayback() tells the Decoder Driver to do whatever is
	// necessary to stop sending media samples downstream, due to the
	// reason given.
	virtual void ImplementEndPlayback(PLAYBACK_END_MODE ePlaybackEndMode, IPipelineComponent *piRequester) = 0;

	// @cmember
	// ImplementTuneEnd() tells the Decoder Driver that a tune has just
	// completed (relative to the media samples previously sent). The
	// argument fTuneIsLive is true if and only if the tune notification
	// is synchronous with a tune in the capture graph. Typically the
	// Decoder Driver will flush the capture graph to speed up the tune
	// when the tune is live.  TBD: should we define a notification?
	virtual void ImplementTuneEnd(bool fTuneIsLive, LONGLONG hyChannelStartPos,
		bool fCalledFromAppThread) = 0;

	// @cmember
	// ImplementBeginFlush() tells the Decoder Driver to flush either the
	// whole filter graph. ImplementEndFlush() tells the decoder driver to
	// end the flush. These methods may only be called on an application
	// thread.
	virtual bool ImplementBeginFlush() = 0;
	virtual void ImplementEndFlush() = 0;

	// @cmember
	// ImplementRun() tells the Decoder Driver that the playback graph
	// must be in either State_Stopped or State_Running. State_Paused
	// is not legal because the current position is sitting too close
	// to the about-to-be-truncated beginning of a temporary recording.
	virtual void ImplementRun(bool fOnAppThread) = 0;

	// @cmember
	// ImplementGraphConfused() tells the Decoder Driver that an error
	// has happened that makes further playback problematic in the sense
	// that the playback graph may be playing the wrong thing or it can
	// no longer tell whether it is correctly tracking the limits of the
	// pause buffer. The Decoder Driver should do what it can to put the
	// capture graph into a safe state and notify the application.  TBD:
	// figure out the details of this.
	virtual void ImplementGraphConfused(HRESULT hr) = 0;

	// @cmember
	// ImplementDisabledInputs() tells the Decoder Driver that the
	// Sample Consumer has disabled all sources of media samples. The
	// decoder driver must be prepared to adjust time-stamps when it
	// finally does receive another sample to compensate for the gap.
	virtual void ImplementDisabledInputs(bool fSampleComingFirst) = 0;

	// @cmember
	// ImplementLoad() tells the Decoder Driver that a Load() call is
	// underway -- far enough so that the old content should be
	// assumed to be done.
	virtual void ImplementLoad() = 0;

	// @cmember
	// IsAtEOS() returns TRUE if the Decoder Driver has passed an
	// end-of-stream downstream (and that end-of-stream has not been
	// cleared by stopping the filter graph).
	virtual bool IsAtEOS() = 0;

	// @cmember
	// SetEndOfStreamPending() is called when the Sample Consumer chooses to issue
	// or pass through an end-of-stream notification.
	virtual void SetEndOfStreamPending() = 0;

	// @cmember
	// The Sample Consumer's SetRate() implementation needs to know if it
	// should flush the filter graph and seek to a position about where
	// what the user is viewing when switching rates.  IsSeekRecommendedForRate()
	// returns S_FALSE if no seek is recommended. It returns S_OK and
	// sets its argument to the recommended position if a seek is recommended.
	virtual HRESULT IsSeekRecommendedForRate(double dblRate, LONGLONG &hyRecommendedPosition) = 0;

	// @cmember
	// The Sample Consumer sometimes needs to issue events that don't require any decoder driver
	// smarts. The events should, however, be coordinated with sample deliveries. This API
	// tells the decoder driver to deliver the event, with queueing policy info:
	virtual void IssueNotification(IPipelineComponent *piPipelineComponent,
		long lEventID, long lParam1 = 0, long lParam2 = 0,
		bool fDeliverNow = false, bool fDeliverOnlyIfStreaming = false) = 0;

	// @cmember
	// DeferToAppThread() queues up a request to carry out some action on
	// the decoder driver's application thread:
	virtual void DeferToAppThread(CAppThreadAction *pcAppThreadAction) = 0;

	// @cmember
	// GetPipelineComponent() returns the IDecoderDriver as an IPipelineComponent:
	virtual IPipelineComponent *GetPipelineComponent() = 0;

	// @cmember
	// ImplementThrottling() is called to enable or disable throttling of
	// samples while the filter graph is in state running.
	virtual void ImplementThrottling(bool fThrottle) = 0;

	// @cmember
	// EstimatePlaybackPosition() returns the estimated playback position
	// (extrapolated from the current policy for converting A/V positions
	// to stream times):
	//
	// Special values:
	//		-1   ... playback position not known (DECODER_DRIVER_PLAYBACK_POSITION_UNKNOWN)
	//      -2   ... playback position = end of media (DECODER_DRIVER_PLAYBACK_POSITION_END_OF_MEDIA)
	//	    -3   ... playback position = beginning of media (DECODER_DRIVER_PLAYBACK_POSITION_START_OF_MEDIA)
	virtual LONGLONG EstimatePlaybackPosition(bool fExemptEndOfStream = false, bool fAdviseIfEndOfMedia = false) = 0;

	// @cmember
	// RegisterIdleAction() registers an action to be executed periodically
	// on the decoder driver's application thread. The ping will happen
	// at least every half second.
	virtual void RegisterIdleAction(IPipelineComponentIdleAction *piPipelineComponentIdleAction) = 0;

	// @cmember
	// UnregisterIdleAction() unregisters an action to be executed periodically.
	virtual void UnregisterIdleAction(IPipelineComponentIdleAction *piPipelineComponentIdleAction) = 0;

	// @cmember
	// IsFlushNeededForRateChange() returns TRUE if a flush is always required to change the
	// rate. It returns FALSE otherwise.
	virtual BOOL IsFlushNeededForRateChange() = 0;

	// @cmember Alerts the Reader to cut back the priority of its threads
	// if in background mode (e.g., DVD burning takes a back seat to DVR
	// playback).
	virtual void SetBackgroundPriorityMode(BOOL fUseBackgroundPriority) = 0;

};

// The following helper class invokes IDecoderDriver::ImplementBeginFlush()
// and, if that works, ImplementEndFlush(). Use this to ensure that an
// exception doesn't cause ImplementEndFlush() to never be called.

class CDecoderDriverFlushRequest
{
public:
	CDecoderDriverFlushRequest(IDecoderDriver *piDecoderDriver)
		: m_piDecoderDriver(piDecoderDriver)
		, m_fFlushInProgress(false)
		{
		}

	~CDecoderDriverFlushRequest()
	{
		EndFlush();
	}

	bool BeginFlush()
		{
			if (m_piDecoderDriver && !m_fFlushInProgress)
			{
				m_fFlushInProgress = m_piDecoderDriver->ImplementBeginFlush();
				return m_fFlushInProgress;
			}
			return false;
		}

	void EndFlush()
	{
		try {
			if (m_fFlushInProgress)
			{
				m_fFlushInProgress = false;
				m_piDecoderDriver->ImplementEndFlush();
			}
		}
		catch (const std::exception &)
		{
		}
	}

private:
	IDecoderDriver *m_piDecoderDriver;
	bool m_fFlushInProgress;
};


#include "Plumbing\\PipelineInterfaces_Inc.h"

}
