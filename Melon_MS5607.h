/***************************************************************************
This is a library for the MS5607-02BA03 barometric pressure sensor

These sensors use I2C or SPI to communicate.

Written by Paul Guenette

MIT License
***************************************************************************/

#ifndef __MS5607_H__
#define __MS5607_H__

#if (ARDUINO >= 100)
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

/*=========================================================================
COMMANDS
-----------------------------------------------------------------------*/
enum
{
    MS5607_RESET                = 0x1E,
    MS5607_CONVERT_D1_OSR512    = 0x40,
    MS5607_CONVERT_D1_OSR256    = 0x42,
    MS5607_CONVERT_D1_OSR2048   = 0x44,
    MS5607_CONVERT_D1_OSR4096   = 0x44,
    MS5607_CONVERT_D2_OSR512    = 0x46,
    MS5607_CONVERT_D2_OSR256    = 0x48,
    MS5607_CONVERT_D2_OSR2048   = 0x50,
    MS5607_CONVERT_D2_OSR4096   = 0x52,
    MS5607_ADC_READ             = 0x00,
    MS5607_PROM_READ            = 0xA0
};

class Melon_MS5607
{
public:
    Melon_MS5607(void);
    Melon_MS5607(int8_t cspin);

    bool begin(uint8_t addr);

private:
    uint8_t _i2caddr;
};


#endif