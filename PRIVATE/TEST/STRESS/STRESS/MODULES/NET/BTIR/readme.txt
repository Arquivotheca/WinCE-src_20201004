btir is a new stress module written for both Bluetooth and IR. It is intended to be used with actual Bluetooth or IR 
hardware. It has a client and a server. If a server Bluetooth address is supplied as part of client's command line, 
Bluetooth transport is enabled. If client detects IR device nearby it will enabled IR transport. Server will try to enable
both Bluetooth and IR unless it fails to do so.