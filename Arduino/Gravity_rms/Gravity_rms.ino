/*
sensitivity:
https://dfimg.dfrobot.com/wiki/21162/SEN0412_h3lis200dl-triple-axis-accelerometer_datasheet_V1.0.pdf
page 9: Factor So:
  // g factor at 100g range = 780  m/g
  // g factor at 200g range = 1560 m/g

########## Registers:: 
const uint8_t CTRL_REG1    = 0x20; // Selects power mode, output data rate, and enables the X, Y, and Z axes.
const uint8_t CTRL_REG2    = 0x21; // Configures the high-pass filter mode, cutoff frequency, and filter routing.
const uint8_t CTRL_REG3    = 0x22; // Configures interrupt polarity, push-pull/open-drain mode, latching, and pin routing.
const uint8_t CTRL_REG4    = 0x23; // Selects the ±100 g or ±200 g range and the SPI 3-wire/4-wire mode.
const uint8_t CTRL_REG5    = 0x24; // Enables or disables the sleep-to-wake function.
const uint8_t REFERENCE    = 0x26; // Sets the reference value used by the high-pass filter.

const uint8_t INT1_CFG     = 0x30; // Selects X/Y/Z high/low events and AND/OR logic for interrupt generator 1.
const uint8_t INT1_THS     = 0x32; // Sets the acceleration threshold for interrupt generator 1.
const uint8_t INT1_DURATION = 0x33; // Sets how long an event must persist before interrupt 1 is recognized.

const uint8_t INT2_CFG     = 0x34; // Selects X/Y/Z high/low events and AND/OR logic for interrupt generator 2.
const uint8_t INT2_THS     = 0x36; // Sets the acceleration threshold for interrupt generator 2.
const uint8_t INT2_DURATION = 0x37; // Sets how long an event must persist before interrupt 2 is recognized.

- Maximum output data rate: 1,000 samples/second
- Maximum useful signal bandwidth: approximately 500 Hz
- Maximum I²C clock: 400 kHz
For accurate sampling, check bit 3 (ZYXDA) of STATUS_REG (0x27) 
and only accumulate when a new XYZ sample is available. 
Otherwise, a fast loop could count duplicate samples.  

*/
#include <math.h>
#include <Wire.h>

const uint8_t ADDR = 0x19;
const uint16_t SAMPLE_SIZE = 100; // effects report time
const float G_PER_COUNT = 1.560f;

uint32_t sumX2 = 0, sumY2 = 0, sumZ2 = 0;
uint16_t samples = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  Wire.beginTransmission(ADDR);
  Wire.write(0x23);       // CTRL_REG4
  Wire.write(0x10);       // ±200 g
  Wire.endTransmission();

  Wire.beginTransmission(ADDR);
  Wire.write(0x20);       // CTRL_REG1
  Wire.write(0x3F);       // 1000 Hz, enable XYZ
  Wire.endTransmission();
}

void loop() {
  int8_t x, y, z;

  // Wait for a new XYZ sample
  Wire.beginTransmission(ADDR);
  Wire.write(0x27);       // STATUS_REG
  Wire.endTransmission(false);
  Wire.requestFrom(ADDR, 1);

  if (!(Wire.read() & 0x08))
    return;

  Wire.beginTransmission(ADDR);
  Wire.write(0xA9);       // OUT_X + auto-increment
  Wire.endTransmission(false);
  Wire.requestFrom(ADDR, 5);

  x = (int8_t)Wire.read();
  Wire.read();
  y = (int8_t)Wire.read();
  Wire.read();
  z = (int8_t)Wire.read();

  sumX2 += (int32_t)x * x;
  sumY2 += (int32_t)y * y;
  sumZ2 += (int32_t)z * z;

  if (++samples == SAMPLE_SIZE) {
    Serial.print("RMS: ");
    Serial.print(sqrtf((float)sumX2 / SAMPLE_SIZE) * G_PER_COUNT);
    Serial.print(", ");
    Serial.print(sqrtf((float)sumY2 / SAMPLE_SIZE) * G_PER_COUNT);
    Serial.print(", ");
    Serial.println(sqrtf((float)sumZ2 / SAMPLE_SIZE) * G_PER_COUNT);

    sumX2 = sumY2 = sumZ2 = samples = 0;
  }
}
