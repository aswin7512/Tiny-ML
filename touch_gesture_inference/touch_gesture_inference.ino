/*
  Touch Gesture TinyML Inference
  --------------------------------
  Runs the trained model.tflite (embedded via model_data.h) directly on the
  ESP32. Reads the touch pin, buffers a window of readings, feeds it through
  the model, and toggles the onboard LED based on the predicted gesture.
  No laptop involved at runtime -- this is the "pure TinyML" step.

  Setup:
    1. Install the "TensorFlowLite_ESP32" library via Arduino Library
       Manager (search "TensorFlowLite ESP32"). If your installed version
       exposes slightly different class/header names, check the examples
       bundled with the library and adjust the includes below to match --
       the overall structure (load model -> allocate tensors -> quantize
       input -> invoke -> dequantize output) stays the same either way.
    2. Copy model_data.h (produced by train_model.py) into this sketch's
       folder, next to this .ino file.
    3. Upload to the ESP32.

  Touch pin must match the one used in touch_data_streamer.ino during
  data collection (T0 = GPIO4 by default).
*/

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "model_data.h"

#define TOUCH_PIN T0
const int LED_PIN = 2;               // onboard LED on most ESP32 dev boards
const int SAMPLE_INTERVAL_MS = 20;   // must match the value used during data collection

// Tensor arena: scratch RAM the model uses while running.
// If AllocateTensors() fails below, try increasing this.
constexpr int kTensorArenaSize = 8 * 1024;
uint8_t tensor_arena[kTensorArenaSize];

tflite::MicroErrorReporter micro_error_reporter;
tflite::ErrorReporter* error_reporter = &micro_error_reporter;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

float touch_window[MODEL_INPUT_SIZE];
int window_index = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  model = tflite::GetModel(g_model);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema version mismatch -- re-export model_data.h");
    while (1) { delay(1000); }
  }

  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("AllocateTensors() failed -- try increasing kTensorArenaSize");
    while (1) { delay(1000); }
  }

  input = interpreter->input(0);
  output = interpreter->output(0);

  Serial.println("Model loaded. Starting inference...");
}

void loop() {
  int raw = touchRead(TOUCH_PIN);
  float normalized = (raw - MODEL_NORM_MIN) / (MODEL_NORM_MAX - MODEL_NORM_MIN);
  touch_window[window_index++] = normalized;

  if (window_index >= MODEL_INPUT_SIZE) {
    window_index = 0;
    runInference();
  }

  delay(SAMPLE_INTERVAL_MS);
}

void runInference() {
  // Quantize float input into the model's expected int8 range
  float input_scale = input->params.scale;
  int input_zero_point = input->params.zero_point;
  for (int i = 0; i < MODEL_INPUT_SIZE; i++) {
    int q = (int)roundf(touch_window[i] / input_scale) + input_zero_point;
    if (q < -128) q = -128;
    if (q > 127) q = 127;
    input->data.int8[i] = (int8_t)q;
  }

  if (interpreter->Invoke() != kTfLiteOk) {
    Serial.println("Invoke() failed");
    return;
  }

  // Dequantize output scores and pick the highest
  float output_scale = output->params.scale;
  int output_zero_point = output->params.zero_point;
  int best_class = 0;
  float best_score = -1e9;
  for (int i = 0; i < MODEL_NUM_CLASSES; i++) {
    float score = (output->data.int8[i] - output_zero_point) * output_scale;
    if (score > best_score) {
      best_score = score;
      best_class = i;
    }
  }

  Serial.print("Predicted: ");
  Serial.print(MODEL_LABELS[best_class]);
  Serial.print("  (confidence: ");
  Serial.print(best_score, 2);
  Serial.println(")");

  // Map predicted gesture to LED state -- adjust label names to match yours
  if (strcmp(MODEL_LABELS[best_class], "hold") == 0) {
    digitalWrite(LED_PIN, HIGH);
  } else if (strcmp(MODEL_LABELS[best_class], "tap") == 0) {
    digitalWrite(LED_PIN, LOW);
  }
  // "swipe" and "idle" leave the LED state unchanged in this example
}
