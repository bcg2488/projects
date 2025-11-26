#include <Wire.h>
#include <Adafruit_ICM20948.h>
//#include "Adafruit_BMP3XX.h"
#include "Adafruit_VL53L0X.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_ICM20X.h>
#include <Servo.h>
#define SEALEVELPRESSURE_HPA (1013.25)

// Global sensor objects to avoid re-instantiation
//Adafruit_BMP3XX bmp;
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
Adafruit_ICM20948 icm;

Servo left;
Servo Right;  
Servo frontleft; 
Servo frontright; 
Servo backleft; 
Servo backright; 

int Speed;

void setup() {
  
  pinMode(LED_BUILTIN, OUTPUT);
  left.attach(9,1000,2000);
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  while (!Serial) delay(10); // Wait for serial console
  //delay(100);

  if (!lox.begin()) {
    Serial.println(F("LIDAR Error"));
    while (1);
  }

  /*
  if (!bmp.begin_I2C()) {
    Serial.println(F("BMP Error"));
    while (1);
  }
  */

  if (!icm.begin_I2C()) {
    Serial.println(F("IMU Error"));
    while (1);
  }

  /*
  // BMP sensor setup
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);
  */
}

void loop() {

  //check for incoming data
  if(Serial.available() > 0)
  {
    //
    String message = Serial.readStringUntil('\n');
    message.trim();
    Serial.print("got message");
    Serial.print(message);
    Serial.print('\n');

    int colonDex = message.indexOf(':');

    if(colonDex != -1)
    {
      //getting command from message
      String Command = message.substring(0, colonDex);
      Command.trim();

      //getting value
      String value = message.substring(colonDex + 1);
      value.trim();


      //printing command
      Serial.print("command = ");
      Serial.println(Command);

      //printing value
      Serial.print("value = ");
      Serial.println(value);

      //controlling speed
      if(Command.equals("left"))
      {
        Speed = value.toInt();
        left.write(Speed);

      }

      if(Command.equals("right"))
      {
        Speed = value.toInt();
        //right.write(Speed);

      }

      if(Command.equals("frontleft"))
      {
        Speed = value.toInt();
        //frontleft.write(Speed);

      }

      if(Command.equals("frontright"))
      {
        Speed = value.toInt();
        //frontright.write(Speed);

      }

      //just in case
      if(Command.equals("backleft"))
      {
        Speed = value.toInt();
        //backleft.write(Speed);

      }

      //just in case
      if(Command.equals("backright"))
      {
        Speed = value.toInt();
        //backright.write(Speed);

      }

    }
  }
  // Get IMU data (omit mag to save space)
  sensors_event_t accel, gyro, mag, temp;

  icm.getEvent(&accel, &gyro, &temp, &mag);

  Serial.println(F("Accelerometer in m/s^2"));
  Serial.print("X: ");
  Serial.println(accel.acceleration.x);
  Serial.print("Y: ");
  Serial.println(accel.acceleration.y);
  Serial.print("Z: ");
  Serial.println(accel.acceleration.z);

  Serial.println(F("Magnetometer in microtesla (uT)"));
  Serial.print("X: ");
  Serial.println(mag.magnetic.x);
  Serial.print("Y: ");
  Serial.println(mag.magnetic.y);
  Serial.print("Z: ");
  Serial.println(mag.magnetic.z);

  Serial.println(F("Gyro in radians per second"));
  Serial.print("X: ");
  Serial.println(gyro.gyro.x);
  Serial.print("Y: ");
  Serial.println(gyro.gyro.y);
  Serial.print("Z: ");
  Serial.println(gyro.gyro.z);

  delay(1000);

  /*
  // Read BMP280
  if (bmp.performReading()) {
    Serial.print(F("Temperature: ")); Serial.print(bmp.temperature); Serial.println(F(" *C"));
    Serial.print(F("Pressure: ")); Serial.print(bmp.pressure / 100.0); Serial.println(F(" hPa"));
    Serial.print(F("Altitude: ")); Serial.print(bmp.readAltitude(SEALEVELPRESSURE_HPA)); Serial.println(F(" m"));
  } else {
    Serial.println(F("BMP read fail"));
  }

  delay(3000);
  */

  // Read LIDAR
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);

  if (measure.RangeStatus != 4) {
    Serial.print(F("Distance: ")); Serial.print(measure.RangeMilliMeter); Serial.println(F(" mm"));
  } else {
    Serial.println(F("LIDAR senses no obstacle(s)"));
  }

  delay(1000);

  Serial.println("done");
}
