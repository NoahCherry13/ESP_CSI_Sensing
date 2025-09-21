import serial
import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# --- CONFIGURATION ---
# IMPORTANT: Change this to the serial port of your ESP32-S2
SERIAL_PORT = 'COM10'  # For Linux/macOS. For Windows, it might be 'COM3', 'COM4', etc.
BAUD_RATE = 115200

# The ESP32 sends CSI data for 52 subcarriers in a 20MHz channel (HT-LTF format)
# Each subcarrier has an I and a Q value (2 values total).
EXPECTED_VALUES = 128

# --- Plot Setup ---
fig, (ax_amp, ax_phase) = plt.subplots(2, 1, figsize=(10, 8))
x_data = np.arange(EXPECTED_VALUES)

# Amplitude Plot
line_amp, = ax_amp.plot(x_data, np.zeros(EXPECTED_VALUES), 'b-')
ax_amp.set_title('CSI Amplitude')
ax_amp.set_xlabel('Subcarrier Index')
ax_amp.set_ylabel('Amplitude')
ax_amp.set_ylim(0, 100)
ax_amp.grid(True)

# Phase Plot
line_phase, = ax_phase.plot(x_data, np.zeros(EXPECTED_VALUES), 'r-')
ax_phase.set_title('CSI Phase')
ax_phase.set_xlabel('Subcarrier Index')
ax_phase.set_ylabel('Phase (radians)')
ax_phase.set_ylim(-np.pi, np.pi)
ax_phase.grid(True)

fig.tight_layout()

# --- Serial Port Connection ---
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print(f"Connected to serial port {SERIAL_PORT}")
except serial.SerialException as e:
    print(f"Error: Could not open serial port {SERIAL_PORT}. {e}")
    print("Please make sure you have the correct port name and the device is connected.")
    exit()

def update(frame):
    """
    This function is called by FuncAnimation for each new frame.
    It reads a line from the serial port, parses it, and updates the plot.
    """
    try:
        line = ser.readline().decode('utf-8').strip()

        # Look for lines that start with our CSI data prefix
        if line.startswith('CSI_DATA,['):
            # Use regex to find all numbers within the brackets
            match = re.findall(r'\-?[0-9]+', line)
            if not match:
                return line_amp, line_phase

            # Split the string of numbers by commas and convert to integers
            csi_values = list(map(int, match))
            # We expect an even number of values (I and Q pairs)
            if len(csi_values) < EXPECTED_VALUES:
                print(f"Warning: Received incomplete CSI data. Expected {EXPECTED_VALUES}, got {len(csi_values)}")
                return line_amp, line_phase

            # Separate I and Q values
            # The ESP32 sends them interleaved: [I0, Q0, I1, Q1, ...]
            csi_i = np.array(csi_values[0::2])
            csi_q = np.array(csi_values[1::2])

            # Calculate amplitude and phase
            amplitude = np.sqrt(csi_i**2 + csi_q**2)
            phase = np.arctan2(csi_q, csi_i)
            print(amplitude)
            print(phase)
            # Update plot data
            line_amp.set_ydata(amplitude)
            line_phase.set_ydata(phase)
            
            # Adjust y-axis for amplitude automatically
            ax_amp.set_ylim(0, np.max(amplitude) * 1.2)


    except Exception as e:
        print(f"An error occurred: {e}")

    return line_amp, line_phase


def on_close(event):
    """
    This function is called when the plot window is closed.
    """
    print("Plot window closed. Closing serial port.")
    ser.close()

# --- Main Execution ---
if __name__ == '__main__':
    # Register the close event handler
    fig.canvas.mpl_connect('close_event', on_close)

    # Create the animation
    ani = FuncAnimation(fig, update, blit=True, interval=10, cache_frame_data=False)

    print("Starting plot. Close the plot window to stop the script.")
    plt.show()

    # The script will block here until the plot window is closed.
    # The on_close function will handle closing the serial port.
