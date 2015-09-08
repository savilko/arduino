
#include <Wire.h>


const int thisDeviceID = 5;
long time = millis();
    
long last_ping_time = 0;
long last_restart = 0;

int arduino_reset_pin = 2;

void setup() {
   Serial.begin(9600);
   Wire.begin(thisDeviceID);
   Wire.onRequest(receiveEvent);

   pinMode(arduino_reset_pin, OUTPUT);
   digitalWrite(arduino_reset_pin, HIGH);
   
   Serial.println("Init..");
   delay(100);
}

void loop() {
  time = millis();
  
  //CHECK TO RESTART ARDUINO
  if( last_ping_time < time-25000 && millis() > 10000 && last_restart < time-60000 ){
      //restart ARDUINO
      last_restart = millis();
      digitalWrite(arduino_reset_pin, LOW);
      delay(100);
      digitalWrite(arduino_reset_pin, HIGH);
      Serial.println("RESET MASTER");
  }
  delay(1000);
}


void receiveEvent(){
  
  Wire.write("PONG ");
  last_ping_time = millis();
  Serial.print("Ping from master");
}


