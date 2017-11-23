// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*   ADS1015 example sccript
     Author:  Ton Rutgers
     Date:    21-7-2017
     Version: 1.0
*/
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include <Wire.h>
#include <ADS1015_async.h>

// Receiving variables
float voltage = 0.0;  // Voltage measured by ADS1015 for 8 different input configurations
float gain = 0.0;     // Gain used for the conversion
byte precision = 0;   // Precision of the voltage and gain variables
byte result;          // return status of ADS1015 functions

/*  These variables are not used in this example sketch, but you can use them 
 *  to keep the result for each input, instead of one variable for all inputs.
float AIN0 = 0.0;
float AIN1 = 0.0;
float AIN2 = 0.0;
float AIN3 = 0.0;
float AIN0_1 = 0.0;
float AIN0_3 = 0.0;
float AIN1_3 = 0.0;
float AIN2_3 = 0.0;
 */

// configuration variables for the ADS1015
byte I2C_ADS1015 = 0x48;                      // the I2C address of the device
const byte ADS1015_inputSelect  =  B11111111; // input enable configuration (all 8 input modes enabled)
unsigned long ADS1015_inputGain = 0x55455555; // set the gain for each input
byte ADS1015_autoGainAdjust     =  B11111111; // enable/disable Auto Gain Adjust (all 8 enabled)

/* ADS1015 configuration variable description

 byte ADS1015_inputSelect:
 ------------------------
   Each bit will enable/disable an input multiplexer type (see MUX in ADS1015 datasheet):

   bit  description
   ---  --------------------------------
    0   enable differential AIN0 -> AIN1
    1   enable differential AIN0 -> AIN3
    2   enable differential AIN1 -> AIN3
    3   enable differential AIN2 -> AIN3
    4   enable AIN0 single ended
    5   enable AIN1 single ended
    6   enable AIN2 single ended
    7   enable AIN3 single ended

unsigned long ADS1015_inputGain:
------------------------------- 
   Each nibble (group of four bits) will set the gain for the corresponding input as
   configured in variable ADS1015_inputSelect (PGA in ADS1015 datasheet). The nibble
   number corresponds with the bit number in ADS1015_inputSelect.
   Only three bits of each nibble are used, and maximum is 0x5. Any higher value will be
   treated as 0x5.
   
   value  Gain (FSR = Full Scale Range)
   -----  -----------------------------
   0x0    FSR = ±6.144 V, 0,003000V/digit
   0x1    FSR = ±4.096 V, 0,002000V/digit
   0x2    FSR = ±2.048 V, 0,001000V/digit
   0x3    FSR = ±1.024 V, 0.000500V/digit
   0x4    FSR = ±0.512 V, 0.000250V/digit
   0x5    FSR = ±0.256 V, 0.000125V/digit
  
byte ADS1015_autoGainAdjust:
---------------------------
   Each bit will enable/disable auto gain adjust for the corresponding input as configured
   in ADS1015_inputSelect. Auto gain is performed as follows:
   - if enabled for the input:
    * gain is set to FSR = ±6.144 V, 3mV/digit for the first conversion
    * after the first conversion the library will determine if it is possible to increase gain:
      + if the result was less than 3/4 of the NEXT full scale range, gain is increased 1 scale
      + if the result was less than 3/4 of the SECOND NEXT scale, gain is increased 2 scales
      + etc (maximum is an increase of 5 scales)
      + scale increase is limited by the gain nibble in ADS1015_inputGain. Gain will not 
        increase beyond the value in this nibble.
    * gain is drecreased:
      + if the result is more than 19/20 of the full scale range. Decrease will be 1 scale
      + if the result is near 2047 (maximum result) gain will decrease to lowest (FSR = ±6.144 V,
        3mV/digit). From there gain may be increased again. Note: this is an overflow situation, and
        therefore the result is probably invalid. This result is skipped, so you will not see it.
   - if disabled for the input:
    * gain is fixed to the value in the nibble for the input.

Error handling: the 'begin' and the 'poll' function return a byte number:
- in general, a return value of 0 is OK, meaning: no action needed 
- a negative number (> 0x7F or 127 decimal) means there was an error.
If the 'poll' function returns a byte number between 1 and 8, it tells you the ADS1015 has finished a
conversion for the corresponding input (bit number set in ADS1015_inputSelect). 
For example: a return number  of 1 corresponds to the input selected in bit 0 in variable aDS1015_inputSelect.
Number 2 correcponds to bit number 1, etcetra. 

The follwing error numbers (> 0x7F or 127 decimal) may be returned:
-1 (0xFF):  device not found at specified I2C address; check your hardware/wiring
-2 (0xFE):  communication error during write/read; check hardware/wiring
-3 (0xFD):  unexpected number of bytes returned by ADS1015; communication error
-4 (0xFC):  no input selected in variable ADS1015_inputSelect (configuration error)

 */

// Misc variables
unsigned long TimeStart_us;
unsigned long TimeConversion_us;
byte idelay = 0;  // used for a delay after 256 conversions so you can take a screenshot
boolean serialPrint;

// create instance and pass settings
ADS1015_async ADS(I2C_ADS1015, ADS1015_inputSelect, ADS1015_autoGainAdjust, ADS1015_inputGain);

void setup() {
  Serial.begin(115200);
  serialPrint = true;   // Serial printing greatly influences time measurement!
                        // you may switch off Serial printing to measure accurate conversion time

  Wire.begin();         // the ADS15_async library does NOT start the Wire library, so you have
                        // to do that in your sketch in setup.
  
//  Wire.setClock(400000);    // uncomment to increase I2C clock speed to "fast mode"

  Serial.println("Example of the ADS1015 +/-11 bit ADC with features:");
  Serial.println("- automatic gain adjust");
  Serial.println("- combinations of single ended and differential conversion");
  Serial.println();

  result = ADS.begin();  // initialize ADS1015 and start first conversion
  if ( result ) {
    Serial.print("Error in initializing ADS1015, error#: ");
    Serial.println(result);
    Serial.println("Cannot continue");
    while (1);
  }
  Serial.print("Input\t");
  Serial.print("Voltage (V)\t");
  Serial.println("Gain (V/digit)");

  TimeStart_us = micros();  // for measuring conversion time
}

void loop() {
  // configure ADS1015 & start single shot conversion
  result = ADS.poll();
  if (result) {
    // result was != 0 -> something happened
    precision = ADS.getPrecision();     // get the number of significant decimals after the decimal point
    gain = ADS.getGain();               // get the gain in Volt/digit
    voltage = ADS.getVoltage();         // get the Voltage in Volt
    printOutput(result, voltage, gain); // print
    idelay++;
    if (idelay == 0) {
      TimeConversion_us = (micros() - TimeStart_us) / 256; // average time after 256 conversions
      Serial.println();
      Serial.print("Average conversion time (us): "); 
      Serial.println(TimeConversion_us);
      Serial.println("Note: disable serial printing (except this one) to get the correct value");
      Serial.println();
      delay(4000);  // take a break so you can copy the output to clipboard
      TimeStart_us = micros(); // start new loop
    }
  }
}

void printOutput(byte input, float voltage, float gain) {  
  if ( !serialPrint ) return;
  if (input <= 8) {
    if (idelay % 8 == 0) Serial.println(); // insert linefeed every 8 lines for readability
    Serial.print("AIN");
    switch (input) {
      // print the correspondig input mode
      case 1: Serial.print("0-1"); break;
      case 2: Serial.print("0-3"); break;
      case 3: Serial.print("1-3"); break;
      case 4: Serial.print("2-3"); break;
      case 5: Serial.print("0"); break;
      case 6: Serial.print("1"); break;
      case 7: Serial.print("2"); break;
      case 8: Serial.print("3"); break;
    }
    Serial.print("\t");
    if (voltage >=0) Serial.print(" "); // for aligning output for positive/negative values
    Serial.print(voltage, precision);
    Serial.print("\t");
    if (precision < 5) Serial.print("\t");  // add another tab to align
    Serial.println(gain, precision);
  }
  else {
    Serial.print("Error in conversion: "); Serial.println(input);
  }
}


// END OF PROGRAM

