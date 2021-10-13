#pragma once
#ifndef __DDRAWUTY_TYPES_H__
#define __DDRAWUTY_TYPES_H__

#include <QATestUty/PresetUty.h>
#include <QATestUty/Sized_struct.h>

namespace DDrawUty
{
    typedef sized_struct<DDBLTFX> CDDBltFX;
    typedef sized_struct<DDPIXELFORMAT> CDDPixelFormat;
    typedef sized_struct<DDCOLORCONTROL> CDDColorControl;
    typedef sized_struct<DDSURFACEDESC> CDDSurfaceDesc;
    typedef sized_struct<DDOVERLAYFX> CDDOverlayFX;
    typedef sized_struct<DDCAPS> CDDCaps;
    
    typedef sized_struct<DDVIDEOPORTINFO> CDDVideoPortInfo;
    typedef sized_struct<DDVIDEOPORTCAPS> CDDVideoPortCaps;
    typedef sized_struct<DDVIDEOPORTCONNECT> CDDVideoPortConnect;
    typedef sized_struct<DDVIDEOPORTBANDWIDTH> CDDVideoPortBandwidth;
    
    class CDDVideoPortDesc : public sized_struct<DDVIDEOPORTDESC> 
    {
    public:
        inline void Clear()
        {
            memset(this, 0, sizeof(DDVIDEOPORTDESC));
            dwSize=sizeof(DDVIDEOPORTDESC);
            VideoPortType.dwSize=sizeof(VideoPortType);
        }
        inline void Preset() 
        { 
            PresetUty::PresetStruct(*this); 
            dwSize=sizeof(DDVIDEOPORTDESC); 
            VideoPortType.dwSize=sizeof(VideoPortType);
        }
    };
    
    class CDDVideoPortStatus : public sized_struct<DDVIDEOPORTSTATUS> 
    {
    public:
        inline void Clear()
        {
            memset(this, 0, sizeof(DDVIDEOPORTSTATUS));
            dwSize=sizeof(DDVIDEOPORTSTATUS);
            VideoPortType.dwSize=sizeof(VideoPortType);
        }
        inline void Preset() 
        { 
            PresetUty::PresetStruct(*this); 
            dwSize=sizeof(DDVIDEOPORTSTATUS); 
            VideoPortType.dwSize=sizeof(VideoPortType);
        }
    };
}

namespace PrestUty
{
#   define DECLARE_PRESET(CLASS) \
    inline void Preset(CLASS& v) { v.Preset(); } \
    inline bool IsPreset(CLASS& v) { return v.IsPreset(); }

    DECLARE_PRESET(DDrawUty::CDDBltFX);
    DECLARE_PRESET(DDrawUty::CDDPixelFormat);
    DECLARE_PRESET(DDrawUty::CDDColorControl);
    DECLARE_PRESET(DDrawUty::CDDSurfaceDesc);
    DECLARE_PRESET(DDrawUty::CDDOverlayFX);
    DECLARE_PRESET(DDrawUty::CDDCaps);
    DECLARE_PRESET(DDrawUty::CDDVideoPortInfo);
    DECLARE_PRESET(DDrawUty::CDDVideoPortConnect);
    DECLARE_PRESET(DDrawUty::CDDVideoPortBandwidth);
}

/*class DIRECTDRAW1TYPES
{
public:
    typedef ::IDirectDraw IDirectDraw;
    typedef ::IDirectDrawSurface IDirectDrawSurface;
    typedef ::DDSURFACEDESC DDSURFACEDESC;
};

class DIRECTDRAW2TYPES
{
public:
    typedef ::IDirectDraw2 IDirectDraw;
    typedef ::IDirectDrawSurface2 IDirectDrawSurface;
    typedef ::DDSURFACEDESC DDSURFACEDESC;
};*/

#endif // #ifndef __DDRAWUTY_TYPES_H__

