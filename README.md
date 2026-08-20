# ESP32 WS2812B archery timer

Countdown timer for an ESP32-WROOM and two 8x32 WS2812B panels. The panels form a 32x16 display and use vertical serpentine wiring inside each panel.

## Build and flash

Install PlatformIO in VS Code, open this folder, and use the `esp32dev` environment. From the project directory, run:

```text
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

The upload command may require holding the ESP32 BOOT button while the board enters download mode. After boot, connect to the `Archery-Timer` Wi-Fi network with password `archery123`, then open the IP address printed in the serial monitor, normally `192.168.4.1`.

The configured duration and brightness are stored in ESP32 NVS and survive a reboot. The timer starts paused at the configured duration.

## Panel connection

Viewed from the front, arrange the panels vertically:

```text
			 +-------------------------------+
			 | Panel 1                       |
			 | rotated 180 degrees           |
			 +-------------------------------+
						 ^
						 | data output from Panel 0
			 +-------------------------------+
			 | Panel 0                       |
			 | chain input                   |
			 +-------------------------------+
ESP32 GPIO 13 -> Panel 0 DIN -> Panel 0 DOUT -> Panel 1 DIN
```

Panel 0 is the lower panel and Panel 1 is the upper panel. Panel 1 must be physically rotated 180 degrees. The firmware expects the first LED in each panel to be at the top-left when that panel is viewed in its logical orientation; the next LED is below it. Each following column reverses direction.

Connect the ESP32 ground to the LED power-supply ground. Connect GPIO 13 to the data input of Panel 0. Do not connect the ESP32 data output to Panel 1 directly; Panel 1 receives data from Panel 0.

## Power and hardware

Use a regulated 5 V supply sized for 512 LEDs, common the supply ground with ESP32 ground, add a 330-470 ohm resistor in the data line, and place a large electrolytic capacitor across the panel supply. Do not power the panels from the ESP32 board. A 3.3 V to 5 V data-level shifter is recommended.

Change `DATA_PIN`, `PANEL_VERTICAL_SERPENTINE`, or `PANEL_ORDER` near the top of `src/main.cpp` if the physical wiring differs. The settings page provides round duration, brightness, start, pause, and reset controls.