#include "HX711.h"
#include <EEPROM.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>

struct Coin {
  float diameter; 
  float weight;   
  float speed;    
};

//--------------------------------------------------------------------------------- ir ---------------------------------------------------------------------------------------------
const int ir1_pin = 6;   
const int ir2_pin = 7; // Changed to avoid conflict with Keypad
int ir1_state = 0;
int ir2_state = 0;   
#define trigPin 10
#define echoPin 13
int space_threshold = 120;

//--------------------------------------------------------------------------------- time state ---------------------------------------------------------------------------------------------
int sensorDistance = 2;      
unsigned long startTime = 0;   
unsigned long endTime_ir1 = 0;     
unsigned long startTime_ir2 = 0;   
unsigned long elapsedTime = 0; 
unsigned long elapsedTime_between = 0;
bool timingStarted = false;

//--------------------------------------------------------------------------------- coins constant ---------------------------------------------------------------------------------------------
const int avg_const = 5;
float diameter, speed, weight;

//--------------------------------------------------------------------------------- load cell  ---------------------------------------------------------------------------------------------
#define calibration_factor 2280.0 // Replace with your calibration factor
#define dataPin  3
#define clockPin  2
int read_time = 10;
HX711 scale;
bool calibration_flag = false;

void coin_detection() {
  if (ir1_state == LOW && !timingStarted) {
    startTime = millis();
    timingStarted = true;
    scale.tare();
  } 
  else if (ir1_state == HIGH && timingStarted) {
    endTime_ir1 = millis();
    elapsedTime = endTime_ir1 - startTime;
    timingStarted = false;
  }

  ir2_state = digitalRead(ir2_pin);
  if (ir2_state == LOW) {
    startTime_ir2 = millis();
    elapsedTime_between = startTime_ir2 - startTime;
  }
  calculate();
}

float set_speed(unsigned long sensorDistance, unsigned long elapsedTime_between) {
  return (float)sensorDistance * 1000000.0 / elapsedTime_between;
}

float set_diameter(float speed, unsigned long elapsedTime) {
  return speed * elapsedTime;
}

float read_weight() {
  float weight = scale.get_units(read_time);
  return weight;
}

void calculate() {
  if (elapsedTime_between > 0) {
    weight = read_weight();
    speed = set_speed(sensorDistance, elapsedTime_between);
    diameter = set_diameter(speed, elapsedTime);
    elapsedTime_between = 0;
    calibration_flag = true;
  }
}

void show_information(float speed, float diameter, float weight) {
  Serial.print("Speed: ");
  Serial.print(speed, 2);
  Serial.print(" cm/s");
  Serial.print("   Estimated Diameter: ");
  Serial.print(diameter);
  Serial.println(" cm");
  Serial.print("Weight: ");
  Serial.println(weight);
}

//-----------------------------------------------------------------------------------------------------------------

const int NUMBER_OF_COINS = 4; 
const int EEPROM_START_ADDRESS = 0; 
const int EEPROM_SIZE_PER_COIN = sizeof(Coin);

Coin readCoinFromEEPROM(int coinIndex) {
  Coin coin;
  int address = EEPROM_START_ADDRESS + coinIndex * EEPROM_SIZE_PER_COIN;
  EEPROM.get(address, coin);
  return coin;
}

void writeCoinToEEPROM(int coinIndex, const Coin& coin) {
  int address = EEPROM_START_ADDRESS + coinIndex * EEPROM_SIZE_PER_COIN;
  EEPROM.put(address, coin);
}

void calibrateCoin(int coinIndex, int numReadings) {
  Coin coin;
  float sumDiameter = 0.0;
  float sumWeight = 0.0;
  float sumSpeed = 0.0;
  for (int i = 0; i < numReadings; ++i) {
    ir1_state = digitalRead(ir1_pin);
    coin_detection();
    if (calibration_flag == true) {
      sumDiameter += diameter;
      sumWeight += weight;
      sumSpeed += speed;
    }
    delay(100);
  }
  coin.diameter = sumDiameter / numReadings;
  coin.weight = sumWeight / numReadings;
  coin.speed = sumSpeed / numReadings;
  writeCoinToEEPROM(coinIndex, coin);
}

//------------------LCD and Keypad Setup----------------------------------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Corrected address

const int rows = 4;
const int columns = 4;
char hexaKeys[rows][columns] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[rows] = {9, 8, 7, 6};  // Adjust row pins
byte colPins[columns] = {5, 4, 3, 2};
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, rows, columns);

//------------------Slot Prices and Servos----------------------------------------------
int total = 0;
int item1_price = 20;
int item2_price = 50;
int item3_price = 65;
int item4_price = 75;

int item1_servo = 0;
int item2_servo = 1;
int item3_servo = 2;
int item4_servo = 3;
const int right = 0;
const int left = 1;
#define PCA9685_ADDRESS 0x40
#define SERVO_FREQ 50

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();


int order_item(int servo_move,int total, int price){
  if(total>=price)
    {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("please wait");
        while(true){
        
        pwm.setPWM(servo_move, 0, 150);
        digitalWrite(trigPin, LOW);
        delayMicroseconds(2);
        digitalWrite(trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(trigPin, LOW);


        float duration = pulseIn(echoPin, HIGH);

        float distance = (duration / 2) * 0.0343;
        if(distance<space_threshold)
        {
         pwm.setPWM(servo_move, 0, 0); 
         break;
        }
           }
        total-=price;
        
    }
    else
    {
     lcd.clear();
     lcd.setCursor(0, 0);
     lcd.print("Total=");
     lcd.print(total);
     lcd.setCursor(4, 0);
     lcd.print(" NOT ENOUGH");
    }
    return total;
}

void key_picker(char customKey){
  if(customKey=='1')
  {
  total=order_item(item1_servo,total,item1_price);
  }
  else if(customKey=='2')
  {
    total=order_item(item2_servo,total,item2_price);
  }
  else if(customKey=='3')
  {
    total=order_item(item3_servo,total,item3_price);
  }
  else if(customKey=='4')
  {
    total=order_item(item4_servo,total,item4_price);
  }
  else if (customKey=='*')
  {
  char coinIndexChar= customKeypad.getKey();
  char numReadingsChar= customKeypad.getKey();
  int coinIndex=coinIndexChar-'0';
  int numReadings=numReadingsChar-'0';
    calibrateCoin( coinIndex,  numReadings);
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("pleasae enter a");
    lcd.setCursor(0, 1);
    lcd.print("valid number");
  }
}


void detectCoinAndDisplayValue(float detectedDiameter, float detectedWeight, float detectedSpeed) {
  // Iterate through known coins
  for (int i = 0; i < NUMBER_OF_COINS; i++) {
    // Retrieve coin properties from EEPROM
    Coin storedCoin = readCoinFromEEPROM(i);
    
    // Compare properties of detected coin with properties of known coins
    if (abs(detectedDiameter - storedCoin.diameter) < 0.1 &&
        abs(detectedWeight - storedCoin.weight) < 0.1 &&
        abs(detectedSpeed - storedCoin.speed) < 0.1) {
      // Display the value of the detected coin on the LCD screen
      switch (i) {
        case 0:
          pwm.setPWM(right, 0, 0);
          lcd.print("You inserted 0.05 dinar");
          //acceptRejectServo;
          total+=.05;
          break;
        case 1:
        //acceptRejectServo;
          pwm.setPWM(right, 0, 0);
          lcd.print("You inserted 0.1 dinar");
          total+=.1;
          break;
        case 2:
          pwm.setPWM(right, 0, 0); 
          lcd.print("You inserted 0.25 dinar");
          total+=.25;
          break;
        case 3:
        //acceptRejectServo;
          pwm.setPWM(right, 0, 0);
          lcd.print("You inserted 0.5 dinar");
          total+=.5;
          break;
         default:
          //acceptRejectServo;
          lcd.print("Unknown coin");
          break;
      }
      return; // Exit the function once a match is found
    }
  }
  // If no match is found, display an appropriate message
  lcd.print("Unknown coin");
}

void setup() {
  Serial.begin(9600);
  pinMode(ir1_pin, INPUT);
  pinMode(ir2_pin, INPUT);
 
  scale.begin(dataPin, clockPin);
  scale.set_scale(calibration_factor);
  scale.tare();

  lcd.init();
  lcd.backlight();
  Serial.begin(9600);


  pwm.begin(PCA9685_ADDRESS);
  pwm.setPWMFreq(SERVO_FREQ);
  pinMode(trigPin,OUTPUT);
  pinMode(echoPin,INPUT);
}
void loop() {
  ir1_state = digitalRead(ir1_pin);
  coin_detection();
  // Get key value if pressed

  char customKey = customKeypad.getKey();
  if(customKey)
  key_picker(customKey);

}
