#include <Arduino.h>

const int BUTTON_PIN = 18;
const int LED_PIN = 4;
void setup()
{
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP); 
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Stage 1: press the button");
}

void loop()
{
  bool pressed = (digitalRead(BUTTON_PIN) == LOW);
  digitalWrite(LED_PIN, pressed ? HIGH : LOW);

  // Print only when the state changes, not on every loop
  static bool lastPressed = false;
  if (pressed != lastPressed)
  {
    lastPressed = pressed;
    Serial.println(pressed ? "pressed" : "released");
  }

  delay(10);
}