#ifndef BIGTHINGARDU_H
#define BIGTHINGARDU_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define MAX_VALUE_NUM 3
#define MAX_FUNCTION_NUM 2
#define MAX_BUFFER_SIZE 60

// #define MEM_ALLOC_CHECK(var)                 \
//   if (var == NULL) {                         \
//     Serial.println("[ERROR] malloc failed"); \
//   }

typedef void (*VoidFunction)(void);
typedef int (*IntegerFunction)(void);
typedef double (*DoubleFunction)(void);
typedef bool (*BoolFunction)(void);

typedef int (*IntegerValue)(void);
typedef double (*DoubleValue)(void);
typedef bool (*BoolValue)(void);
typedef char* (*StringValue)(char*, int);

typedef enum _soptype {
  UNDEFINED = 0, /** < represents UNDEFINED */
  INTEGER,       /** < represents IntegerFunction or IntegerValue */
  DOUBLE,        /** < represents DoubleFunction or DoubleValue */
  BOOL,          /** < represents BoolFunction or BoolValue */
  STRING,        /** < represents Character array Value > */
  BINARY,        /** <represents Binary Value> */
  VOID,          /** < represents VoidFunction */
} SoPType;

class Tag {
 public:
  Tag(const char* name);
  ~Tag();

  String name_;
};

class SoPFunction {
 public:
  SoPFunction(const char* name, VoidFunction func);
  SoPFunction(const char* name, IntegerFunction func);
  SoPFunction(const char* name, DoubleFunction func);
  SoPFunction(const char* name, BoolFunction func);
  ~SoPFunction();

  void AddTag(const char* tag_name);
  void AddTag(Tag& function_tag);

  int Execute(DynamicJsonDocument* p_doc);

  SoPType return_type_;
  String name_;
  void* min_;
  void* max_;

  int num_tags_;
  Tag** function_tags_;

 private:
  void* callback_function_;
  void* return_value_;
};

class SoPValue {
 public:
  SoPValue(const char* name, IntegerValue value, int min, int max, int publish_cycle_);
  SoPValue(const char* name, StringValue value, int min, int max, int publish_cycle_);
  SoPValue(const char* name, DoubleValue value, double min, double max, int publish_cycle_);
  SoPValue(const char* name, BoolValue value, int publish_cycle_);
  ~SoPValue();

  void AddTag(const char* tag_name);
  void AddTag(Tag& value_tag);

  String Fetch();

  SoPType value_type_;
  String name_;
  void* min_;
  void* max_;

  int publish_cycle_;
  unsigned long last_publish_time_;

  int num_tags_;
  Tag** value_tags_;

 private:
  void* callback_function_;

  void* prev_value_;
  void* new_value_;
};

class BigThingArdu {
 public:
  BigThingArdu(const char* client_id, int alive_cycle, PubSubClient mqtt_client);
  ~BigThingArdu();

  void Add(SoPValue& v);
  void Add(SoPFunction& f);

  void Setup(const char* broker_ip, int broker_port);
  void Loop();

 private:
  void Reconnect();
  bool CheckAliveCyclePassed();
  void SendAliveMessage();
  void SendValue();
  void SendFunctionResult();
  void Register();

  PubSubClient mqtt_client_;
  String client_id_;
  unsigned long alive_cycle_;
  unsigned long last_alive_time_;

  int num_functions_;
  SoPFunction* functions_[MAX_FUNCTION_NUM];
  int num_values_;
  SoPValue* values_[MAX_VALUE_NUM];
};

#endif
