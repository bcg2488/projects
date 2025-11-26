import serial
import time
import threading

SER_PORT = '/dev/serial0'
BAUD_RATE = 115200

arduino = None
uart_lock = threading.Lock()

#serial connect
def initUart():
    global arduino

    try:
        arduino = serial.Serial(SER_PORT, BAUD_RATE, timeout=0, write_timeout=0, rtscts=False,dsrdtr=False)
        print(f"UART Connected")
    except serial.SerialException as e:
        print(f"Cound't connect to UART due to {e}")


#UART FUNCTIONS FOR USE IN THE CONTROLELR AND FLASKAPP

#uart read - constant, in tread
_read_buffer = b""

def readArduinoData():
    global _read_buffer
    if not arduino:
        return None
    try:
        data = arduino.read(arduino.in_waiting or 1)
        if data:
            _read_buffer += data

            # split out full line into 3 parts and remove garbage data
            if b'\n' in _read_buffer:
                line, _, remainder = _read_buffer.partition(b'\n')
                _read_buffer = remainder
                return line.decode('utf-8', errors='ignore').strip()
    except Exception as e:
        print(f"Failed to read data due to {e}")
        _read_buffer = b""  # reset buffer on error
    return None


# Write command function to arduino, 1 command at a time
def writeArduinoCommmand(command: str, value: str):
    try:
        message = f"{command}:{value}\n"
        arduino.write(message.encode('utf-8'))
        print(f"[pi send to arduino] {message.strip()}")
    except Exception as e:
            print(f"failed to write data due to {e}")
