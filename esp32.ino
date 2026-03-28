#include <Bluepad32.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define TX_PIN 17
#define RX_PIN 16

// --- NEW SAFE PINS ---
#define TRIG_PIN 4
#define ECHO_PIN 5

Controller* myController = nullptr;

// --- WIFI & UDP SETTINGS ---
const char* ssid = "Name"; 
const char* password = ""; // 
WiFiUDP udp;
const int udpPort = 4210;

// --- WARNING TRACKERS ---
bool isAiWarningActive = false;
bool isSensorWarningActive = false;
unsigned long lastAiWarningTime = 0;
unsigned long lastPingTime = 0;

// ---- Drive tuning ----
const int   MAX_SPEED = 200;
const int   DEADZONE_L = 30; 
const float EXPO_T = 0.35f;
const float EXPO_S = 0.45f;
const int   PIVOT_ZONE = 35;
const float MOVE_TURN_GAIN  = 0.9f;
const float PIVOT_TURN_GAIN = 1.2f;

// ---- Servo tuning ----
const int SERVO_LEFT = 135;
const int SERVO_RIGHT = 45;
const int SERVO_MID = 90;
int servoPos = SERVO_MID; 

bool lineAssistMode = false;
bool lastR3State = false;
unsigned long rumbleStartTime = 0; 

void onConnectedController(Controller* ctlr) {
  if (!myController) myController = ctlr;
  myController->setColorLED(0, 0, 255); 
}

void onDisconnectedController(Controller* ctlr) {
  if (myController == ctlr) myController = nullptr;
}

static int applyDeadzoneInt(int v, int dz) { return (abs(v) < dz) ? 0 : v; }
static float axisNorm(int axis) { return (float)constrain(axis, -512, 512) / 512.0f; }
static float expo(float v, float e) {
  float absV = abs(v);
  float out = pow(absV, 3.0f) * e + absV * (1.0f - e);
  return (v < 0) ? -out : out;
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  
  WiFi.begin(ssid, password);
  int attempts = 0;
  Serial.print("Connecting to WiFi");
  // Only try for 5 seconds (10 attempts * 500ms), then move on!
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    udp.begin(udpPort);
    Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi Failed! AI disabled, but car will still drive.");
  }

  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);
}

void loop() {
  BP32.update();

  // --- 1. ULTRASONIC SENSOR LOGIC (Check every 50ms) ---
  if (millis() - lastPingTime > 50) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    int distance = duration * 0.034 / 2;

    if (distance > 0 && distance <= 20) {
      isSensorWarningActive = true;
    } else {
      isSensorWarningActive = false;
    }
    lastPingTime = millis();
  }

  // --- 2. AI WIFI UDP LOGIC ---
  if (WiFi.status() == WL_CONNECTED) {
    int packetSize = udp.parsePacket();
    if (packetSize) {
      char incomingPacket[255];
      int len = udp.read(incomingPacket, 255);
      if (len > 0) incomingPacket[len] = 0;
      
      if (String(incomingPacket) == "WARN") {
        isAiWarningActive = true;
        lastAiWarningTime = millis();
      }
    }
  }


  if (millis() - lastAiWarningTime > 1000) {
    isAiWarningActive = false;
  }

  if (myController && myController->isConnected()) {
    
    // --- 3. BLINKING LED CONTROLLER LOGIC ---
    if (isSensorWarningActive || isAiWarningActive) {
      if ((millis() / 150) % 2 == 0) {
        myController->setColorLED(255, 0, 0); // Red ON
      } else {
        myController->setColorLED(0, 0, 0);   // Red OFF (Blink)
      }
    } else {
      myController->setColorLED(0, 0, 255); // Solid Blue (Safe)
    }

    bool r3Pressed = myController->thumbR();
    if (r3Pressed && !lastR3State) {
      lineAssistMode = !lineAssistMode;
      myController->playDualRumble(0, 255, 150, 0); 
      rumbleStartTime = millis();
    }
    lastR3State = r3Pressed;

    if (millis() - rumbleStartTime > 150) {
      myController->playDualRumble(0, 0, 0, 0);
    }

    // -------- LEFT STICK DRIVE --------
    int rawX = applyDeadzoneInt(myController->axisX(), DEADZONE_L);
    int rawY = applyDeadzoneInt(myController->axisY(), DEADZONE_L);

    float x = expo(axisNorm(rawX), EXPO_S);
    float y = expo(axisNorm(rawY), EXPO_T);

    int throttle = (int)(-y * MAX_SPEED);
    int steer    = (int)(-x * MAX_SPEED);

    int leftSpeed = 0, rightSpeed = 0;
    if (abs(throttle) < PIVOT_ZONE) {
      leftSpeed  = (int)(steer * PIVOT_TURN_GAIN);
      rightSpeed = -leftSpeed;
    } else {
      int turn = (int)(steer * MOVE_TURN_GAIN);
      leftSpeed  = throttle + turn;
      rightSpeed = throttle - turn;
    }

    leftSpeed  = constrain(leftSpeed,  -MAX_SPEED, MAX_SPEED);
    rightSpeed = constrain(rightSpeed, -MAX_SPEED, MAX_SPEED);

    // -------- BUTTON SERVO CONTROL --------
    if (myController->b()) servoPos = SERVO_RIGHT;
    else if (myController->x()) servoPos = SERVO_LEFT;
    else if (myController->y()) servoPos = SERVO_MID;

    int modeVal = lineAssistMode ? 1 : 0;
    
    String pkt = "<L:" + String(leftSpeed) + 
                 ",R:" + String(rightSpeed) + 
                 ",M:" + String(modeVal) + 
                 ",S:" + String(servoPos) + ">";
    Serial2.println(pkt);
  }
  delay(30); 
}