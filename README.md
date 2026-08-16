# ESP32 Touch Gesture TinyML

A beginner "pure TinyML" project: train a tiny neural network to recognize
touch gestures (tap / hold / swipe / idle) using only the ESP32's built-in
capacitive touch sensor — no camera, no extra hardware. The trained model
runs entirely **on the ESP32 itself**, controlling the onboard LED, with no
laptop needed at runtime.

## How it works

```
Step 1: Collect data   -> touch_data_streamer.ino + collect_touch_data.py
Step 2: Train model    -> train_model.py
Step 3: Quantize/export-> train_model.py (same script, produces model_data.h)
Step 4: Deploy on ESP32-> touch_gesture_inference.ino
Step 5: Test & iterate -> repeat data collection if accuracy is low
```

## Files

| File | Runs on | Purpose |
|---|---|---|
| `touch_data_streamer.ino` | ESP32 | Streams raw touch readings over serial |
| `collect_touch_data.py` | Laptop | Records labeled gesture windows into CSVs |
| `train_model.py` | Laptop | Trains, quantizes, and exports the model as `model_data.h` |
| `touch_gesture_inference.ino` | ESP32 | Runs the trained model on-device, controls the LED |

## Requirements

- ESP32 dev board (any variant with a touch-capable GPIO), USB cable
- Arduino IDE with ESP32 board support installed
- Python 3.8+ on your laptop
- Python packages: `pip install pyserial tensorflow numpy`
- Arduino library: **TensorFlowLite_ESP32** (install via Library Manager, only needed for Step 4)

---

## Step 1 — Collect touch gesture data

**1.1 Flash the streamer sketch**
Upload `touch_data_streamer.ino` to your ESP32. It reads the touch pin
(`T0` = GPIO4 by default — check your board's pinout if using a different pin)
and prints one raw value per line over serial at ~50 samples/second.

**1.2 Record each gesture**
Close the Arduino Serial Monitor first (only one program can hold the port).
Then, from the folder containing `collect_touch_data.py`, run once per
gesture, updating `SERIAL_PORT` in the script to match your ESP32's port
first (Device Manager on Windows, `ls /dev/cu.*` on Mac, `ls /dev/ttyUSB*` on Linux):

```bash
python collect_touch_data.py --label tap
python collect_touch_data.py --label hold
python collect_touch_data.py --label swipe
python collect_touch_data.py --label idle
```

For each run: press Enter, then immediately perform the gesture — the script
captures a 1-second window and appends it as a row to `touch_data/<label>.csv`.
Aim for **30–50 reps per gesture**.

> **Don't skip `idle`.** Without a "nothing happening" class, the model has
> no way to know when *not* to trigger, and will misfire constantly.

---

## Step 2 — Train the tiny model

Make sure `touch_data/` (with all four CSVs) sits next to `train_model.py`,
then run:

```bash
python train_model.py
```

This loads the CSVs, trains a small 2-layer neural network, and prints a
test accuracy. **If accuracy is below ~70%**, the fix is almost always more
reps, or making the physical gestures more distinct from each other —
re-run Step 1 rather than tweaking the model.

---

## Step 3 — Quantize and export

This happens automatically as part of `train_model.py` — no separate step
needed. It produces two output files:

- `model.tflite` — the quantized model (kept for reference)
- `model_data.h` — the same model as a C byte array, ready to embed in Arduino

---

## Step 4 — Deploy on the ESP32

**4.1 Install the library**
In Arduino IDE, Library Manager → search **"TensorFlowLite ESP32"** → install.

**4.2 Copy the model header**
Copy `model_data.h` into the same folder as `touch_gesture_inference.ino`.

**4.3 Upload**
Flash `touch_gesture_inference.ino` to the ESP32. Open Serial Monitor at
115200 baud.

**4.4 Test**
Touch the pin the same way you did during data collection. You should see
live predictions printed (e.g. `Predicted: hold  (confidence: 0.87)`), and
the onboard LED should turn on for `hold` and off for `tap`.

> If the sketch doesn't compile: TFLite Micro Arduino libraries occasionally
> change class/header names between versions. Check the examples bundled
> with the installed library and adjust the `#include` lines to match — the
> overall structure (load model → allocate tensors → quantize input →
> invoke → dequantize output) won't need to change.

> If `AllocateTensors()` fails at runtime: increase `kTensorArenaSize` in
> the sketch (try doubling it) and re-upload.

---

## Step 5 — Test, iterate, and extend

- **Real-world accuracy feels worse than the printed test accuracy?** Normal —
  it usually means your live touch technique drifted from how you did it
  during collection. Collect more varied reps (different pressure, speed,
  angle) and re-run Steps 2–4.
- **Want to swap gestures for something else?** Just re-record `touch_data/`
  with new labels and re-run Steps 2–4 — nothing else changes.
- **Want to move beyond a touch sensor?** The exact same pipeline (collect →
  train → quantize → deploy) works with an accelerometer (e.g. MPU6050) if
  you get one later — only the sensor-reading code and input shape change.
- **Ready to control a real bulb instead of the onboard LED?** Wire a relay
  module to a free GPIO and replace the `digitalWrite(LED_PIN, ...)` calls
  in `touch_gesture_inference.ino` with the same logic on the relay pin.