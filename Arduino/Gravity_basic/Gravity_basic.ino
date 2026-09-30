#include <Wire.h>

// SEN0412: https://wiki.dfrobot.com/sen0412/
// Datasheet:
// https://dfimg.dfrobot.com/wiki/21162/SEN0412_h3lis200dl-triple-axis-accelerometer_datasheet_V1.0.pdf

const uint8_t ADDR = 0x19;

// Range settings must match:
// +/-100 g: CTRL_REG4 = 0x00, G_PER_COUNT = 0.78f
// +/-200 g: CTRL_REG4 = 0x10, G_PER_COUNT = 1.56f
const uint8_t RANGE_SETTING = 0x10;
const float G_PER_COUNT = 1.56f;

void setup() {
  Serial.begin(115200);
  //Serial.begin(921600);  // RP2040 USB serial ignores baud; applies to UART only

  Wire.begin();
  Wire.setClock(400000);  // 400 kHz I2C

  Wire.beginTransmission(ADDR);
  Wire.write(0x23);       // CTRL_REG4: full-scale range selection
  Wire.write(RANGE_SETTING);
  Wire.endTransmission();

  Wire.beginTransmission(ADDR);
  Wire.write(0x20);       // CTRL_REG1: operating mode, sample rate, enable XYZ

  // Normal mode with XYZ enabled:
  // 0x27 =   50 Hz
  // 0x2F =  100 Hz
  // 0x37 =  400 Hz
  // 0x3F = 1000 Hz
  Wire.write(0x3F);
  Wire.endTransmission();
}

void loop() {
  Wire.beginTransmission(ADDR);
  Wire.write(0xA9);       // OUT_X (0x29) + auto-increment
  if (Wire.endTransmission(false) != 0) return;
  if (Wire.requestFrom(ADDR, (uint8_t)5) != 5) return;

  int8_t x = Wire.read();
  Wire.read();            // Skip reserved 0x2A
  int8_t y = Wire.read();
  Wire.read();            // Skip reserved 0x2C
  int8_t z = Wire.read();

  Serial.print(x * G_PER_COUNT);  // Acceleration in g
  Serial.print(',');
  Serial.print(y * G_PER_COUNT);
  Serial.print(',');
  Serial.println(z * G_PER_COUNT);

  //delay(100);
}
