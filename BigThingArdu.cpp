#include <ArduinoJson.h>
#include <BigThingArdu.h>
#include <SPI.h>

bool g_registered = false;
String g_execution_request = "null";

static void callback(String& topic, String& payload) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String payload_temp = payload;

  Serial.println(payload);

  if (strncmp(topic.c_str(), "MT/RESULT/REGISTER", 18) == 0) {
    Serial.print("Register Result: ");
    if (payload_temp == "success") {
      Serial.println("success");
      digitalWrite(13, HIGH);
    } else if (payload_temp == "fail") {
      Serial.println("fail");
      digitalWrite(13, LOW);
    }
    g_registered = true;
  } else if (strncmp(topic.c_str(), "MT/EXECUTE", 10) == 0) {
    Serial.print("Execute ");
    Serial.println(topic);

    char* p_tok = NULL;

    char topic_buf[256] = {0};
    topic.toCharArray(topic_buf, topic.length());
    strtok_r((char*)topic_buf, "/", &p_tok);  // skip 1
    strtok_r(NULL, "/", &p_tok);              // skip 2
    char* function_name = strtok_r(NULL, "/", &p_tok);
    char* thing_name = strtok_r(NULL, "/", &p_tok);
    char* middleware_name = strtok_r(NULL, "/", &p_tok);
    char* requester_name = strtok_r(NULL, "/", &p_tok);

    DynamicJsonDocument doc(128);
    deserializeJson(doc, payload_temp.c_str());

    const char* scenario_name = doc["scenario"];

    // debug
    Serial.print("scenario_name: ");
    Serial.print(scenario_name);
    Serial.print(", function_name: ");
    Serial.print(function_name);
    Serial.print(", thing_name: ");
    Serial.print(thing_name);
    Serial.print(", middleware_name: ");
    Serial.print(middleware_name);
    Serial.print(", requester_name: ");
    Serial.println(requester_name);

    g_execution_request = String(scenario_name) + ":" + String(function_name) +
                          "#" + String(thing_name) + "#" +
                          String(middleware_name) + "#" +
                          String(requester_name) + "#";
  }
}

Tag::Tag(const char* name) { this->name_ = String(name); }
Tag::~Tag() {}

SoPFunction::SoPFunction(const char* name, VoidFunction func) {
  this->name_ = String(name);
  this->return_type_ = VOID;

  this->num_tags_ = 0;
  this->function_tags_ = (Tag**)malloc(sizeof(Tag*));
  this->callback_function_ = (void*)func;
}

SoPFunction::SoPFunction(const char* name, IntegerFunction func) {
  this->name_ = String(name);
  this->return_type_ = INTEGER;

  this->num_tags_ = 0;
  this->function_tags_ = (Tag**)malloc(sizeof(Tag*));
  this->callback_function_ = (void*)func;
}

SoPFunction::SoPFunction(const char* name, DoubleFunction func) {
  this->name_ = String(name);
  this->return_type_ = DOUBLE;

  this->num_tags_ = 0;
  this->function_tags_ = (Tag**)malloc(sizeof(Tag*));
  this->callback_function_ = (void*)func;
}

SoPFunction::SoPFunction(const char* name, BoolFunction func) {
  this->name_ = String(name);
  this->return_type_ = BOOL;

  this->num_tags_ = 0;
  this->function_tags_ = (Tag**)malloc(sizeof(Tag*));
  this->callback_function_ = (void*)func;
}

SoPFunction::~SoPFunction() {
  if (this->callback_function_) free(this->callback_function_);
}

void SoPFunction::AddTag(Tag& function_tag) {
  this->function_tags_ = (Tag**)realloc(this->function_tags_,
                                        sizeof(Tag*) * (this->num_tags_ + 1));
  this->function_tags_[this->num_tags_] = &function_tag;
  this->num_tags_++;
}

void SoPFunction::AddTag(const char* tag_name) {
  Tag* tag = new Tag(tag_name);
  this->AddTag(*tag);
}

int SoPFunction::Execute(DynamicJsonDocument* p_doc) {
  switch (this->return_type_) {
    case INTEGER: {
      (*p_doc)["return_type"] = "int";
      (*p_doc)["return_value"] = ((IntegerValue)this->callback_function_)();
      // *(int*)this->return_value_ =
      // ((IntegerValue)this->callback_function_)();
      break;
    }
    case DOUBLE: {
      (*p_doc)["return_type"] = "double";
      (*p_doc)["return_value"] = ((DoubleValue)this->callback_function_)();
      break;
    }
    case BOOL: {
      (*p_doc)["return_type"] = "bool";
      (*p_doc)["return_value"] = ((BoolValue)this->callback_function_)();
      break;
    }
    case STRING: {
      Serial.println(("[ERROR] string is not supported now"));
      return -6;
      break;
    }
    case VOID:
    case UNDEFINED:
      Serial.println(("[ERROR] not allowed value type"));
      return -1;
      break;
  }

  return 0;
}

SoPValue::SoPValue(const char* name, BoolValue value, int publish_cycle) {
  this->name_ = String(name);
  this->value_type_ = BOOL;

  this->callback_function_ = (void*)value;

  this->prev_value_ = malloc(sizeof(int));
  *(int*)prev_value_ = -1;

  *(int*)this->max_ = 1;
  *(int*)this->min_ = 0;
  this->publish_cycle_ = publish_cycle;
  this->last_publish_time_ = 0;
}

SoPValue::SoPValue(const char* name, IntegerValue value, int min, int max,
                   int publish_cycle) {
  this->name_ = String(name);
  this->value_type_ = INTEGER;

  this->callback_function_ = (void*)value;

  this->prev_value_ = malloc(sizeof(int));
  *(int*)prev_value_ = 0;
  this->new_value_ = malloc(sizeof(int));
  *(int*)new_value_ = 0;

  if (!this->max_) this->max_ = malloc(sizeof(int));
  if (!this->min_) this->min_ = malloc(sizeof(int));

  *(int*)this->max_ = max;
  *(int*)this->min_ = min;
  this->publish_cycle_ = publish_cycle;
  this->last_publish_time_ = 0;
}

SoPValue::SoPValue(const char* name, StringValue value, int min, int max,
                   int publish_cycle) {
  this->name_ = String(name);
  this->value_type_ = STRING;

  this->callback_function_ = (void*)value;

  this->publish_cycle_ = publish_cycle;
  this->last_publish_time_ = 0;
}

SoPValue::SoPValue(const char* name, DoubleValue value, double min, double max,
                   int publish_cycle) {
  this->name_ = String(name);
  this->value_type_ = DOUBLE;

  this->callback_function_ = (void*)value;

  this->prev_value_ = malloc(sizeof(double));
  *(int*)prev_value_ = 0;
  this->new_value_ = malloc(sizeof(double));
  *(int*)new_value_ = 0;

  if (!this->max_) this->max_ = malloc(sizeof(double));
  if (!this->min_) this->min_ = malloc(sizeof(double));

  *(double*)this->max_ = max;
  *(double*)this->min_ = min;
  this->publish_cycle_ = publish_cycle;
  this->last_publish_time_ = 0;
}

SoPValue::~SoPValue() {
  if (this->callback_function_) free(this->callback_function_);
  if (this->min_) free(this->min_);
  if (this->max_) free(this->max_);
  if (this->prev_value_) free(this->prev_value_);
}

void SoPValue::AddTag(Tag& value_tag) {
  this->value_tags_ =
      (Tag**)realloc(this->value_tags_, sizeof(Tag*) * (this->num_tags_ + 1));
  this->value_tags_[this->num_tags_] = &value_tag;
  this->num_tags_++;
}

void SoPValue::AddTag(const char* tag_name) {
  Tag* tag = new Tag(tag_name);
  this->AddTag(*tag);
}

String SoPValue::Fetch() {
  char buffer[MAX_BUFFER_SIZE];

  switch (this->value_type_) {
    case INTEGER: {
      *(int*)this->new_value_ = ((IntegerValue)this->callback_function_)();
      snprintf(buffer, MAX_BUFFER_SIZE,
               "{\"type\" : \"int\" , \"value\" : %d}\n",
               *(int*)this->new_value_);
      break;
    }
    case DOUBLE: {
      *(double*)this->new_value_ = ((DoubleValue)this->callback_function_)();
      snprintf(buffer, MAX_BUFFER_SIZE,
               "{\"type\" : \"double\" , \"value\" : %lf}\n",
               *(double*)this->new_value_);
      break;
    }
    case BOOL: {
      *(bool*)this->new_value_ = ((BoolValue)this->callback_function_)();
      snprintf(buffer, MAX_BUFFER_SIZE,
               "{\"type\" : \"bool\" , \"value\" : %d}\n",
               *(bool*)this->new_value_);
      break;
    }
    case STRING: {
      Serial.println(("[ERROR] string is not supported now"));
      break;
    }
    case VOID:
    case UNDEFINED:
      Serial.println(("[ERROR] not allowed value type"));
      break;
  }

  return String(buffer);
}

void BigThingArdu::Reconnect() {
  while (!this->mqtt_client_.connected()) {
    // 앞서 설정한 클라이언트 ID로 연결합니다.
    if (this->mqtt_client_.connect(this->client_id_.c_str())) {
      Serial.println("Connected to MQTT broker");
      break;
    } else {
      delay(5000);
      Serial.println("Reconnect...");
    }
  }

  this->InitSubscribe();
}

void BigThingArdu::InitSubscribe() {
  String register_result_topic =
      String("MT/RESULT/REGISTER/") + this->client_id_;
  Serial.println(String("Subscribe ") + register_result_topic);
  this->mqtt_client_.subscribe(register_result_topic);
  for (int i = 0; i < this->num_functions_; i++) {
    SoPFunction* f = this->functions_[i];
    String execute_topic = String("MT/EXECUTE/") + f->name_ + String("/") +
                           this->client_id_ + String("/#");
    Serial.println(String("Subscribe ") + execute_topic);
    this->mqtt_client_.subscribe(execute_topic);
  }
}

static bool CheckPublishCyclePassed(SoPValue* v) {
  bool passed = false;
  unsigned long curr_time = 0;
  unsigned long diff_time = 0;

  // set true for the initial case
  if (v->last_publish_time_ == 0) {
    v->last_publish_time_ = millis();
    return true;
  }

  curr_time = millis();

  diff_time = curr_time - v->last_publish_time_;

  // millis() goes back to zero after approximately 50 days
  if ((diff_time < 0) || (diff_time >= (unsigned long)v->publish_cycle_)) {
    v->last_publish_time_ = millis();
    passed = true;
  } else {
    passed = false;
  }

  return passed;
}

bool BigThingArdu::CheckAliveCyclePassed() {
  bool passed = false;
  unsigned long curr_time = 0;
  unsigned long diff_time = 0;

  // set true for the initial case
  if (this->last_alive_time_ == 0) {
    this->last_alive_time_ = millis();
    return true;
  }

  curr_time = millis();

  diff_time = curr_time - this->last_alive_time_;

  // millis() goes back to zero after approximately 50 days
  if ((diff_time < 0) ||
      (diff_time >= (unsigned long)this->alive_cycle_ / 2 * 1000)) {
    this->last_alive_time_ = millis();
    passed = true;
  } else {
    passed = false;
  }

  return passed;
}

void BigThingArdu::SendAliveMessage() {
  if (this->CheckAliveCyclePassed()) {
    String topic = "TM/ALIVE/" + this->client_id_;
    String payload = "alive";

    this->mqtt_client_.publish(topic.c_str(), payload.c_str());
    Serial.print(("Publish "));
    Serial.println(topic.c_str());
    Serial.println(payload.c_str());
  } else {
    // pass
  }
}

void BigThingArdu::SendValue() {
  for (uint8_t i = 0; i < this->num_values_; i++) {
    SoPValue* v = this->values_[i];

    if (CheckPublishCyclePassed(v)) {
      String topic = this->client_id_ + "/" + v->name_;
      String payload = v->Fetch();

      this->mqtt_client_.publish(topic.c_str(), payload.c_str());
      Serial.print(("Publish "));
      Serial.println(topic.c_str());
      Serial.println(payload.c_str());
    } else {
      // pass
    }
  }
}

void BigThingArdu::SendFunctionResult() {
  int err = 0;
  char* p_tok = NULL;

  if (g_execution_request == "null") return;

  char* scenario_name =
      strtok_r((char*)g_execution_request.c_str(), ":", &p_tok);
  char* function_name = strtok_r(NULL, "#", &p_tok);
  char* thing_name = strtok_r(NULL, "#", &p_tok);
  char* middleware_name = strtok_r(NULL, "#", &p_tok);
  char* requester_name = strtok_r(NULL, "#", &p_tok);

  // Serial.println("SendFunctionResult");
  // Serial.print(("scenario_name "));
  // Serial.println(scenario_name);
  // Serial.print(("thing_name "));
  // Serial.println(thing_name);
  // Serial.print(("function_name "));
  // Serial.println(function_name);
  // Serial.print(("middleware_name "));
  // Serial.println(middleware_name);
  // Serial.print(("requester_name "));
  // Serial.println(requester_name);

  for (uint8_t i = 0; i < this->num_functions_; i++) {
    SoPFunction* f = this->functions_[i];

    if (String(function_name) == f->name_) {
      DynamicJsonDocument doc(1024);
      char json_string[512];

      err = f->Execute(&doc);

      doc["scenario"] = scenario_name;
      doc["error"] = err;

      serializeJson(doc, json_string);

      String topic = "TM/RESULT/EXECUTE/" + f->name_ + "/" + this->client_id_;
      String payload = String(json_string);

      this->mqtt_client_.publish(topic.c_str(), payload.c_str());
      Serial.print(("Publish "));
      Serial.println(topic.c_str());
      Serial.println(payload.c_str());
    }
  }

  g_execution_request = "null";
}

void BigThingArdu::Register() {
  DynamicJsonDocument doc(MAX_MQTT_PAYLOAD_SIZE);
  char json_string[MAX_MQTT_PAYLOAD_SIZE];

  if (g_registered) return;

  doc["alive_cycle"] = this->alive_cycle_;
  doc.createNestedArray("values");
  doc.createNestedArray("functions");

  for (uint8_t i = 0; i < this->num_values_; i++) {
    SoPValue* v = this->values_[i];

    doc["values"][i]["name"] = v->name_.c_str();
    doc["values"][i]["description"] = v->name_.c_str();

    switch (v->value_type_) {
      case INTEGER:
        doc["values"][i]["type"] = "int";
        doc["values"][i]["bound"]["min_value"] = (int)(*(int*)v->min_);
        doc["values"][i]["bound"]["max_value"] = (int)(*(int*)v->max_);
        break;
      case DOUBLE:
        doc["values"][i]["type"] = "double";
        doc["values"][i]["bound"]["min_value"] = (double)(*(double*)v->min_);
        doc["values"][i]["bound"]["max_value"] = (double)(*(double*)v->max_);
        break;
      case BOOL:
        doc["values"][i]["type"] = "bool";
        doc["values"][i]["bound"]["min_value"] = 0;
        doc["values"][i]["bound"]["max_value"] = 1;
        break;
      case STRING:
      default:
        break;
    }

    for (int j = 0; j < v->num_tags_; j++) {
      doc["values"][i]["tags"][j]["name"] = v->value_tags_[j]->name_;
    }
  }

  for (uint8_t i = 0; i < this->num_functions_; i++) {
    SoPFunction* f = this->functions_[i];

    doc["functions"][i]["name"] = f->name_.c_str();
    doc["functions"][i]["description"] = f->name_.c_str();
    doc["functions"][i]["use_arg"] = 0;

    switch (f->return_type_) {
      case INTEGER:
        doc["functions"][i]["return_type"] = "int";
        break;
      case DOUBLE:
        doc["functions"][i]["return_type"] = "double";
        break;
      case BOOL:
        doc["functions"][i]["return_type"] = "bool";
        break;
      case STRING:
      default:
        break;
    }

    for (int j = 0; j < f->num_tags_; j++) {
      doc["functions"][i]["tags"][j]["name"] = f->function_tags_[j]->name_;
    }
  }

  serializeJson(doc, json_string);

  String topic = "TM/REGISTER/" + this->client_id_;
  String payload = String(json_string);

  int ret = this->mqtt_client_.publish(topic, payload);
  // int ret = this->mqtt_client_.publish("/hello", "world");
  Serial.print(("Publish "));
  Serial.println((ret));
  Serial.println(topic.c_str());
  Serial.println(payload.c_str());

  delay(3000);
}

void BigThingArdu::Setup(const char* broker_ip, int broker_port) {
  this->mqtt_client_.begin(broker_ip, broker_port, this->wifi_client_);
  this->mqtt_client_.onMessage(callback);

  while (!this->mqtt_client_.connected()) {
    // 앞서 설정한 클라이언트 ID로 연결합니다.
    if (this->mqtt_client_.connect(this->client_id_.c_str())) {
      Serial.println("Connected to MQTT broker");
      break;
    } else {
      delay(5000);
      Serial.println("Reconnect...");
    }
  }

  this->InitSubscribe();
}

#if defined(ARDUINO_ARCH_ESP8266)
#pragma message "ESP8266 version SetupWifi is defined"

void BigThingArdu::SetupWifi(const char* ssid, const char* password) {
  this->client_id_ += String(random(0xffff), HEX);
  delay(1000);

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.print(this->wifi_client_.connected());
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

#elif defined(ARDUINO_SAMD_MKRWIFI1010) ||                                     \
    defined(ARDUINO_SAMD_MKRVIDOR4000) || defined(ARDUINO_SAMD_NANO_33_IOT) || \
    defined(ARDUINO_SAMD_NANO_RP2040_CONNECT) || defined(ARDUINO_ARCH_ESP32)
#include <WiFiNINA.h>
#pragma message "SAMD & ESP32 version SetupWifi is defined"

void BigThingArdu::SetupWifi(const char* ssid, const char* password) {
  this->client_id_ += String(random(0xffff), HEX);
  delay(1000);

  String fv = WiFi.firmwareVersion();
  if (fv != "1.1.0") {
    Serial.println("Please upgrade the firmware");
  }

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("Connected to WiFi");
  printWifiStatus();
}

#endif

void BigThingArdu::Loop() {
  if (!this->mqtt_client_.connected()) {
    this->Reconnect();
  }

  this->mqtt_client_.loop();

#if defined(ARDUINO_ARCH_ESP8266)
  delay(10);
#endif

  if (g_registered) {
    this->SendFunctionResult();
    this->SendAliveMessage();
    this->SendValue();
  } else {
    this->Register();
  }

  // long now = millis();
  // if (now - this->lastMsg > 1000) {
  //   this->lastMsg = now;

  //   if (g_registered) {
  //     this->SendAliveMessage();
  //     this->SendValue();
  //   } else {
  //     this->Register();
  //   }
  // }

  // delay(1000);
  // ESP.deepSleep(5e6);
}

void BigThingArdu::Add(SoPValue& v) {
  if (this->num_values_ < MAX_VALUE_NUM) {
    v.AddTag(this->client_id_.c_str());
    this->values_[this->num_values_++] = &v;
    Serial.print(v.name_.c_str());
    Serial.println((" Value is added"));
  } else {
    Serial.println(("It cannot be added (more than limit)"));
  }
}

void BigThingArdu::Add(SoPFunction& f) {
  if (this->num_functions_ < MAX_FUNCTION_NUM) {
    f.AddTag(this->client_id_.c_str());
    this->functions_[this->num_functions_++] = &f;
    Serial.print(f.name_.c_str());
    Serial.println((" Function is added"));
  } else {
    Serial.println(("It cannot be added (more than limit)"));
  }
}

BigThingArdu::BigThingArdu(const char* client_id, int alive_cycle) {
  this->client_id_ = String(client_id);
  this->alive_cycle_ = alive_cycle;

  this->num_values_ = 0;
  this->num_functions_ = 0;
}

BigThingArdu::~BigThingArdu() {}