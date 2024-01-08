# Firmware Files

Collection of the code I've developed for this bus checking project. It's all written in the Arduino IDE (thus far), and requires the use of the [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32). I'll probably move to using another IDE in the future.

Here is a breakdown of what each set of firmware is for:

| Firmware Name | Purpose |
| --- | --- |
| `ttc_check` | **This is the firmware to run on the "TTC Check" board *(first one with an embedded ESP32)*** |
| `TTC_display` | Used to test my code to drive the the seven segment display, namely ESP32 timers. *NOTE: Originally the boards drove the display directly.* |
| `TTCrequestorV1` | Heavily based on the TinyXML's example for NextBus. Collected predictions and provided them over serial. Did not have any display related code, focused solely on data collection. |
| `TTCrequestorV2` | Incorperated code to directly drive the seven-segment displays from the ESP32 and show the values. |
| `TTCrequestorV3` | Improved the implementation of my debugging features, namely using `#ifdef ... #endif` to include debugging code in the compiled binary if "DEBUG_MODE" was defined or not, instead of using "if" structures. Saves compile time and flash when not debugging. |
| `TTCrequestorV4` | Removed the use of TinyXML, using my own basic parsing function instead to get times. |
