Intel(R) Low Latency Streaming Research Platform (LLSRP)
=================================================

Please read carefully before using! This code is for research purposes only.
Our recommendation is to run the tool inside a private secured network.
Use at your own risk with no expectation of privacy or security.

For avoidance of doubt,

* Data is transferred unencrypted through an open protocol

* Users should expect that potential attackers could get root privileges on both the server and client

Library dependencies
--------------

x264: https://x264.org

avcodec, avdevice: https://www.ffmpeg.org

X11, Xext, Xi, Xtst: https://www.freedesktop.org

pthread

popt

isal

Setting up environment for LLSRP (you will need sudo privileges)
--------------

* Install.sh

Installs all required libs if missing.

If there is no libisal2 for your Linux - download, build and install from:
https://github.com/intel/isa-l.git

Building LLSRP
--------------

* Make: GNU 'make'

* Compiler: gcc

On client side (where user is controlling):
  make client

On server side (where game or some application running):
  make server

Running LLSRP
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
  To exit client and server (if Ctrl^C does not work), hold ESC and left mouse button simultaniously.

* sudo ./client eth0 eth0 [server.ip.address]:5910 [server.ip.address]:5911 -l=2 -r=2 -n=5 -d=2 -g

  This will start client that will try to initiate connection with server using
  network interface "eth0" with server at [server.ip.address] and ports 5910, 5911
  All receivers will be at "eth0". Client will expect 4 links: 2 local (-l=2) and 2 remote (-r=2);
  An encoding to prevent packet loss will be with 2 redundant packets (-d=2) per 5 real (-n=5);
  The client will start in game mode (-g) meaning window with server content will pop up
  in the center, mouse will not be able to exit the window, only relative mouse coordinates
  will be used.

License
--------------

Apache 2.0

http://www.apache.org/licenses/LICENSE-2.0
