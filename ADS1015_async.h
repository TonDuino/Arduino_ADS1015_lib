/*
  LIBRARY:  ADS1015_async
  Author:   Ton Rutgers, the Netherlands
  Version:	1.1
  License:  public domain

  Version 1.1:
  - minor correction for negative values (it was 1 bit off)
  
  Library for reading the ADS1015 12 bit ADC in an asynchronous way.
  This means, the library starts a conversion by the ADC, but the program does not wait
  until it is finished. You come back later to fetch the result, so you avoid a 'long' time
  (about 2 ms per conversion) waiting for the result.
  The library also provides:
  - selecting any of 8 possible ways of conversion:
    + 4 single ended types on inputs AIN0, AIN1, AIN2 and AIN3
    + 4 types of differential conversions:
      * AIN0 -> AIN1
      * AIN0 -> AIN3
      * AIN1 -> AIN3
      * AIN2 -> AIN3
      Note: the last three conversion types provide three differential channels (AIN0, AIN1 and AIN2)
      with one common referencepoint: AIN3.
    + it is possible to select ALL 8 conversion types, so you can do single ended as well as
      differential conversions at the same time
    + auto gain adjust. For each input type, you can enable auto gain adjust. This means the
      library will automatically increase or decrease the gain of the ADS1015 amplifier, if
      possible and necessary.
*/

#ifndef ADS1015_async_h
#define ADS1015_async_h

#include "Arduino.h"
#include <Wire.h>

class ADS1015_async {
  public:
    ADS1015_async(byte I2Caddr, byte inputSelect, byte autoGainAdjust, unsigned long setPGA);
    byte begin();
    byte poll();
    float getVoltage();
	float getGain();
	byte getPrecision();
  private:
    byte _I2Caddr;
    byte _inputSelect;
    byte _autoGainAdjust;
    unsigned long _setPGA;
    unsigned long _currentPGA;
    byte _available;
    const byte configReg = B01;
    const byte conversReg = B00;
    float _voltage;
	float _gain;
    byte _precision;
    byte PGA;
    byte startConversion();
	unsigned long clearAGAnibble(unsigned long pga, byte MUX);
};

#endif
