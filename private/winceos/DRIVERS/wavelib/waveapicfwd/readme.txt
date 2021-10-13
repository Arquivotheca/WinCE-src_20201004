This directory contains stub function for each of the APIs forwarded by coredll.def into waveapic.
These stub functions are needed during the link phase as a hint to the linker that the functions use "C"
decoration rather than "C++". Without this hint, the linker will fail when it tries to sysgen coredll.

The linker doesn't care about the parameters or return values, so the minimal declaration for each function is:
    extern "C" void FunctionName(){}

