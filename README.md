What is it?
===========

GlowLight is an Arduino-based clone of Philips' Ambilight.

Hardware
========

1 x [Arduino Uno](http://arduino.cc/en/Main/ArduinoBoardUno) board

1 x [ShiftBrite Shield](http://macetech.com/store/index.php?main_page=product_info&products_id=7)

16 x [ShiftBrite LEDs](http://macetech.com/store/index.php?main_page=product_info&cPath=1&products_id=1)

1 x laptop with a webcam

and some necessary interconnects


Software
========

1. Build GlowLightArduino sketch and upload to Arduino board
2. Download and install [Cinder](http://libcinder.org)
3. You'll need Visual Studio 2010 to build GlowLightCinder project as-is, with some adjustment for Cinder paths. Code should be able to be built on any platform supported by Cinder, I plan on adding Xcode project when have a chance.