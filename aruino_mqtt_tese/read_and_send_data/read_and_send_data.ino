#include <ArduinoMqttClient.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <stdbool.h>
#include <stdio.h>
#include <Arduino.h>
#include <string.h>

#define BAUDRATE 9600

#define AMT22_NOP 0x00
#define AMT22_RESET 0x60
#define AMT22_ZERO 0x70

#define NEWLINE 0x0A

#define RES12 12
#define RES14 14

#define ENC_0 2  // Ajuste para o pino adequado no MKR 1010
#define SPI_MOSI MOSI
#define SPI_MISO MISO
#define SPI_SCLK SCK

#define SUB_TOPIC "data/confirm"
#define PUB_TOPIC "django/gait_values"



WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);


const long interval = 59990;
unsigned int previousMillis = 0;

int count = 0;

struct data{
  String degree;
};

bool permission = false;



//-----------------------------------------------------------------------------------------------------------------------------------------------
// MQTT
bool Connect_to_WiFI(char ssid[], char pass[]){
  // Function to connect to WiFi
  return WiFi.begin(ssid, pass) == WL_CONNECTED;
}

void Connect_to_Broker(char broker[], int port){
  // Function to connect to mqtt broker
  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();
}

void publisher(char topic[], String data){
  //send data to broker
  // Serial.print("Sending data ");
  // Serial.print(data);
  // Serial.print("\n");
  mqttClient.beginMessage(topic);
  mqttClient.print(data);
  mqttClient.endMessage();
}

void onMqttMessage(int messageSize) {
  // we received a message, print out the topic and contents
  Serial.println("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  // use the Stream interface to print the contents
  while (mqttClient.available()) {
    Serial.print((char)mqttClient.read());
    permission = true;
  }
  Serial.println();
  Serial.println();
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------
//Encoder readings 

uint16_t getPositionSPI(uint8_t encoder, uint8_t resolution) {
  uint16_t currentPosition;
  bool binaryArray[16];

  currentPosition = spiWriteRead(AMT22_NOP, encoder, false) << 8;
  delayMicroseconds(3);
  currentPosition |= spiWriteRead(AMT22_NOP, encoder, true);

  for (int i = 0; i < 16; i++) {
    binaryArray[i] = (0x01) & (currentPosition >> (i));
  }

  if ((binaryArray[15] == !(binaryArray[13] ^ binaryArray[11] ^ binaryArray[9] ^ binaryArray[7] ^ binaryArray[5] ^ binaryArray[3] ^ binaryArray[1])) && (binaryArray[14] == !(binaryArray[12] ^ binaryArray[10] ^ binaryArray[8] ^ binaryArray[6] ^ binaryArray[4] ^ binaryArray[2] ^ binaryArray[0]))) {
    currentPosition &= 0x3FFF;
  } 
  else {
    Serial.println("Encoder error.");
    currentPosition = 0xFFFF;
  }

  if ((resolution == RES12) && (currentPosition != 0xFFFF)) {
    currentPosition = currentPosition >> 2;
  }

  return currentPosition;
}

uint8_t spiWriteRead(uint8_t sendByte, uint8_t encoder, uint8_t releaseLine)
{
  //holder for the received over SPI
  uint8_t data;

  //set cs low, cs may already be low but there's no issue calling it again except for extra time
  setCSLine(encoder ,LOW);

  //There is a minimum time requirement after CS goes low before data can be clocked out of the encoder.
  //We will implement that time delay here, however the arduino is not the fastest device so the delay
  //is likely inherantly there already
  delayMicroseconds(3);

  //send the command  
  data = SPI.transfer(sendByte);
  delayMicroseconds(3); //There is also a minimum time after clocking that CS should remain asserted before we release it
  setCSLine(encoder, releaseLine); //if releaseLine is high set it high else it stays low
  
  return data;
}

void setCSLine (uint8_t encoder, uint8_t csLine)
{
  digitalWrite(encoder, csLine);
}

void setZeroSPI(uint8_t encoder)
{
  spiWriteRead(AMT22_NOP, encoder, false);

  //this is the time required between bytes as specified in the datasheet.
  //We will implement that time delay here, however the arduino is not the fastest device so the delay
  //is likely inherantly there already
  delayMicroseconds(3); 
  
  spiWriteRead(AMT22_ZERO, encoder, true);
  delay(250); //250 second delay to allow the encoder to reset
}


float read_encoder(){
  // code to read data from encoder  

  uint8_t attempts = 0;
  float encoderPositionDegree;
  uint16_t encoderPosition = getPositionSPI(ENC_0,RES14);


  while (encoderPosition == 0xFFFF && ++attempts < 3) {
    encoderPosition = getPositionSPI(ENC_0, RES14);
  }

  if (encoderPosition == 0xFFFF) {
    Serial.print("Encoder error. Attempts: ");
    Serial.print(attempts, DEC);
    //Serial.write(NEWLINE);
  } else {
    // Serial.print("Encoder: ");
    // Serial.print(encoderPosition, DEC);
    // Serial.print("   -   ");
    encoderPositionDegree = encoderPosition / 45.5083; //transforma em graus
    encoderPositionDegree=360-encoderPositionDegree;   //muda o sentido de rotação do encoder
    encoderPositionDegree=round(encoderPositionDegree * 100) / 100;  //arredonda para um float de 2 casas decimais
    if(encoderPositionDegree>180){
      encoderPositionDegree = encoderPositionDegree-360;
    }
    else{
      encoderPositionDegree = encoderPositionDegree;
    }
    // Serial.println(encoderPositionDegree);
    //Serial.write(NEWLINE);
  }
  return encoderPositionDegree;
;}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void setup() {
  char ssid[] = "Tunes";
  char pass[] = "tunesblack";
  char broker[] = "broker.emqx.io";
  int port = 1883;  
  
  // put your setup code here, to run once:
  pinMode(SPI_SCLK, OUTPUT);
  pinMode(SPI_MOSI, OUTPUT);
  pinMode(SPI_MISO, INPUT);
  pinMode(ENC_0, OUTPUT);

  Serial.begin(BAUDRATE);
  digitalWrite(ENC_0, HIGH);
  SPI.begin();

  setZeroSPI(ENC_0);

  

  while(Connect_to_WiFI(ssid, pass) == false){
    Serial.print("Attempting to connect to WiFi");
  }
  Serial.print("Connected to WiFi");

  Connect_to_Broker(broker, port);
  
  // Subscription method
  mqttClient.onMessage(onMqttMessage);

  Serial.print("Subscribing to topic: ");
  Serial.println(SUB_TOPIC);
  Serial.println();

  // subscribe to a topic
  mqttClient.subscribe(SUB_TOPIC);

  // topics can be unsubscribed using:
  // mqttClient.unsubscribe(topic);

  Serial.print("Waiting for messages on topic: ");
  Serial.println(SUB_TOPIC);
  Serial.println();

  
  // char buffer[50];
  
  
  
}

void loop() {
  // put your main code here, to run repeatedly:
  String data;
  
  // create loop to save all reading for a predefined session time
  if(permission == true){
    Serial.print("Begining session");
    delay(1000);
    save_data();
    permission = false;
    publisher(PUB_TOPIC,"End");
  }
  else{
    ;
  }
  
  mqttClient.poll();
  //delay(5000);

}

void save_data(){
  float reading;
  String buffer;
  String message;
  for(int i = 0; i < 50; i++){
      reading = read_encoder();
      
      buffer = String(reading, 2);

      publisher(PUB_TOPIC, buffer);
      // if(i < 149){
      //   message = message + buffer + ",";
      // }
      // else{
      //   message = message + buffer;
      // }
      // Serial.print("\nBuffer = ");
      // Serial.print(buffer);
      // Serial.print("\n");
      // Serial.print("\nEncoder reading = ");
      // Serial.print(reading);
      // Serial.print("\n");
      delay(10);
  }
}