User-space multi-touch driver 
---
For
usb 6-1: new full-speed USB device number 3 using sunxi-ohci  
usb 6-1: New USB device found, idVendor=0eef, idProduct=0005  
usb 6-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3  
usb 6-1: Product: WaveShare Touchscreen  
usb 6-1: Manufacturer: WaveShare  
usb 6-1: SerialNumber: 2016-11-06  

shop: http://www.aliexpress.com/item/Waveshare-7-inch-800x480-Raspberry-Pi-HDMI-Touch-Screen-Raspberry-Pi2-LCD-Capacitive-Display-Support-RPi/32512419201.html  
wiki: https://www.waveshare.com/wiki/7inch_HDMI_LCD_(B)  

Touch events consists of 11 bytes, one example is  
  `01 01 00 03 e1 01 3a 01 69 1a 01`  
check with: sudo xxd -c11 -g 1 /dev/hidraw3  

``` 
1 byte "01" beginning of the message
2 byte "00"/"01" touch / no touch
3 byte "00"..."04" multitouch point number
4 byte takes from 01 to some value: contact spot / pressure
5 byte takes from 00 to EE and changes with movement on X axis
6 byte takes from 00 to 02 and changes with movement on X axis
7 byte takes from 00 to EE and changes with movement on Y axis
8 byte takes from 00 to 01 and changes with movement on Y axis
9 increment byte, value increases with each message
10 increment byte, value increases with 9 byte exceed EE value
11 byte "00"/"05" number of touch points
9th and 10th byte giving the unique message number
```

There is another version of hidraw protocol which consist of 25 bytes, check links:  
https://github.com/bsteinsbo/rpi_touch_driver  
https://habr.com/post/267655/  
https://github.com/derekhe/waveshare-7inch-touchscreen-driver  

1. This driver reads /dev/ folder first and if there is any /hidraw* files it gathering its VID and PID and comparing to TARGET_VID TARGET_PID.
2. If there is no target dev it starts listening /dev folder via inotify and if "hidraw" file appeears it cheks VID PID again.
3. if there is a match it creates uinput device.
4. After that it collects raw data from target hidraw device using protocol mentioned above and injects it into uinput device.
5. If unplug the touch-screen it will back to listening /dev via inotify (point 2)

This driver using struct uinput_user_dev to set up the uinput device. This is deprecated, however this method must be used in Linux distros using legacy kernels.
Check the version of uinput on your traget device using 
$ cat /usr/include/linux/uinput.h | grep UINPUT_VERSION
If version is >= 5 this driver might not be suitable however you can modify it.

This driver may be needed to those who use Linux distros with Legacy Kernels < 4. Since in new Kernels this tocuhscreen supports right from a box.

Inforamtion sources:  
https://github.com/bsteinsbo/rpi_touch_driver  
https://habr.com/post/267655/  
https://github.com/derekhe/waveshare-7inch-touchscreen-driver  
https://www.kernel.org/doc/Documentation/hid/hidraw.txt  
https://www.kernel.org/doc/html/v4.12/input/uinput.html  
https://www.kernel.org/doc/html/v4.12/input/multi-touch-protocol.html  

## Installation ##

1. Add hid_multitouch module to /etc/modules
```
sudo nano /etc/modules
+ hid_multitouch
```
2. Reboot  
```sudo reboot now```
3. Make sure module is loaded  
```lsmod | grep hid_multitouch```
4. Build the driver  
```gcc ./driver.c -o touch_drv```
5. Try the driver  
```./touch_drv```
6. Enable autostart  
```
$sudo cp touch_drv /usr/local/bin
$sudo nano /etc/rc.local
+ /usr/local/bin/touch_drv &
```
7. Reboot  
```sudo reboot now```

Copyright (c) 2018 Mike Ovchinnikov

This program is free software; you can redistribute it and/or modify it
under the terms of the MIT License.
