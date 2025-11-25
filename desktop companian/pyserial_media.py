import serial
import pyautogui
import time

# --- CONFIGURATION ---
SERIAL_PORT = '/dev/ttyUSB0'  # Make sure this matches your ESP
BAUD_RATE = 115200

def listen_for_commands():
    ser = None
    print(f"Attempting to connect to {SERIAL_PORT}...")
    
    while True:
        try:
            # 1. Connection Logic
            if ser is None:
                ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
                ser.flush()
                print(f"Successfully connected to {SERIAL_PORT}.")
                print("Listening for gestures. Press Ctrl+C to exit.")

            # 2. Read Data
            if ser.in_waiting > 0:
                # FIX: errors='ignore' prevents crash on startup garbage data
                line = ser.readline().decode('utf-8', errors='ignore').rstrip()
                
                if line:
                    print(f"Received command: {line}")
                    execute_command(line)

        except serial.SerialException:
            # Simple disconnect handler
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

def execute_command(command):
    # Added a small guard to prevent executing empty or garbage strings
    if not command or len(command) < 2:
        return

    if command == "PLAY_PAUSE":
        pyautogui.press('playpause')
    elif command == "NEXT":
        pyautogui.press('nexttrack')
    elif command == "PREV":
        pyautogui.press('prevtrack')
    elif command == "VOL_UP":
        pyautogui.press('volumeup')
    elif command == "VOL_DOWN":
        pyautogui.press('volumedown')
    else:
        print(f"Ignored unknown command: {command}")

if __name__ == "__main__":
    listen_for_commands()


    # xhost +SI:localuser:$(whoami)