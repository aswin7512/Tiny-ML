/*
  Touch Sensor Data Streamer
  ---------------------------
  Continuously reads the ESP32's built-in capacitive touch pin and prints
  one raw value per line over serial. Used as the data source for
  collect_touch_data.py while building a TinyML gesture dataset.

  Common touch-capable pins on most ESP32 dev boards:
    T0 = GPIO4   T3 = GPIO15   T6 = GPIO14
    T2 = GPIO2   T4 = GPIO13   T7 = GPIO27
  (Check your specific board's pinout diagram if unsure.)

  Lower readings = finger touching / closer. Higher readings = not touching.
  (This is the opposite of what you might expect -- capacitance increases
  when touched, which lowers the reading.)
*/

#define TOUCH_PIN T0          // change if you're using a different pin
const int SAMPLE_INTERVAL_MS = 20;   // ~50 samples/second

void setup() {
  Serial.begin(115200);
  delay(500);
}

void loop() {
  int value = touchRead(TOUCH_PIN);
  Serial.println(value);
  delay(SAMPLE_INTERVAL_MS);
}
