#ifndef __PRESETUTY_H__
#define __PRESETUTY_H__

namespace PresetUty
{
    inline void PresetStruct(BYTE* pb, int nBytes)
    {
        for (BYTE key = 0xAE; nBytes; nBytes--, pb++, key++)
            *pb = key;
    }

    inline bool IsStructPreset(const BYTE* pb, int nBytes)
    {        
        for (BYTE key = 0xAE; nBytes; pb++, key++)
            if (*pb != key)
                return false;
        return true;
    }

    template <class T>
    inline void PresetStruct(T& v)
    {
        PresetStruct(reinterpret_cast<BYTE*>(&v), sizeof(T));
    }

    template <class T>
    inline bool IsStructPreset(const T& v)
    {        
        return IsStructPreset(reinterpret_cast<const BYTE*>(&v), sizeof(T));
    }

    template <class T>
    inline void Preset(T& v)
    {
        v = reinterpret_cast<T>(reinterpret_cast<void*>(0xca510dad));
    }

    template <class T>
    inline bool IsPreset(const T& v)
    {
        return v==reinterpret_cast<T>(reinterpret_cast<void*>(0xca510dad));
    }

    template <>
    inline void Preset(double& v)
    {
        v = 12345.6789;
    }

    template <>
    inline bool IsPreset(const double& v)
    {
        return v == 12345.6789;
    }

    template <>
    inline void Preset(float& v)
    {
        v = 12345.6789f;
    }

    template <>
    inline bool IsPreset(const float& v)
    {
        return v == 12345.6789f;
    }

#   define DECLARE_PRESET_FUNCTIONS(__STRUCTNAME) \
    inline void Preset(__STRUCTNAME& v) \
    { \
        PresetStruct(v); \
    } \
    \
    inline bool IsPreset(const __STRUCTNAME& v) \
    { \
        return IsStructPreset(v); \
    }

}


#endif
