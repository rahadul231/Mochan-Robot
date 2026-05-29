# Mochan Robot

Mochan Robot is a small ESP32-C3 based expressive robot with two DC geared motors, a 0.96 inch OLED display, WiFi captive portal control, animated RoboEyes, and TTP223 touch sensor reactions.

## Features

- ESP32-C3 based robot control
- 0.96 inch SSD1306 OLED animated eyes
- FluxGarage RoboEyes animated cartoon eye expressions
- WiFi captive portal control page
- Five main modes:
  - Sleep
  - Rush
  - Constant
  - Wiggle
  - Curious
- Manual movement controls:
  - Up
  - Down
  - Left
  - Right
  - Stop
- TTP223 touch sensor actions:
  - 1 tap = pain reaction
  - 2 taps = mode change
  - 3 taps = motor lock / unlock
  - long press = squash reaction
- Sleep mode stays sleeping until the next command
- Motor lock buttons in web control panel

## Hardware Used

- ESP32-C3 board
- 0.96 inch OLED display, SSD1306, 128x64, I2C
- 2 DC geared motors
- Motor driver module such as L9110S or similar
- TTP223 touch sensor module
- Battery charging circuit
- Jumper wires
- Robot body/chassis

## Pin Configuration

| Component | Pin / Connection |
|---|---|
| OLED SDA | GPIO 8 |
| OLED SCL | GPIO 9 |
| OLED VCC | ESP32-C3 3.3V |
| OLED GND | ESP32-C3 GND |
| TTP223 VCC | Same 3.3V point as OLED VCC |
| TTP223 GND | GND |
| TTP223 OUT | GPIO 4 |
| Left Motor IN1 | GPIO 3 |
| Left Motor IN2 | GPIO 2 |
| Right Motor IN1 | GPIO 1 |
| Right Motor IN2 | GPIO 0 |

## Required Arduino Libraries

Install these libraries in Arduino IDE:

1. `Adafruit GFX Library`
2. `Adafruit SSD1306`
3. `FluxGarage RoboEyes`
4. `esp32 by Espressif Systems` board package

The ESP32 board package provides:

- `WiFi.h`
- `WebServer.h`
- `DNSServer.h`

## Arduino IDE Setup

1. Open Arduino IDE.
2. Go to `File > Preferences`.
3. Add this URL in **Additional Boards Manager URLs**:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

4. Go to `Tools > Board > Boards Manager`.
5. Search `esp32`.
6. Install `esp32 by Espressif Systems`.
7. Select board:

```text
Tools > Board > ESP32 Arduino > ESP32C3 Dev Module
```

8. Select the correct COM port.
9. Open `Mochan_Robot.ino`.
10. Click **Verify**.
11. Click **Upload**.

## WiFi Control

After uploading the code:

1. Open phone WiFi settings.
2. Connect to:

```text
mochan
```

3. Password:

```text
12345678
```

4. The captive portal should open automatically.
5. If it does not open, go to:

```text
192.168.4.1
```

## Robot Modes

### Sleep Mode

The robot does not move. Motors stay completely OFF. The eyes stay closed and show a sleeping `Z z` symbol. The robot remains asleep until another command is given.

### Rush Mode

The robot moves quickly with stronger expressions.

### Constant Mode

The motors stay off, but the robot shows different facial reactions on the OLED.

### Wiggle Mode

The robot moves slowly and gently with calmer eye reactions.

### Curious Mode

The robot moves at medium speed and looks around with curious-style expressions.

### Manual Mode

The web control buttons directly control robot movement. Movement continues until STOP or another mode is selected.

## Touch Sensor Behavior

| Touch Action | Robot Reaction |
|---|---|
| 1 tap | Pain reaction |
| 2 taps | Change mode |
| 3 taps | Motor lock / unlock |
| Long press | Squash reaction |

## Important Notes

- Do not connect TTP223 VCC to a GPIO pin.
- TTP223 VCC must connect to 3.3V.
- OLED VCC and TTP223 VCC can share the same 3.3V pin.
- If the screen is upside down, change this line in the code:

```cpp
display.setRotation(2);
```

to:

```cpp
display.setRotation(0);
```

- If the TTP223 sensor does not respond, change:

```cpp
#define TOUCH_ACTIVE HIGH
```

to:

```cpp
#define TOUCH_ACTIVE LOW
```

## Repository Structure

```text
Mochan_Robot/
├── Mochan_Robot.ino
├── README.md
├── LICENSE
└── .gitignore
```

## Author

Muntasir Tahsan Ashpee

## License

This project is licensed under the MIT License.
