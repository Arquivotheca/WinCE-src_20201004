To run this test:

1. Modify platform.bib file in release directory to include Bootstress.exe:

      IF IMGBOOTTEST                                                                    
          bootStress.exe    $(_FLATRELEASEDIR)\bootStress.exe         NK SH
      ENDIF IMGBOOTTEST          


    
2. Modify platform.reg file in release directory to run Bootstress.exe at
   startup:

      IF IMGBOOTTEST
          [HKEY_LOCAL_MACHINE\init]                                                                     
          "Launch200"="bootStress.exe"                                                            
      ENDIF IMGBOOTTEST                         

    

3. Set IMGBOOTTEST=1



4. Issue the “makeimg” command from a build window with Bootstress.exe present
   in the release directory.



5. After the new run-time image is built and downloaded to the device, issue
   the following command in the Target Control window:

       s tux -o -d downloadstress.dll -c "-t 10" 
            // "-t" specifies number of reboots (10 in this example).
            //      Default number of iterations = 100

                                                     

6. The test run creates and reads the following files in the release directory:
     a) BootStress.txt : Contains the number of times the test is to run, as
                         well as the reset-to-bootup times (in milliseconds) 
                         of each run.
     b) BootStressLastTick.txt : Contains the system time of when the system
                                 last rebooted. This is used to calculate the
                                 reset-to-bootup time.
     c) BootStressTotalRan.txt : Number of times the device have been reset.

   Note: If you wish to abort the test, delete the BootStress*.txt files to stop 
         your device from resetting on boot.


         
7.  When the test completes after the specified number of reboots, expect to
    find a test summary in the debug output with the following header:
      ********************************
      Reboot test Complete:
      [... data appears here ...]
      ********************************
