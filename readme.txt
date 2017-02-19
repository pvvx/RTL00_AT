AT command 
RTL00 module (RTL8710AF) 

# ATSV
DeviceID: FC, Flash Size: 1048576 bytes, FlashID: C22014/1,  SpicMode: DIO
v2.2.1,v3.5

UART2 AT:
RX - GA0
TX - GA4
CTS - GA1
RTS - GA2

###########################################################################################
Used GCC SDK RTL8710 basic version
set paths.mk

SDK_PATH = ../RTL00MP3/RTL00_SDKV35a/
..

see: EclipseLinkedSDK.gif


help:
WLAN AT COMMAND SET:
==============================
1. Wlan Scan for Network Access Point
   # ATWS
2. Connect to an AES AP
   # ATPN=<ssid>,<pwd>,<key_id>(,<bssid>)
3. Create an AES AP
   # ATPA=<ssid>,<pwd>,<chl>,<hidden>
4. Set auto connect
   # ATPG=<0/1>


Init AT-Web:

ATPW=3
ATPA=RTL8710,0123456789,1,0
 ATSW=c

Next init:
ATSW=c



