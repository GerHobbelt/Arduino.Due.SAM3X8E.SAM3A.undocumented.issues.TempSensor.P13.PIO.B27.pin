# Arduino Due SAM3X8E (SAM3A) undocumented MCU + board issues

The top one (below) cost' me a couple of days wall clock time and many hours researching, digging and testing code. And then, as usual, once you have unearthed what's going on, you discover the *right question to ask the search engines* and you find others have run into these issues previously. And *sometimes* there might even be a *fix* or *work-around* already! 😅

## 1. activating ADC channel 15 (internal Temperate Sensor) conflicts with Arduino output pin P13 ('blinking LED') a.k.a. SAM3X/SAM3A PIO B bit B27 a.k.a. TIOB0 pin

Demo code demonstrating the issue is provided as `ADC15-tempsensor-conflict-output-pin13-LED/demo-of-conflict/main.ino`:

- simply load the `ino` file in your Arduino IDE, build and upload, then run/reset your Due board.
- **observe** the LED blinking for 5 seconds, then *STOPS BLINKING* until the 20th second after boot/reset.
- meanwhile, the serial output logs what's being done and read: these transitions in behaviour are marked by the ADC channel 15 being *activated* and *deactivated* repectively, using the CMSIS API calls as provided by the Arduino Due build environment. (Later replaced by more direct SAM3 port interaction as the CMSIS codebase is dubious when you become paranoid while hunting this kind of hardware error. See the `main.ino` source code.)
- The demo code repeats (resets) the cycle after 61 seconds: I use an *odd* number of seconds for the major cycle so it becomes perfectly obvious that the actual state of the LED is irrelevant to the issue and the observed phenomenon.

  > **observe** that when you touch `P13` and the adjacent `GND` pin at the connector on the Due board, the LED 'semi-randomly' **dims** during the 5-to-20-second period, depending on your touch & finger conductivity: this is due to the fact that the MCU switches the P13/TIOB0 pin to **high impedance input mode** during this episode.
  >

### References

- [Arduino forum: Reading the temperature sensor inside Arduino Due](https://forum.arduino.cc/t/reading-the-temperature-sensor-inside-arduino-due/180830)
- [Microchip/Atmel forum: SAM3A using ADC Internal Temp sensor on AD15 that somehow takes over PB27 GPIO](https://forum.microchip.com/s/topic/a5C3l000000UfdyEAC/t173189)
- [Atmel -> Microchip: SAM3X8E MCU datasheet, et al](https://www.microchip.com/en-us/product/ATSAM3X8E)
- 




## 2. Due (clone) board does not start/run the freshly uploaded firmware code, unless you manually RESET the board

TBD



### References

-
-
-



