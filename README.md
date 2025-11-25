# Gesture-Based Media Controller 
# ID: 2025AAPS0760G

A cross-platform Python application that receives gesture commands from an ESP microcontroller via serial communication and controls media playback and volume on your computer.

## Features

- 🎵 **Play/Pause** - Control media playback
- ⏭️ **Next/Previous Track** - Skip between tracks
- 🔊 **Volume Up/Down** - Adjust system volume
- 🖥️ **Cross-platform** - Works on macOS, Linux, and Windows
- 🔌 **Auto-detection** - Automatically detects serial port and OS
- 🎧 **Universal Media Control** - Works with ANY media player (Spotify, Apple Music, YouTube, VLC, etc.)

## Supported Commands

| Command | Action |
|---------|--------|
| `PLAY_PAUSE` | Toggle play/pause |
| `NEXT` | Next track |
| `PREV` | Previous track |
| `VOL_UP` | Increase volume |
| `VOL_DOWN` | Decrease volume |

## Installation

### 1. Clone or download the repository
### 2. Create a virtual environment (recommended)

```bash
python3 -m venv venv
source venv/bin/activate  # On macOS/Linux
# OR
venv\Scripts\activate     # On Windows
```

### 3. Install dependencies

```bash
pip install -r requirements.txt
```

## Platform-Specific Setup

### macOS

#### Required Package
The `pyobjc-framework-Quartz` package is automatically installed from requirements.txt. This enables universal media control that works with **any** media player.

#### Grant Accessibility Permissions
1. Go to **System Settings** → **Privacy & Security** → **Accessibility**
2. Add your terminal app (Terminal.app, iTerm, or VS Code) to the list
3. Enable the toggle for the app
4. Restart the terminal after granting permissions

#### How it works on macOS
- **Media Control**: Uses system media keys via Quartz framework (works with ANY app)
- **Volume Control**: Uses native AppleScript
- **Fallback**: If Quartz fails, falls back to direct AppleScript control of Music/Spotify

### Linux

Install the following tools for media and volume control:

```bash
# For media control (highly recommended)
sudo apt install playerctl

# For volume control (usually one of these is pre-installed)
sudo apt install pulseaudio-utils   # provides pactl (PulseAudio)
# OR
sudo apt install alsa-utils         # provides amixer (ALSA)
```

#### How it works on Linux
- **Media Control**: Uses `playerctl` (preferred) or D-Bus directly
- **Volume Control**: Uses `pactl` (PulseAudio) or `amixer` (ALSA)

### Windows

No additional setup required - uses `pyautogui` for media key simulation.

#### How it works on Windows
- **Media Control**: Uses `pyautogui` to simulate media keys
- **Volume Control**: Uses `pyautogui` to simulate volume keys

## Usage

### 1. Connect your ESP device

Connect your ESP microcontroller to your computer via USB.

### 2. Find your serial port (if auto-detection fails)

**macOS:**
```bash
ls /dev/tty.*
```

**Linux:**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

**Windows:**
Check Device Manager for COM port number.

### 3. Run the script

```bash
python pyserial_media.py
```

The script will:
1. Display detected OS and available tools
2. Auto-detect your serial port
3. Connect and listen for gesture commands

Example output:
```
==================================================
SYSTEM DETECTION
==================================================
  OS: macOS (using Quartz for universal media control)
  Volume control: AppleScript
==================================================
Auto-detected serial port: /dev/tty.usbserial-0001
Attempting to connect to /dev/tty.usbserial-0001...
Successfully connected to /dev/tty.usbserial-0001.
Listening for gestures. Press Ctrl+C to exit.
```

### 4. Manual port configuration (optional)

If auto-detection doesn't work, edit `pyserial_media.py` and modify the fallback port in the `get_serial_port()` function:

```python
if sys.platform == 'darwin':  # macOS
    return '/dev/tty.usbserial-XXXX'  # Replace with your port
elif sys.platform.startswith('linux'):
    return '/dev/ttyUSB0'  # Or /dev/ttyACM0
else:  # Windows
    return 'COM3'  # Replace with your COM port
```

## Troubleshooting

### macOS Issues

#### Media keys not working
1. Ensure Accessibility permissions are granted to your terminal
2. Restart your terminal after granting permissions
3. Make sure `pyobjc-framework-Quartz` is installed:
   ```bash
   pip install pyobjc-framework-Quartz
   ```
4. Have a media player open and playing

#### Volume not changing
Volume control uses AppleScript and should work out of the box. If not working:
1. Try running manually: `osascript -e 'set volume output volume 50'`

### Linux Issues

#### "Permission denied" on serial port
Add your user to the `dialout` group:
```bash
sudo usermod -a -G dialout $USER
```
Then log out and log back in.

#### Media keys not working
1. Verify `playerctl` is installed: `playerctl --version`
2. Check if a player is detected: `playerctl status`
3. List available players: `playerctl -l`

#### Volume not changing
1. Check if PulseAudio is running: `pactl info`
2. Or verify ALSA is available: `amixer`

### Windows Issues

#### Media keys not working
1. Ensure `pyautogui` is installed
2. Try running as administrator

### General Issues

#### Serial port not found
1. Ensure the ESP device is connected
2. Check if the device appears in the port list
3. Try unplugging and replugging the device
4. Check USB cable (some cables are charge-only)

## Dependencies

| Package | Purpose | Platform |
|---------|---------|----------|
| `pyserial` | Serial communication | All |
| `pyautogui` | Media key simulation | Windows |
| `pyobjc-framework-Quartz` | System media keys | macOS only |

### Linux system packages
| Package | Purpose |
|---------|---------|
| `playerctl` | Media control (recommended) |
| `pulseaudio-utils` | Volume control (pactl) |
| `alsa-utils` | Volume control (amixer) |

## Hardware Requirements

- ESP8266 or ESP32 microcontroller
- IMU sensor (for gesture detection)
- USB cable for serial communication

## Project Structure

```
chotubot/
├── pyserial_media.py    # Main Python script
├── requirements.txt     # Python dependencies
├── README.md           # This file
├── imu_songs/          # Arduino sketch for IMU
│   └── imu_songs.ino
└── songs_animation/    # Arduino sketch with animations
    ├── songs_animation.ino
    └── logo.h
```

## Supported Media Players

### macOS (Universal - works with all)
- Apple Music
- Spotify
- YouTube (in browser)
- VLC
- Any app that responds to media keys

### Linux (via playerctl)
- Spotify
- VLC
- Rhythmbox
- Any MPRIS-compatible player

### Windows
- Spotify
- Windows Media Player
- Any app that responds to media keys

## License

MIT License


http://arduino.esp8266.com/stable/package_esp8266com_index.json
