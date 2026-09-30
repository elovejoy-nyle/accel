#include <Wire.h>
#include <arduinoFFT.h>

const uint8_t ADDR = 0x19;
const uint16_t N = 1024;          // Must be a power of two
const float SAMPLE_RATE = 1000.0;
const float G_PER_COUNT = 0.780;  // ±100 g mode

int8_t xData[N], yData[N], zData[N];
float vReal[N], vImag[N];

ArduinoFFT<float> FFT(vReal, vImag, N, SAMPLE_RATE);

void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ADDR, (uint8_t)1);
  return Wire.read();
}

bool readXYZ(int8_t &x, int8_t &y, int8_t &z) {
  Wire.beginTransmission(ADDR);
  Wire.write(0xA9);  // OUT_X + auto-increment
  Wire.endTransmission(false);

  if (Wire.requestFrom(ADDR, (uint8_t)5) != 5)
    return false;

  x = (int8_t)Wire.read();
  Wire.read();
  y = (int8_t)Wire.read();
  Wire.read();
  z = (int8_t)Wire.read();

  return true;
}

void setup() {
  Serial.begin(115200);

  Wire.begin();
  Wire.setClock(400000);

  writeRegister(0x23, 0x00);  // ±100 g
  writeRegister(0x20, 0x3F);  // 1000 Hz, enable XYZ
}

void loop() {
  // Collect exactly 1024 new XYZ samples
  for (uint16_t i = 0; i < N;) {
    if (!(readRegister(0x27) & 0x08))
      continue;

    if (readXYZ(xData[i], yData[i], zData[i]))
      i++;
  }

  int32_t sumX = 0, sumY = 0, sumZ = 0;
  uint32_t sumX2 = 0, sumY2 = 0, sumZ2 = 0;

  int8_t minX = 127, minY = 127, minZ = 127;
  int8_t maxX = -128, maxY = -128, maxZ = -128;

  for (uint16_t i = 0; i < N; i++) {
    int16_t x = xData[i];
    int16_t y = yData[i];
    int16_t z = zData[i];

    sumX += x;
    sumY += y;
    sumZ += z;

    sumX2 += x * x;
    sumY2 += y * y;
    sumZ2 += z * z;

    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
    if (z < minZ) minZ = z;
    if (z > maxZ) maxZ = z;
  }

  float meanX = (float)sumX / N;
  float meanY = (float)sumY / N;
  float meanZ = (float)sumZ / N;

  float varianceX = (float)sumX2 / N - meanX * meanX;
  float varianceY = (float)sumY2 / N - meanY * meanY;
  float varianceZ = (float)sumZ2 / N - meanZ * meanZ;

  if (varianceX < 0) varianceX = 0;
  if (varianceY < 0) varianceY = 0;
  if (varianceZ < 0) varianceZ = 0;

  // Total mean-removed XYZ RMS
  float rmsTotal =
      sqrtf(varianceX + varianceY + varianceZ) * G_PER_COUNT;

  float ppX = (maxX - minX) * G_PER_COUNT;
  float ppY = (maxY - minY) * G_PER_COUNT;
  float ppZ = (maxZ - minZ) * G_PER_COUNT;

  // Root-sum-square of the three axis peak-to-peak values
  float ppTotal = sqrtf(ppX * ppX + ppY * ppY + ppZ * ppZ);

  // Select the axis containing the most vibration energy
  int8_t *fftData = xData;
  float fftMean = meanX;
  char fftAxis = 'X';

  if (varianceY > varianceX && varianceY >= varianceZ) {
    fftData = yData;
    fftMean = meanY;
    fftAxis = 'Y';
  } else if (varianceZ > varianceX && varianceZ > varianceY) {
    fftData = zData;
    fftMean = meanZ;
    fftAxis = 'Z';
  }

  // Remove DC offset and prepare FFT
  for (uint16_t i = 0; i < N; i++) {
    vReal[i] = (fftData[i] - fftMean) * G_PER_COUNT;
    vImag[i] = 0;
  }

  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  float frequency = FFT.majorPeak();

  Serial.print("RMS total: ");
  Serial.print(rmsTotal, 2);
  Serial.print(" g, P-P total: ");
  Serial.print(ppTotal, 2);
  Serial.print(" g, Frequency: ");
  Serial.print(frequency, 2);
  Serial.print(" Hz, Axis: ");
  Serial.println(fftAxis);

  Serial.print("P-P XYZ: ");
  Serial.print(ppX, 2);
  Serial.print(", ");
  Serial.print(ppY, 2);
  Serial.print(", ");
  Serial.println(ppZ, 2);
}
