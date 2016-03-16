/* serial data format:
 *  header:
 *    flag: 1 byte, 0xaa, filter out debug message
 *    type: 1 bytes, 1: upstream, 2: downstream
 *    body length: 2 bytes, network byte order
 *  body:
 *    json data from yunba service
 */ 
#include <ArduinoJson.h>

#define MSG_TYPE_UP 0x01
#define MSG_TYPE_DOWN 0x02

#define HEADER_LEN 4
#define BUF_LEN 256
#define FLAG_CHAR 0xaa
#define FIRST_PIN 5

#define CHILD_NUM 3

const char *g_appkey = "5697113d4407a3cd028abead";
const char *g_topic = "smart_office";
const char *g_devid = "switch_0";

uint8_t g_header[HEADER_LEN];
uint8_t g_buf[BUF_LEN];
int g_step = 1; // 1: recv header, 2 recv body
uint16_t g_body_len = 0;
uint16_t g_recv_len = 0;

char g_status[CHILD_NUM];

char g_need_report = 1;
unsigned long g_check_ms = 0;

void recv_header() {
  while (Serial.available() >= HEADER_LEN) {
    Serial.readBytes(g_header, 1);
//    Serial.println((uint8_t)g_header[0], HEX);
    if (g_header[0] == FLAG_CHAR) {
      break;
    }
  }

  if (g_header[0] != FLAG_CHAR) { // no enough data
    return;
  }

  Serial.readBytes(g_header + 1, HEADER_LEN - 1);

  if (g_header[1] != MSG_TYPE_DOWN) { // not a downstream message
    return;
  }

  /* le */
  ((uint8_t *)&g_body_len)[0] = g_header[3];
  ((uint8_t *)&g_body_len)[1] = g_header[2];

  Serial.println("len:");
  Serial.println(g_body_len);

  if (g_body_len > (BUF_LEN - HEADER_LEN - 1)) {
    Serial.println("too long msg");
    return;
  }

  g_recv_len = 0;
  g_step = 2;
}

void recv_body() {
  uint16_t len = 0;

  len = Serial.readBytes(g_buf + g_recv_len, g_body_len - g_recv_len);
  g_recv_len += len;

  if (g_recv_len != g_body_len) {
    return;
  }

  // now got a completed msg
  g_buf[g_body_len] = 0;
  Serial.println("got a msg:");
  Serial.println((char *)g_buf);

  handle_msg();

  memset(g_header, 0, HEADER_LEN);
  g_step = 1;
}

void handle_msg() {
  StaticJsonBuffer<128> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject((char *)g_buf);
  if (!root.success()) {
    Serial.println("parseObject");
    return;
  }

  if (strcmp(root["devid"], g_devid) != 0) {
    Serial.println("not my devid");
    return;
  }

  if (strcmp(root["cmd"], "switch_set") == 0) {
    int j = min(CHILD_NUM, root["status"].size());
    for (int i = 0; i < j; i++) {
      uint8_t st = root["status"][i];
      set_status(i, st);
    }
  } else if (strcmp(root["cmd"], "switch_get") == 0) {
    g_need_report = 1;
  }
}

void set_status(int index, char st) {
  if (st != 0)
    st = 1;

  Serial.print("set:");
  Serial.println(index);
  Serial.println((int)st);

  if (g_status[index] != st) {
    g_status[index] = st;
  }
   
  if (st == 0) {
    digitalWrite(FIRST_PIN + index, LOW);
  } else {
    digitalWrite(FIRST_PIN + index, HIGH);
  }
  g_need_report = 1;
}

void get_all_status() {
  
}

void check_status() {
  char st[CHILD_NUM] = {0};
  int i = 0;

  for (i = 0; i < CHILD_NUM; i++) {
    st[i] = g_status[i];
  }

  get_all_status();

  for (i = 0; i < CHILD_NUM; i++) {
    if (g_status[i] != st[i]) {
      g_need_report = 1;
      break;
    }
  }
}

void send_msg() {
  g_buf[0] = FLAG_CHAR;
  g_buf[1] = MSG_TYPE_UP; // 1: upstream
  g_buf[2] = ((uint8_t *)&g_body_len)[1];
  g_buf[3] = ((uint8_t *)&g_body_len)[0];

  Serial.write(g_buf, g_body_len + HEADER_LEN);
///  Serial.flush();
}

void report_status() {
  StaticJsonBuffer<128> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  root["devid"] = g_devid;

  JsonArray& st = root.createNestedArray("status");
  for (int i = 0; i < CHILD_NUM; i++) {
    st.add(g_status[i]);
  }
  g_body_len = root.printTo((char *)g_buf + HEADER_LEN, BUF_LEN - HEADER_LEN);

  send_msg();
}

void handle_input() {
  if (g_step == 1) {
    recv_header();
  } else if (g_step == 2) {
    recv_body();
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  //Serial.setTimeout(100);
  Serial.println("setup...");

  for (int i = 0; i < CHILD_NUM; i++) {
    pinMode(FIRST_PIN + i, OUTPUT);
//    digitalWrite(FIRST_PIN + i, LOW);
  }

  get_all_status();
}

void loop() {
  // put your main code here, to run repeatedly:
  handle_input();

  if (millis() - g_check_ms > 1000) {
    check_status();
    g_check_ms = millis();
  }

  if (g_need_report) {
    report_status();
    g_need_report = 0;
  }

  delay(20);
}
