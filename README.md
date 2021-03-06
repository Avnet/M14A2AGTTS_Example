These files will allow you to build a simple HTTP/HTTPS application that will exchange data with httpbin.org and ensure the WNC14A2A driver is working correctly.

# Note
  -The following firmware versions running on the WNC14A2A have been tested against: 

    MPSS: M14A2A_V11.50.164451 APSS: M14A2A_V11.53.164451

    MPSS: IMA3_V10.58.174043   APSS: IMA3_V10.52.174043

# Required tools
1. mbed-cli (version 1.3.0 or later)
2. latest version of GNU ARM Embedded Toolchain: **https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads**

# Create Project
1. Import the M14A2AGTSS_Example project: **mbed import http://github.com/Avnet/M14A2AGTTS_Example**

2. Goto the M14A2AGTSS_Example folder, Then edit mbed_settings.py and add the path to your compiler using GCC_ARM_PATH

# Build Application
1.  Build the program by executing **'mbed compile -m K64F -t GCC_ARM'**

2.  When building the tests, it is possible to enable varying amounts of debug information to be output by the 
    Network driver.  These settings are located in the 'mbed_app.json' file (the wnc_config.json file is used when
    building for Green Tea).  The initial setting is to enable or disable WNC Debug output by setting 'wnc_debug' 
    to 'true' or by setting it to 0 to disable.  The amount of debug information output is controlled by the 
    ‘wnc_debug_setting’.  The values for this flag enabled levels from basic socket level debug, to low-level driver 
    interactions. The current default settings are wnc_debug = false, and wnd_debug_setting=4 (basic socket level debug).

3. Verify operation of the basic application program by executing it on the target hardware.  Verify the 
   program runs correctly by opening a minicom window (115200-N81) and observing the program 
   output.  This program flow should resemble:

> 
        Test HTTP and HTTPS interface                                                                       
        [EasyConnect] Using WNC14A2A                                                                        
        [EasyConnect] Connected to Network successfully                                                     
        [EasyConnect] MAC address 11:02:72:14:96:23                                                         
        [EasyConnect] IP address 10.192.220.207                                                             
         software.                                                                                          
        My IP Address is: 10.192.220.207                                                                    
                                                                                                            
        >>>>>>>>>>>><<<<<<<<<<<<                                                                            
        >>>  TEST HTTPClient <<<                                                                            
        >>>>>>>>>>>><<<<<<<<<<<<                                                                            
        
        Connected over TCP to developer.mbed.org
        
        >>>First, lets get a page from http://developer.mbed.org
        
        ----- RESPONSE: -----
        Status: 301 - Moved Permanently
        Headers:
                Server: nginx/1.11.10
                Date: Thu, 28 Dec 2017 22:21:36 GMT
                Content-Type: text/html
                Content-Length: 186
                Connection: keep-alive
                Location: https://os.mbed.com/media/uploads/mbed_official/hello.txt
        
        Body (186 bytes):
        
        <html>
        <head><title>301 Moved Permanently</title></head>
        <body bgcolor="white">
        <center><h1>301 Moved Permanently</h1></center>
        <hr><center>nginx/1.11.10</center>
        </body>
        </html>
        
        Connected over TCP to httpbin.org
        
        
        
        >>>Post data... **
        
        ----- RESPONSE: -----
        Status: 200 - OK
        Headers:
                Connection: keep-alive
                Server: meinheld/0.6.1
                Date: Thu, 28 Dec 2017 22:21:36 GMT
                Content-Type: application/json
                Access-Control-Allow-Origin: *
                Access-Control-Allow-Credentials: true
                X-Powered-By: Flask
                X-Processed-Time: 0.00166702270508
                Content-Length: 335
                Via: 1.1 vegur
        
        Body (335 bytes):
        
        {
          "args": {}, 
          "data": "{\"hello\":\"world\"},{\"test\":\"1234\"}", 
          "files": {}, 
         "form": {}, 
         "headers": {
           "Connection": "close", 
           "Content-Length": "33", 
           "Content-Type": "application/json", 
           "Host": "httpbin.org"
         }, 
         "json": null, 
         "origin": "205.197.242.103", 
         "url": "http://httpbin.org/post"
       }
       
       
       
       >>>Put data... 
       
       ----- RESPONSE: -----
       Status: 200 - OK
       Headers:
               Connection: keep-alive
               Server: meinheld/0.6.1
               Date: Thu, 28 Dec 2017 22:21:38 GMT
               Content-Type: application/json
               Access-Control-Allow-Origin: *
               Access-Control-Allow-Credentials: true
               X-Powered-By: Flask
               X-Processed-Time: 0.000751972198486
               Content-Length: 312
               Via: 1.1 vegur
       
       Body (312 bytes):
       
       {
         "args": {}, 
         "data": "This is a PUT test!", 
         "files": {}, 
         "form": {}, 
         "headers": {
           "Connection": "close", 
           "Content-Length": "19", 
           "Content-Type": "application/json", 
           "Host": "httpbin.org"
         }, 
         "json": null, 
         "origin": "205.197.242.103", 
         "url": "http://httpbin.org/put"
       }
       
       
       
       >>>Delete data... 
       
       ----- RESPONSE: -----
       Status: 200 - OK
       Headers:
               Connection: keep-alive
               Server: meinheld/0.6.1
               Date: Thu, 28 Dec 2017 22:21:38 GMT
               Content-Type: application/json
               Access-Control-Allow-Origin: *
               Access-Control-Allow-Credentials: true
               X-Powered-By: Flask
               X-Processed-Time: 0.000715970993042
               Content-Length: 295
               Via: 1.1 vegur
       
       Body (295 bytes):
       
       {
         "args": {}, 
         "data": "", 
         "files": {}, 
         "form": {}, 
         "headers": {
           "Connection": "close", 
           "Content-Length": "0", 
           "Content-Type": "application/json", 
           "Host": "httpbin.org"
         }, 
         "json": null, 
         "origin": "205.197.242.103", 
         "url": "http://httpbin.org/delete"
       }
       
       
       
       >>>HTTP:stream, send http://httpbin.org/stream/10... 
       Chunk Received:
       {"url": "http://httpbin.org/stream/10", "args": {}, "origin": "205.197.242.103", "id": 0, "headers": {"Connection": "close", "Host"}
       Chunk Received:
       {"url": "http://httpbin.org/stream/10", "args": {}, "origin": "205.197.242.103", "id": 1, "headers": {"Connection": "close", "Host"}
       Chunk Received:
       {"url": "http://httpbin.org/stream/10", "args": {}, "origin": "205.197.242.103", "id": 2, "headers": {"Connection": "close", "Host"}
       Chunk Received:
       {"url": "http://httpbin.org/stream/10", "args": {}, "origin": "205.197.242.103", "id": 3, "headers": {"Connection": "close", "Host"}
       Chunk Received:
       {"url": "http://httpbin.org/stream/10", "args": {}, "origin": "205.197.242.103", "id": 4, "headers": {"Connection": "close", "Host"}
       Chunk Received:
       {"url": "http://httpbin.org/stream/10", "args": {}, "origin": "205.197.242.103", "id": 5, "headers": {"Connection": "close", "Host"}
       Chunk Received:
       {"url": "http://httpbin.org/stream/10", "args": {}, "origin": "205.197.242.103", "id": 6, "headers": {"Connection": "close", "Host"}
       Chunk Received:
       {"url": "http://httpbin.org/stream/10", "args": {}, "origin": "205.197.242.103", "id": 7, "headers": {"Connection": "close", "Host"}
       Chunk Received:
       {"url": "http://httpbin.org/stream/10", "args": {}, "origin": "205.197.242.103", "id": 8, "headers": {"Connection": "close", "Host"}
       Chunk Received:
       {"url": "http://httpbin.org/stream/10", "args": {}, "origin": "205.197.242.103", "id": 9, "headers": {"Connection": "close", "Host"}
       
       
       
       >>>HTTP:Status...
       
       ----- RESPONSE: -----
       Status: 200 - OK
       Headers:
               Connection: keep-alive
               Server: meinheld/0.6.1
               Date: Thu, 28 Dec 2017 22:21:41 GMT
               Content-Type: application/json
               Access-Control-Allow-Origin: *
               Access-Control-Allow-Credentials: true
               X-Powered-By: Flask
               X-Processed-Time: 0.000663042068481
               Content-Length: 485
               Via: 1.1 vegur
       
       Body (485 bytes):
       
       {
         "args": {
           "show_env": "1"
         }, 
         "headers": {
           "Connect-Time": "1", 
           "Connection": "close", 
           "Host": "httpbin.org", 
           "Total-Route-Time": "0", 
           "Via": "1.1 vegur", 
           "X-Forwarded-For": "205.197.242.103", 
           "X-Forwarded-Port": "80", 
           "X-Forwarded-Proto": "http", 
           "X-Request-Id": "d88c0a3d-beb4-456c-a782-82939d7a9d20", 
           "X-Request-Start": "1514499701436"
         }, 
         "origin": "205.197.242.103", 
         "url": "http://httpbin.org/get?show_env=1"
       }
       
       >>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<
       >>>  TEST HTTPS - set up TLS connection  <<<
       >>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<
       
       Connecting to httpbin.org:443
       Starting the TLS handshake...
       TLS connection to httpbin.org:443 established
       Server certificate:
           cert. version     : 3
           serial number     : 03:CF:2E:EB:06:44:87:54:6A:E3:E5:88:DC:77:2B:A0:4D:1C
           issuer name       : C=US, O=Let's Encrypt, CN=Let's Encrypt Authority X3
           subject name      : CN=httpbin.org
           issued  on        : 2017-11-12 23:32:05
           expires on        : 2018-02-10 23:32:05
           signed using      : RSA with SHA-256
           RSA key size      : 2048 bits
           basic constraints : CA=false
           subject alt name  : httpbin.org, www.httpbin.org
           key usage         : Digital Signature, Key Encipherment
           ext key usage     : TLS Web Server Authentication, TLS Web Client Authentication
       Certificate verification passed
       
       
       
       >>>Post data... **
       
       ----- RESPONSE: -----
       Status: 200 - OK
       Headers:
               Connection: keep-alive
               Server: meinheld/0.6.1
               Date: Thu, 28 Dec 2017 22:21:46 GMT
               Content-Type: application/json
               Access-Control-Allow-Origin: *
               Access-Control-Allow-Credentials: true
               X-Powered-By: Flask
               X-Processed-Time: 0.0014750957489
               Content-Length: 336
               Via: 1.1 vegur
       
       Body (336 bytes):
       
       {
         "args": {}, 
         "data": "{\"hello\":\"world\"},{\"test\":\"1234\"}", 
         "files": {}, 
         "form": {}, 
         "headers": {
           "Connection": "close", 
           "Content-Length": "33", 
           "Content-Type": "application/json", 
           "Host": "httpbin.org"
         }, 
         "json": null, 
         "origin": "205.197.242.103", 
         "url": "https://httpbin.org/post"
       }
       
       
       
       >>>Put data... 
       
       ----- RESPONSE: -----
       Status: 200 - OK
       Headers:
               Connection: keep-alive
               Server: meinheld/0.6.1
               Date: Thu, 28 Dec 2017 22:21:46 GMT
               Content-Type: application/json
               Access-Control-Allow-Origin: *
               Access-Control-Allow-Credentials: true
               X-Powered-By: Flask
               X-Processed-Time: 0.000731945037842
               Content-Length: 313
               Via: 1.1 vegur
       
       Body (313 bytes):
       
       {
         "args": {}, 
         "data": "This is a PUT test!", 
         "files": {}, 
         "form": {}, 
         "headers": {
           "Connection": "close", 
           "Content-Length": "19", 
           "Content-Type": "application/json", 
           "Host": "httpbin.org"
         }, 
         "json": null, 
         "origin": "205.197.242.103", 
         "url": "https://httpbin.org/put"
       }
       
       
       
       >>>Delete data... 
       
       ----- RESPONSE: -----
       Status: 200 - OK
       Headers:
               Connection: keep-alive
               Server: meinheld/0.6.1
               gate: Thu, 28 Dec 2017 22:21:47 GMT
               Content-Type: application/json
               Access-Control-Allow-Origin: *
               Access-Control-Allow-Credentials: true
               X-Powered-By: Flask
               X-Processed-Time: 0.00133299827576
               Content-Length: 296
               Via: 1.1 vegur
       
       Body (296 bytes):
       
       {
         "args": {}, 
         "data": "", 
         "files": {}, 
         "form": {}, 
         "headers": {
           "Connection": "close", 
           "Content-Length": "0", 
           "Content-Type": "application/json", 
           "Host": "httpbin.org"
         }, 
         "json": null, 
         "origin": "205.197.242.103", 
         "url": "https://httpbin.org/delete"
       }
       
       
       
       >>>HTTP:stream, send http://httpbin.org/stream/10... 
       Chunk Received:
       {"headers": {"Connection": "close", "Host": "httpbin.org"}, "args": {}, "url": "https://httpbin.org/stream/10", "origin": "205.197.}
       Chunk Received:
       {"headers": {"Connection": "close", "Host": "httpbin.org"}, "args": {}, "url": "https://httpbin.org/stream/10", "origin": "205.197.}
       Chunk Received:
       {"headers": {"Connection": "close", "Host": "httpbin.org"}, "args": {}, "url": "https://httpbin.org/stream/10", "origin": "205.197.}
       Chunk Received:
       {"headers": {"Connection": "close", "Host": "httpbin.org"}, "args": {}, "url": "https://httpbin.org/stream/10", "origin": "205.197.}
       Chunk Received:
       {"headers": {"Connection": "close", "Host": "httpbin.org"}, "args": {}, "url": "https://httpbin.org/stream/10", "origin": "205.197.}
       Chunk Received:
       {"headers": {"Connection": "close", "Host": "httpbin.org"}, "args": {}, "url": "https://httpbin.org/stream/10", "origin": "205.197.}
       Chunk Received:
       {"headers": {"Connection": "close", "Host": "httpbin.org"}, "args": {}, "url": "https://httpbin.org/stream/10", "origin": "205.197.}
       Chunk Received:
       {"headers": {"Connection": "close", "Host": "httpbin.org"}, "args": {}, "url": "https://httpbin.org/stream/10", "origin": "205.197.}
       Chunk Received:
       {"headers": {"Connection": "close", "Host": "httpbin.org"}, "args": {}, "url": "https://httpbin.org/stream/10", "origin": "205.197.}
       Chunk Received:
       {"headers": {"Connection": "close", "Host": "httpbin.org"}, "args": {}, "url": "https://httpbin.org/stream/10", "origin": "205.197.}
       
       
       
       >>>HTTP:Status...
       
       ----- RESPONSE: -----
       Status: 200 - OK
       Headers:
               Connection: keep-alive
               Server: meinheld/0.6.1
               Date: Thu, 28 Dec 2017 22:21:49 GMT
               Content-Type: application/json
               Access-Control-Allow-Origin: *
               Access-Control-Allow-Credentials: true
               X-Powered-By: Flask
               X-Processed-Time: 0.000669956207275
               Content-Length: 488
               Via: 1.1 vegur
       
       Body (488 bytes):
       
       {
         "args": {
           "show_env": "1"
         }, 
         "headers": {
           "Connect-Time": "3", 
           "Connection": "close", 
           "Host": "httpbin.org", 
           "Total-Route-Time": "0", 
           "Via": "1.1 vegur", 
           "X-Forwarded-For": "205.197.242.103", 
           "X-Forwarded-Port": "443", 
           "X-Forwarded-Proto": "https", 
           "X-Request-Id": "07d5a2bb-b991-4886-a72c-e1d81c1cf824", 
           "X-Request-Start": "1514499710271"
         }, 
         "origin": "205.197.242.103", 
         "url": "https://httpbin.org/get?show_env=1"
       }
       
       - - - - - - - ALL DONE - - - - - - - 
> 

# Build for Greentea testing
After the basic application has been verified, build for the Greentea test suite using the following steps:
1. There is a known issue when using Greentea (https://os.mbed.com/docs/v5.7/tools/testing-applications.html)
   whereby there cannot be a main() function outside of a TESTS directory when building and running tests. This 
   is because all nontest code is compiled and linked with the test code and a linker error will occur due to 
   their being multiple main() functions defined. For this reason, please rename the main application file if 
   you need to build and run tests. Note that this only affects building and running tests.

   So, rename the application source file 'source/main-x.cpp' to 'source/main-x.keepcpp'.
   UPDATE: it is easier to add a ".mbedignore" file to the main directory and insert "source/*". This instructs
   the compiler to ignore the files in the source directory.

2. Execute the command: **mbed test -m K64F -t GCC_ARM -c --test-config wnc_config.json -n mbed-os-tests-netsocket-\***
   When running the test suite, it programs different test files into the hardware to run so execution will take
   some time to complete.  When finished, you will get a summary report similar to:

<code>
    mbedgt: test suite report:

    +--------------+---------------+--------------------------------------------+--------+--------------------+-------------+
    | target       | platform_name | test suite                                 | result | elapsed_time (sec) | copy_method |
    +--------------+---------------+--------------------------------------------+--------+--------------------+-------------+
    | K64F-GCC_ARM | K64F          | mbed-os-tests-netsocket-connectivity       | OK     | 49.19              | shell       |
    | K64F-GCC_ARM | K64F          | mbed-os-tests-netsocket-gethostbyname      | OK     | 26.62              | shell       |
    | K64F-GCC_ARM | K64F          | mbed-os-tests-netsocket-ip_parsing         | OK     | 15.82              | shell       |
    | K64F-GCC_ARM | K64F          | mbed-os-tests-netsocket-socket_sigio       | OK     | 59.7               | shell       |
    | K64F-GCC_ARM | K64F          | mbed-os-tests-netsocket-tcp_echo           | OK     | 41.52              | shell       |
    | K64F-GCC_ARM | K64F          | mbed-os-tests-netsocket-tcp_hello_world    | OK     | 42.01              | shell       |
    | K64F-GCC_ARM | K64F          | mbed-os-tests-netsocket-udp_dtls_handshake | FAIL   | 126.89             | shell       |
    | K64F-GCC_ARM | K64F          | mbed-os-tests-netsocket-udp_echo           | OK     | 63.61              | shell       |
    +--------------+---------------+--------------------------------------------+--------+--------------------+-------------+

    mbedgt: test suite results: 1 FAIL / 7 OK
    mbedgt: test case report:

    +--------------+------+--------------------------------------------+----------------------------------------+---+---+------+-------+
    | target       |platfm| test suite                                 | test case                              | p | f | rslt | time  |
    +--------------+------+--------------------------------------------+----------------------------------------+---+---+------+-------+
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-connectivity       | Bringing the network up and down       | 1 | 0 | OK   | 22.08 |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-connectivity       | Bringing the network up and down twice | 1 | 0 | OK   | 11.72 |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-gethostbyname      | DNS literal                            | 1 | 0 | OK   | 0.78  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-gethostbyname      | DNS preference literal                 | 1 | 0 | OK   | 0.79  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-gethostbyname      | DNS preference query                   | 1 | 0 | OK   | 0.85  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-gethostbyname      | DNS query                              | 1 | 0 | OK   | 1.09  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-ip_parsing         | Hollowed IPv6 address                  | 1 | 0 | OK   | 0.05  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-ip_parsing         | Left-weighted IPv4 address             | 1 | 0 | OK   | 0.06  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-ip_parsing         | Left-weighted IPv6 address             | 1 | 0 | OK   | 0.06  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-ip_parsing         | Null IPv4 address                      | 1 | 0 | OK   | 0.05  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-ip_parsing         | Null IPv6 address                      | 1 | 0 | OK   | 0.05  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-ip_parsing         | Right-weighted IPv4 address            | 1 | 0 | OK   | 0.06  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-ip_parsing         | Right-weighted IPv6 address            | 1 | 0 | OK   | 0.06  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-ip_parsing         | Simple IPv4 address                    | 1 | 0 | OK   | 0.05  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-ip_parsing         | Simple IPv6 address                    | 1 | 0 | OK   | 0.06  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-socket_sigio       | Socket Attach Test                     | 1 | 0 | OK   | 9.81  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-socket_sigio       | Socket Detach Test                     | 1 | 0 | OK   | 9.61  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-socket_sigio       | Socket Reattach Test                   | 1 | 0 | OK   | 4.61  |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-tcp_echo           | TCP echo                               | 1 | 0 | OK   | 25.96 |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-tcp_hello_world    | TCP hello world                        | 1 | 0 | OK   | 26.34 |
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-udp_dtls_handshake | UDP DTLS handshake                     | 0 | 1 | FAIL | 111.09|
    | K64F-GCC_ARM | K64F | mbed-os-tests-netsocket-udp_echo           | UDP echo                               | 1 | 0 | OK   | 48.52 |
    +--------------+------+--------------------------------------------+----------------------------------------+---+---+------+-------+

    mbedgt: test case results: 1 FAIL / 21 OK
    mbedgt: completed in 425.57 sec
    mbedgt: exited with code 1
    [mbed] ERROR: "mbedgt" returned error code 1.
    [mbed] ERROR: Command "mbedgt --test-spec /home/jflynn/AvNet/test/BUILD/tests/K64F/GCC_ARM/test_spec.json -n mbed-os-tests-netsocket-*" in "/home/jflynn/AvNet/test"
    ---

</code>

NOTE: the "UDP DTLS handshake" test is a known failure.


