#include <BigThingArdu.h>

#include "secret.h"

#define WINDOW_SIZE 3

const int Kbutton = 16;

int on_click() {
  static int button_window[WINDOW_SIZE] = {0};
  static int window_index = 0;

  if (window_index >= WINDOW_SIZE) {
    window_index = 0;
  }

  button_window[window_index++] = digitalRead(Kbutton);

  for (int i = 0; i < WINDOW_SIZE; i++) {
    if (button_window[i] == 1) {
      return 1;
    }
  }
  return 0;
}

int get_on_click() { return on_click(); }

char ssid[] = SSID;
char password[] = PASSWORD;
char mqtt_server[] = "iotdev.snu.ac.kr";
int mqtt_port = 41083;

BigThingArdu thing("Button", 10);
SoPValue value_on_click((const char*)"on_click", on_click, 0, 250, 100);
SoPFunction function_get_on_click((const char*)"get_on_click", get_on_click);

void setup() {
  // Setup serial communication
  Serial.begin(9600);

  // set I/O pins
  pinMode(Kbutton, INPUT);

  thing.SetupWifi(ssid, password);

  thing.Add(value_on_click);
  thing.Add(function_get_on_click);
  thing.Setup(mqtt_server, mqtt_port);
}

void loop() { thing.Loop(); }