"""
Touch Gesture Data Collector
-----------------------------
Connects to the ESP32 running touch_data_streamer.ino, and lets you record
labeled "windows" of touch sensor readings -- one window per gesture repetition.
Each window becomes one row in a CSV: label, v0, v1, ..., v(N-1)

Later, this CSV is what you'll load in the training script to build the
tiny neural network.

Install dependencies first:
    pip install pyserial

Usage:
    python collect_touch_data.py --label tap
    python collect_touch_data.py --label hold
    python collect_touch_data.py --label swipe
    python collect_touch_data.py --label idle

Run it once per gesture, and within each run, press Enter repeatedly to
record many repetitions (aim for at least 30-50 reps per gesture for a
usable dataset). Perform the gesture right as you hit Enter, and hold the
motion for about a second -- the window duration below.
"""

import argparse
import csv
import os
import time
import serial

# ---------------- CONFIG ----------------
SERIAL_PORT = "COM5"          # <-- change to your ESP32's port
BAUD_RATE = 115200
SAMPLE_INTERVAL_MS = 20       # must match SAMPLE_INTERVAL_MS in the .ino file
WINDOW_SECONDS = 1.0          # length of each recorded gesture window
OUTPUT_DIR = "touch_data"     # CSVs saved here, one file per label
# -----------------------------------------

WINDOW_SAMPLES = int(WINDOW_SECONDS * 1000 / SAMPLE_INTERVAL_MS)


def read_window(ser):
    """Flush stale data, then block until WINDOW_SAMPLES fresh readings arrive."""
    ser.reset_input_buffer()
    values = []
    while len(values) < WINDOW_SAMPLES:
        line = ser.readline().decode(errors="ignore").strip()
        if line.isdigit() or (line.startswith("-") and line[1:].isdigit()):
            values.append(int(line))
    return values


def main():
    parser = argparse.ArgumentParser(description="Record labeled touch-gesture windows")
    parser.add_argument("--label", required=True, help="Gesture name, e.g. tap / hold / swipe / idle")
    parser.add_argument("--port", default=SERIAL_PORT, help="Serial port for the ESP32")
    args = parser.parse_args()

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    out_path = os.path.join(OUTPUT_DIR, f"{args.label}.csv")
    file_exists = os.path.exists(out_path)

    ser = serial.Serial(args.port, BAUD_RATE, timeout=1)
    time.sleep(2)  # let ESP32 reset after serial connect
    print(f"Connected to {args.port}. Recording gesture: '{args.label}'")
    print(f"Each window = {WINDOW_SAMPLES} samples (~{WINDOW_SECONDS}s).")
    print("Press Enter to record one rep, perform the gesture right after pressing.")
    print("Type 'q' + Enter to stop.\n")

    with open(out_path, "a", newline="") as f:
        writer = csv.writer(f)
        if not file_exists:
            header = ["label"] + [f"v{i}" for i in range(WINDOW_SAMPLES)]
            writer.writerow(header)

        rep_count = 0
        while True:
            cmd = input(f"[{args.label}] Rep {rep_count + 1} -- Enter to record, 'q' to quit: ")
            if cmd.strip().lower() == "q":
                break

            values = read_window(ser)
            writer.writerow([args.label] + values)
            f.flush()
            rep_count += 1
            print(f"  -> recorded ({len(values)} samples)")

    ser.close()
    print(f"\nSaved {rep_count} reps to {out_path}")


if __name__ == "__main__":
    main()
