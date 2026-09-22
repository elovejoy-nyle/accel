/*
x,y,z sensor reads
squqre every sample of X and sum them.
divide by number of samples.
take the square root. 

axrms = ax^2
ayrms = ay^2 
azrms = az^2
 
aRMS_total = sqrt()


*/


#include <Wire.h>
const uint8_t ADDR = 0x19;

void setup() {
  Serial.begin(115200); 
  //Serial.begin(921600);   // kee boar max baud (rpi 2040) ignored on usb-C port only valid on UART interface
  Wire.begin();
  Wire.setClock(400000);  // 400 kHz I²C
  Wire.beginTransmission(ADDR);
  Wire.write(0x20);       // CTRL_REG1
  //Wire.write(0x27);       // Normal mode, 50 Hz, enable XYZ
  Wire.write(0x3F);       // Normal mode, 1000 Hz, enable XYZ
  Wire.endTransmission();
}

void loop() {
  Wire.beginTransmission(ADDR);
  Wire.write(0xA9);       // OUT_X (0x29) + auto-increment
  Wire.endTransmission(false);
  Wire.requestFrom(ADDR, 5);

  int8_t x = Wire.read();
  Wire.read();            // Skip reserved 0x2A
  int8_t y = Wire.read();
  Wire.read();            // Skip reserved 0x2C
  int8_t z = Wire.read();

  Serial.print(x * 0.78);
  Serial.print(',');
  Serial.print(y * 0.78);
  Serial.print(',');
  Serial.println(z * 0.78);

  //delay(100);
}
