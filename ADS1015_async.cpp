/*
  LIBRARY:  ADS1015_async
  Author:   Ton Rutgers, the Netherlands
  Date:     04 august 2017
  Version:	1.1
  License:  public domain

  Version 1.1:
  - minor correction for negative values (it was 1 bit off)
 */
 
#include "Arduino.h"
#include <Wire.h>
#include "ADS1015_async.h"

// create instance definition
ADS1015_async::ADS1015_async(byte I2Caddr,
                             byte inputSelect,
							 byte autoGainAdjust,
							 unsigned long setPGA)
{
  _I2Caddr = I2Caddr;
  _inputSelect = inputSelect;
  _autoGainAdjust = autoGainAdjust;
  _setPGA = setPGA;
  _available = 0;
}

// initialization
byte ADS1015_async::begin()
{
  // check if the ADS1015 is present at the given I2C address
  Wire.beginTransmission (_I2Caddr);
  byte error = Wire.endTransmission();
  if ( error ) {
    return (-1);
  }
  else {
	// initialize the gain administration variables
	// check if each nibble <= 5; if not, set to 0x5 to avoid unexpected results
	for (byte i = 0; i < 8; i++) {
      if ( (_setPGA >> (4 * i) & 0xF) > 0x5) {
		_setPGA = clearAGAnibble(_setPGA, i);
        _setPGA |= 0x5L << (4 * i);
	  }
    }
    _currentPGA = _setPGA;
    for (byte i = 0; i<8; i++) {
	 if (_autoGainAdjust & (1 << i)) {
		// auto gain enabled for this input -> set this nibble to lowest gain (0x0)
		_currentPGA = clearAGAnibble(_currentPGA, i);
      }
    }
    return(startConversion()); // start first conversion and return
  }
}

// clear the given nibble of the gain admin variable
unsigned long ADS1015_async::clearAGAnibble(unsigned long pga, byte nibbleNr) 
{
	for (byte j = 0; j < 4; j++) {
	  bitClear(pga, 4 * nibbleNr + j);
	}
  return(pga);
}

// poll the device:
// - return(0) if the device is still converting
// - read the raw value from ADS1015 and convert to 11 bit signed value
// - determine gain from the PGA setting
// - adjust the gain for the NEXT conversion if necessary & possible
// - calculate the voltage from (integer) result and gain
// - start the next conversion
// - return(the_number_of_the_input_for_which_the_value_is_available)
// - this number is from 1 ... 8
// - the caller should read the value from this input by calling read()
// - although a new conversion is started, this function can only continue if
//   the value has been read by the read() function, initiated by the caller.
byte ADS1015_async::poll() 
{
  byte configMSb; // most significant byte of config register
  byte MUX;       // input select: 3 bit value retrieved from the ADS1015 config register
  byte i;
  byte j;
  long result;     // raw conversion value
  long absResult;  // raw absolute value
  boolean returnResult = true;  // set to false if overflow is detected

  if (_inputSelect == 0) return (-4); // no input selected

  if (_available) {
    // there is a value available which wasn't fetched yet by the caller -> return the number
    for (i = 0; i < 8; i++) {
      if (_available & (1 << i)) break; // found!
    }
    return (i + 1); // the (decimal) number for the available value
	                // we cannot continue here before the caller has read the value
  }

  // check conversion status:
  Wire.beginTransmission (_I2Caddr);
  Wire.write(configReg);  // point to the config register
  if ( Wire.endTransmission() ) return (-2); // communication error
  Wire.requestFrom(int(_I2Caddr), 1);        // request the most significatnt byte of config
  if (Wire.available() != 1) return (-3);    // if slave does not return 1 byte -> error
  configMSb = Wire.read();                   // read the most sign. byte of config register
  
  // bit 7 reflects status of conversion:
  //  0: conversion in progress
  //  1: device is not performing a conversion
  if ( !(configMSb & 0x80) ) return(0); // device is busy converting -> nothing to do here

  // conversion has finished, fetch conversion result
  Wire.beginTransmission (_I2Caddr);
  Wire.write(conversReg);     // point to the conversion register
  if ( Wire.endTransmission() ) return (-2); // comm error
  Wire.requestFrom(int(_I2Caddr), 2);        // request 2 bytes
  if (Wire.available() != 2) return (-3);    // if slave does not return 2 bytes -> error
  // let's compose the reading
  result  = 0;
  result  = Wire.read() << 4;         // read first byte and shift 4 bits left into result
  result |= Wire.read() >> 4;         // read second byte, shift out the last 4 bytes and add to result

  if (result & 0x0800) {
    // it is a negative number; make it an abolute value and put the minus sign on again
    result = -1 -(result ^ 0xFFF);    // toggle bits 0:11 for the absolute value & make minus
  }

  MUX = (configMSb & B01110000) >> 4; // the input number for which the conversion is available
  PGA = (configMSb & B00001110) >> 1; // tha PGA value used for the conversion

  // determine gain from PGA
  switch (PGA) {
    case 0:
      _gain = 0.003;  // FSR = +/- 6.144V, 3mV/digit
	  _precision = 3; // precision in decimal digits
      break;
    case 1:
      _gain = 0.002;  // FSR = +/- 4.096V, 2mV/digit
	  _precision = 3; // precision in decimal digits
      break;
    case 2:
      _gain = 0.001;  // FSR = +/- 2.048V, 1mV/digit
	  _precision = 3; // precision in decimal digits
      break;
    case 3:
      _gain = 0.0005; // FSR = +/- 1.024V, 0,5mV/digit
	  _precision = 4; // precision in decimal digits
      break;
    case 4:
      _gain = 0.00025;  //FSR = +/- 0.512V, 0.25mV/digit
	  _precision = 5; // precision in decimal digits
      break;
    default:
      // values 5 ... 7 are the same
      _gain = 0.000125; //FSR = +/- 0.256V, 0.125mV/digit
	  _precision = 6; // precision in decimal digits
      break;
  }
  
  // adjust the gain if allowed, necessary and possible  
  if (_autoGainAdjust & (1 << MUX)) {
    // auto gain adjust is enabled for this input
    absResult = abs(result);
    if (absResult < 1024) {
      // candidate for gain increase
      if (PGA == 0) j = 6;	// divider for PGA == 0
      else j = 8;           // divider for any other PGA
      // increase gain if absResult is less than 3/4 of the NEXT scale
      // repeat this for the second next scale; then absResult must be
      // less than 3/8 of that scale. Etcetera.
      // So we can adjust gain from 1 to max 5 scales.
      for (i = PGA; i < 5; i++) {
        if (absResult < 6144 / j) {  // 6144 = 3 * 2048
          PGA = PGA + 1; // increase (may be done up to 5 times)
          j = 2 * j;	 // next divider factor
        }
        else break;      // we are done 
      }
      // check if there is enough 'room', regarding the limit in _setPGA
      i = _setPGA >> (4 * MUX) & 0x7;  // retrieve limiting PGA from _setPGA
      if (PGA > i) PGA = i; // there is not enough room -> take the limit in _setPGA
	}
    else {
      // decrease 1 scale if absResult > 19/20 from max value
	  // decrease to lowest possible gain (two thirds) in case of overflow
      if (absResult > 1945) {
        // gain decrease
        if (absResult >= 2045) {
          // overflow!! -> set PGA to lowest and DO NOT return result
          PGA = 0;
          returnResult = false;
        }
        else {
          PGA = PGA - 1;  // decrease gain
        }
	  }
	}
    _currentPGA = clearAGAnibble(_currentPGA, MUX);       // clear this nibble
    _currentPGA = _currentPGA + (long(PGA) << (4 * MUX)); // and set to PGA
  }  

  // calculate the actual _voltage and store for the read() function
  _voltage = (float)result * _gain;
  _available = 1 << MUX;           // set bit in availability register

  byte error = startConversion();  // Start next conversion
  if (error) return (error);

  // return the input number (+1) of which the value is available for read
  if (returnResult) {
    return (MUX + 1); // the input number + 1
  }
  else {
    // do NOT return result, because the value is invalid, due to overflow
	_available = 0;
	return(0);
  }
}

float ADS1015_async::getVoltage() 
{
  _available = 0;  // reset the availability bit
  return (_voltage); 
}

// calling this function is optional, but should be called BEFORE read()
float ADS1015_async::getGain() 
{
  return(_gain);
}

byte ADS1015_async::getPrecision() 
{
  return (_precision); 
}

// Start next conversion
byte ADS1015_async::startConversion() 
{
  byte configMSb = B10000001;
  byte configLSb = B00000011;
  const byte SPS = B111;  // fixed sample rate (but you can change it here)
  byte MUX;
  byte i;

  // search for availability bit = 1
  for (i = 0; i < 8; i++) {
    if (_available & (1 << i)) break;  // found
  }
  // when this is the first time, no converson was performed yet and no bit was set; i will be 8
  // in this case start with -1 (which is 0xFF)
  if (i > 7) i = 0xFF;
  
  // look for the next selected input
  MUX = i;    // start at the position where the availability bit was set and scan for max 8 bits
  for (i = 0; i < 8; i++) {
    MUX = (MUX + 1) % 8;    // goto next bit position; handle overflow with modulo
    if (_inputSelect & (1 << MUX)) break;  // found
  }
  if (i > 7) return (-4);   // no input selected
  
  // MUX is now the input for the next conversion
  // Determine which PGA value to use for the programmable gain amplifier
  // this value has been set in the 4-byte _currentPGA variable ->
  // take the nibble with number MUX
  PGA = (_currentPGA >> (4 * MUX)) & 0x7;

  // prepare config bytes by adding MUX, PGA and SPS
  configMSb |= MUX << 4;
  configMSb |= PGA << 1;
  configLSb |= SPS << 5;

  // configure ADS1015 & start single shot conversion for input = MUX and gain = PGA
  Wire.beginTransmission (_I2Caddr);
  Wire.write(configReg);   // point to the config register
  Wire.write(configMSb);   // write MSbyte of config word with bit 7 set
  Wire.write(configLSb);   // write LSbyte of config word
  if ( Wire.endTransmission() ) return (-2); // comm error
  return (0);
}

// END OF FILE

