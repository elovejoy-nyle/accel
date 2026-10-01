#include <Wire.h>
#include <math.h>

const uint8_t ADXL357_ADDR = 0x1D;

const uint8_t REG_STATUS    = 0x04;
const uint8_t REG_XDATA3    = 0x08;
const uint8_t REG_FILTER    = 0x28;
const uint8_t REG_RANGE     = 0x2C;
const uint8_t REG_POWER_CTL = 0x2D;
const uint8_t REG_RESET     = 0x2F;

const uint16_t SAMPLE_SIZE = 100;

// 400000 supports up to 500 SPS with available sensor settings.
// Change to 1000000 for 1000 or 2000 SPS.
const uint32_t I2C_SPEED = 400000;

// 0x03 = 500 SPS, 125 Hz low-pass; use with 400 kHz I2C.
// 0x02 = 1000 SPS, 250 Hz low-pass; use 1 MHz I2C.
// 0x01 = 2000 SPS, 500 Hz low-pass; use 1 MHz I2C.
// 0x00 = 4000 SPS, 1000 Hz low-pass; use SPI.
const uint8_t FILTER_SETTING = 0x03;

// 0x01 = +/-10 g
// 0x02 = +/-20 g
// 0x03 = +/-40 g maximum; there is no 200 g mode.
const uint8_t RANGE_SETTING = 0x03;

int32_t minX;
int32_t maxX;
int32_t minY;
int32_t maxY;
int32_t minZ;
int32_t maxZ;

double sumMagnitudeSquared;
uint16_t samples;

bool writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(ADXL357_ADDR);
    Wire.write(reg);
    Wire.write(value);

    return Wire.endTransmission() == 0;
}

bool readRegister(uint8_t reg, uint8_t &value)
{
    Wire.beginTransmission(ADXL357_ADDR);
    Wire.write(reg);

    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom(ADXL357_ADDR, (uint8_t)1) != 1) {
        return false;
    }

    value = Wire.read();
    return true;
}

int32_t decode20Bit(uint8_t high, uint8_t middle, uint8_t low)
{
    int32_t value =
        ((int32_t)high << 12) |
        ((int32_t)middle << 4) |
        ((int32_t)low >> 4);

    if (value & 0x80000) {
        value |= 0xFFF00000;
    }

    return value;
}

bool readAxes(int32_t &x, int32_t &y, int32_t &z)
{
    uint8_t data[9];

    Wire.beginTransmission(ADXL357_ADDR);
    Wire.write(REG_XDATA3);

    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom(ADXL357_ADDR, (uint8_t)9) != 9) {
        return false;
    }

    for (uint8_t i = 0; i < 9; i++) {
        data[i] = Wire.read();
    }

    x = decode20Bit(data[0], data[1], data[2]);
    y = decode20Bit(data[3], data[4], data[5]);
    z = decode20Bit(data[6], data[7], data[8]);

    return true;
}

bool configureSensor()
{
    if (!writeRegister(REG_RESET, 0x52)) {
        return false;
    }

    delay(20);

    // Standby while changing settings.
    if (!writeRegister(REG_POWER_CTL, 0x01)) {
        return false;
    }

    if (!writeRegister(REG_FILTER, FILTER_SETTING)) {
        return false;
    }

    if (!writeRegister(REG_RANGE, RANGE_SETTING)) {
        return false;
    }

    // Measurement mode.
    if (!writeRegister(REG_POWER_CTL, 0x00)) {
        return false;
    }

    delay(20);
    return true;
}

void resetWindow()
{
    minX = INT32_MAX;
    maxX = INT32_MIN;

    minY = INT32_MAX;
    maxY = INT32_MIN;

    minZ = INT32_MAX;
    maxZ = INT32_MIN;

    sumMagnitudeSquared = 0.0;
    samples = 0;
}

void setup()
{
    Serial.begin(115200);

    Wire.begin();
    Wire.setClock(I2C_SPEED);

    while (!configureSensor()) {
        delay(1000);
    }

    resetWindow();
}

void loop()
{
    uint8_t status;

    if (!readRegister(REG_STATUS, status)) {
        while (!configureSensor()) {
            delay(1000);
        }

        resetWindow();
        return;
    }

    if (!(status & 0x01)) {
        return;
    }

    int32_t x;
    int32_t y;
    int32_t z;

    if (!readAxes(x, y, z)) {
        return;
    }

    if (x < minX) minX = x;
    if (x > maxX) maxX = x;

    if (y < minY) minY = y;
    if (y > maxY) maxY = y;

    if (z < minZ) minZ = z;
    if (z > maxZ) maxZ = z;

    sumMagnitudeSquared +=
        (double)x * x +
        (double)y * y +
        (double)z * z;

    samples++;

    if (samples >= SAMPLE_SIZE) {
        int32_t peakToPeakX = maxX - minX;
        int32_t peakToPeakY = maxY - minY;
        int32_t peakToPeakZ = maxZ - minZ;

        double rmsTotal =
            sqrt(sumMagnitudeSquared / (double)samples);

        Serial.print(peakToPeakX);
        Serial.print(',');
        Serial.print(peakToPeakY);
        Serial.print(',');
        Serial.print(peakToPeakZ);
        Serial.print(',');
        Serial.println(rmsTotal, 3);

        resetWindow();
    }
}
