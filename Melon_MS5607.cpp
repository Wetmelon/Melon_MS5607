/***************************************************************************
This is a library for the MS5607-02BA03 barometric pressure sensor

These sensors use I2C or SPI to communicate, though only I2C is implemented

Written by Paul Guenette
Apr 8, 2016

MIT License
***************************************************************************/

#include "Melon_MS5607.h"

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#ifdef __AVR_ATtiny85__
#include "TinyWireM.h"
#define Wire TinyWireM
#elif defined(TEENSYDUINO)
#include <i2c_t3.h>
#else
#include <Wire.h>
#endif

Melon_MS5607::Melon_MS5607(void){
    // i2c
}

/* Set the i2c address, reset the chip, and read the calibration values from PROM */
bool Melon_MS5607::begin(uint8_t addr)
{
    this->_i2caddr = addr;

    Wire.begin();

    // Send a software reset, per datasheet
    this->reset();
    
    // Must delay before getting calibration data, or we read a bunch of ACKs as values
    delay(20);

    this->getCalibrationData(_calibData);
    printCalibData();
    return true;
}

/* Get the compensated temperature as a floating point value, in �C */
double Melon_MS5607::getTemperature(){
    getCompensatedTemperature();
    compensateSecondOrder();
    return TEMP / 100.0;            // TEMP is a fixed-point int - 2000 = 20.00�C
}

/* Get the compensated pressure as a floating point value, in mbar */
double Melon_MS5607::getPressure(){
    getCompensatedPressure();                   
    return P / 100.0;
}

void Melon_MS5607::setOversamplingRate(uint8_t rate){
    _oversamplingRate = rate;
    
    // Set the delay between conversion and ADC read to a value
    // just above the max conversion time for each oversampling rate
    switch (rate)
    {
    case 0:
        _osrdelay = 1;
        break;
    case 2:
        _osrdelay = 2;
        break;
    case 4:
        _osrdelay = 3;
        break;
    case 6:
        _osrdelay = 5;
        break;
    case 8:
        _osrdelay = 10;
        break;
    }
}

/* Write the reset command to the MS5607 */
void Melon_MS5607::reset(){
    write8(MS5607_RESET);
}

/* Prints off the values stored in _calibdata */
void Melon_MS5607::printCalibData(){
    Serial.print("C1: ");
    Serial.println(_calibData.C1);
    Serial.print("C2: ");
    Serial.println(_calibData.C2);
    Serial.print("C3: ");
    Serial.println(_calibData.C3);
    Serial.print("C4: ");
    Serial.println(_calibData.C4);
    Serial.print("C5: ");
    Serial.println(_calibData.C5);
    Serial.print("C6: ");
    Serial.println(_calibData.C6);
}

/***************************************************************************
 PRIVATE FUNCTIONS
 ***************************************************************************/
 
/* Reads the PROM from the MS5607 and stores the values into a struct */
void Melon_MS5607::getCalibrationData(ms5607_calibration &calib){
    calib.C1 = read16(MS5607_PROM_READ_C1);
    calib.C2 = read16(MS5607_PROM_READ_C2);
    calib.C3 = read16(MS5607_PROM_READ_C3);
    calib.C4 = read16(MS5607_PROM_READ_C4);
    calib.C5 = read16(MS5607_PROM_READ_C5);
    calib.C6 = read16(MS5607_PROM_READ_C6);
}

/* Run the 2nd order compensation tree (see datasheet) */
void Melon_MS5607::compensateSecondOrder()
{
    int32_t T2 = 0;
    int64_t OFF2 = 0;
    int64_t SENS2 = 0;

    // Low Temperature
    if (TEMP < 2000){
        T2 = ((dT * dT) / (1 << 31));                       // T2 = dT^2 / 2^31
        OFF2 = 61 * (TEMP - 2000)*(TEMP - 2000) / 16;       // OFF2 = 61 * (TEMP-2000)^2 / 2^4
        SENS2 = 2 * (TEMP - 2000)*(TEMP - 2000);            // SENS2 = 2 * (TEMP-2000)^2

        // Very Low Temperature
        if (TEMP < -1500) {
            OFF2 += 15 * (TEMP + 1500)*(TEMP + 1500);       // OFF2 = OFF2 + 15 * (TEMP + 1500)^2
            SENS2 += 8 * (TEMP + 1500)*(TEMP + 1500);       // SENS2 = SENS2 + 8 * (TEMP + 1500)^2
        }
        TEMP = TEMP - T2;
        OFF = OFF - OFF2;
        SENS = SENS - SENS2;
    }
}

/* Returns temperature compensated temperature values */
int32_t Melon_MS5607::getCompensatedTemperature(){
    write8(MS5607_CONVERT_D2 + _oversamplingRate);          // Start a temperature conversion with the specified oversampling rate
    delay(_osrdelay);                                       // Wait for conversion to finish
    D2 = read24(MS5607_ADC_READ);                           // Read and store the Digital temperature value

    // Compensate for calibration data
    dT =  D2 - ((uint32_t)_calibData.C5 << 8);                       // D2 - T_ref
    TEMP = 2000 + ((dT*(int64_t)_calibData.C6) >> 23);           // 20.00�C + dT * TEMPSENS or 2000 + dT * C6 / 2^23

    return TEMP;
}

/* Returns temperature compensated pressure values */
int32_t Melon_MS5607::getCompensatedPressure(){
    write8(MS5607_CONVERT_D1 + _oversamplingRate);          // Start a pressure conversion with the speciifed oversampling rate
    delay(_osrdelay);                                       // Wait for conversion to finish
    D1 = read24(MS5607_ADC_READ);                           // Read and store the Digital pressure value

    // Compensate for calibration data
    getCompensatedTemperature();

    OFF = ((int64_t)_calibData.C2 << 17) + ((dT * _calibData.C4) >> 6);    // OFF = OFF_t1 + TCO * dT  or  OFF = C2 * 2^17 + (C4 * dT) / 2^6
    SENS = ((int64_t)_calibData.C1 << 16) + ((dT * _calibData.C3) >> 7);  // SENS = SENS_t1 + TCS * dT or SENS = C1 * 2^16 + (C3 * dT) / 2^7

    compensateSecondOrder();

    P = ((int64_t)D1 * (SENS >> 21) - OFF) >> 15;              // P = (D1 * SENS / 2^21 - OFF) / 2^15

    return P;
}

/* Reads an 8-bit unsigned integer from the MS5607 */
uint8_t Melon_MS5607::read8(uint8_t reg){
    uint8_t value;

    Wire.beginTransmission(_i2caddr);
    Wire.write(reg);
    Wire.endTransmission(_i2caddr);

    Wire.requestFrom(_i2caddr, (uint8_t)1);
    value = Wire.read();
    
    return value;
}

/* Reads a 16-bit unsigned integer from the MS5607 */
uint16_t Melon_MS5607::read16(uint8_t reg){
    uint16_t value;

    Wire.beginTransmission(_i2caddr);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(_i2caddr, (uint8_t)2);
    value = (Wire.read() << 8) | Wire.read();       // Big-Endian

    return value;
}

/* Reads a 24-bit unsigned integer from the MS5607 */
uint32_t Melon_MS5607::read24(uint8_t reg){
    uint32_t value;

    Wire.beginTransmission(_i2caddr);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(_i2caddr, (uint8_t)3);
    value = (Wire.read() << 16) | (Wire.read() << 8) | Wire.read();

    return value;
}

/* Write an 8-bit value to the device */
void Melon_MS5607::write8(uint8_t value){
    Wire.beginTransmission(_i2caddr);
    Wire.write(value);
    Wire.endTransmission();
}

