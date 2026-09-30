/* CONFIG INFO::
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

// SEN0412: https://wiki.dfrobot.com/sen0412/
// Datasheet:
// https://dfimg.dfrobot.com/wiki/21162/SEN0412_h3lis200dl-triple-axis-accelerometer_datasheet_V1.0.pdf

const uint8_t ADDR = 0x19;
const uint16_t SAMPLE_SIZE = 100; // Samples per report; ~100 ms at 1000 Hz

// Range settings must match:
// +/-100 g: CTRL_REG4 = 0x00, G_PER_COUNT = 0.780f
// +/-200 g: CTRL_REG4 = 0x10, G_PER_COUNT = 1.560f
const uint8_t RANGE_SETTING = 0x10;
const float G_PER_COUNT = 1.560f;

uint32_t sumX2 = 0, sumY2 = 0, sumZ2 = 0;
uint16_t samples = 0;
int8_t minX, maxX, minY, maxY, minZ, maxZ;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);  // 400 kHz I2C

  Wire.beginTransmission(ADDR);
  Wire.write(0x23);       // CTRL_REG4: full-scale range selection
  Wire.write(RANGE_SETTING);
  Wire.endTransmission();

  Wire.beginTransmission(ADDR);
  Wire.write(0x20);       // CTRL_REG1: normal mode, sample rate, enable XYZ
  // 0x27 = 50 Hz, 0x2F = 100 Hz, 0x37 = 400 Hz, 0x3F = 1000 Hz
  Wire.write(0x3F);
  Wire.endTransmission();
}

void loop() {
  // Wait for a new XYZ sample
  Wire.beginTransmission(ADDR);
  Wire.write(0x27);       // STATUS_REG
  if (Wire.endTransmission(false) != 0) return;
  if (Wire.requestFrom(ADDR, (uint8_t)1) != 1) return;
  if (!(Wire.read() & 0x08)) return;

  Wire.beginTransmission(ADDR);
  Wire.write(0xA9);       // OUT_X (0x29) + auto-increment
  if (Wire.endTransmission(false) != 0) return;
  if (Wire.requestFrom(ADDR, (uint8_t)5) != 5) return;

  int8_t x = (int8_t)Wire.read();
  Wire.read();            // Skip reserved 0x2A
  int8_t y = (int8_t)Wire.read();
  Wire.read();            // Skip reserved 0x2C
  int8_t z = (int8_t)Wire.read();

  if (samples == 0) {
    minX = maxX = x;
    minY = maxY = y;
    minZ = maxZ = z;
  } else {
    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
    if (z < minZ) minZ = z;
    if (z > maxZ) maxZ = z;
  }

  sumX2 += (int32_t)x * x;
  sumY2 += (int32_t)y * y;
  sumZ2 += (int32_t)z * z;

  if (++samples == SAMPLE_SIZE) {
    float rmsTotal = sqrtf(
      ((float)sumX2 + (float)sumY2 + (float)sumZ2) / samples
    ) * G_PER_COUNT;

    // Raw CSV: X peak-to-peak, Y peak-to-peak, Z peak-to-peak, total RMS
    Serial.print(((int16_t)maxX - minX) * G_PER_COUNT, 3);
    Serial.print(',');
    Serial.print(((int16_t)maxY - minY) * G_PER_COUNT, 3);
    Serial.print(',');
    Serial.print(((int16_t)maxZ - minZ) * G_PER_COUNT, 3);
    Serial.print(',');
    Serial.println(rmsTotal, 3);

    sumX2 = sumY2 = sumZ2 = 0;
    samples = 0;
  }
}
