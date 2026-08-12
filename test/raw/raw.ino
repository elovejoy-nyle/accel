#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL343.h>

#define ADXL343_SCK 13
#define ADXL343_MISO 12
#define ADXL343_MOSI 11
#define ADXL343_CS 10

Adafruit_ADXL343 accel = Adafruit_ADXL343(343, &Wire);

//const uint8_t ADXL343_ADDRESS = 0x53;
const uint8_t REG_DEVID = 0x00;
const uint8_t REG_BW_RATE = 0x2C;
const uint8_t REG_POWER_CTL = 0x2D;
const uint8_t REG_DATA_FORMAT = 0x31;
const uint8_t REG_DATA_START = 0x32;

const uint16_t SAMPLE_RATE_HZ = 800;
const uint32_t SAMPLE_PERIOD_US = 1000000UL / SAMPLE_RATE_HZ;
const uint32_t STATUS_PERIOD_MS = 1000;

uint32_t sequenceNumber = 0;
uint32_t nextSampleUs = 0;
uint32_t nextStatusMs = 0;
bool sensorReady = false;

bool readRegisterChecked(uint8_t reg, uint8_t &value){
    Wire.beginTransmission(ADXL343_ADDRESS);
    Wire.write(reg);

    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom(ADXL343_ADDRESS, (uint8_t)1) != 1) {
        return false;
    }

    value = Wire.read();
    return true;
}

bool readRaw(int16_t &x, int16_t &y, int16_t &z){
    uint8_t data[6];

    Wire.beginTransmission(ADXL343_ADDRESS);
    Wire.write(REG_DATA_START);

    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom(ADXL343_ADDRESS, (uint8_t)6) != 6) {
        return false;
    }

    for (uint8_t i = 0; i < 6; i++) {
        data[i] = Wire.read();
    }

    x = (int16_t)((uint16_t)data[1] << 8 | data[0]);
    y = (int16_t)((uint16_t)data[3] << 8 | data[2]);
    z = (int16_t)((uint16_t)data[5] << 8 | data[4]);
    return true;
}

bool configurationIsValid(){
    uint8_t deviceId;
    uint8_t bandwidth;
    uint8_t powerControl;
    uint8_t dataFormat;

    if (!readRegisterChecked(REG_DEVID, deviceId) || deviceId != 0xE5) {
        return false;
    }

    if (!readRegisterChecked(REG_BW_RATE, bandwidth) ||
        !readRegisterChecked(REG_POWER_CTL, powerControl) ||
        !readRegisterChecked(REG_DATA_FORMAT, dataFormat)) {
        return false;
    }

    // 0x0D = 800 Hz, POWER_CTL bit 3 = measurement mode.
    // DATA_FORMAT bit 3 = full resolution and bits 1:0 = +/-16 g.
    return (bandwidth & 0x0F) == 0x0D &&
           (powerControl & 0x08) != 0 &&
           (dataFormat & 0x0B) == 0x0B;
}

void printStatus(const char *status, const char *reason){
    Serial.print("#sensor=ADXL343,id=0xE5,status=");
    Serial.print(status);
    Serial.print(",rate=");
    Serial.print(SAMPLE_RATE_HZ);
    Serial.print(",range=16g,format=raw,reason=");
    Serial.println(reason);
}

bool startSensor(const char *reason){
    if (!accel.begin(ADXL343_ADDRESS)) {
        Serial.print("#sensor=ADXL343,id=invalid,status=error,reason=");
        Serial.println(reason);
        return false;
    }

    accel.setRange(ADXL343_RANGE_16_G);
    accel.setDataRate(ADXL343_DATARATE_800_HZ);

    if (!configurationIsValid()) {
        Serial.println("#sensor=ADXL343,id=0xE5,status=error,reason=config_failed");
        return false;
    }

    sequenceNumber = 0;
    nextSampleUs = micros() + SAMPLE_PERIOD_US;
    nextStatusMs = millis() + STATUS_PERIOD_MS;
    printStatus("ok", reason);
    return true;
}

void setup(){
    Serial.begin(115200);
    Wire.begin();
    Wire.setClock(400000);

    delay(100);
    sensorReady = startSensor("startup");
}

void loop(){
    if (!sensorReady) {
        delay(1000);
        sensorReady = startSensor("retry");
        return;
    }

    uint32_t nowMs = millis();
    if ((int32_t)(nowMs - nextStatusMs) >= 0) {
        nextStatusMs += STATUS_PERIOD_MS;

        if (!configurationIsValid()) {
            Serial.println("#sensor=ADXL343,status=error,reason=disconnected_or_reset");
            sensorReady = false;
            return;
        }
    }

    uint32_t nowUs = micros();
    if ((int32_t)(nowUs - nextSampleUs) < 0) {
        return;
    }

    nextSampleUs += SAMPLE_PERIOD_US;

    int16_t x;
    int16_t y;
    int16_t z;

    if (!readRaw(x, y, z)) {
        Serial.println("#sensor=ADXL343,status=error,reason=i2c_read_failed");
        sensorReady = false;
        return;
    }

    //Serial.print("n");  // frame count
    //Serial.print(sequenceNumber++);
    Serial.print(",x");
    Serial.print(x);
    Serial.print(",y");
    Serial.print(y);
    Serial.print(",z");
    Serial.println(z);
}
