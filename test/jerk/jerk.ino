/*
Magnitude: total acceleration regardless of direction.
Dynamic magnitude: acceleration after removing gravity.
Shock: peak dynamic acceleration exceeding a threshold.
Jerk: how quickly acceleration changes, in m/s³.
Vibration: RMS dynamic acceleration over a time window.
Peak-to-peak vibration.
Crest factor: peak vibration divided by RMS vibration.
raw: maybe better to just return raw
error correction:
   place in space, elevation, heat, gps location, could alter
   responses. in theory we can correct for a certain amount of
   Noise.
*/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL343.h>
#include <math.h>

Adafruit_ADXL343 accel = Adafruit_ADXL343(12345);

const float SAMPLE_RATE = 400.0;
const uint32_t SAMPLE_US = 1000000UL / SAMPLE_RATE;

// Adjust these after observing the equipment.
const float SHOCK_THRESHOLD_G = 3.0;
const float JERK_THRESHOLD_G_S = 100.0;

float previousDynamicG = 0.0;
float gravityG = 1.0;

// Vibration measurement window
const int WINDOW_SAMPLES = 200;  // 0.5 second at 400 Hz
float sumSquares = 0.0;
float peakDynamicG = 0.0;
float minimumDynamicG = 1000.0;
float maximumDynamicG = -1000.0;
int sampleCount = 0;

uint32_t nextSample = 0;

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        delay(10);
    }

    Wire.setClock(400000);

    if (!accel.begin()) {
        Serial.println("ADXL343 not detected.");
        while (1) {
            delay(10);
        }
    }

    accel.setRange(ADXL343_RANGE_16_G);
    accel.setDataRate(ADXL343_DATARATE_400_HZ);

    nextSample = micros();

    Serial.println("ADXL343 shock, jerk, and vibration monitor");
    Serial.println("magnitude_g,dynamic_g,jerk_g_s,rms_g,peak_g");
}

void loop()
{
    if ((int32_t)(micros() - nextSample) < 0) {
        return;
    }

    nextSample += SAMPLE_US;

    sensors_event_t event;
    accel.getEvent(&event);

    float xG = event.acceleration.x / SENSORS_GRAVITY_STANDARD;
    float yG = event.acceleration.y / SENSORS_GRAVITY_STANDARD;
    float zG = event.acceleration.z / SENSORS_GRAVITY_STANDARD;

    float magnitudeG = sqrtf(xG * xG + yG * yG + zG * zG);

    // Slowly track gravity and mounting orientation.
    gravityG += 0.01f * (magnitudeG - gravityG);

    float dynamicG = magnitudeG - gravityG;
    float jerkGS = (dynamicG - previousDynamicG) * SAMPLE_RATE;
    previousDynamicG = dynamicG;

    float absoluteDynamicG = fabsf(dynamicG);

    sumSquares += dynamicG * dynamicG;

    if (absoluteDynamicG > peakDynamicG) {
        peakDynamicG = absoluteDynamicG;
    }

    if (dynamicG < minimumDynamicG) {
        minimumDynamicG = dynamicG;
    }

    if (dynamicG > maximumDynamicG) {
        maximumDynamicG = dynamicG;
    }

    sampleCount++;

    if (absoluteDynamicG >= SHOCK_THRESHOLD_G) {
        Serial.print("SHOCK,");
        Serial.print(absoluteDynamicG, 3);
        Serial.println(" g");
    }

    if (fabsf(jerkGS) >= JERK_THRESHOLD_G_S) {
        Serial.print("JERK,");
        Serial.print(jerkGS, 1);
        Serial.println(" g/s");
    }

    if (sampleCount >= WINDOW_SAMPLES) {
        float rmsG = sqrtf(sumSquares / sampleCount);
        float peakToPeakG = maximumDynamicG - minimumDynamicG;
        float crestFactor = rmsG > 0.0 ? peakDynamicG / rmsG : 0.0;

        Serial.print(magnitudeG, 3);
        Serial.print(",");
        Serial.print(dynamicG, 3);
        Serial.print(",");
        Serial.print(jerkGS, 1);
        Serial.print(",");
        Serial.print(rmsG, 4);
        Serial.print(",");
        Serial.print(peakDynamicG, 3);
        Serial.print(",p2p=");
        Serial.print(peakToPeakG, 3);
        Serial.print(",crest=");
        Serial.println(crestFactor, 2);

        sumSquares = 0.0;
        peakDynamicG = 0.0;
        minimumDynamicG = 1000.0;
        maximumDynamicG = -1000.0;
        sampleCount = 0;
    }
}
