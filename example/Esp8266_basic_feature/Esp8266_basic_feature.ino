#include <BigThingArdu.h>

#include "secret.h"

int return_int() { return 111; }

double return_double() { return 111.111; }

bool return_bool() { return true; }

char ssid[] = SSID;
char password[] = PASSWORD;
char mqtt_server[] = "iotdev.snu.ac.kr";
int mqtt_port = 41083;

BigThingArdu thing("Button", 10);
SoPValue value_return_int((const char*)"return_int_value", return_int, 0, 1000,
                          1000);
SoPValue value_return_double((const char*)"return_double_value", return_double,
                             0, 1000.0, 1000);
// SoPValue value_return_bool((const char*)"return_bool_value", return_bool, 0,
// 2,
//                            1000);
SoPFunction function_return_int((const char*)"return_int_function", return_int);
SoPFunction function_return_double((const char*)"return_double_function",
                                   return_double);
// SoPFunction function_return_bool((const char*)"return_bool_function",
//                                  return_bool);

void setup() {
  // Setup serial communication
  Serial.begin(9600);
  thing.SetupWifi(ssid, password);

  thing.Add(value_return_int);
  thing.Add(value_return_double);
  // thing.Add(value_return_bool);
  thing.Add(function_return_int);
  thing.Add(function_return_double);
  // thing.Add(function_return_bool);

  thing.Setup(mqtt_server, mqtt_port);
}

void loop() { thing.Loop(); }