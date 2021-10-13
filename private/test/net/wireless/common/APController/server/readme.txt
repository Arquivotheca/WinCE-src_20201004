Cisco 1200 Series Access Point Setup

To set up a Cisco 1200 Series Access Point, perform the following steps:

1. Turn off password encryption.  This can be done via telnet or serial console.
   Once logged in, execute the following commands:
     
   enable 
   <enter password>
   configure terminal
   no service password-encryption
   <ctrl+z>
   exit

2. Add a RADIUS server.  This can be done in the web interface by clicking the 
   SECURITY main menu, then the Server Manager sub menu, or by typing in the 
   URL: /ap_sec_network-security_a.shtml.

   On this page, select RAIDUS from Corporate Servers - Current Server List.
   Verify there is one server listed in the list box.  If there isn't, enter
   0.0.0.0 in the Server text box, 'abc' in the Shared Secret text box, and
   0 in the Authentication Port text box, then click the Apply button.

3. Add one SSID for each radio interface your AP supports.  This can be done in
   the web interface by clicking the SECURITY main menu, then the SSID Manager
   sub menu, or by typing in the URL: /ap_sec_ap-client-security.shtml.

   Verify there is one SSID listed in the Current SSID List list box for each radio
   interface your AP supports.  Select each SSID in turn, and verify that each is
   bound to exactly one unique radio interface, as indicated by the Interface check
   box to the right.

   To add an SSID, select < NEW > in the Current SSID List, enter the SSID name
   in the SSID: text box, check exactly one unique Interface check box, then 
   scroll down the page and click the first Apply button you see on the right.

