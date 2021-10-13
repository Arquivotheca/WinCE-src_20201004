Notifications API test.

    notify
	|____nfyApp
	|____nfyBrdth
	|____nfyBVT
	|____nfyDepth
	|____nfyLib
	|____nfyUI
	|____Ack
	|____nfyOld

nfyBVT:
	Contains BVTs. Single call, smoke test.

nfyBrdth:
	Contains parameter tests. Tests a broad range of parameters

nfyDepth:
	Contains functionality tests. Tests each and every function and feature.

nfyApp:
	Application launched by various notification tests. This opens a memory mapped file for communication with test programs.

nfyLib:
	Contains functions for performing systemtime arithmetic and classes that encapsulate memory mapped file operations. Communication between the tests and the notification app (nfyapp) occurs through memory mapped files.

nfyUI:
	To replace default Notify UI component and test its interface with the notify sub-system. Code was grabbed from dev, this will be tweaked around by me to find bugs.

Ack:
	Acknowledge application launched by nfyOld.

nfyOld:
	Rest of the tests from NotifT (Pett based). These are not yet implemented in the new Tux based suite.
