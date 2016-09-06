#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <SD.h>

Adafruit_INA219 ina219;

void setup(void)
{
    Serial.begin(115200);

    uint32_t currentFrequency;

    if (!SD.begin(4)) {
        while (true) {
            pinMode(13, HIGH);
            delay(250);
            pinMode(13, LOW);
            delay(250);
        }
    }

    // Initialize the INA219.
    // By default the initialization will use the largest range (32V, 2A).  However
    // you can call a setCalibration function to change this range (see comments).
    ina219.begin();
    // To use a slightly lower 32V, 1A range (higher precision on amps):
    ina219.setCalibration_32V_1A();
    // Or to use a lower 16V, 400mA range (higher precision on volts and amps):
    // ina219.setCalibration_16V_400mA();

    Serial.println("Measuring voltage and current with INA219 ...");
}

void loop(void)
{
    float shuntvoltage = 0;
    float busvoltage = 0;
    float current_mA = 0;
    float loadvoltage = 0;

    File file = SD.open("LOAD.CSV", FILE_WRITE);
    if (!file) {
        for (uint8_t i = 0; i < 10; ++i) {
            delay(1000);
        }
        return;
    }

    while (true) {
        shuntvoltage = ina219.getShuntVoltage_mV();
        busvoltage = ina219.getBusVoltage_V();
        current_mA = ina219.getCurrent_mA();
        loadvoltage = busvoltage + (shuntvoltage / 1000);

        // Serial.print("Bus Voltage:   "); Serial.print(busvoltage); Serial.println(" V");
        // Serial.print("Shunt Voltage: "); Serial.print(shuntvoltage); Serial.println(" mV");
        // Serial.print("Load Voltage:  "); Serial.print(loadvoltage); Serial.println(" V");
        // Serial.print("Current:       "); Serial.print(current_mA); Serial.println(" mA");
        // Serial.println("");

        file.print(millis());
        file.print(",");
        file.print(busvoltage);
        file.print(",");
        file.print(shuntvoltage);
        file.print(",");
        file.print(loadvoltage);
        file.print(",");
        file.print(current_mA);
        file.println("");
        file.flush();

        delay(500);
    }
}
