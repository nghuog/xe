#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <esp_system.h>
#include <U8g2lib.h>

const char* ssid = "ESP32_Robot";
const char* password = "12345678";

// --- SO DO CHAN TB6612FNG ---
#define BIN1 14
#define BIN2 13
#define PWMB 23

#define AIN1 25
#define AIN2 26
#define PWMA 27

#define STBY 4

// Chan tin hieu coi chuyen sang GPIO5 de tach khoi GPIO2 (chan boot).
#define HORN_PIN 5

// MPU6050 I2C pins tren ESP32 DevKit thong dung.
#define MPU_SDA 21
#define MPU_SCL 22
#define MPU_ADDR_LOW 0x68
#define MPU_ADDR_HIGH 0x69
#define OLED_ADDR 0x3C

#define DRIVE_WATCHDOG_MS 650

// --- SO DO CHAN ENCODER ---
#define ENC_L_A 18  // Motor Trai - Trang
#define ENC_L_B 19  // Motor Trai - Vang
#define ENC_R_A 32  // Motor Phai - Vang
#define ENC_R_B 33  // Motor Phai - Trang / Xanh

// Bien dieu khien
int currentSpeed = 0;       // Toc do ga (0 - 255)
int commandedSpeed = 0;
int steerAngle = 0;         // Goc vo lang (-45 do den +45 do)
int commandedSteerAngle = 0;
String gear = "D";          // D: Tien (Drive), R: Lui (Reverse)
String driveDirection = "stop";
unsigned long lastCmdTime = 0;
unsigned long lastDriveHeartbeat = 0;
unsigned long lastHornHeartbeat = 0;
bool hornActive = false;
bool mpuReady = false;
uint8_t mpuAddress = MPU_ADDR_LOW;
unsigned long lastMpuRead = 0;
unsigned long lastMpuMicros = 0;
float accelX = 0, accelY = 0, accelZ = 0;
float gyroX = 0, gyroY = 0, gyroZ = 0;
float pitch = 0, roll = 0;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C mochi(U8G2_R0, U8X8_PIN_NONE);
bool oledReady = false;
unsigned long lastOledUpdate = 0;
unsigned long lastBlinkChange = 0;
unsigned long nextBlinkDelay = 2500;
unsigned long nextMoodTime = 8000;
unsigned long moodEndTime = 0;
unsigned long lastLookTime = 0;
bool mochiBlink = false;
int mochiMode = 0; // 0 binh thuong, 1 vui, 2 nhay mat, 3 lim dim, 4 bat ngo.
int lookOffset = 0;
int bounceY = 0;

WebServer server(80);
bool i2cDevicePresent(uint8_t address);

void writeMpuRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

bool readMpuData() {
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x3B); // Bat dau tu ACCEL_XOUT_H, doc lien tuc 14 byte.
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)mpuAddress, (uint8_t)14) != 14) return false;

  int16_t rawAx = (Wire.read() << 8) | Wire.read();
  int16_t rawAy = (Wire.read() << 8) | Wire.read();
  int16_t rawAz = (Wire.read() << 8) | Wire.read();
  Wire.read(); Wire.read(); // Bo qua nhiet do.
  int16_t rawGx = (Wire.read() << 8) | Wire.read();
  int16_t rawGy = (Wire.read() << 8) | Wire.read();
  int16_t rawGz = (Wire.read() << 8) | Wire.read();

  accelX = rawAx / 16384.0f;
  accelY = rawAy / 16384.0f;
  accelZ = rawAz / 16384.0f;
svgdag\
gsg
Wire.beginTransmission(mpuAddress);
  Wire.write(0x6B); // PWR_MGMT_1
  Wire.write(0x00); // Thoat che do ngu.
  mpuReady = Wire.endTransmission() == 0;
  if (mpuReady) {
    delay(100);
    writeMpuRegister(0x1A, 0x03); // Loc thong thap.
    writeMpuRegister(0x1B, 0x00); // Gyro +/-250 deg/s.
    writeMpuRegister(0x1C, 0x00); // Accelerometer +/-2g.
    mpuReady = readMpuData();
  }
  if (mpuReady) {
    Serial.printf("MPU6050 da ket noi tai dia chi 0x%02X\n", mpuAddress);
  } else {
    Serial.println("Khong tim thay MPU6050 tai 0x68/0x69; kiem tra nguon, GND, SDA, SCL, AD0");
  }

  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT); pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT); pinMode(PWMB, OUTPUT);
  pinMode(STBY, OUTPUT);
  pinMode(HORN_PIN, OUTPUT);
  digitalWrite(HORN_PIN, LOW); // Mac dinh tat coi.

  digitalWrite(STBY, HIGH);
  applyMotorControl();

  pinMode(ENC_L_A, INPUT_PULLUP); pinMode(ENC_L_B, INPUT_PULLUP);
  pinMode(ENC_R_A, INPUT_PULLUP); pinMode(ENC_R_B, INPUT_PULLUP);

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false); // Giu ket noi web on dinh.
  WiFi.softAP(ssid, password);
  Serial.println("\nWiFi San Sang!");
  Serial.print("Dia chi Cockpit Web: ");
  Serial.println(WiFi.softAPIP());
  if (!oledReady) Serial.println("Khong tim thay OLED tai dia chi 0x3C");

  server.on("/", []() { server.send(200, "text/html", HTML_CONTENT); });

  server.on("/imu", []() {
    String json = "{\"ok\":" + String(mpuReady ? "true" : "false") +
      ",\"pitch\":" + String(pitch, 2) + ",\"roll\":" + String(roll, 2) +
      ",\"ax\":" + String(accelX, 3) + ",\"ay\":" + String(accelY, 3) +
      ",\"az\":" + String(accelZ, 3) + "}";
    server.send(200, "application/json", json);
  });



  // Coi chi keu khi nut dang duoc giu; heartbeat mat thi tu tat.
  server.on("/horn", []() {
    hornActive = server.arg("state") == "on";
    digitalWrite(HORN_PIN, hornActive ? HIGH : LOW);
    if (hornActive) lastHornHeartbeat = millis();
    server.send(200, "text/plain", "OK");
  });

  // Dieu khien 4 huong tu cac nut tren web.
  server.on("/drive", []() {
    String requestedDirection = server.arg("dir");
    driveDirection = requestedDirection;
    commandedSpeed = constrain(server.arg("speed").toInt(), 0, 255);
    commandedSteerAngle = server.hasArg("steer") ? constrain(server.arg("steer").toInt(), -45, 45) : 0;
    if (driveDirection == "stop") commandedSpeed = 0;
    lastDriveHeartbeat = millis();
    applyDriveCommand();
    server.send(200, "text/plain", "OK");
  });

  // API Dap ga / Nha ga
  server.on("/setSpeed", []() {
    if (server.hasArg("val")) {
      currentSpeed = server.arg("val").toInt();
      applyMotorControl();
    }
    server.send(200, "text/plain", "OK");
  });

  // API Dap phanh
  server.on("/brake", []() {
    currentSpeed = 0;
    applyMotorControl();
    server.send(200, "text/plain", "OK");
  });

  // API Vo lang be lai
  server.on("/setSteer", []() {
    if (server.hasArg("angle")) {
steerAngle = server.arg("angle").toInt();
      applyMotorControl();
    }
    server.send(200, "text/plain", "OK");
  });

  // API Chuyen can so D / R
  server.on("/setGear", []() {
    if (server.hasArg("g")) {
      gear = server.arg("g");
      applyMotorControl();
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  server.handleClient();
  if (driveDirection != "stop" && commandedSpeed > 0 && millis() - lastDriveHeartbeat > DRIVE_WATCHDOG_MS) {
    driveDirection = "stop";
    commandedSpeed = 0;
    currentSpeed = 0;
    applyMotorControl();
  }
  updateMpu();
  updateMochi();
  if (hornActive && millis() - lastHornHeartbeat > DRIVE_WATCHDOG_MS) {
    digitalWrite(HORN_PIN, LOW);
    hornActive = false;
  }
  delay(2);
}