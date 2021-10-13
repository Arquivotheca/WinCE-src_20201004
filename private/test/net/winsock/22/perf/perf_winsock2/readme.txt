Test
----
perf_winsock2

Goal
----

This test measures the send and receive throughput for TCP and UDP and the
response time for a ping request/reply transaction over TCP and UDP. 

Test Hardware Setup
-------------------

o CE device with some form of network connection hardware, i.e. network card,
  serial cable, modem etc.
o Peer machine (preferably a desktop machine, ex. Windows 2000, Windows XP,
  etc.) with some form of network connection hardware.
o Network connecting the CE device with the peer machine; preferably private,
  quiet network.

Test Software Setup
-------------------

o CE device - boot it up, establish network connection such that the peer
  machine is reachable via a ping command, make sure you have the test DLL
  called perf_winsock2.dll (and its dependencies like tux.exe) in the release
  directory.
o Peer machine - boot it up, establish network connection such that the CE
  device is reachable via a ping command. Start perf_winsockd2 server from
  command prompt window:

  On NT:

      >perf_winsockd2 -install      - To install and start perf_winsockd2
                                      service.
      >perf_winsockd2 -remove       - To stop and uninstall the serivce
                                      (Note: you can control this service
                                      using the services application on NT).
      >perf_winsockd2 -debug        - To run the service in console/debugging
                                      mode. (This will cause debugging info
                                      to appear in the cmd window from which
                                      the program was started the quantity
                                      of debugging info is limited and
                                      should not affect network throughput
                                      performance numbers).

   On CE:

      >perf_winsockd2               - Will start the program on CE in
                                      debugging mode.

Test Execution
--------------

o By default each test is run once for a specified list of buffer sizes -
  currently this list includes the following buffer/packet sizes:

      TCP: 16, 32, 64, 128, 256, 512, 1024, 1460, 2048, 2920, 4096, 8192
      UDP: 16, 32, 64, 128, 256, 512, 1024, 1460

  The list of buffers/packets can be specified by the -b parameter (see below).
  The default number of buffers/packets send/received per test iteration is

      Throughput tests: 10000
      Ping tests:         100

  Both values can be adjusted by -b and -g parameters, respectively (see
  below).

  Supported tests are:

      1001: TCP Send Throughput
      1002: TCP Recv Throughput
      1003: TCP Ping
      1004: TCP Send Throughput Nagle Off
      1005: TCP Recv Throughput Nagle Off
      1006: TCP Ping Nagle Off
      1007: UDP Send Throughput
      1008: UDP Send Packet Loss
      1009: UDP Recv Throughput
      1010: UDP Recv Packet Loss
      1011: UDP Ping

  The usage and meaning of command line is:

      Usage: tux -o -d PERF_WINSOCK2 [-x <test_id>] -c"[-s <server>]
                                  [-i <ip_version>]
                                  [-b <buffer_size_list>] [-n <num-buffers>]
                                  [-g <num-ping-buffers>]"

      Parameters:
          -s <server>           : Specifies the server name or IP address
                                  (default: localhost)
          -i <ip_version>       : IP version to use; must be 4 or 6
                                  (default: 4)
      Advanced parameters:
          -b <buffer_size_list> : Allows listing of comma-delimited buffer
                                  sizes to iterate through, where
                                  <buffer_size_list> =
                                      <buffer_size_1>,<buffer_size_2>,...,
                                      <buffer_size_N>
          -n <num-buffers>      : Number of buffers to send/receive
                                  (default: 10000)
          -p <num-ping-buffers> : Number of buffers to send/receive in
                                  ping-type tests (default 100)
          -r <send_rate>        : Rate at which to send data in Kbits/s.
                                  Applies only for UDP tests (default: max)
          -g <gw_option>        : Gateway test option; enables send
                                  throttling on client-part
                                  (default: 0 (Disabled))

  Note: Option -b specifies buffer sizes for both UDP and TCP tests.

Test Execution Tips
-------------------

o Run the test at least 3 times (with the same parameters) to get a good
  average measurement.
