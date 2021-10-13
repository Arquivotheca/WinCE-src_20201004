
Simple Tech Detector

This simple sample detector is intended to be a quick starting point for creating peripheral 
detectors for the Seven Test Kit.  The code is stripped down and quick, easy to understand.  Follow
the below instructions and you should be done in no time flat.


Steps:
1. copy files from private\test\baseos\drivers\techdetector to a folder local to your team.
2. choose a target binary file name like XXX_Detector.dll where XXX is the name of your tech.
	A. update TARGETNAME in sources file
	B. update __BIN_NAME__ in detector.cpp to match XXX_Detector.dll
        C. update g_lpFTE in detector.cpp to match the XXX name of your detector	
	D. rename tech_detector.def to be XXX_Detector.dll
	E. update contents.oak to contain your def file
3. Add detection code to IsTechPresent function in detector.cpp 
	Whatever must be done to detect the driver needs to go here.  Potentially you will need to look
	for a driver file, or look in the registry for a filename, or call an API and see if it
	returns expected information.
	return TRUE if you found your technology
	return FALSE in all other cases
4. compile
5. test and see if the detector works in cases where you tech is present and not present.

You're done.  It was that simple.