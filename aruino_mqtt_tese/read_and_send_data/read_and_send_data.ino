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

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);


const long interval = 59990;
unsigned int previousMillis = 0;

int count = 0;

struct data{
  String degree;
};

bool permission = false;
bool session_end_flag = false;
int all_received = 0;


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

void publisher(char topic[], String data, int qos){
  //send data to broker
  if(session_end_flag == true){
    Serial.print("Sending data ");
    Serial.print(data);
    Serial.print("\n");
  }
  
  mqttClient.beginMessage(topic,false,qos);
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
  char message[messageSize + 1];
  int index = 0;

  // use the Stream interface to read the contents
  while (mqttClient.available() && index < messageSize) {
    message[index++] = (char)mqttClient.read();
  }
  message[index] = '\0'; // Null-terminate the message string

  Serial.print("Message: ");
  Serial.println(message);

  if (strcmp(message, "Begin") == 0) {
    permission = true;
    
  } else if (strcmp(message, "End") == 0) {
    permission = false;
    session_end_flag = true;
  }
  else if (strcmp(message, "Received") == 0){
    all_received = 1;
    Serial.println(all_received);
  }
  Serial.println();
  Serial.println();
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------
//Encoder readings 

/*
 * This function gets the absolute position from the AMT22 encoder using the SPI bus. The AMT22 position includes 2 checkbits to use
 * for position verification. Both 12-bit and 14-bit encoders transfer position via two bytes, giving 16-bits regardless of resolution.
 * For 12-bit encoders the position is left-shifted two bits, leaving the right two bits as zeros. This gives the impression that the encoder
 * is actually sending 14-bits, when it is actually sending 12-bit values, where every number is multiplied by 4.
 * This function takes the pin number of the desired device as an input
 * This funciton expects res12 or res14 to properly format position responses.
 * Error values are returned as 0xFFFF
 */
uint16_t getPositionSPI(uint8_t encoder, uint8_t resolution)
{
  uint16_t currentPosition;       //16-bit response from encoder
  bool binaryArray[16];           //after receiving the position we will populate this array and use it for calculating the checksum

  //get first byte which is the high byte, shift it 8 bits. don't release line for the first byte
  currentPosition = spiWriteRead(AMT22_NOP, encoder, false) << 8;  

  //this is the time required between bytes as specified in the datasheet.
  //We will implement that time delay here, however the arduino is not the fastest device so the delay
  //is likely inherantly there already
  delayMicroseconds(3);

  //OR the low byte with the currentPosition variable. release line after second byte
  currentPosition |= spiWriteRead(AMT22_NOP, encoder, true);        

  //run through the 16 bits of position and put each bit into a slot in the array so we can do the checksum calculation
  for(int i = 0; i < 16; i++) binaryArray[i] = (0x01) & (currentPosition >> (i));

  //using the equation on the datasheet we can calculate the checksums and then make sure they match what the encoder sent
  if ((binaryArray[15] == !(binaryArray[13] ^ binaryArray[11] ^ binaryArray[9] ^ binaryArray[7] ^ binaryArray[5] ^ binaryArray[3] ^ binaryArray[1]))
          && (binaryArray[14] == !(binaryArray[12] ^ binaryArray[10] ^ binaryArray[8] ^ binaryArray[6] ^ binaryArray[4] ^ binaryArray[2] ^ binaryArray[0])))
    {
      //we got back a good position, so just mask away the checkbits
      currentPosition &= 0x3FFF;
    }
  else
  {
    currentPosition = 0xFFFF; //bad position
  }

  //If the resolution is 12-bits, and wasn't 0xFFFF, then shift position, otherwise do nothing
  if ((resolution == RES12) && (currentPosition != 0xFFFF)) currentPosition = currentPosition >> 2;

  return currentPosition;
}

/*
 * This function does the SPI transfer. sendByte is the byte to transmit.
 * Use releaseLine to let the spiWriteRead function know if it should release
 * the chip select line after transfer.  
 * This function takes the pin number of the desired device as an input
 * The received data is returned.
 */
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

/*
 * This function sets the state of the SPI line. It isn't necessary but makes the code more readable than having digitalWrite everywhere
 * This function takes the pin number of the desired device as an input
 */
void setCSLine (uint8_t encoder, uint8_t csLine)
{
  digitalWrite(encoder, csLine);
}

/*
 * The AMT22 bus allows for extended commands. The first byte is 0x00 like a normal position transfer, but the
 * second byte is the command.  
 * This function takes the pin number of the desired device as an input
 */
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

/*
 * The AMT22 bus allows for extended commands. The first byte is 0x00 like a normal position transfer, but the
 * second byte is the command.  
 * This function takes the pin number of the desired device as an input
 */
void resetAMT22(uint8_t encoder)
{
  spiWriteRead(AMT22_NOP, encoder, false);

  //this is the time required between bytes as specified in the datasheet.
  //We will implement that time delay here, however the arduino is not the fastest device so the delay
  //is likely inherantly there already
  delayMicroseconds(3);
  
  spiWriteRead(AMT22_RESET, encoder, true);
  
  delay(250); //250 second delay to allow the encoder to start back up
}


float read_encoder(uint8_t encoder){
  // code to read data from encoder  

  uint8_t attempts = 0;
  float encoderPositionDegree;
  uint16_t encoderPosition = getPositionSPI(encoder,RES14);


  while (encoderPosition == 0xFFFF && ++attempts < 3) {
    encoderPosition = getPositionSPI(encoder, RES14);
  }

  if (encoderPosition == 0xFFFF) {
    Serial.print("Encoder error. Attempts: ");
    Serial.print(attempts, DEC);
    Serial.write(NEWLINE);
  } else {
    
    encoderPositionDegree = encoderPosition / 45.5083; //transforma em graus
    if(encoder == right){
      encoderPositionDegree=360-encoderPositionDegree;   //muda o sentido de rotação do encoder
    }
    else{
      encoderPositionDegree=encoderPositionDegree;   //muda o sentido de rotação do encoder
    }
    
    encoderPositionDegree=round(encoderPositionDegree * 100) / 100;  //arredonda para um float de 2 casas decimais
    if(encoderPositionDegree>180){
      encoderPositionDegree = encoderPositionDegree-360;
    }
    else{
      encoderPositionDegree = encoderPositionDegree;
    }
    // Serial.println(encoderPositionDegree);
    //Serial.write(NEWLINE);
    // Serial.print("Encoder ");
    // Serial.print(encoder);
    // Serial.print(": ");
    // Serial.println(encoderPositionDegree, DEC);
    // Serial.print("   -   ");
  }
  return encoderPositionDegree;
;}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------



void setup() {
  char ssid[] = "Tunes";
  char pass[] = "tunesblack";
  char broker[] = "test.mosquitto.org";
  int port = 1883;  
  
  // put your setup code here, to run once:
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
  

  while(Connect_to_WiFI(ssid, pass) == false){
    Serial.println("Attempting to connect to WiFi");
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  Serial.print("Connected to WiFi");
  digitalWrite(LED_PIN, LOW);
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

  
  // create loop to save all reading for a predefined session time
  if(permission == true){
    // begin_reading();
    Serial.print("Begining session\n");
    delay(1000);
    save_data();
    
  }
  else{

    if(session_end_flag == true){
      begin_reading();
      String count_buffer;
      count_buffer = String("End");
      publisher(PUB_TOPIC, count_buffer, 1);
      publisher(PUB_TOPIC_1, count_buffer, 1);
      session_end_flag = false;
      delay(5000);
    }
    else{
      Serial.print("No session or data to be sent\n");
      if(all_received == 1){
        alld_data_received();
      }
      delay(1000);
    }
    mqttClient.poll();
  }
  
  mqttClient.poll();
  //delay(5000);

}

void save_data(){
  float reading_r, reading_l;
  String buffer_r, buffer_l;
  const int batchSize = 20; // Define your desired batch size
  int readingsCount = 0;

  Serial.print("Beginning session\n");
  begin_reading();
  while(permission == true){
    
    reading_r = read_encoder(ENC_0);
    reading_l = read_encoder(ENC_1);
    // Serial.print(reading_r);
    // Serial.print("\n");
    // Serial.print(reading_l);
    // Serial.print("\n");

    buffer_r += String(reading_r, 2);
    buffer_l += String(reading_l, 2);
    readingsCount++;

    // Add a comma between readings, except after the last reading in a batch
    if(readingsCount < batchSize){
      buffer_r += ",";
      buffer_l += ",";
    }

    // Check if the batch size is reached
    if(readingsCount == batchSize){
      // Send the batched readings
      publisher(PUB_TOPIC, buffer_r, 1);
      publisher(PUB_TOPIC_1, buffer_l, 1);
      mqttClient.poll(); // Ensure the poll function is called during the loop

      // Clear the buffers and reset the readings count for the next batch
      buffer_r = "";
      buffer_l = "";
      readingsCount = 0;
    }

    delay(10); // Adjust the delay as needed to match your desired reading frequency
  }

  // Handle any remaining readings that didn't fill a complete batch
  if(readingsCount > 0){
    publisher(PUB_TOPIC, buffer_r, 1);
    publisher(PUB_TOPIC_1, buffer_l, 1);
  }
}

void begin_reading(){
  digitalWrite(BUZZER_PIN, HIGH); // Turn the buzzer on
  delay(1000); // Wait for 1 second
  digitalWrite(BUZZER_PIN, LOW); // Turn the buzzer off
}

void alld_data_received(){
  int melody[] = {
    NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, 
    NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5
  };

  int noteDurations[] = {
    500, 500, 500, 500, 
    500, 500, 500, 500
  };
  Serial.print("\n\n\t\tIM HERE\n\n");
  for (int thisNote = 0; thisNote < 8; thisNote++) {

    // Calculate the note duration
    int noteDuration = noteDurations[thisNote];
    tone(BUZZER_PIN, melody[thisNote], noteDuration);

    // Pause for the note duration plus 30ms
    int pauseBetweenNotes = noteDuration + 30;
    delay(pauseBetweenNotes);

    // Stop the tone playing
    noTone(BUZZER_PIN);
    
  }
  all_received = 0;
}