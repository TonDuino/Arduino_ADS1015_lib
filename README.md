# Arduino_ADS1015_lib
ADS1015 12 bit ADC library for Arduino

The ADS1015 is a 12 bit ADC and there are many boards available for interfacing with an Arduino through I2C.
The featues of this library are:
- all eight inputmodes can be used: four differential modes and four single ended modes
- all eight modes can be enabled and disabled seperately
- it will work asynchronous: your sketch starts an analog -> digital conversion and comes back later to get the result (which will start the next conversion). This will avoid waiting for the conversion to complete
- fixed gain or automatic gain adjust. With automatic gain adjust the library will automatically choose the best full scale range
- overflow detection. In case the input voltage exceeds the Full Scale

Fur further information see the file ADS1015_async_library.pdf
An Arduino example sketch is included.
I have a similar library for the ADS1115 16 bit ADC in one of my repositories.
