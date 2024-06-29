#include <ArduinoMqttClient.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <stdbool.h>
#include <stdio.h>
#include <Arduino.h>
#include <string.h>
#include <CircularBuffer.hpp>

// Constants
#define BAUDRATE 9600

#define AMT22_NOP 0x00
#define AMT22_RESET 0x60
#define AMT22_ZERO 0x70

#define NEWLINE 0x0A

#define RES12 12
#define RES14 14

#define ENC_0 2
#define ENC_1 3
#define SPI_MOSI MOSI
#define SPI_MISO MISO
#define SPI_SCLK SCK

#define SUB_TOPIC "django/confirm"
#define PUB_TOPIC "django/gait_values_right"
#define PUB_TOPIC_1 "django/gait_values_left"

#define BUZZER_PIN 4
#define LED_PIN 5

#define left ENC_0
#define right ENC_1

// Define note frequencies (in Hz)
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

const long INTERVAL = 59990;
const int MAX_READINGS = 2000;
const int BATCH_SIZE = 20;
const int BUZZER_ON_DURATION = 1000;
const int NOTE_DURATION = 500;
const int PAUSE_BETWEEN_NOTES = 530;

unsigned int previousMillis = 0;
int count = 0;

struct data {
  String degree;
};

bool permission = false;
bool session_end_flag = false;
int all_received = 0;

CircularBuffer<float, MAX_READINGS> reading_r;
CircularBuffer<float, MAX_READINGS> reading_l;

WiFiClient wifiClient;

class WiFiManager {
public:
  bool connect(const char* ssid, const char* pass, int maxAttempts = 10) {
    int attempts = 0;
    while (WiFi.begin(ssid, pass) != WL_CONNECTED && attempts < maxAttempts) {
      Serial.println("Attempting to connect to WiFi");
      digitalWrite(LED_PIN, HIGH);
      delay(1000);
      digitalWrite(LED_PIN, LOW);
      delay(100);
      attempts++;
    }
    return WiFi.status() == WL_CONNECTED;
  }
};

class MQTTManager {
  MqttClient mqttClient;
public:
  MQTTManager(WiFiClient& wifiClient) : mqttClient(wifiClient) {}

  bool connect(const char* broker, int port, int maxAttempts = 10) {
    int attempts = 0;
    while (!mqttClient.connect(broker, port) && attempts < maxAttempts) {
      Serial.print("MQTT connection failed! Error code = ");
      Serial.println(mqttClient.connectError());
      delay(1000);
      attempts++;
    }
    return mqttClient.connected();
  }

  void publish(const char* topic, const String& data, int qos) {
    mqttClient.beginMessage(topic, false, qos);
    mqttClient.print(data);
    mqttClient.endMessage();
  }

  void onMessage(void (*callback)(int)) {
    mqttClient.onMessage(callback);
  }

  void poll() {
    mqttClient.poll();
  }

  bool available() {
    return mqttClient.available();
  }

  int read() {
    return mqttClient.read();
  }

  String messageTopic() {
    return mqttClient.messageTopic();
  }

  void subscribe(const char* topic) {
    mqttClient.subscribe(topic);
  }
};

WiFiManager wifiManager;
MQTTManager mqttManager(wifiClient);

bool Connect_to_WiFI(char ssid[], char pass[], int maxAttempts = 10) {
  return wifiManager.connect(ssid, pass, maxAttempts);
}

void Connect_to_Broker(char broker[], int port, int maxAttempts = 10) {
  if (mqttManager.connect(broker, port, maxAttempts)) {
    Serial.println("You're connected to the MQTT broker!");
    Serial.println();
  } else {
    Serial.println("Failed to connect to MQTT broker after multiple attempts.");
  }
}

void publisher(char topic[], String data, int qos) {
  // if (session_end_flag == true) {
  //   Serial.print("Sending data ");
  //   Serial.print(data);
  //   Serial.print("\n");
  // }
  mqttManager.publish(topic, data, qos);
}

void onMqttMessage(int messageSize) {
  Serial.println("Received a message with topic '");
  Serial.print(mqttManager.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  char message[messageSize + 1];
  int index = 0;

  while (mqttManager.available() && index < messageSize) {
    message[index++] = (char)mqttManager.read();
  }
  message[index] = '\0';

  Serial.print("Message: ");
  Serial.println(message);

  if (strcmp(message, "Begin") == 0) {
    permission = true;
  } else if (strcmp(message, "End") == 0) {
    permission = false;
    session_end_flag = true;
  } else if (strcmp(message, "Received") == 0) {
    all_received = 1;
    Serial.println(all_received);
  }
  Serial.println();
  Serial.println();
}

uint16_t getPositionSPI(uint8_t encoder, uint8_t resolution) {
  uint16_t currentPosition;
  bool binaryArray[16];

  currentPosition = spiWriteRead(AMT22_NOP, encoder, false) << 8;
  delayMicroseconds(3);
  currentPosition |= spiWriteRead(AMT22_NOP, encoder, true);

  for (int i = 0; i < 16; i++) binaryArray[i] = (0x01) & (currentPosition >> (i));

  if ((binaryArray[15] == !(binaryArray[13] ^ binaryArray[11] ^ binaryArray[9] ^ binaryArray[7] ^ binaryArray[5] ^ binaryArray[3] ^ binaryArray[1]))
      && (binaryArray[14] == !(binaryArray[12] ^ binaryArray[10] ^ binaryArray[8] ^ binaryArray[6] ^ binaryArray[4] ^ binaryArray[2] ^ binaryArray[0]))) {
    currentPosition &= 0x3FFF;
  } else {
    currentPosition = 0xFFFF;
  }

  if ((resolution == RES12) && (currentPosition != 0xFFFF)) currentPosition = currentPosition >> 2;

  return currentPosition;
}

uint8_t spiWriteRead(uint8_t sendByte, uint8_t encoder, uint8_t releaseLine) {
  uint8_t data;

  setCSLine(encoder, LOW);
  delayMicroseconds(3);
  data = SPI.transfer(sendByte);
  delayMicroseconds(3);
  setCSLine(encoder, releaseLine);

  return data;
}

void setCSLine(uint8_t encoder, uint8_t csLine) {
  digitalWrite(encoder, csLine);
}

void setZeroSPI(uint8_t encoder) {
  spiWriteRead(AMT22_NOP, encoder, false);
  delayMicroseconds(3);
  spiWriteRead(AMT22_ZERO, encoder, true);
  delay(250);
}

void resetAMT22(uint8_t encoder) {
  spiWriteRead(AMT22_NOP, encoder, false);
  delayMicroseconds(3);
  spiWriteRead(AMT22_RESET, encoder, true);
  delay(250);
}

float read_encoder(uint8_t encoder) {
  uint8_t attempts = 0;
  float encoderPositionDegree;
  uint16_t encoderPosition = getPositionSPI(encoder, RES14);

  while (encoderPosition == 0xFFFF && ++attempts < 3) {
    encoderPosition = getPositionSPI(encoder, RES14);
  }

  if (encoderPosition == 0xFFFF) {
    Serial.print("Encoder error. Attempts: ");
    Serial.print(attempts, DEC);
    Serial.write(NEWLINE);
  } else {
    encoderPositionDegree = encoderPosition / 45.5083;
    if (encoder == right) {
      encoderPositionDegree = 360 - encoderPositionDegree;
    }
    encoderPositionDegree = round(encoderPositionDegree * 100) / 100;
    if (encoderPositionDegree > 180) {
      encoderPositionDegree = encoderPositionDegree - 360;
    }
  }
  return encoderPositionDegree;
}

void setup() {
  char ssid[] = "Tunes";
  char pass[] = "tunesblack";
  char broker[] = "test.mosquitto.org";
  int port = 1883;

  pinMode(SPI_SCLK, OUTPUT);
  pinMode(SPI_MOSI, OUTPUT);
  pinMode(SPI_MISO, INPUT);
  pinMode(ENC_0, OUTPUT);
  pinMode(ENC_1, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(BAUDRATE);
  digitalWrite(ENC_0, HIGH);
  digitalWrite(ENC_1, HIGH);
  SPI.begin();

  setZeroSPI(ENC_0);
  setZeroSPI(ENC_1);
  SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));

  while (!Connect_to_WiFI(ssid, pass)) {
    Serial.println("Attempting to connect to WiFi");
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  Serial.print("Connected to WiFi");
  digitalWrite(LED_PIN, LOW);
  Connect_to_Broker(broker, port);

  mqttManager.onMessage(onMqttMessage);

  Serial.print("Subscribing to topic: ");
  Serial.println(SUB_TOPIC);
  Serial.println();

  mqttManager.subscribe(SUB_TOPIC);

  Serial.print("Waiting for messages on topic: ");
  Serial.println(SUB_TOPIC);
  Serial.println();
}

void loop() {
  if (permission == true) {
    Serial.print("Beginning session\n");
    begin_reading();
    delay(1000);
    save_data();
  } else {
    if (session_end_flag == true) {
      String count_buffer;
      count_buffer = String("End");
      publisher(PUB_TOPIC, count_buffer, 1);
      publisher(PUB_TOPIC_1, count_buffer, 1);
      session_end_flag = false;
      delay(5000);
    } else {
      Serial.print("No session or data to be sent\n");
      if (all_received == 1) {
        alld_data_received();
      }
      delay(1000);
    }
    mqttManager.poll();
  }
}

void save_data() {
  reading_r.clear();
  reading_l.clear();
  int readingsCount = 0;

  while (permission == true && readingsCount < MAX_READINGS){
    reading_r.push(read_encoder(ENC_0));
    reading_l.push(read_encoder(ENC_1));
    readingsCount++;
    mqttManager.poll();
    delay(10);
  }

  int index = 0;
  begin_reading();
  while (index < readingsCount) {
    String buffer_r, buffer_l;
    for (int i = 0; i < BATCH_SIZE && index < readingsCount; i++, index++) {
      buffer_r += String(reading_r[index], 2);
      buffer_l += String(reading_l[index], 2);

      if (i < BATCH_SIZE - 1 && index < readingsCount - 1) {
        buffer_r += ",";
        buffer_l += ",";
      }
    }
    publisher(PUB_TOPIC, buffer_r, 2);
    publisher(PUB_TOPIC_1, buffer_l, 2);
    if (permission == true) {
      permission = false;
      session_end_flag = true;
    }
  }
}

void begin_reading() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(BUZZER_ON_DURATION);
  digitalWrite(BUZZER_PIN, LOW);
}

void alld_data_received() {
  int melody[] = {
    NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4,
    NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5
  };

  int noteDurations[] = {
    NOTE_DURATION, NOTE_DURATION, NOTE_DURATION, NOTE_DURATION,
    NOTE_DURATION, NOTE_DURATION, NOTE_DURATION, NOTE_DURATION
  };
  Serial.print("\n\n\t\tIM HERE\n\n");
  for (int thisNote = 0; thisNote < 8; thisNote++) {
    int noteDuration = noteDurations[thisNote];
    tone(BUZZER_PIN, melody[thisNote], noteDuration);
    int pauseBetweenNotes = noteDuration + 30;
    delay(pauseBetweenNotes);
    noTone(BUZZER_PIN);
  }
  all_received = 0;
}
