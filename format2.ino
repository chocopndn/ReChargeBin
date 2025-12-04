#include <SoftwareSerial.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>

#define RX 8
#define TX 9
SoftwareSerial link(RX, TX);

Servo topLid;
Servo bottomLid;
Servo solarServo;

#define IR_PIN 4
#define TRIG 3
#define ECHO 2
#define BIN_TRIG 10
#define BIN_ECHO 11

#define LED_GREEN 12
#define LED_RED 13

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define LDR_L A0
#define LDR_R A1

int bottleCount = 0;

float tooCloseCM = 15;
float clearCM = 70;

float LARGE_H = 15;
float MEDIUM_H = 25;

unsigned long lastBinCheck = 0;
const unsigned long BIN_CHECK_INTERVAL = 60000;
unsigned long waitingStart = 0;
unsigned long waitingTimeout = 5000;

bool busy = false;
bool waiting = false;
bool sizing = false;
bool afterClose = false;
bool tooCloseRecovery = false;
bool recoveryClear = false;
bool rejectRecovery = false;
bool solarEnabled = true;

unsigned long detectTime = 0;
unsigned long recoveryStart = 0;
unsigned long rejectStart = 0;

bool binOK = true;

int solarPos = 90;
int SOLAR_CENTER = 90;
int SOLAR_MIN_ANGLE = 0;
int SOLAR_MAX_ANGLE = 180;
int SOLAR_NIGHT_THRESHOLD = 40;

unsigned long lcdTimer = 0;
bool lcdTimed = false;

unsigned long lidCloseDelay = 800;
unsigned long servoSettleMS = 700;

void startLCDTimer()
{
  lcdTimed = true;
  lcdTimer = millis();
}

long ultraRead(int trig, int echo)
{
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long u = pulseIn(echo, HIGH, 30000);
  if (u == 0)
    return 999;
  return (u * 0.034) / 2;
}

float heightCM() { return ultraRead(TRIG, ECHO); }
float binCM() { return ultraRead(BIN_TRIG, BIN_ECHO); }

String detectSize(float h)
{
  if (h < LARGE_H)
    return "L";
  if (h < MEDIUM_H)
    return "M";
  return "S";
}

void openTop()
{
  if (binOK)
    topLid.write(0);
}
void closeTop() { topLid.write(90); }

void openBottom()
{
  if (binOK)
    bottomLid.write(0);
}
void closeBottom() { bottomLid.write(90); }

void ledGreen()
{
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);
}
void ledRed()
{
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, HIGH);
}

void lcdInsertBottle()
{
  lcdTimed = false;
  lcd.clear();
  lcd.print("Insert bottle");
}

void lcdTooHeavy()
{
  lcd.clear();
  lcd.print("Too heavy");
  lcd.setCursor(0, 1);
  lcd.print("Invalid bottle");
  startLCDTimer();
}

void lcdAccepted()
{
  lcd.clear();
  lcd.print("Bottle accepted");
  startLCDTimer();
}

void lcdCount()
{
  if (!binOK)
    return;
  lcdTimed = false;
  lcd.clear();
  lcd.print("Bottle count:");
  lcd.setCursor(0, 1);
  lcd.print(bottleCount);
}

void lcdFull()
{
  lcdTimed = false;
  lcd.clear();
  lcd.print("BIN IS FULL");
}

void updateLED()
{
  if (!binOK)
  {
    ledRed();
    return;
  }
  if (busy || sizing || afterClose || tooCloseRecovery || rejectRecovery)
  {
    ledRed();
    return;
  }
  ledGreen();
}

void sendReady()
{
  if (!binOK)
    return;
  if (digitalRead(IR_PIN) == 0)
    return;
  link.println("READY");
}

void updateBinStatus()
{
  float d = binCM();
  if (d < 10)
  {
    binOK = false;
    lcdFull();
  }
  else
  {
    binOK = true;
    lcdCount();
  }
  lastBinCheck = millis();
}

void fullResetState()
{
  busy = false;
  waiting = false;
  sizing = false;
  afterClose = false;
  tooCloseRecovery = false;
  recoveryClear = false;
  rejectRecovery = false;

  solarEnabled = true;

  closeTop();
  closeBottom();
  solarServo.write(SOLAR_CENTER);
  solarPos = SOLAR_CENTER;
}

void solarTrack()
{
  if (!solarEnabled)
    return;

  long sumL = 0, sumR = 0;
  for (int i = 0; i < 10; i++)
  {
    sumL += analogRead(LDR_L);
    sumR += analogRead(LDR_R);
  }

  int L = sumL / 10;
  int R = sumR / 10;

  if (L < SOLAR_NIGHT_THRESHOLD && R < SOLAR_NIGHT_THRESHOLD)
    return;

  int diff = L - R;
  if (abs(diff) < 200)
    return;

  int step = diff > 0 ? 1 : -1;
  int target = solarPos + step;

  if (target < SOLAR_MIN_ANGLE)
    target = SOLAR_MIN_ANGLE;
  if (target > SOLAR_MAX_ANGLE)
    target = SOLAR_MAX_ANGLE;

  if (target != solarPos)
  {
    solarPos = target;
    solarServo.write(solarPos);
  }
}

void setup()
{
  Serial.begin(9600);
  link.begin(4800);

  pinMode(IR_PIN, INPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BIN_TRIG, OUTPUT);
  pinMode(BIN_ECHO, INPUT);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  topLid.attach(5);
  bottomLid.attach(6);
  solarServo.attach(7);

  topLid.write(90);
  bottomLid.write(90);
  solarServo.write(SOLAR_CENTER);

  lcd.init();
  lcd.backlight();

  updateBinStatus();
}

void loop()
{

  if (lcdTimed && millis() - lcdTimer >= 2000)
  {
    lcdTimed = false;
    lcdCount();
  }

  if (!waiting && !sizing && !afterClose && !rejectRecovery && !tooCloseRecovery)
    solarTrack();

  if (millis() - lastBinCheck >= BIN_CHECK_INTERVAL && !busy)
    updateBinStatus();

  String s = "";
  if (link.available())
  {
    s = link.readStringUntil('\n');
    s.trim();
  }

  if (!binOK)
  {
    if (s == "START" || s == "ACCEPT" || s == "REJECT")
    {
      link.println("REJECT");
      return;
    }
  }

  if (s == "RESET")
  {
    fullResetState();
    updateBinStatus();
    link.println("READY");
    return;
  }

  int ir = digitalRead(IR_PIN);
  float h = heightCM();

  if (s == "START" && !busy)
  {
    if (topLid.read() == 0)
      return;

    busy = true;
    solarEnabled = false;

    waiting = true;
    waitingStart = millis();

    openTop();
    lcdInsertBottle();
  }

  if (waiting)
  {
    if (h < tooCloseCM)
    {
      tooCloseRecovery = true;
      waiting = false;
      recoveryStart = millis();
      return;
    }

    if (ir == 0)
    {
      waiting = false;
      detectTime = millis();
      sizing = true;
    }

    if (millis() - waitingStart >= waitingTimeout)
    {
      waiting = false;
      busy = false;
      closeTop();
      solarEnabled = true;
      sendReady();
      return;
    }
  }

  if (sizing)
  {
    if (millis() - detectTime >= lidCloseDelay)
    {
      closeTop();
      delay(servoSettleMS);
      sizing = false;
      afterClose = true;
    }
  }

  if (afterClose)
  {

    float sum = 0;
    for (int i = 0; i < 5; i++)
    {
      sum += heightCM();
      delay(20);
    }

    float hh = sum / 5.0;
    String sz = detectSize(hh);

    link.print("SIZE:");
    link.println(sz);

    afterClose = false;
  }

  if (s == "WEIGH")
  {
    link.println("DO_WEIGH");
  }

  if (s == "ACCEPT")
  {
    lcdAccepted();

    openBottom();
    delay(700);
    closeBottom();

    bottleCount++;
    updateBinStatus();

    busy = false;
    solarEnabled = true;
    sendReady();
  }

  if (s == "REJECT")
  {
    lcdTooHeavy();

    delay(1000);
    openTop();
    delay(1200);

    rejectRecovery = true;
    rejectStart = millis();
  }

  if (tooCloseRecovery)
  {
    if (millis() - recoveryStart >= 2000)
    {
      if (heightCM() > clearCM)
      {
        tooCloseRecovery = false;
        recoveryClear = true;
      }
    }
  }

  if (recoveryClear)
  {
    if (millis() - recoveryStart >= 4000)
    {
      recoveryClear = false;
      busy = false;
      solarEnabled = true;
      sendReady();
    }
  }

  if (rejectRecovery)
  {
    if (millis() - rejectStart < 2000)
      return;
    if (digitalRead(IR_PIN) == 0)
      return;

    closeTop();

    rejectRecovery = false;
    busy = false;
    solarEnabled = true;
    sendReady();
  }

  updateLED();
  delay(10);
}
