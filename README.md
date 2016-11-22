# AMEIS-Arduino
Code running on the Arduino Uno R3 driving the switchboard in the AMEIS setup.

# Commands
This Arduino firmware supports a range of commands which are expected at 9600 baud and terminated with `\r\n`. It sends some analytics back via the serial port communication.

1. `setclockspeed 1\r\n` - sets the half-clock interval (time that the clock is being held high or low) to 1ms.
2. `setbitsperchunk 9\r\n` - for binary bit banging we read 9 bits per time step (half clock).
3. `setpins 5 4 7 2 8 9 10 11 12\r\n` - setting output pins of the Arduino in the following order:
  - index 1 (`5`) = clock: connects to the clock pin of the switches on the switch board
  - index 2 (`4`) = sync: connects to the sync pin of the switches on the switch board
  - index 3 (`7`) = din: connects to the first data input of the first switch on the switch board
  - index 4 (`2`) = tilt: connects to the BNC connector of a custom-made tilt-stage which tilts on falling or raising edge.
  - index 5-9 (`8 9 10 11 12`) = output pins of the arduino that are encoding the current recording site. This can be used to send the current recording site to, e.g. the digital input of the impedance spectroscope.
  
4. `setreset 2` - sets the inverted reset pin (pin which is always held high) to pin number 2. Setting a value of -1 is deactivating any reset pins (hard-wired to pull high)
5. `test\r\n` - blinks the LED on Arduino output pin 13
6. `sendbinarydata 2  4\r\n1a2b3c4d` - switches to binary mode so that Arduino expects data in chunks of 2 bytes per chunk and at bit1 to pin[1], ..., total length is 4 chunks. Warning: If this is not followed by exactly (in this case) 8 bytes (2 * 4), then the following command will get truncated and not work.
