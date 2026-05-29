/*
  Mochan Robot - Final Sleep Locked Full Code
  Board: ESP32-C3
  OLED: 0.96 inch SSD1306 128x64 I2C
  Eye Library: FluxGarage RoboEyes
  Motor Driver: L9110S / similar
  Touch Sensor: TTP223

  Main Sleep Mode Fix:
  - Sleep mode stays sleeping until next instruction
  - Eyes do NOT open automatically after a few seconds
  - Motor remains fully OFF
  - Only closed sleepy eyes + Z z animation

  Wiring:
  OLED VCC    -> ESP32-C3 3.3V
  TTP223 VCC  -> same ESP32-C3 3.3V point with OLED VCC
  OLED GND    -> GND
  TTP223 GND  -> GND
  TTP223 OUT  -> GPIO 4

  Motor pins:
  MOTOR_L_IN1 = 3
  MOTOR_L_IN2 = 2
  MOTOR_R_IN1 = 1
  MOTOR_R_IN2 = 0

  OLED pins:
  OLED_SDA = 8
  OLED_SCL = 9
*/

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>
#include <esp_arduino_version.h>

// ================= OLED CONFIG =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define OLED_SDA 8
#define OLED_SCL 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display);

// ================= MOTOR PINS =================
#define MOTOR_L_IN1 3
#define MOTOR_L_IN2 2
#define MOTOR_R_IN1 1
#define MOTOR_R_IN2 0

// ================= TTP223 TOUCH SENSOR =================
#define TOUCH_PIN 4
#define TOUCH_ACTIVE HIGH

// ================= PWM CONFIG =================
#define PWM_FREQ 1000
#define PWM_RESOLUTION 8

#define CH_L1 0
#define CH_L2 1
#define CH_R1 2
#define CH_R2 3

// ================= SPEED SETTINGS =================
const int WIGGLE_SPEED  = 85;
const int CURIOUS_SPEED = 145;
const int MANUAL_SPEED  = 200;
const int RUSH_SPEED    = 225;
const int PAIN_SPEED    = 180;

// ================= WIFI CONFIG =================
DNSServer dnsServer;
WebServer server(80);

const byte DNS_PORT = 53;

const char* AP_NAME = "mochan";
const char* AP_PASS = "12345678";

IPAddress apIP(192, 168, 4, 1);

// ================= MOOD SYSTEM =================
enum RobotMood {
  SLEEP,
  RUSH,
  CONSTANT,
  WIGGLE,
  CURIOUS,
  MANUAL
};

RobotMood currentMood = CONSTANT;

// ================= TOUCH SYSTEM =================
unsigned long lastTouchChangeTime = 0;
unsigned long touchPressStartTime = 0;
unsigned long lastTapTime = 0;

bool lastTouchState = false;
bool currentTouchState = false;
bool longPressDone = false;

int tapCount = 0;

const unsigned long TOUCH_DEBOUNCE_MS = 45;
const unsigned long TAP_GAP_MS = 430;
const unsigned long LONG_PRESS_MS = 900;

bool motorLocked = false;

// ================= REACTION SYSTEM =================
enum SpecialReaction {
  NO_REACTION,
  PAIN_REACTION,
  SQUASH_REACTION,
  MOTOR_LOCK_REACTION
};

SpecialReaction activeReaction = NO_REACTION;

unsigned long reactionStartTime = 0;
unsigned long reactionDuration = 0;

// ================= TIMERS =================
unsigned long rushMoveTimer = 0;
unsigned long rushEyeTimer = 0;

unsigned long wiggleMoveTimer = 0;
unsigned long wiggleEyeTimer = 0;

unsigned long curiousMoveTimer = 0;
unsigned long curiousEyeTimer = 0;

unsigned long constantExpressionTimer = 0;
unsigned long constantEyeMoveTimer = 0;

unsigned long sleepZTimer = 0;

int sleepZStep = 0;

// ================= SLOW TIMING SETTINGS =================
const unsigned long CONSTANT_EXPRESSION_INTERVAL = 4200;
const unsigned long CONSTANT_EYE_MOVE_INTERVAL   = 3500;

const unsigned long CURIOUS_EXPRESSION_INTERVAL  = 3800;
const unsigned long CURIOUS_MOVE_INTERVAL        = 2600;

const unsigned long WIGGLE_EXPRESSION_INTERVAL   = 4000;
const unsigned long WIGGLE_MOVE_INTERVAL         = 2800;

const unsigned long RUSH_EYE_INTERVAL            = 1600;
const unsigned long RUSH_MOVE_INTERVAL           = 650;

const unsigned long SLEEP_Z_INTERVAL             = 850;

// ================= FUNCTION DECLARATIONS =================
void setupPWM();
void pwmWritePin(uint8_t pin, uint8_t channel, int duty);

void moveForward(int leftSpeed, int rightSpeed);
void moveBackward(int leftSpeed, int rightSpeed);
void softTurnLeft(int speedValue);
void softTurnRight(int speedValue);
void spinLeft(int speedValue);
void spinRight(int speedValue);
void stopMotors();

void setMood(RobotMood mood);
void runMood();

void runSleepMode();
void runRushMode();
void runConstantMode();
void runWiggleMode();
void runCuriousMode();
void runManualMode();

void manualForward();
void manualBackward();
void manualLeft();
void manualRight();
void manualStop();

void configureEyesForMood(RobotMood mood);
void setEyesByDirection(int directionValue);
void drawSleepZ();

void setupWiFiPortal();
void setupWebRoutes();
void handleRoot();

void handleTouchSensor();
void processTouchAction(int count);
void startPainReaction();
void startSquashReaction();
void startMotorLockReaction();
void updateSpecialReaction();

void changeToNextMood();

void applySoftExpression();
void applyStrongExpression();
void applyCuriousExpression();
void applyCuteExpression();
void applyRandomEyeLook();

// ================= SETUP =================
void setup() {
  setCpuFrequencyMhz(80);

  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed!");
    while (true) {
      stopMotors();
    }
  }

  // If screen is upside-down/wrong, change 2 to 0.
  display.setRotation(2);

  display.clearDisplay();
  display.display();

  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);

  // Slow natural blink
  roboEyes.setAutoblinker(ON, 6, 2);

  // Stop fast auto eye movement
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);

  roboEyes.setMood(DEFAULT);
  roboEyes.setPosition(DEFAULT);
  roboEyes.open();

  setupPWM();
  stopMotors();

  setupWiFiPortal();
  setupWebRoutes();

  // Start in CONSTANT mode, not SLEEP
  setMood(CONSTANT);

  Serial.println("Mochan Robot Ready!");
}

// ================= LOOP =================
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  handleTouchSensor();
  updateSpecialReaction();

  if (activeReaction == NO_REACTION) {
    runMood();
  }

  roboEyes.update();

  if (currentMood == SLEEP) {
    drawSleepZ();
  }
}

// ================= PWM SETUP =================
void setupPWM() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(MOTOR_L_IN1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_L_IN2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_R_IN1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_R_IN2, PWM_FREQ, PWM_RESOLUTION);
#else
  ledcSetup(CH_L1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(CH_L2, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(CH_R1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(CH_R2, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(MOTOR_L_IN1, CH_L1);
  ledcAttachPin(MOTOR_L_IN2, CH_L2);
  ledcAttachPin(MOTOR_R_IN1, CH_R1);
  ledcAttachPin(MOTOR_R_IN2, CH_R2);
#endif
}

void pwmWritePin(uint8_t pin, uint8_t channel, int duty) {
  duty = constrain(duty, 0, 255);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);
#else
  ledcWrite(channel, duty);
#endif
}

// ================= MOTOR CONTROL =================
void moveForward(int leftSpeed, int rightSpeed) {
  if (motorLocked) {
    stopMotors();
    return;
  }

  pwmWritePin(MOTOR_L_IN1, CH_L1, leftSpeed);
  pwmWritePin(MOTOR_L_IN2, CH_L2, 0);

  pwmWritePin(MOTOR_R_IN1, CH_R1, rightSpeed);
  pwmWritePin(MOTOR_R_IN2, CH_R2, 0);
}

void moveBackward(int leftSpeed, int rightSpeed) {
  if (motorLocked) {
    stopMotors();
    return;
  }

  pwmWritePin(MOTOR_L_IN1, CH_L1, 0);
  pwmWritePin(MOTOR_L_IN2, CH_L2, leftSpeed);

  pwmWritePin(MOTOR_R_IN1, CH_R1, 0);
  pwmWritePin(MOTOR_R_IN2, CH_R2, rightSpeed);
}

void softTurnLeft(int speedValue) {
  if (motorLocked) {
    stopMotors();
    return;
  }

  pwmWritePin(MOTOR_L_IN1, CH_L1, speedValue / 3);
  pwmWritePin(MOTOR_L_IN2, CH_L2, 0);

  pwmWritePin(MOTOR_R_IN1, CH_R1, speedValue);
  pwmWritePin(MOTOR_R_IN2, CH_R2, 0);
}

void softTurnRight(int speedValue) {
  if (motorLocked) {
    stopMotors();
    return;
  }

  pwmWritePin(MOTOR_L_IN1, CH_L1, speedValue);
  pwmWritePin(MOTOR_L_IN2, CH_L2, 0);

  pwmWritePin(MOTOR_R_IN1, CH_R1, speedValue / 3);
  pwmWritePin(MOTOR_R_IN2, CH_R2, 0);
}

void spinLeft(int speedValue) {
  if (motorLocked) {
    stopMotors();
    return;
  }

  pwmWritePin(MOTOR_L_IN1, CH_L1, 0);
  pwmWritePin(MOTOR_L_IN2, CH_L2, speedValue);

  pwmWritePin(MOTOR_R_IN1, CH_R1, speedValue);
  pwmWritePin(MOTOR_R_IN2, CH_R2, 0);
}

void spinRight(int speedValue) {
  if (motorLocked) {
    stopMotors();
    return;
  }

  pwmWritePin(MOTOR_L_IN1, CH_L1, speedValue);
  pwmWritePin(MOTOR_L_IN2, CH_L2, 0);

  pwmWritePin(MOTOR_R_IN1, CH_R1, 0);
  pwmWritePin(MOTOR_R_IN2, CH_R2, speedValue);
}

void stopMotors() {
  pwmWritePin(MOTOR_L_IN1, CH_L1, 0);
  pwmWritePin(MOTOR_L_IN2, CH_L2, 0);

  pwmWritePin(MOTOR_R_IN1, CH_R1, 0);
  pwmWritePin(MOTOR_R_IN2, CH_R2, 0);
}

// ================= MOOD SET =================
void setMood(RobotMood mood) {
  currentMood = mood;

  rushMoveTimer = millis();
  rushEyeTimer = millis();

  wiggleMoveTimer = millis();
  wiggleEyeTimer = millis();

  curiousMoveTimer = millis();
  curiousEyeTimer = millis();

  constantExpressionTimer = millis();
  constantEyeMoveTimer = millis();

  sleepZTimer = millis();
  sleepZStep = 0;

  activeReaction = NO_REACTION;

  configureEyesForMood(mood);

  if (mood == SLEEP || mood == CONSTANT) {
    stopMotors();
  }
}

// ================= MOOD RUNNER =================
void runMood() {
  switch (currentMood) {
    case SLEEP:
      runSleepMode();
      break;

    case RUSH:
      runRushMode();
      break;

    case CONSTANT:
      runConstantMode();
      break;

    case WIGGLE:
      runWiggleMode();
      break;

    case CURIOUS:
      runCuriousMode();
      break;

    case MANUAL:
      runManualMode();
      break;
  }
}

// ================= LOCKED SLEEP MODE =================
void runSleepMode() {
  // Sleep mode must stay sleeping until next instruction.
  // No internal timer is allowed to open the eyes.
  stopMotors();

  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);
  roboEyes.setMood(TIRED);
  roboEyes.setPosition(DEFAULT);
  roboEyes.close();
}

// Animated Z z beside the eyes
void drawSleepZ() {
  if (millis() - sleepZTimer >= SLEEP_Z_INTERVAL) {
    sleepZStep++;
    if (sleepZStep > 2) {
      sleepZStep = 0;
    }
    sleepZTimer = millis();
  }

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  if (sleepZStep == 0) {
    display.setCursor(96, 4);
    display.print("Z");
    display.setCursor(106, 14);
    display.print("z");
  }
  else if (sleepZStep == 1) {
    display.setCursor(100, 8);
    display.print("Z");
    display.setCursor(110, 18);
    display.print("z");
  }
  else {
    display.setCursor(94, 10);
    display.print("z");
    display.setCursor(104, 20);
    display.print("Z");
  }

  display.display();
}

// ================= RUSH MODE =================
void runRushMode() {
  roboEyes.open();
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);

  if (millis() - rushEyeTimer >= RUSH_EYE_INTERVAL) {
    int eyeAction = random(0, 5);

    if (eyeAction == 0) {
      roboEyes.setMood(ANGRY);
      roboEyes.setPosition(W);
    }
    else if (eyeAction == 1) {
      roboEyes.setMood(ANGRY);
      roboEyes.setPosition(E);
    }
    else if (eyeAction == 2) {
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT);
    }
    else if (eyeAction == 3) {
      roboEyes.setMood(ANGRY);
      roboEyes.anim_confused();
    }
    else {
      roboEyes.setMood(DEFAULT);
      roboEyes.blink();
    }

    rushEyeTimer = millis();
  }

  if (millis() - rushMoveTimer >= RUSH_MOVE_INTERVAL) {
    int action = random(0, 7);

    if (action == 0) {
      moveForward(RUSH_SPEED, RUSH_SPEED);
    }
    else if (action == 1) {
      softTurnLeft(RUSH_SPEED);
    }
    else if (action == 2) {
      softTurnRight(RUSH_SPEED);
    }
    else if (action == 3) {
      spinLeft(RUSH_SPEED);
    }
    else if (action == 4) {
      spinRight(RUSH_SPEED);
    }
    else if (action == 5) {
      moveBackward(RUSH_SPEED - 30, RUSH_SPEED - 30);
    }
    else {
      moveForward(RUSH_SPEED, RUSH_SPEED - 45);
    }

    rushMoveTimer = millis();
  }
}

// ================= CONSTANT MODE =================
void runConstantMode() {
  stopMotors();

  roboEyes.open();
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);

  if (millis() - constantEyeMoveTimer >= CONSTANT_EYE_MOVE_INTERVAL) {
    applyRandomEyeLook();
    constantEyeMoveTimer = millis();
  }

  if (millis() - constantExpressionTimer >= CONSTANT_EXPRESSION_INTERVAL) {
    int expressionGroup = random(0, 4);

    if (expressionGroup == 0) {
      applySoftExpression();
    }
    else if (expressionGroup == 1) {
      applyCuteExpression();
    }
    else if (expressionGroup == 2) {
      applyCuriousExpression();
    }
    else {
      applyStrongExpression();
    }

    constantExpressionTimer = millis();
  }
}

// ================= WIGGLE MODE =================
void runWiggleMode() {
  roboEyes.open();
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);

  if (millis() - wiggleEyeTimer >= WIGGLE_EXPRESSION_INTERVAL) {
    int expression = random(0, 4);

    if (expression == 0) {
      applySoftExpression();
    }
    else if (expression == 1) {
      applyCuteExpression();
    }
    else if (expression == 2) {
      applyCuriousExpression();
    }
    else {
      roboEyes.setMood(DEFAULT);
      roboEyes.blink();
    }

    wiggleEyeTimer = millis();
  }

  if (millis() - wiggleMoveTimer >= WIGGLE_MOVE_INTERVAL) {
    int directionValue = random(0, 6);

    if (directionValue == 0) {
      roboEyes.setPosition(DEFAULT);
      moveForward(WIGGLE_SPEED, WIGGLE_SPEED);
    }
    else if (directionValue == 1) {
      roboEyes.setPosition(W);
      softTurnLeft(WIGGLE_SPEED);
    }
    else if (directionValue == 2) {
      roboEyes.setPosition(E);
      softTurnRight(WIGGLE_SPEED);
    }
    else if (directionValue == 3) {
      roboEyes.setPosition(W);
      spinLeft(WIGGLE_SPEED);
    }
    else if (directionValue == 4) {
      roboEyes.setPosition(E);
      spinRight(WIGGLE_SPEED);
    }
    else {
      stopMotors();
      roboEyes.setPosition(DEFAULT);
      roboEyes.blink();
    }

    wiggleMoveTimer = millis();
  }
}

// ================= CURIOUS MODE =================
void runCuriousMode() {
  roboEyes.open();
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);

  if (millis() - curiousEyeTimer >= CURIOUS_EXPRESSION_INTERVAL) {
    int expression = random(0, 5);

    if (expression == 0) {
      applyCuriousExpression();
    }
    else if (expression == 1) {
      applyCuteExpression();
    }
    else if (expression == 2) {
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(W);
    }
    else if (expression == 3) {
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(E);
    }
    else {
      roboEyes.setMood(DEFAULT);
      roboEyes.blink();
    }

    curiousEyeTimer = millis();
  }

  if (millis() - curiousMoveTimer >= CURIOUS_MOVE_INTERVAL) {
    int directionValue = random(0, 6);

    if (directionValue == 0) {
      roboEyes.setPosition(DEFAULT);
      moveForward(CURIOUS_SPEED, CURIOUS_SPEED);
    }
    else if (directionValue == 1) {
      roboEyes.setPosition(W);
      softTurnLeft(CURIOUS_SPEED);
    }
    else if (directionValue == 2) {
      roboEyes.setPosition(E);
      softTurnRight(CURIOUS_SPEED);
    }
    else if (directionValue == 3) {
      roboEyes.setPosition(DEFAULT);
      moveForward(CURIOUS_SPEED, CURIOUS_SPEED - 35);
    }
    else if (directionValue == 4) {
      roboEyes.setPosition(DEFAULT);
      moveForward(CURIOUS_SPEED - 35, CURIOUS_SPEED);
    }
    else {
      stopMotors();
      roboEyes.blink();
    }

    curiousMoveTimer = millis();
  }
}

// ================= MANUAL MODE =================
void runManualMode() {
  // Manual movement continues until STOP or another mode button is pressed.
}

// ================= MANUAL CONTROL FUNCTIONS =================
void manualForward() {
  currentMood = MANUAL;
  activeReaction = NO_REACTION;

  roboEyes.open();
  roboEyes.setMood(DEFAULT);
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);
  roboEyes.setPosition(N);

  moveForward(MANUAL_SPEED, MANUAL_SPEED);
}

void manualBackward() {
  currentMood = MANUAL;
  activeReaction = NO_REACTION;

  roboEyes.open();
  roboEyes.setMood(TIRED);
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);
  roboEyes.setPosition(S);

  moveBackward(MANUAL_SPEED, MANUAL_SPEED);
}

void manualLeft() {
  currentMood = MANUAL;
  activeReaction = NO_REACTION;

  roboEyes.open();
  roboEyes.setMood(DEFAULT);
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);
  roboEyes.setPosition(W);

  spinLeft(MANUAL_SPEED);
}

void manualRight() {
  currentMood = MANUAL;
  activeReaction = NO_REACTION;

  roboEyes.open();
  roboEyes.setMood(DEFAULT);
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);
  roboEyes.setPosition(E);

  spinRight(MANUAL_SPEED);
}

void manualStop() {
  currentMood = MANUAL;
  activeReaction = NO_REACTION;

  stopMotors();

  roboEyes.open();
  roboEyes.setMood(DEFAULT);
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);
  roboEyes.setPosition(DEFAULT);
}

// ================= TOUCH SENSOR HANDLING =================
void handleTouchSensor() {
  bool rawTouch = (digitalRead(TOUCH_PIN) == TOUCH_ACTIVE);

  if (rawTouch != lastTouchState) {
    lastTouchChangeTime = millis();
    lastTouchState = rawTouch;
  }

  if (millis() - lastTouchChangeTime > TOUCH_DEBOUNCE_MS) {
    if (rawTouch != currentTouchState) {
      currentTouchState = rawTouch;

      if (currentTouchState == true) {
        touchPressStartTime = millis();
        longPressDone = false;
      }
      else {
        if (!longPressDone) {
          tapCount++;
          lastTapTime = millis();
        }
      }
    }
  }

  if (currentTouchState == true && !longPressDone) {
    if (millis() - touchPressStartTime >= LONG_PRESS_MS) {
      longPressDone = true;
      tapCount = 0;
      startSquashReaction();
    }
  }

  if (tapCount > 0 && millis() - lastTapTime > TAP_GAP_MS) {
    processTouchAction(tapCount);
    tapCount = 0;
  }
}

void processTouchAction(int count) {
  if (count == 1) {
    startPainReaction();
  }
  else if (count == 2) {
    changeToNextMood();
  }
  else if (count >= 3) {
    motorLocked = !motorLocked;
    startMotorLockReaction();
  }
}

// ================= TOUCH REACTIONS =================
void startPainReaction() {
  activeReaction = PAIN_REACTION;
  reactionStartTime = millis();
  reactionDuration = 1600;

  roboEyes.open();
  roboEyes.setMood(ANGRY);
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);
  roboEyes.anim_confused();

  if (!motorLocked) {
    moveBackward(PAIN_SPEED, PAIN_SPEED);
  }
}

void startSquashReaction() {
  activeReaction = SQUASH_REACTION;
  reactionStartTime = millis();
  reactionDuration = 2000;

  stopMotors();

  roboEyes.open();
  roboEyes.setMood(TIRED);
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);
  roboEyes.setPosition(S);
  roboEyes.blink();
}

void startMotorLockReaction() {
  activeReaction = MOTOR_LOCK_REACTION;
  reactionStartTime = millis();
  reactionDuration = 1600;

  stopMotors();

  roboEyes.open();
  roboEyes.setIdleMode(OFF);
  roboEyes.setCuriosity(OFF);

  if (motorLocked) {
    roboEyes.setMood(TIRED);
    roboEyes.anim_confused();
  }
  else {
    roboEyes.setMood(HAPPY);
    roboEyes.anim_laugh();
  }
}

void updateSpecialReaction() {
  if (activeReaction == NO_REACTION) {
    return;
  }

  unsigned long elapsed = millis() - reactionStartTime;

  if (activeReaction == PAIN_REACTION) {
    if (elapsed < 450) {
      if (!motorLocked) {
        moveBackward(PAIN_SPEED, PAIN_SPEED);
      }
      roboEyes.setMood(ANGRY);
      roboEyes.setPosition(S);
    }
    else if (elapsed < 900) {
      if (!motorLocked) {
        spinLeft(PAIN_SPEED);
      }
      roboEyes.setMood(ANGRY);
      roboEyes.setPosition(W);
    }
    else if (elapsed < 1300) {
      if (!motorLocked) {
        spinRight(PAIN_SPEED);
      }
      roboEyes.setMood(ANGRY);
      roboEyes.setPosition(E);
    }
    else {
      stopMotors();
      roboEyes.setMood(TIRED);
      roboEyes.setPosition(DEFAULT);
      roboEyes.blink();
    }
  }

  if (activeReaction == SQUASH_REACTION) {
    stopMotors();

    if (elapsed < 600) {
      roboEyes.setMood(TIRED);
      roboEyes.setPosition(S);
    }
    else if (elapsed < 1300) {
      roboEyes.close();
    }
    else {
      roboEyes.open();
      roboEyes.setMood(TIRED);
      roboEyes.blink();
    }
  }

  if (activeReaction == MOTOR_LOCK_REACTION) {
    stopMotors();

    if (motorLocked) {
      roboEyes.setMood(TIRED);
      roboEyes.setPosition(DEFAULT);
    }
    else {
      roboEyes.setMood(HAPPY);
      roboEyes.setPosition(DEFAULT);
    }
  }

  if (elapsed >= reactionDuration) {
    activeReaction = NO_REACTION;

    if (motorLocked) {
      stopMotors();
    }

    configureEyesForMood(currentMood);
  }
}

// ================= MODE CHANGE BY DOUBLE TOUCH =================
void changeToNextMood() {
  activeReaction = NO_REACTION;

  if (currentMood == SLEEP) {
    setMood(CONSTANT);
  }
  else if (currentMood == CONSTANT) {
    setMood(WIGGLE);
  }
  else if (currentMood == WIGGLE) {
    setMood(CURIOUS);
  }
  else if (currentMood == CURIOUS) {
    setMood(RUSH);
  }
  else if (currentMood == RUSH) {
    setMood(CONSTANT);
  }
  else if (currentMood == MANUAL) {
    setMood(CONSTANT);
  }

  roboEyes.open();
  roboEyes.setMood(HAPPY);
  roboEyes.setPosition(DEFAULT);
  roboEyes.anim_laugh();
}

// ================= EYE EXPRESSION HELPERS =================
void applyRandomEyeLook() {
  int look = random(0, 5);

  if (look == 0) {
    roboEyes.setPosition(DEFAULT);
  }
  else if (look == 1) {
    roboEyes.setPosition(W);
  }
  else if (look == 2) {
    roboEyes.setPosition(E);
  }
  else if (look == 3) {
    roboEyes.setPosition(N);
  }
  else {
    roboEyes.setPosition(S);
  }
}

void applySoftExpression() {
  int expression = random(0, 5);

  roboEyes.open();
  roboEyes.setCuriosity(OFF);

  if (expression == 0) {
    roboEyes.setMood(DEFAULT);
    roboEyes.setPosition(DEFAULT);
  }
  else if (expression == 1) {
    roboEyes.setMood(DEFAULT);
    roboEyes.blink();
  }
  else if (expression == 2) {
    roboEyes.setMood(TIRED);
    roboEyes.setPosition(DEFAULT);
  }
  else if (expression == 3) {
    roboEyes.setMood(DEFAULT);
    roboEyes.setPosition(W);
  }
  else {
    roboEyes.setMood(DEFAULT);
    roboEyes.setPosition(E);
  }
}

void applyCuteExpression() {
  int expression = random(0, 4);

  roboEyes.open();
  roboEyes.setCuriosity(OFF);

  if (expression == 0) {
    roboEyes.setMood(HAPPY);
    roboEyes.setPosition(DEFAULT);
  }
  else if (expression == 1) {
    roboEyes.setMood(HAPPY);
    roboEyes.anim_laugh();
  }
  else if (expression == 2) {
    roboEyes.setMood(DEFAULT);
    roboEyes.blink();
  }
  else {
    roboEyes.setMood(HAPPY);
    roboEyes.setPosition(N);
  }
}

void applyCuriousExpression() {
  int expression = random(0, 5);

  roboEyes.open();
  roboEyes.setCuriosity(OFF);

  if (expression == 0) {
    roboEyes.setMood(DEFAULT);
    roboEyes.setPosition(W);
  }
  else if (expression == 1) {
    roboEyes.setMood(DEFAULT);
    roboEyes.setPosition(E);
  }
  else if (expression == 2) {
    roboEyes.setMood(DEFAULT);
    roboEyes.setPosition(N);
  }
  else if (expression == 3) {
    roboEyes.setMood(DEFAULT);
    roboEyes.setPosition(S);
  }
  else {
    roboEyes.setMood(DEFAULT);
    roboEyes.anim_confused();
  }
}

void applyStrongExpression() {
  int expression = random(0, 4);

  roboEyes.open();
  roboEyes.setCuriosity(OFF);

  if (expression == 0) {
    roboEyes.setMood(ANGRY);
    roboEyes.setPosition(DEFAULT);
  }
  else if (expression == 1) {
    roboEyes.setMood(ANGRY);
    roboEyes.anim_confused();
  }
  else if (expression == 2) {
    roboEyes.setMood(TIRED);
    roboEyes.setPosition(S);
  }
  else {
    roboEyes.setMood(HAPPY);
    roboEyes.anim_laugh();
  }
}

// ================= EYE HELPERS =================
void configureEyesForMood(RobotMood mood) {
  roboEyes.setHFlicker(OFF);
  roboEyes.setVFlicker(OFF);
  roboEyes.setCuriosity(OFF);

  if (mood == SLEEP) {
    roboEyes.setIdleMode(OFF);
    roboEyes.setMood(TIRED);
    roboEyes.setPosition(DEFAULT);
    roboEyes.close();
    stopMotors();
  }
  else if (mood == RUSH) {
    roboEyes.open();
    roboEyes.setMood(ANGRY);
    roboEyes.setIdleMode(OFF);
    roboEyes.setCuriosity(OFF);
  }
  else if (mood == CONSTANT) {
    roboEyes.open();
    roboEyes.setMood(DEFAULT);
    roboEyes.setIdleMode(OFF);
    roboEyes.setCuriosity(OFF);
    stopMotors();
  }
  else if (mood == WIGGLE) {
    roboEyes.open();
    roboEyes.setMood(DEFAULT);
    roboEyes.setIdleMode(OFF);
    roboEyes.setCuriosity(OFF);
  }
  else if (mood == CURIOUS) {
    roboEyes.open();
    roboEyes.setMood(DEFAULT);
    roboEyes.setIdleMode(OFF);
    roboEyes.setCuriosity(OFF);
  }
  else if (mood == MANUAL) {
    roboEyes.open();
    roboEyes.setMood(DEFAULT);
    roboEyes.setIdleMode(OFF);
    roboEyes.setCuriosity(OFF);
  }
}

void setEyesByDirection(int directionValue) {
  if (directionValue == 0) {
    roboEyes.setPosition(DEFAULT);
  }
  else if (directionValue == 1) {
    roboEyes.setPosition(W);
  }
  else if (directionValue == 2) {
    roboEyes.setPosition(E);
  }
  else if (directionValue == 3) {
    roboEyes.setPosition(N);
  }
  else if (directionValue == 4) {
    roboEyes.setPosition(S);
  }
  else {
    roboEyes.setPosition(DEFAULT);
  }
}

// ================= WIFI CAPTIVE PORTAL =================
void setupWiFiPortal() {
  WiFi.mode(WIFI_AP);

  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_NAME, AP_PASS);

  dnsServer.start(DNS_PORT, "*", apIP);

  Serial.print("WiFi Name: ");
  Serial.println(AP_NAME);

  Serial.print("WiFi Password: ");
  Serial.println(AP_PASS);

  Serial.print("Portal IP: ");
  Serial.println(apIP);
}

// ================= WEB ROUTES =================
void setupWebRoutes() {
  server.on("/", handleRoot);

  server.on("/up", []() {
    manualForward();
    server.send(200, "text/plain", "OK");
  });

  server.on("/down", []() {
    manualBackward();
    server.send(200, "text/plain", "OK");
  });

  server.on("/left", []() {
    manualLeft();
    server.send(200, "text/plain", "OK");
  });

  server.on("/right", []() {
    manualRight();
    server.send(200, "text/plain", "OK");
  });

  server.on("/stop", []() {
    manualStop();
    server.send(200, "text/plain", "OK");
  });

  server.on("/sleep", []() {
    setMood(SLEEP);
    server.send(200, "text/plain", "OK");
  });

  server.on("/rush", []() {
    setMood(RUSH);
    server.send(200, "text/plain", "OK");
  });

  server.on("/constant", []() {
    setMood(CONSTANT);
    server.send(200, "text/plain", "OK");
  });

  server.on("/wiggle", []() {
    setMood(WIGGLE);
    server.send(200, "text/plain", "OK");
  });

  server.on("/curious", []() {
    setMood(CURIOUS);
    server.send(200, "text/plain", "OK");
  });

  server.on("/motorlock", []() {
    motorLocked = true;
    startMotorLockReaction();
    server.send(200, "text/plain", "OK");
  });

  server.on("/motorunlock", []() {
    motorLocked = false;
    startMotorLockReaction();
    server.send(200, "text/plain", "OK");
  });

  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
}

// ================= WEB PAGE =================
void handleRoot() {
  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Mochan Robot</title>

<style>
body {
  background: #0a0e1a;
  font-family: Arial, sans-serif;
  text-align: center;
  color: #00ffcc;
  margin: 0;
  padding: 20px;
}

.container {
  max-width: 370px;
  margin: auto;
  background: #0f1729;
  border: 2px solid #00ffcc;
  border-radius: 20px;
  padding: 20px;
  box-shadow: 0 0 20px #00ffcc;
}

h1 {
  color: #00ffcc;
  margin: 10px 0 20px 0;
  text-shadow: 0 0 10px #00ffcc;
}

.btn {
  background: #00d4aa;
  border: none;
  color: #0a0e1a;
  padding: 0;
  margin: 5px;
  border-radius: 12px;
  font-weight: bold;
  box-shadow: 0 4px 15px #00ffcc80;
}

.btn:active {
  transform: scale(0.95);
  box-shadow: 0 2px 8px #00ffcc80;
}

.dir {
  width: 74px;
  height: 64px;
  font-size: 14px;
}

.stop {
  background: #ff3366 !important;
  color: white !important;
}

.mode {
  width: 86px;
  height: 38px;
  font-size: 12px;
  background: #00d4aa;
}

.lock {
  width: 130px;
  height: 36px;
  font-size: 12px;
}

.label {
  font-size: 11px;
  color: #00ffcc;
  margin: 3px 0 0 0;
}

.note {
  font-size: 10px;
  color: #00ffcc90;
  margin-top: 10px;
  line-height: 1.4;
}

.footer {
  font-size: 11px;
  color: #00ffcc90;
  margin-top: 18px;
}
</style>
</head>

<body>
<div class="container">
  <h1>Mochan Robot</h1>

  <div class="label">UP</div>
  <button class="btn dir" onclick="sendCmd('/up')">UP</button>

  <div>
    <div style="display:inline-block">
      <div class="label">LEFT</div>
      <button class="btn dir" onclick="sendCmd('/left')">LEFT</button>
    </div>

    <div style="display:inline-block">
      <div class="label">STOP</div>
      <button class="btn dir stop" onclick="sendCmd('/stop')">STOP</button>
    </div>

    <div style="display:inline-block">
      <div class="label">RIGHT</div>
      <button class="btn dir" onclick="sendCmd('/right')">RIGHT</button>
    </div>
  </div>

  <div class="label">DOWN</div>
  <button class="btn dir" onclick="sendCmd('/down')">DOWN</button>

  <br><br>

  <button class="btn mode" onclick="sendCmd('/sleep')">SLEEP</button>
  <button class="btn mode" onclick="sendCmd('/rush')">RUSH</button>
  <button class="btn mode" onclick="sendCmd('/constant')">CONSTANT</button>

  <br>

  <button class="btn mode" onclick="sendCmd('/wiggle')">WIGGLE</button>
  <button class="btn mode" onclick="sendCmd('/curious')">CURIOUS</button>

  <br><br>

  <button class="btn lock stop" onclick="sendCmd('/motorlock')">MOTOR OFF</button>
  <button class="btn lock" onclick="sendCmd('/motorunlock')">MOTOR ON</button>

  <div class="note">
    Touch: 1 tap = Pain | 2 taps = Mode Change | 3 taps = Motor Off/On | Hold = Squash
  </div>

  <div class="footer">Muntasir Tahsan Ashpee</div>
</div>

<script>
function sendCmd(path) {
  fetch(path)
    .then(response => console.log("Command sent:", path))
    .catch(error => console.log("Command error:", error));
}
</script>

</body>
</html>
)=====";

  server.send(200, "text/html", html);
}
