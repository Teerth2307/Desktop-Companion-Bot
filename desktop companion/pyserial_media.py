import serial
import serial.tools.list_ports
import time
import sys
import subprocess
import shutil
import threading

# --- CONFIGURATION ---
BAUD_RATE = 115200

# Detect OS once at startup
IS_MACOS = sys.platform == 'darwin'
IS_LINUX = sys.platform.startswith('linux')
IS_WINDOWS = sys.platform == 'win32'

# Check available Linux tools
HAS_PLAYERCTL = IS_LINUX and shutil.which('playerctl') is not None
HAS_PACTL = IS_LINUX and shutil.which('pactl') is not None
HAS_AMIXER = IS_LINUX and shutil.which('amixer') is not None

# macOS media key codes (NX_KEYTYPE)
MACOS_MEDIA_KEYS = {
    'play_pause': 16,  # NX_KEYTYPE_PLAY
    'next': 17,        # NX_KEYTYPE_NEXT
    'prev': 18,        # NX_KEYTYPE_PREVIOUS (rewind)
}

# Windows volume control using pycaw (advanced)
if IS_WINDOWS:
    try:
        from ctypes import POINTER, cast
        from comtypes import CLSCTX_ALL
        from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume
        import math
        WINDOWS_VOLUME_CONTROL_AVAILABLE = True
    except ImportError:
        WINDOWS_VOLUME_CONTROL_AVAILABLE = False

def send_macos_media_key(key_type):
    """Send a media key event on macOS using Quartz/CoreGraphics."""
    try:
        import Quartz
        
        # Key down event
        event = Quartz.NSEvent.otherEventWithType_location_modifierFlags_timestamp_windowNumber_context_subtype_data1_data2_(
            Quartz.NSEventTypeSystemDefined,  # 14
            (0, 0),
            0xa00,  # Key down flag
            0,
            0,
            None,
            8,  # Media key subtype
            (key_type << 16) | (0xa << 8),
            -1
        )
        if event:
            cg_event = event.CGEvent()
            if cg_event:
                Quartz.CGEventPost(Quartz.kCGHIDEventTap, cg_event)
        
        # Key up event
        event_up = Quartz.NSEvent.otherEventWithType_location_modifierFlags_timestamp_windowNumber_context_subtype_data1_data2_(
            Quartz.NSEventTypeSystemDefined,
            (0, 0),
            0xb00,  # Key up flag
            0,
            0,
            None,
            8,
            (key_type << 16) | (0xb << 8),
            -1
        )
        if event_up:
            cg_event_up = event_up.CGEvent()
            if cg_event_up:
                Quartz.CGEventPost(Quartz.kCGHIDEventTap, cg_event_up)
        
        return True
    except ImportError:
        return False
    except Exception as e:
        print(f"Error sending media key: {e}")
        return False

def get_serial_port():
    """Auto-detect or return platform-specific default port."""
    ports = list(serial.tools.list_ports.comports())
    for port in ports:
        if 'USB' in port.description or 'Serial' in port.description or 'usbserial' in port.device or 'usbmodem' in port.device:
            print(f"Auto-detected serial port: {port.device}")
            return port.device
    
    # Fallback defaults
    if IS_MACOS:
        return '/dev/tty.usbserial-0001'
    elif IS_LINUX:
        return '/dev/ttyUSB0'
    else:  # Windows
        return 'COM3'

SERIAL_PORT = get_serial_port()

def run_applescript(script):
    try:
        subprocess.run(['osascript', '-e', script], check=True)
    except subprocess.CalledProcessError as e:
        print(f"AppleScript error: {e}")

def run_command(cmd):
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def media_play_pause():
    if IS_MACOS:
        if not send_macos_media_key(MACOS_MEDIA_KEYS['play_pause']):
            run_applescript('''
                if application "Music" is running then
                    tell application "Music" to playpause
                else if application "Spotify" is running then
                    tell application "Spotify" to playpause
                end if
            ''')
    elif IS_LINUX:
        if HAS_PLAYERCTL:
            run_command(['playerctl', 'play-pause'])
        else:
            run_command(['dbus-send', '--print-reply', '--dest=org.mpris.MediaPlayer2.spotify',
                        '/org/mpris/MediaPlayer2', 'org.mpris.MediaPlayer2.Player.PlayPause'])
    else:  # Windows
        try:
            import pyautogui
            pyautogui.press('playpause')
        except ImportError:
            print("pyautogui required on Windows for media key control")

def media_next():
    if IS_MACOS:
        if not send_macos_media_key(MACOS_MEDIA_KEYS['next']):
            run_applescript('''
                if application "Music" is running then
                    tell application "Music" to next track
                else if application "Spotify" is running then
                    tell application "Spotify" to next track
                end if
            ''')
    elif IS_LINUX:
        if HAS_PLAYERCTL:
            run_command(['playerctl', 'next'])
        else:
            run_command(['dbus-send', '--print-reply', '--dest=org.mpris.MediaPlayer2.spotify',
                        '/org/mpris/MediaPlayer2', 'org.mpris.MediaPlayer2.Player.Next'])
    else:
        try:
            import pyautogui
            pyautogui.press('nexttrack')
        except ImportError:
            print("pyautogui required on Windows for media key control")

def media_prev():
    if IS_MACOS:
        if not send_macos_media_key(MACOS_MEDIA_KEYS['prev']):
            run_applescript('''
                if application "Music" is running then
                    tell application "Music" to previous track
                else if application "Spotify" is running then
                    tell application "Spotify" to previous track
                end if
            ''')
    elif IS_LINUX:
        if HAS_PLAYERCTL:
            run_command(['playerctl', 'previous'])
        else:
            run_command(['dbus-send', '--print-reply', '--dest=org.mpris.MediaPlayer2.spotify',
                        '/org/mpris/MediaPlayer2', 'org.mpris.MediaPlayer2.Player.Previous'])
    else:
        try:
            import pyautogui
            pyautogui.press('prevtrack')
        except ImportError:
            print("pyautogui required on Windows for media key control")

def volume_up():
    if IS_MACOS:
        run_applescript('set volume output volume (min ((output volume of (get volume settings)) + 6.25, 100))')
    elif IS_LINUX:
        if HAS_PACTL:
            run_command(['pactl', 'set-sink-volume', '@DEFAULT_SINK@', '+5%'])
        elif HAS_AMIXER:
            run_command(['amixer', 'set', 'Master', '5%+'])
        else:
            print("No Linux volume control tool found!")
    else:
        if WINDOWS_VOLUME_CONTROL_AVAILABLE:
            change_windows_volume(5)
        else:
            try:
                import pyautogui
                pyautogui.press('volumeup')
            except ImportError:
                print("pyautogui or pycaw required on Windows for volume control")

def volume_down():
    if IS_MACOS:
        run_applescript('set volume output volume (max ((output volume of (get volume settings)) - 6.25, 0))')
    elif IS_LINUX:
        if HAS_PACTL:
            run_command(['pactl', 'set-sink-volume', '@DEFAULT_SINK@', '-5%'])
        elif HAS_AMIXER:
            run_command(['amixer', 'set', 'Master', '5%-'])
        else:
            print("No Linux volume control tool found!")
    else:
        if WINDOWS_VOLUME_CONTROL_AVAILABLE:
            change_windows_volume(-5)
        else:
            try:
                import pyautogui
                pyautogui.press('volumedown')
            except ImportError:
                print("pyautogui or pycaw required on Windows for volume control")

def volume_set(level_percent):
    """Set volume to a specific percentage (0-100)."""
    level_percent = max(0, min(100, level_percent))
    if IS_MACOS:
        run_applescript(f'set volume output volume {level_percent}')
    elif IS_LINUX:
        if HAS_PACTL:
            run_command(['pactl', 'set-sink-volume', '@DEFAULT_SINK@', f'{level_percent}%'])
        elif HAS_AMIXER:
            # Approximate amixer set command (may vary by system)
            run_command(['amixer', 'set', 'Master', f'{level_percent}%'])
        else:
            print("No Linux volume control tool found!")
    else:
        if WINDOWS_VOLUME_CONTROL_AVAILABLE:
            set_windows_volume(level_percent)
        else:
            print("Windows pycaw not available: use pyautogui or install pycaw")

def volume_mute_toggle():
    if IS_MACOS:
        run_applescript('set volume output muted not (output muted of (get volume settings))')
    elif IS_LINUX:
        if HAS_PACTL:
            run_command(['pactl', 'set-sink-mute', '@DEFAULT_SINK@', 'toggle'])
        elif HAS_AMIXER:
            run_command(['amixer', 'set', 'Master', 'toggle'])
        else:
            print("No Linux volume control tool found!")
    else:
        if WINDOWS_VOLUME_CONTROL_AVAILABLE:
            toggle_windows_mute()
        else:
            print("Windows pycaw not available: mute toggle not supported")

def change_windows_volume(delta_percent):
    devices = AudioUtilities.GetSpeakers()
    interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
    volume = cast(interface, POINTER(IAudioEndpointVolume))
    current_level = volume.GetMasterVolumeLevelScalar()
    new_level = max(0.0, min(1.0, current_level + delta_percent / 100.0))
    volume.SetMasterVolumeLevelScalar(new_level, None)

def set_windows_volume(level_percent):
    devices = AudioUtilities.GetSpeakers()
    interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
    volume = cast(interface, POINTER(IAudioEndpointVolume))
    volume.SetMasterVolumeLevelScalar(level_percent / 100.0, None)

def toggle_windows_mute():
    devices = AudioUtilities.GetSpeakers()
    interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
    volume = cast(interface, POINTER(IAudioEndpointVolume))
    current_mute = volume.GetMute()
    volume.SetMute(not current_mute, None)

def execute_command(command):
    if not command or len(command) < 2:
        return
    command = command.strip().upper()

    response = "OK"
    try:
        if command == "PLAY_PAUSE":
            media_play_pause()
        elif command == "NEXT":
            media_next()
        elif command == "PREV":
            media_prev()
        elif command == "VOL_UP":
            volume_up()
        elif command == "VOL_DOWN":
            volume_down()
        elif command.startswith("VOL_SET:"):
            try:
                level = int(command.split(":")[1])
                volume_set(level)
            except ValueError:
                response = "ERR: Invalid volume level"
        elif command == "VOL_MUTE":
            volume_mute_toggle()
        else:
            response = f"IGNORED: Unknown command {command}"
    except Exception as e:
        response = f"ERR: {e}"

    # Send response back to serial (acknowledgement)
    try:
        if ser and ser.is_open:
            ser.write((response + "\n").encode('utf-8'))
    except Exception as e:
        print(f"Error sending serial response: {e}")

def read_serial_in_background(ser, callback):
    buffer = b''
    while True:
        try:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                buffer += data
                while b'\n' in buffer:
                    line, buffer = buffer.split(b'\n', 1)
                    line_str = line.decode('utf-8', errors='ignore').strip()
                    if line_str:
                        callback(line_str)
            time.sleep(0.1)
        except serial.SerialException:
            print("Serial connection lost, attempting to reconnect...")
            ser.close()
            time.sleep(2)
            try:
                ser.open()
            except Exception as e:
                print(f"Reconnect failed: {e}")
                time.sleep(5)
        except Exception as e:
            print(f"Unexpected error in serial thread: {e}")

def print_system_info():
    print("=" * 50)
    print("SYSTEM DETECTION")
    print("=" * 50)
    if IS_MACOS:
        print("  OS: macOS (using AppleScript and Quartz)")
    elif IS_LINUX:
        print("  OS: Linux")
        print(f"  Media control: {'playerctl' if HAS_PLAYERCTL else 'dbus (fallback)'}")
        print(f"  Volume control: {'pactl' if HAS_PACTL else ('amixer' if HAS_AMIXER else 'NOT AVAILABLE')}")
        if not HAS_PLAYERCTL:
            print("  TIP: Install playerctl for better media control: sudo apt install playerctl")
        if not HAS_PACTL and not HAS_AMIXER:
            print("  WARNING: No volume control tool found! Install pulseaudio-utils or alsa-utils")
    elif IS_WINDOWS:
        print(f"  OS: Windows (using pyautogui or pycaw)")
        if not WINDOWS_VOLUME_CONTROL_AVAILABLE:
            print("  TIP: Install comtypes and pycaw for advanced volume control support")
    print("=" * 50)

ser = None

def on_serial_line(line):
    print(f"Received command: {line}")
    execute_command(line)

def main():
    global ser
    print_system_info()

    while True:
        try:
            if ser is None or not ser.is_open:
                print(f"Attempting to connect to {SERIAL_PORT}...")
                ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0)  # non-blocking mode
                print(f"Connected to {SERIAL_PORT}. Listening for commands...")
                # Start background thread to read serial lines
                threading.Thread(target=read_serial_in_background, args=(ser, on_serial_line), daemon=True).start()

            time.sleep(0.5)  # Main thread idle, all work done in background thread

        except serial.SerialException:
            print(f"Connection lost with {SERIAL_PORT}. Retrying in 2s...")
            if ser:
                ser.close()
            ser = None
            time.sleep(2)
        except KeyboardInterrupt:
            print("\nExiting program.")
            if ser:
                ser.close()
            break
        except Exception as e:
            print(f"Unexpected error: {e}")
            time.sleep(1)

if __name__ == "__main__":
    main()
