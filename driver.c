/*
Waveshare 7 inch LCD touchscreen driver
User-space multi-touch driver for
usb 6-1: new full-speed USB device number 3 using sunxi-ohci
usb 6-1: New USB device found, idVendor=0eef, idProduct=0005
usb 6-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
usb 6-1: Product: WaveShare Touchscreen
usb 6-1: Manufacturer: WaveShare
usb 6-1: SerialNumber: 2016-11-06

shop: http://www.aliexpress.com/item/Waveshare-7-inch-800x480-Raspberry-Pi-HDMI-Touch-Screen-Raspberry-Pi2-LCD-Capacitive-Display-Support-RPi/32512419201.html
wiki: https://www.waveshare.com/wiki/7inch_HDMI_LCD_(B)

Touch events consists of 11 bytes, one example is
  01 01 00 03 e1 01 3a 01 69 1a 01
check with: sudo xxd -c11 -g 1 /dev/hidraw3
 
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

There is a new touchscreen module in rev 3.1 of present dispaly
Because of it X_RATIO and Y_RATIO was added
recommeneded values are 0.78 and 0.8 respectively

Inforamtion sources:
https://github.com/bsteinsbo/rpi_touch_driver
https://habr.com/post/267655/
https://github.com/derekhe/waveshare-7inch-touchscreen-driver
https://www.kernel.org/doc/Documentation/hid/hidraw.txt
https://www.kernel.org/doc/html/v4.12/input/uinput.html
https://www.kernel.org/doc/html/v4.12/input/multi-touch-protocol.html

Copyright (c) 2018 Mike Ovchinnikov

 This program is free software; you can redistribute it and/or modify it
 under the terms of the MIT License.
 
 */

#include <dirent.h> //directry entry
#include <linux/hidraw.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strcmp
#include <unistd.h> // read write sleep close
#include <sys/inotify.h> // /dev/ watching
#include <fcntl.h> // some constants
#include <linux/uinput.h>

#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + 16))

#define TARGET_VID 0x0eef
#define TARGET_PID 0x0005

#define X_RATIO 0.78
#define Y_RATIO 0.8

void emit(int fd, int type, int code, int val)
{
   struct input_event ie;

   ie.type = type;
   ie.code = code;
   ie.value = val;
   /* timestamp values below are ignored */
   ie.time.tv_sec = 0;
   ie.time.tv_usec = 0;

   write(fd, &ie, sizeof(ie));
}

int uinput_2(void)
{
   struct uinput_user_dev uud;
   int version, rc, fd;

   fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
   rc = ioctl(fd, UI_GET_VERSION, &version);

   if (rc == 0 && version >= 5) {
      /* use UI_DEV_SETUP */
      return -1;
   }
   
   ioctl(fd, UI_SET_EVBIT, EV_KEY);
   ioctl(fd, UI_SET_EVBIT, EV_ABS);
   ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
   ioctl(fd, UI_SET_ABSBIT, ABS_X);
   ioctl(fd, UI_SET_ABSBIT, ABS_Y);
   ioctl(fd, UI_SET_ABSBIT, ABS_PRESSURE);
   ioctl(fd, UI_SET_ABSBIT, ABS_MT_SLOT);
   ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
   ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
   ioctl(fd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
   ioctl(fd, UI_SET_ABSBIT, ABS_MT_PRESSURE);
   ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);
   
   memset(&uud, 0, sizeof(uud));
   snprintf(uud.name, UINPUT_MAX_NAME_SIZE, "Waveshare touch uinput");
   uud.id.bustype = BUS_VIRTUAL;
	uud.id.vendor = 0x1234;
	uud.id.product = 0x5678;
	uud.id.version = 0x9101;
	uud.absmin[ABS_MT_SLOT] = 0;
   uud.absmax[ABS_MT_SLOT] = 5; // track up to 5 fingers
   uud.absmin[ABS_MT_TOUCH_MAJOR] = 0;
   uud.absmax[ABS_MT_TOUCH_MAJOR] = 15;
   uud.absmin[ABS_MT_POSITION_X] = 0; // screen dimension
   uud.absmax[ABS_MT_POSITION_X] = 800; // screen dimension
   uud.absmin[ABS_MT_POSITION_Y] = 0; // screen dimension
   uud.absmax[ABS_MT_POSITION_Y] = 480; // screen dimension
   uud.absmin[ABS_MT_TRACKING_ID] = 0;
   uud.absmax[ABS_MT_TRACKING_ID] = 65535;
   uud.absmin[ABS_MT_PRESSURE] = 0;
   uud.absmax[ABS_MT_PRESSURE] = 255;
   write(fd, &uud, sizeof(uud));

   ioctl(fd, UI_DEV_CREATE);

   /*
    * On UI_DEV_CREATE the kernel will create the device node for this
    * device. We are inserting a pause here so that userspace has time
    * to detect, initialize the new device, and can start listening to
    * the event, otherwise it will not notice the event we are about
    * to send. This pause is only needed in our example code!
    */
   sleep(1);
   return fd;
}

int usbraw_(const char * path)
{
   int fd;
   fd = open(path, O_RDONLY);
   if (fd < 0) 
   {
      printf("HIDRAW port doesn't open. Run as root?\n");
      return -1;
   }
   struct hidraw_devinfo devinfo;
   
   int rc = ioctl(fd, HIDIOCGRAWINFO, &devinfo);
   if(rc < 0) return -1;
   
   if(devinfo.vendor != TARGET_VID && devinfo.product != TARGET_PID)
   {
      close(fd);
      return -1;
   }   
   return fd;
}

int intf()
{
   int fd = inotify_init(); 
   if (fd < 0) return -1;
   
   int wd = inotify_add_watch (fd, "/dev", IN_CREATE);
   if (wd < 0) return -1;
   
   char buffer[BUF_LEN];
   while(1)
   {
      int hidraw_fd;
      int length = read(fd, buffer, BUF_LEN);
      if (length < 0) return -1;
   
      const char hid[]= "hidraw";
   
      for(const char *p = buffer, *end = buffer + length; p < end; )
      {
         struct inotify_event *e = (struct inotify_event *) p;
         //~ if (e->mask & IN_CREATE)
         //~ printf("%s\n", e->name);
         if (strncmp(e->name, hid, sizeof(hid) - 1) == 0)
         {
            printf("HIDRAW DEV FOUND\n");
            char path [14];
            const char * prefix = "/dev/";
            strcpy(path, prefix);
            strcat(path, e->name);
            printf("%s\n", path);
            hidraw_fd = usbraw_(path);
            if (hidraw_fd < 0)
               break;
            else
            {
               inotify_rm_watch(fd, wd);
               close(fd);
               return hidraw_fd;
            }               
         }
         p += sizeof(struct inotify_event) + e->len;
      }
   }
}

int main(void)
{
   DIR *dir;
   struct dirent *entry;
   const char filename[] = "hidraw";
   const char * prefix = "/dev/";
   int usbraw_fd;   
   
   dir = opendir(prefix);
   if(!dir)
   {
      perror("diropen");
      exit(1);
   }
   entry = readdir(dir);
   while (entry != NULL)
   {
      if(strncmp(entry->d_name, filename, sizeof(filename) - 1) == 0)
      {
         printf("%s\n", entry->d_name);
         char path [14];
         strcpy (path, prefix);
         strcat (path, entry->d_name);
         usbraw_fd = usbraw_(path);
         if (usbraw_fd > 0) break;
      }
      entry = readdir(dir);
   }
   closedir(dir);
   if (usbraw_fd < 0) usbraw_fd = intf();
   
   int uinput_fd = uinput_2();
   
   if (uinput_fd < 0)
   {
      printf("Unable to open uipnut device. Did you forget sudo?");
      return 0;
   }
   
   int rowc = 0; //ROW COUNT ~ how many rows in a single batch
   unsigned char data[11]; //data gathering from usb raw
   while (1)
   {
      int n = read(usbraw_fd, data, sizeof(data));
      if (n < 0) 
      {
         printf("NO DATA / DISCONNECTED\n");
         close(usbraw_fd);
         usbraw_fd = intf();
         continue;
      }
      if (n != sizeof(data))
      {
         printf("WRONG DATA SIZE");
      }
      if(data[0] != 1)
      {
         printf("WRONG MESSAGE\n");
         continue;
      }
      
      if (rowc < 1)
         rowc = data[10];
      rowc--;
      
      if(data[1] != 1)
      {
         printf("ABS_MT_SLOT: %d\n", data[2]);
         emit(uinput_fd, EV_ABS, ABS_MT_SLOT, data[2]);
         printf("ABS_MT_TRACKING_ID -1\n");
         emit(uinput_fd, EV_ABS, ABS_MT_TRACKING_ID, -1);
         printf("SYN_REPORT\n"); //END OF BATCH
         emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
         continue; // ABS_MT_TRACKING_ID -1
      }         
        
      float x;
      float y;      
        
      for (int i = 0; i < sizeof(data); i++)
         printf("%02x ", (unsigned int)data[i]);
      printf("\n");
      printf("ABS_MT_SLOT: %d\n", data[2]);
      emit(uinput_fd, EV_ABS, ABS_MT_SLOT, data[2]);
      printf("ABS_MT_TRACKING_ID %d\n", data[2]);
      emit(uinput_fd, EV_ABS, ABS_MT_TRACKING_ID, data[2]);
      x = (data[4] + data[5]*256) * X_RATIO;
      y = (data[6] + data[7]*256) * Y_RATIO;
      printf("ABS_MT_POSITION_X: %d\n", (int)x); //X POSITION
      emit(uinput_fd, EV_ABS, ABS_MT_POSITION_X, (int)x);
      printf("ABS_MT_POSITION_Y: %d\n", (int)y); //Y POSITION
      emit(uinput_fd, EV_ABS, ABS_MT_POSITION_Y, (int)y);
      printf("ABS_PRESSURE: %d\n", data[3]); //CONTACT SPOT
      emit(uinput_fd, EV_ABS, ABS_PRESSURE, data[3]);
      if (rowc < 1)         
      {
         printf("SYN_REPORT\n"); //END OF BATCH
         emit(uinput_fd, EV_SYN, SYN_REPORT, 0);   
      }
      fflush(stdout);
   }

   close(usbraw_fd);
   sleep(1);
   ioctl(uinput_fd, UI_DEV_DESTROY);
   close(uinput_fd);
   return 0;
}
