#include <Wire.h>
#include "Sodaq_DS3231.h"
#include <LiquidCrystal_I2C.h>



LiquidCrystal_I2C lcd(0x3F,16,2);
long microtime = 0;

const int slaveDeviceID = 5;

const int well_pump = 8;
const uint32_t MAX_WELL_PUMP_OPERATION_TIME = 60*60*2; // 2 hours;
const uint32_t MAX_WELL_PUMP_INTERVAL_BETWEEN_OPERATIONS = 60*60; //1 hour;
const uint32_t WELL_PUMP_START_LEVEL = 90; 
const uint32_t WELL_PUMP_STOP_LEVEL = 96; 
uint32_t well_pump_last_start = 0;
uint32_t well_pump_last_stop = 0;

const int sprayers_pump = 7;
const uint32_t MAX_SPRAYERS_PUMP_OPERATION_TIME = 60*30; // 30 min;
const uint32_t MAX_SPRAYERS_PUMP_INTERVAL_BETWEEN_OPERATIONS = 60*60; //1 hour;
const uint32_t SPRAYERS_PUMP_STOP_MINIMUM_LEVEL = 15; 
const uint32_t SPRAYERS_PUMP_START_MINIMUM_LEVEL = 30; 
uint32_t sprayers_pump_last_start = 0;
uint32_t sprayers_pump_last_stop = 0;

char lastPong[32];
char slaveState[32];
long last_ping_time = millis();
long last_pong_time = millis();
long last_restart_slave_time = millis();

const int reset_wifi_pin = 6;

int on_hours[] = {0}; // Will be ON from 00:00 to 00:59; 

const int critical_min_water_level = 10; 
const int critical_max_water_level = 98; 

int level_trig = 9;
int level_echo = 10;

int water_level = 0;

int time = 0;
long last_pump_check_loop_time = 0;
long last_time_serial_send = 0;
long last_reset_check_time = 0;

String state = "";
char valueSeparator = 1;
char valuesStart = 0;
char valuesEnd = 2;

char weekDay[][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };



void setup () 
{
  microtime = micros();

  state = "init";
  lcd.init();                            
  delay(100);
  lcd.clear();                         
  delay(100);
  lcd.setBacklight(1);
  //lcd.setCursor(2,0);              
  delay(100);
  lcd.print("Init..");    
  
  Serial.begin(9600);
  Serial.println("Init...");
  Wire.begin();
  rtc.begin();
  //DateTime dt(2015, 07, 22, 15, 56, 0, 0);
  // rtc.setDateTime(dt); //Adjust date-time as defined 'dt' above 
  
  pinMode(sprayers_pump, OUTPUT);
  digitalWrite(sprayers_pump, LOW);
  pinMode(well_pump, OUTPUT);
  digitalWrite(well_pump, LOW);
  
  pinMode(level_trig, OUTPUT);
  digitalWrite(level_trig, LOW);
  pinMode(level_echo, INPUT);
  
  pinMode(reset_wifi_pin, OUTPUT);
  digitalWrite(reset_wifi_pin, HIGH);
  
  delay(100);
  lcd.clear();
}




void loop () 
{
  microtime = millis();
  DateTime now = rtc.now();
  time = now.getEpoch();
  water_level = check_level();
  
  if( last_pump_check_loop_time <  microtime - 5000 && millis() > 10000){
    last_pump_check_loop_time = microtime;
    
    if( water_level > critical_max_water_level || water_level < critical_min_water_level ){
      state = "ERR";
      //Serial.println("Bad water situation TURN OFF ALL");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("WARNING!!!");    
      well_pump_stop();
      sprayers_pump_stop();
    }else{
      state = "OK";
      lcd.setCursor(0,0);
      lcd.print("Bibo:");    
      lcd.print(digitalReadOutputPin(well_pump) == HIGH ? "ON " : "OFF");    
    
      lcd.print(" Prs:");    
      lcd.print(digitalReadOutputPin(sprayers_pump) == HIGH ? "ON " : "OFF");  
      
      well_pump_operation_check();
      sprayers_pump_operation_check();
    }
  
    
    
  }
  
  lcd.setCursor(0,1);
  lcd.print("nivo:"); 
  lcd.print(water_level);
  lcd.print("  ");
  lcd.print(now.hour(), DEC);
  lcd.print(time%2 ? ':' : ' ');
  lcd.print(now.minute(), DEC);
  lcd.print("h     ");
    
  //PING SLAVE
  if(last_ping_time < microtime - 3000){
    last_ping_time = microtime;
    Serial.print("PING ");
    char pong[5];
    int i = 0;
    memset(pong, 0, sizeof(pong));
    Wire.requestFrom(slaveDeviceID, 5);
    while(Wire.available())    // slave may send less than requested
    {
      pong[i++] = Wire.read();;
    }
    Serial.print(" ... ");
    Serial.println(pong);
    
    if(pong[0] == 'P' && pong[1] == 'O' && pong[2] == 'N' && pong[3] == 'G'){
      last_pong_time = millis();
      Serial.print("Pong time: ");
      Serial.println(last_pong_time);
    }    
  }
  
  if( last_pong_time < microtime-60000 && last_restart_slave_time < microtime - 60000 ){
    last_restart_slave_time = millis();
    Serial.println("RESET SLAVE");
    Serial.print("last_pong_time:" );
    Serial.print(last_pong_time);
    Serial.print(" last_restart_slave_time:");
    Serial.print(last_restart_slave_time);
    Serial.print(" time:");
    Serial.println(microtime-60000);
    digitalWrite(reset_wifi_pin, LOW);
    delay(100);
    digitalWrite(reset_wifi_pin, HIGH);
  }
  delay(500);
}



String getStatus(){
  //struct "state|time|level|wellPumpState|sprayersPumpState";
  //DateTime now = rtc.now().getEpoch();
  String stat = valuesStart + state + valueSeparator + time + valueSeparator + water_level + valueSeparator + digitalReadOutputPin(well_pump) + valueSeparator + digitalReadOutputPin(sprayers_pump) + valuesEnd;
  return stat;
  
}


int check_level(){
   
  digitalWrite(level_trig, LOW);
  delayMicroseconds(2);
  digitalWrite(level_trig, HIGH);
  delayMicroseconds(5);
  digitalWrite(level_trig, LOW);

  return ( 100 - pulseIn(level_echo, HIGH) / 29 / 2 );
}



int sprayers_pump_start(){
  //DateTime now = rtc.now();
  sprayers_pump_last_start = time; //now.getEpoch();
  digitalWrite(sprayers_pump, HIGH); 
  Serial.println("Start sprayers Pump");
}

int sprayers_pump_stop(){
  //DateTime now = rtc.now();
  sprayers_pump_last_stop = time; //now.getEpoch();
  digitalWrite(sprayers_pump, LOW); 
  Serial.println("Stop sprayers Pump"); 
}


int sprayers_pump_operation_check(){
  
  if(sprayers_pump_last_start < time - MAX_SPRAYERS_PUMP_OPERATION_TIME && digitalReadOutputPin(sprayers_pump) == HIGH){
    Serial.println("Max Sprayers Pump Operating Time Reach");
    sprayers_pump_stop();
  }
  
  if(digitalReadOutputPin(sprayers_pump) == LOW && is_on_time(on_hours) && sprayers_pump_last_stop < time - MAX_SPRAYERS_PUMP_INTERVAL_BETWEEN_OPERATIONS && water_level > SPRAYERS_PUMP_START_MINIMUM_LEVEL + 2){
    sprayers_pump_start();
  }
  
  if(digitalReadOutputPin(sprayers_pump) == HIGH && water_level < SPRAYERS_PUMP_STOP_MINIMUM_LEVEL){
    sprayers_pump_stop();
  }
  
  if(digitalReadOutputPin(sprayers_pump) == HIGH && !is_on_time(on_hours) ){
    sprayers_pump_stop();
  }
  
}

bool is_on_time(int hours[]){
  DateTime now = rtc.now();
  bool ret = false;
  for(int a = 0; a < sizeof(hours); a++){
    if(now.hour()==hours[a]){
      ret = true;    
    }
  }
  return ret;
}

int well_pump_start(){
  //DateTime now = rtc.now();
  well_pump_last_start = time; //now.getEpoch();
  digitalWrite(well_pump, HIGH); 
  Serial.println("Start Well Pump");
}

int well_pump_stop(){
  //DateTime now = rtc.now();
  well_pump_last_stop = time; //now.getEpoch();
  digitalWrite(well_pump, LOW); 
  Serial.println("Stop Well Pump"); 
}



int well_pump_operation_check(){
  
  //DateTime now = rtc.now();

  //max time check
  if(well_pump_last_start < time - MAX_WELL_PUMP_OPERATION_TIME && digitalReadOutputPin(well_pump) == HIGH){
    Serial.println("Max Well Pump Operating Time Reach");
    well_pump_stop();
  }
  
  //start if level is low;
  if( well_pump_last_stop < time - MAX_WELL_PUMP_INTERVAL_BETWEEN_OPERATIONS && digitalReadOutputPin(well_pump) == LOW){
    if(water_level < WELL_PUMP_START_LEVEL){
      Serial.println("Well Level is on a minimum");
      well_pump_start();
    }
  }
  
  //stop on max water level
  if(digitalReadOutputPin(well_pump) == HIGH && water_level > WELL_PUMP_STOP_LEVEL){
    Serial.println("Well Level is on a maximum");
    well_pump_stop();
  }
  
}


int digitalReadOutputPin(uint8_t pin)
{
 uint8_t bit = digitalPinToBitMask(pin);
 uint8_t port = digitalPinToPort(pin);
 if (port == NOT_A_PIN) 
   return LOW;

 return (*portOutputRegister(port) & bit) ? HIGH : LOW;
}
