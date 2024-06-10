# DISCONTINUATION OF PROJECT #  
This project will no longer be maintained by Intel.  
Intel has ceased development and contributions including, but not limited to, maintenance, bug fixes, new releases, or updates, to this project.  
Intel no longer accepts patches to this project.  
 If you have an ongoing need to use this project, are interested in independently developing it, or would like to maintain patches for the open source software community, please create your own fork of this project.  
  
Intel(R) Low Latency Streaming Research Platform (LLSRP)
=================================================

Please read carefully before using! This code is for research purposes only.
Our recommendation is to run the tool inside a private secured network.
Use at your own risk with no expectation of privacy or security.

For avoidance of doubt,

* Data is transferred unencrypted through an open protocol

* Users should expect that potential attackers could get root privileges on both the server and client

Linux library dependencies
--------------

x264: https://x264.org

avcodec, avdevice: https://www.ffmpeg.org

X11, Xext, Xi, Xtst: https://www.freedesktop.org

pthread

popt

isal

Linux setting up environment for LLSRP (you will need sudo privileges)
--------------

* Install.sh

Installs all required libs if missing.

If there is no libisal2 for your Linux - download, build and install from:
https://github.com/intel/isa-l.git

Linux building LLSRP
--------------

* Make: GNU 'make'

* Compiler: gcc

On client side (where user is controlling):
  make client

On server side (where game or some application running):
  make server

Linux running LLSRP
--------------

For help:

On client side (to print help):
* sudo ./client --help

On Server side (to print help):
* sudo ./server --help

To test client side:
* make clientMouse
* ./clientMouse

To start Server on a running network interface (for example eth0, 4 links, ports 5910, 5911):
  Interface name could differ from "eth0". You can look up the name(s) using "ifconfig".

* sudo ./server eth0:5910 eth0:5911 -n=5 -d=2 -l=2 -r=2 -D=:12 -s=4

  This will start server that expect 4 links:2 local (-l=2) and 2 remote (-r=2);
  All transmitters will be at "eth0", 2 on port 5910, 2 on port 5911;
  An encoding to prevent packet loss will be with 2 redundant packets (-d=2) per 5 real (-n=5);
  A display ":12" (-D=:12) square from 0, 0 to 1024, 768 will be grabbed with 60fps (-s=4) and send to network;
  Other modes are 1920x1080 60fps (-s=0)... full list is in help "./server --help"

To start Client:

  WARNING! When connection established client will grab mouse and keyboard control.
  To exit client and server (if Ctrl^C does not work): hold left mouse button, then press ESC.

  To switch to relative mouse: hold left mouse button, then press left shift.

  To switch to absolute mouse: hold left mouse button, then press right shift.

* sudo ./client eth0 eth0 [server.ip.address]:5910 [server.ip.address]:5911 -l=2 -r=2 -n=5 -d=2 -g

  This will start client that will try to initiate connection with server using
  network interface "eth0" with server at [server.ip.address] and ports 5910, 5911
  All receivers will be at "eth0". Client will expect 4 links: 2 local (-l=2) and 2 remote (-r=2);
  An encoding to prevent packet loss will be with 2 redundant packets (-d=2) per 5 real (-n=5);
  The client will start in game mode (-g) meaning window with server content will pop up
  in the center, mouse will not be able to exit the window, only relative mouse coordinates
  will be used.

Windows library dependencies
--------------

avcodec, avdevice: https://www.ffmpeg.org

sdl 2.0: https://libsdl.org

isal: https://github.com/intel/isa-l.git

Windows setting up environment for LLSRP
--------------

Install to some folder:

isal from: https://github.com/intel/isa-l.git

avcodec, avdevice (https://www.ffmpeg.org) and SDL 2.0 (https://libsdl.org)

Windows building LLSRP
--------------

* Compiler: Microsoft or Intel compiler for Windows

set REPO to folder where you installed all libs from above

To build client: compile.bat

Server is not supported on Windows

To build test: compile_test.bat

Windows running LLSRP
--------------

For help:

On client side (to print help):
* Client.exe --help

To test client side:
* ClientTest.exe

To start Client:

  WARNING! When connection established client will grab mouse and keyboard control.

* Make sure all library paths are added (for the installed libraries)

* Client ethernet_0 ethernet_0 [server.ip.address]:5910 [server.ip.address]:5911 -l=2 -r=2 -n=5 -d=2 -g

  This will start client that will try to initiate connection with server using
  network interface "ethernet_0" with server at [server.ip.address] and ports 5910, 5911
  All receivers will be at "ethernet_0". Client will expect 4 links: 2 local (-l=2) and 2 remote (-r=2);
  An encoding to prevent packet loss will be with 2 redundant packets (-d=2) per 5 real (-n=5);
  The client will start in game mode (-g) meaning window with server content will pop up
  in the center, mouse will not be able to exit the window, only relative mouse coordinates
  will be used.

License
--------------

Apache 2.0

http://www.apache.org/licenses/LICENSE-2.0
