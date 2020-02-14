#include <ESP8266WiFi.h> 
#include <PubSubClient.h>
#include <Stepper.h>

/***** libs used ********
  PubSubClient by Nick O'Leary (ManageLibraries)
  ESP8266 via Boards Manager using "http://arduino.esp8266.com/stable/package_esp8266com_index.json"
  Arduino Stepper Library
*/

// WIFI Settings
const char* wifiSSID = "YOURWIFI";
const char* wifiPWD = "YOURPWD";
char wifimac_char[18];
WiFiClient espClient;

// MQTT Settings
const char* mqttBroker = "your.mqtt.server";
const char* mqttUser = "user";
const char* mqttPwd = "secret";

char inTopic[50] = "smartblinds/";
const char outTopic[] = "smartblinds/";
char helloMsg[100] = "Online:";

PubSubClient client(espClient);

// Stepper Settings
const int stepsPerRevolution = 2048; 
Stepper myStepper(stepsPerRevolution, 5, 0, 4, 2); //beware port no of ESP are switched
int divider = 6;

// Blind settings
float currLength = 0;
int windowHeight = 1300;
const float matThickness = 0.25;
const float coreDia = 18.5;
float currDia = 36.5;

// Setup

void setup() {
  Serial.begin(74880);
  
  //ESP.wdtDisable();
  
  setup_wifi();
  setup_mqtt();

  // setup stepper library
  myStepper.setSpeed(14);
  ESP.wdtFeed();
}

// wifi setup

void setup_wifi() {
  String wifimac_string;
  wifimac_string = WiFi.macAddress();
  wifimac_string.replace(":", "-");
  wifimac_string.toCharArray(wifimac_char,18);
  
  Serial.print("\nConnecting to WiFi: ");
  Serial.print(wifiSSID);
  WiFi.begin(wifiSSID, wifiPWD);

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    ESP.wdtFeed();
    Serial.print(".");
  }

  Serial.println("connected");
  Serial.println("IP Address: " + WiFi.localIP());
}

void setup_mqtt() {
  
  Serial.println("Connecting to MQTT Broker");
  
  client.setServer(mqttBroker, 1883);
  client.setCallback(mqtt_handler);
  ESP.wdtFeed();
  strcat(inTopic,wifimac_char);
  strcat(helloMsg, inTopic);

  Serial.println(helloMsg);
  Serial.println(inTopic);
}

// MQTT handling method
void mqtt_handler(char* topic, byte* payload, unsigned int length) {
  ESP.wdtFeed();
  // assemble message
  char message[length+1];
  for(int i=0; i<length;i++){
    message[i] = (char)payload[i]; 
  }
  message[length] = '\0';

  Serial.println("Message arrived: ");
  Serial.print(topic);
  Serial.print(":");
  Serial.print(message);

   char* function = strtok(message, "("); //return everything before ( 
   char* parameters = strtok(NULL, ")"); // return everything between ( and )
   ESP.wdtFeed();
   if(strcmp(function, "up") == 0) { move_blinds(0); }
   if(strcmp(function, "down") == 0) { move_blinds(100); }
   if(strcmp(function, "f_move") == 0) { move_forced(atoi(parameters)); }
   if(strcmp(function, "f_pos") == 0) { reset_pos(atof(parameters)); }
   if(strcmp(function, "f_dia") == 0) { set_dia(atof(parameters)); }
   if(strcmp(function, "pos") == 0) { move_blinds(atoi(parameters)); }
   
}
void set_dia(float dia) {
  currDia = dia;
}

void reset_pos(float pos) {
  currLength = pos;
}

void move_forced(int rotations) {
  ESP.wdtFeed(); //feed the watchdog
  int y = rotations;
  int dir = rotations > 0 ? 1 : -1; 
  while (y != 0){
    for(int i = 0; i < divider; i++) {
       myStepper.step(stepsPerRevolution/divider*dir);
       ESP.wdtFeed();
    }
    if (y<0) {
      y++;
    } else {
      y--;
    }
  }
}
void move_blinds(int pos) {

  //get position
 // int pos = int(posi);
  int directionMultiplier = -1; //switch to -1 if motor mounting point changes
  float rotationMultiplier = 1;
  Serial.print("Position requested: ");
  Serial.print(pos);
  Serial.println();
  // Direction
  // percentage of total height
  // TODO: build this

    float deltaL = currLength - windowHeight*pos/100;
    Serial.print("deltaL: ");
    Serial.print(deltaL);
    Serial.println();
    
    if (deltaL < 0) {
        directionMultiplier = 1; //switch these for changed motor position
        deltaL*=directionMultiplier;
    }
    ESP.wdtFeed();
    while (deltaL > 0) {
        //calculate how much material will be moved by the next turn
        float matMoved = currDia*3.14;
      
        if (deltaL < matMoved) 
            rotationMultiplier = deltaL/matMoved;
        else
            rotationMultiplier = 1.0;

        //now for the fancy part
        ESP.wdtFeed(); //feed the watchdog
        for(int i = 0; i < divider; i++) {
          myStepper.step(stepsPerRevolution*rotationMultiplier*directionMultiplier/divider);
          ESP.wdtFeed();
        }
        
        currLength = currLength-(matMoved*rotationMultiplier*directionMultiplier);
        
        currDia = currDia+2*matThickness*directionMultiplier;//*rotationMultiplier;
        
        deltaL = floor(deltaL - matMoved*rotationMultiplier);
    }
 
    ESP.wdtFeed();

}

void mqtt_reconnect() {
  ESP.wdtFeed();
  while (!client.connected()) {
    if (client.connect(wifimac_char, mqttUser, mqttPwd)) {  // wifimac_char used as clientid
      Serial.println("...connected");
      ESP.wdtFeed();
      client.publish(outTopic, helloMsg);
      client.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
      ESP.wdtFeed();
    }
  }
}

long last = 0;

void loop() {
  // put your main code here, to run repeatedly:
  if (!client.connected()) { mqtt_reconnect(); }
  client.loop();
  ESP.wdtFeed();
  long now = millis();
  if (now - last > 2000) { last = now; ESP.wdtFeed();}
}
