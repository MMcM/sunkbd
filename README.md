# Another Sun Keyboard USB driver #

This one has fewer features than most others because the goal is to
make it basic enough that [LUFA](http://www.fourwalledcubicle.com/LUFA.php)
and an ATmega32U4 can do all the work.

## Hardware ##

Any of these boards should work.

* Arduino [Leonardo](http://arduino.cc/en/Main/arduinoBoardLeonardo) / [Micro](http://arduino.cc/en/Main/arduinoBoardMicro).
* [Sparkfun Pro Micro](https://www.sparkfun.com/products/12640).
* [Adafruit Atmega32u4 breakout](http://www.ladyada.net/products/atmega32u4breakout/).
* [Teensy 2.0](https://www.pjrc.com/teensy/index.html).
* [A-Star 32U4 Micro](http://www.pololu.com/product/3101).

A 5V inverter is needed so that the MCU's UART can process the
keyboards 1200 baud, which while TTL level is idle low like RS232. I
used a 7404.

### Connections ###

The 8-pin DIN is connected as follows.

| Connector | Signal | Pin     | Arduino | Color  |
|-----------|--------|---------|---------|--------|
| 1         | GND    | GND     | GND     | brown  |
| 2         | GND    | GND     | GND     | white  |
| 3         | +5V    | VCC     | 5V      | black  |
| 4         | MOUSE  | N/C     |         | blue   |
| 5         | RX     | PD3 (*) | TX (1)  | green  |
| 6         | TX     | PD2 (*) | RX (0)  | yellow |
| 7         | POWER  | PD0     | 3       | orange |
| 8         | +5V    | VCC     | 5V      | red    |

(*) Keyboard RX goes to 1Y of the inverter and 1A to AVR TX.
    Keyboard TX goes to 2A of the inverter and 2Y to AVR RX.
