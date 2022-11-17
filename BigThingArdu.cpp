#include <ArduinoJson.h>
#include <BigThingArdu.h>
#include <SPI.h>

static void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String payloadTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadTemp += (char)payload[i];
  }
  Serial.println();

  if (String(topic) == "MT/RESULT/REGISTER") {
    Serial.print("Register Result: ");
    if (payloadTemp == "success") {
      Serial.println("success");
      digitalWrite(13, HIGH);
    } else if (payloadTemp == "fail") {
      Serial.println("fail");
      digitalWrite(13, LOW);
    }
  } else if (String(topic) == "MT/EXECUTE") {
    Serial.println("Execute");
  }
}

Value::Value(const char* name, BoolValue value, int publish_cycle) {
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

Value::Value(const char* name, IntegerValue value, int min, int max, int publish_cycle) {
  this->name_ = String(name);
  this->value_type_ = INTEGER;

  this->callback_function_ = (void*)value;

  this->prev_value_ = malloc(sizeof(int));
  *(int*)prev_value_ = 0;

  if (!max_) max_ = malloc(sizeof(int));
  if (!min_) min_ = malloc(sizeof(int));

  *(int*)this->max_ = max;
  *(int*)this->min_ = min;
  this->publish_cycle_ = publish_cycle;
  this->last_publish_time_ = 0;
}

Value::Value(const char* name, StringValue value, int min, int max, int publish_cycle) {
  this->name_ = String(name);
  this->value_type_ = STRING;

  this->callback_function_ = (void*)value;

  this->publish_cycle_ = publish_cycle;
  this->last_publish_time_ = 0;
}

Value::Value(const char* name, DoubleValue value, double min, double max, int publish_cycle) {
  this->name_ = String(name);
  this->value_type_ = DOUBLE;

  this->callback_function_ = (void*)value;

  this->prev_value_ = malloc(sizeof(double));
  *(int*)prev_value_ = 0;

  if (!max_) max_ = malloc(sizeof(double));
  if (!min_) min_ = malloc(sizeof(double));

  *(double*)this->max_ = max;
  *(double*)this->min_ = min;
  this->publish_cycle_ = publish_cycle;
  this->last_publish_time_ = 0;
}

Value::~Value() {
  if (this->callback_function_) free(this->callback_function_);
  if (this->min_) free(this->min_);
  if (this->max_) free(this->max_);
  if (this->prev_value_) free(this->prev_value_);
}

String Value::Fetch() {
  char buffer[MAX_BUFFER_SIZE];

  switch (this->value_type_) {
    case INTEGER: {
      this->new_value_ = new int;
      *(int*)new_value_ = ((IntegerValue)this->callback_function_)();
      snprintf(buffer, MAX_BUFFER_SIZE, "{\"type\" : \"int\" , \"value\" : %d}\n", *(int*)new_value_);
      break;
    }
    case DOUBLE: {
      this->new_value_ = new double;
      *(double*)new_value_ = ((DoubleValue)this->callback_function_)();
      snprintf(buffer, MAX_BUFFER_SIZE, "{\"type\" : \"double\" , \"value\" : %lf}\n", *(double*)new_value_);
      break;
    }
    case BOOL: {
      this->new_value_ = new bool;
      *(bool*)new_value_ = ((BoolValue)this->callback_function_)();
      snprintf(buffer, MAX_BUFFER_SIZE, "{\"type\" : \"bool\" , \"value\" : %d}\n", *(bool*)new_value_);
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
    if (this->mqtt_client_.connect(this->client_id_.c_str())) {  // 앞서 설정한 클라이언트 ID로 연결합니다.
      this->mqtt_client_.subscribe("MT/RESULT/REGISTER");
      this->mqtt_client_.subscribe("MT/EXECUTE");
      Serial.println("Subscribe MT/RESULT/REGISTER");
      Serial.println("Subscribe MT/EXECUTE");
    } else {
      delay(5000);
      Serial.println("Reconnect...");
    }
  }
}

static bool CheckPublishCyclePassed(Value* v) {
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
  if ((diff_time < 0) || (diff_time >= (unsigned long)v->publish_cycle_ * 1000)) {
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
  if ((diff_time < 0) || (diff_time >= (unsigned long)this->alive_cycle_ / 2 * 1000)) {
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
    Value* v = this->values_[i];

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

void BigThingArdu::Register() {
  DynamicJsonDocument doc(1024);
  char json_string[256];

  doc["alive_cycle"] = this->alive_cycle_;

  for (uint8_t i = 0; i < this->num_values_; i++) {
    Value* v = this->values_[i];

    doc["values"][i]["name"] = v->name_.c_str();

    switch (v->value_type_) {
      case INTEGER:
        doc["values"][i]["type"] = "int";
        doc["values"][i]["bound"]["min_value"] = *(int*)v->min_;
        doc["values"][i]["bound"]["max_value"] = *(int*)v->max_;
        break;
      case DOUBLE:
        doc["values"][i]["type"] = "double";
        doc["values"][i]["bound"]["min_value"] = *(double*)v->min_;
        doc["values"][i]["bound"]["max_value"] = *(double*)v->max_;
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
  }

  serializeJson(doc, json_string);

  String topic = "TM/REGISTER/" + this->client_id_;

  this->mqtt_client_.publish(topic.c_str(), json_string);
  Serial.print(("Publish "));
  Serial.println(topic.c_str());
  Serial.println(json_string);
}

void BigThingArdu::Setup(const char* broker_ip, int broker_port) {
  this->client_id_ += String(random(0xffff), HEX);
  this->mqtt_client_.setServer(broker_ip, broker_port);
  this->mqtt_client_.setCallback(callback);

  this->Register();
}

void BigThingArdu::Loop() {
  if (!this->mqtt_client_.connected()) {
    this->Reconnect();
  }

  this->mqtt_client_.loop();

  this->SendAliveMessage();
  this->SendValue();

  // delay(1000);
  // ESP.deepSleep(5e6);
}

void BigThingArdu::Add(Value& v) {
  if (this->num_values_ < MAX_VALUE_NUM) {
    this->values_[this->num_values_++] = &v;
    Serial.print(v.name_.c_str());
    Serial.println((" Value is added"));
  } else {
    Serial.println(("It cannot be added (more than limit)"));
  }
}

BigThingArdu::BigThingArdu(const char* client_id, int alive_cycle, PubSubClient mqtt_client) {
  this->mqtt_client_ = mqtt_client;

  this->client_id_ = String(client_id);
  this->alive_cycle_ = alive_cycle;

  this->num_values_ = 0;
}

BigThingArdu::~BigThingArdu() {}