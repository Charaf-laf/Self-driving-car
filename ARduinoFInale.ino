#include <SoftwareSerial.h>
#include <Servo.h>

// ===== Elegoo V4.0 TB6612 shield pin map =====
#define PIN_MOTOR_L_PWM  5   
#define PIN_MOTOR_R_PWM  6   
#define PIN_MOTOR_L_DIR  7   
#define PIN_MOTOR_R_DIR  8   
#define PIN_MOTOR_STBY   3   

#define PIN_SERVO1       10
#define PIN_ESP_RX       12
#define PIN_ESP_TX       13

// ===== LINE ASSIST PINS =====
#define SENSOR_LEFT  A0 
#define SENSOR_MID   A1
#define SENSOR_RIGHT A2

SoftwareSerial espSerial(PIN_ESP_RX, PIN_ESP_TX);
Servo myServo;

String inputString = "";
bool stringComplete = false;
bool isLineAssistMode = false;

int followSpeed = 90;  
int turnSpeed   = 85;  
int searchSpeed = 65;  

// GLOBAL SERVO TRACKER
int lastServoAngle = 90;

void setup() {
  Serial.begin(115200);
  espSerial.begin(9600);

  pinMode(PIN_MOTOR_L_DIR, OUTPUT);
  pinMode(PIN_MOTOR_L_PWM, OUTPUT);
  pinMode(PIN_MOTOR_R_DIR, OUTPUT);
  pinMode(PIN_MOTOR_R_PWM, OUTPUT);
  pinMode(PIN_MOTOR_STBY, OUTPUT);

  pinMode(SENSOR_LEFT, INPUT);
  pinMode(SENSOR_MID, INPUT);
  pinMode(SENSOR_RIGHT, INPUT);

  // Center servo on boot, then immediately kill power
  myServo.attach(PIN_SERVO1);
  myServo.write(90); 
  delay(300);
  myServo.detach();

  digitalWrite(PIN_MOTOR_STBY, HIGH);
  inputString.reserve(50);
}

void driveCar(int leftSpeed, int rightSpeed) {
  if (leftSpeed >= 0) {
    digitalWrite(PIN_MOTOR_L_DIR, HIGH);
    analogWrite(PIN_MOTOR_L_PWM, leftSpeed);
  } else {
    digitalWrite(PIN_MOTOR_L_DIR, LOW);
    analogWrite(PIN_MOTOR_L_PWM, -leftSpeed);
  }

  if (rightSpeed >= 0) {
    digitalWrite(PIN_MOTOR_R_DIR, HIGH);
    analogWrite(PIN_MOTOR_R_PWM, rightSpeed);
  } else {
    digitalWrite(PIN_MOTOR_R_DIR, LOW);
    analogWrite(PIN_MOTOR_R_PWM, -rightSpeed);
  }
}

void loop() {
  while (espSerial.available()) {
    char c = (char)espSerial.read();
    if (c == '<') inputString = ""; 
    else if (c == '>') stringComplete = true; 
    else inputString += c;
  }

  if (stringComplete) {
    int idxL = inputString.indexOf("L:");
    int idxR = inputString.indexOf("R:");
    int idxM = inputString.indexOf("M:"); 
    int idxS = inputString.indexOf("S:");

    if (idxL != -1 && idxR != -1 && idxM != -1 && idxS != -1) {
      int commaL = inputString.indexOf(",", idxL);
      int commaR = inputString.indexOf(",", idxR);
      int commaM = inputString.indexOf(",", idxM);
      
      if (commaL != -1 && commaR != -1 && commaM != -1) {
        int motorLeft  = inputString.substring(idxL + 2, commaL).toInt();
        int motorRight = inputString.substring(idxR + 2, commaR).toInt();
        int modeValue  = inputString.substring(idxM + 2, commaM).toInt();
        int servoAngle = inputString.substring(idxS + 2).toInt();

        // THE "DEAD SERVO" FIX
        if (servoAngle != lastServoAngle && servoAngle >= 45 && servoAngle <= 135) {
          myServo.attach(PIN_SERVO1);  // Wake up
          myServo.write(servoAngle);   // Move
          delay(250);                  // Wait for movement to finish
          myServo.detach();            // Kill power entirely
          lastServoAngle = servoAngle; // Save position
        }

        isLineAssistMode = (modeValue == 1);

        if (!isLineAssistMode) {
          driveCar(motorLeft, motorRight);
        }
      }
    }
    inputString = "";
    stringComplete = false;
  }

  if (isLineAssistMode) {
    int sL = digitalRead(SENSOR_LEFT);
    int sM = digitalRead(SENSOR_MID);
    int sR = digitalRead(SENSOR_RIGHT);

    if (sM == HIGH && sL == LOW && sR == LOW) driveCar(followSpeed, followSpeed); 
    else if (sL == HIGH && sR == LOW) driveCar(-turnSpeed, turnSpeed); 
    else if (sR == HIGH && sL == LOW) driveCar(turnSpeed, -turnSpeed); 
    else driveCar(0, searchSpeed); 
  }
}