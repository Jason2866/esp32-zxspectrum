import serial
import serial.tools.list_ports
from pynput import keyboard

# Mapping from pynput keys to SpecKeys enumeration values
key_map = {
    '1': 1, '2': 2, '3': 3, '4': 4, '5': 5,
    '6': 6, '7': 7, '8': 8, '9': 9, '0': 10,
    'q': 11, 'w': 12, 'e': 13, 'r': 14, 't': 15,
    'y': 16, 'u': 17, 'i': 18, 'o': 19, 'p': 20,
    'a': 21, 's': 22, 'd': 23, 'f': 24, 'g': 25,
    'h': 26, 'j': 27, 'k': 28, 'l': 29, 'enter': 30,
    'shift': 31, 'z': 32, 'x': 33, 'c': 34, 'v': 35,
    'b': 36, 'n': 37, 'm': 38, 'space': 40
    # Add any additional keys you need to map
}

def get_key_value(key):
    try:
        return key_map[key.char]
    except AttributeError:
        # Handle special keys
        if key == keyboard.Key.enter:
            return key_map['enter']
        elif key == keyboard.Key.space:
            return key_map['space']
        elif key == keyboard.Key.shift:
            return key_map['shift']
        # Add additional special key handling here
    except KeyError:
        # If the key is not in our mapping, return None
        return None


def list_serial_ports():
    ports = serial.tools.list_ports.comports()
    return ports

def select_serial_port():
    ports = list_serial_ports()
    for index, port in enumerate(ports):
        print(f"{index}: {port.device} - {port.description}")
    choice = input("Select the port index (refresh with 'r'): ")
    if choice.lower() == 'r':
        return select_serial_port()
    elif choice.isdigit() and 0 <= int(choice) < len(ports):
        return ports[int(choice)].device
    else:
        print("Invalid selection.")
        return select_serial_port()

def on_press(key):
    key_value = get_key_value(key)
    if key_value is not None:
        msg = f"down:{key_value}\n"
        print(msg)
        ser.write(msg.encode())

def on_release(key):
    key_value = get_key_value(key)
    if key_value is not None:
        msg = f"up:{key_value}\n"
        print(msg)
        ser.write(msg.encode())

if __name__ == "__main__":
    port = select_serial_port()
    if port:
        ser = serial.Serial(port, 115200, timeout=1)
        print(f"Connected to {port}. Listening for keyboard events...")

        # Set up the listener for keyboard events
        listener = keyboard.Listener(on_press=on_press, on_release=on_release)
        listener.start()

        try:
            while listener.running:
                if ser.in_waiting:
                    incoming_data = ser.readline().decode().strip()
                    print(f"Received: {incoming_data}")
        except KeyboardInterrupt:
            print("Program exited.")
        finally:
            ser.close()
            listener.stop()