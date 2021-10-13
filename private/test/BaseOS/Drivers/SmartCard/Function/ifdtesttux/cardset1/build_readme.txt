Building from this directory will create the deprecated 
PC/SC Workgroup IFDTest1, which will require use of PC/SC Test Cards v.1.0.

For this test to work in Kernel-mode in Windows Embedded CE 6.0 R2, 
which is required with legacy drivers such as stcusb.dll, you also need 
to make the following change before building.  This is so that .rc files
can properly be accessed when running tux in kernel mode (-n switch):

In ..\ifdtest.cpp, in the TestDispatch method, replace:
------8<-------
g_hInst = g_pShellInfo->hLib;
------>8-------

with:
------8<-------
g_hInst = GetModuleHandle(L"ifdtesttux");
------>8-------
