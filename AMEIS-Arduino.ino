#include <Wire.h>
#define POLYNOMIAL 0x131
#include "Arduino.h"
#define TSIC_HIGH digitalRead(m_signal_pin)
#define TSIC_LOW  !digitalRead(m_signal_pin)
#define Cancel()  if (timeout > 10000){Serial.println("Error occured");}       // Cancel if sensor is disconnected

String inputCommand = "";          // a string to hold incoming data
boolean commandComplete = false;   // whether the command is complete
unsigned int clockSpeedMilliseconds = 1;
int csdtiIndices[] = {5, 4, 7, 2, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};  // output pins for clock, sync, din, tilt, chamberIndex1, chamberIndex2, ..., chamberIndexN
int csdtiIndicesCount = 11;        // number of items in csdrti array
int bytesPerChunk = 0;
int numberOfChunks = 0;
bool currentlyBinaryMode = false;
int binaryByteCount = 0;
bool binaryDataAvailable = false;
char *binaryData;
unsigned long startTime = 0;
unsigned long endTime = 0;

//SHT31 definitions
uint8_t bytebuffer[6];
unsigned int temp;
unsigned int temp1;
unsigned int temp2;
unsigned int hum;
unsigned int hum1;
unsigned int hum2;
//TSIC716 definitions
uint8_t signal_pin=2;
uint8_t getTemperature(uint16_t *temp_value16);
double calc_Celsius(uint16_t *temperature16);
uint8_t m_signal_pin=2;
uint8_t checkParity(uint16_t *temp_value);
uint16_t temp_value16;

bool parseCommand(String newCommandText)
{
  //Serial.println("start parsing");
  newCommandText.toLowerCase();
  if (newCommandText.startsWith("test"))
  {  // "test" is blinking the on-board LED for 200ms
    digitalWrite(13, HIGH);
    delay(500);
    digitalWrite(13, LOW);
    Serial.println("Info: Executed test.");
    return true;
  }
  else if (newCommandText.startsWith("getversion"))
  {
    Serial.println("Arduino Uno, ArduinoHandler V0.1");
  }
  else if (newCommandText.startsWith("gettemp"))
  { // read temperature from temperature sensor
    uint16_t temp_value1 = 0;
    uint16_t temp_value2 = 0;
    getTemperature(&temp_value1);
    getTemperature(&temp_value2);
    checkParity(&temp_value1);
    checkParity(&temp_value2);
    temp_value16 = (temp_value1 << 8) + temp_value2;
    Serial.println(calc_Celsius(&temp_value16), 4);   // write temperature with 4 significant digitis
  }
  else if (newCommandText.startsWith("gethumid"))
  { // read humidity from humidity sensor
    Wire.beginTransmission(68); //0x44 (default adress) ADDR connected to VSS
    Wire.write(byte(0x2C));
    Wire.write(byte(0x06));
    Wire.endTransmission();
    int i=1;
    Wire.requestFrom(68, 6);
    while(Wire.available())
    { 
      bytebuffer[i]=Wire.read();
      i++;
    }
    temp1=bytebuffer[1]<<8;
    temp2=bytebuffer[2];
    uint8_t tempcrc[]={bytebuffer[1], bytebuffer[2]};
    int crc=SHT3X_CalcCrc(tempcrc,2);
    //Serial.print(" CRC:   crc calculated: ");Serial.print(crc); Serial.print("   crc received: "); Serial.println(bytebuffer[3]);
    hum1=bytebuffer[4]<<8;
    hum2=bytebuffer[5];
    temp=temp1 + temp2;
    hum=hum1 + hum2;
    double tempd=(double)temp;
    double humd=(double)hum;
    Serial.println(100*humd/(pow(2,16)-1), 4);   // write humidity with 4 significant digitis
  }
  else if (newCommandText.startsWith("gethumidtemp"))
  { // read temperature from humidity sensor
    Wire.beginTransmission(68); //0x44 (default adress) ADDR connected to VSS
    Wire.write(byte(0x2C));
    Wire.write(byte(0x06));
    Wire.endTransmission();
    int i=1;
    Wire.requestFrom(68, 6);
    while(Wire.available())
    { 
      bytebuffer[i]=Wire.read();
      i++;
    }
    temp1=bytebuffer[1]<<8;
    temp2=bytebuffer[2];
    uint8_t tempcrc[]={bytebuffer[1], bytebuffer[2]};
    int crc=SHT3X_CalcCrc(tempcrc,2);
    //Serial.print(" CRC:   crc calculated: ");Serial.print(crc); Serial.print("   crc received: "); Serial.println(bytebuffer[3]);
    hum1=bytebuffer[4]<<8;
    hum2=bytebuffer[5];
    temp=temp1 + temp2;
    hum=hum1 + hum2;
    double tempd=(double)temp;
   double humd=(double)hum;
    Serial.println(-45 + 175*tempd/(pow(2,16)-1), 4);   // write temperature with 4 significant digitis
  }
  else if (newCommandText.startsWith("setclockspeed"))
  {  // "setClockSpeed 10" is setting the clock (of the output data) to 10 microseconds 
    int whitespaceIndex = newCommandText.indexOf(" ");
    String textPar = newCommandText.substring(whitespaceIndex);
    Serial.println(sizeof(int));
    int intPar = textPar.toInt();
    clockSpeedMilliseconds = intPar;
    Serial.println("Set clockspeed to " + String(clockSpeedMilliseconds) + " milliseconds.");
  }
  else if (newCommandText.startsWith("setbitsperchunk"))
  {  // "setBitsPerChunk 11" is setting the number of bits read from a bit sequence to 11, i.e.: clock, sync, data, tilt + 7 bits for encoding the chamber index
    int whitespaceIndex = newCommandText.indexOf(" ");
    String textPar = newCommandText.substring(whitespaceIndex);
    Serial.println(sizeof(int));
    int intPar = textPar.toInt();
    csdtiIndicesCount = intPar;
    if (csdtiIndicesCount > 16)
    {
      csdtiIndicesCount = 16;
    }
    Serial.println("Set setbitsperchunk to " + String(clockSpeedMilliseconds) + " bit.");
  }
  else if (newCommandText.startsWith("setpins"))
  { /* "setpins 5 4 7 2 8 9 10 11 12" is setting the following Arduino pins as outputs:
        5: clock
        4: sync
        7: data
        2: tilt
        8-12: identifier pins encoding the currently active recording site (8-12 = 5 bits = 32 sites)
        WARNING: if number of indices > csdtiIndicesCount, the remaining indices are skipped without warning! -> initialize correct number first by calling setbitsperchunk X
     */
    bool foundParameter = true;
    int whitespaceIndex = 0;
    int c = 0;
    
    // parse all pin indices 
    do
    {
      whitespaceIndex = newCommandText.indexOf(" ", whitespaceIndex) + 1;
      //Serial.println(whitespaceIndex);
      String textPar = newCommandText.substring(whitespaceIndex);
      //Serial.println(textPar);
      int intPar = textPar.toInt();
      // TODO: check that this pin index is not in use internally (sensors?)
      csdtiIndices[c] = intPar;
      //Serial.println(intPar);
    } while (whitespaceIndex != -1 && c++ < csdtiIndicesCount);
    
    // set corresponding pins as output:
    int numPins = sizeof(csdtiIndices)/sizeof(int);
    String confMsg = "";
    for (c = 0; c < numPins; c++)
    {
      confMsg += String(c) + ":" + String(csdtiIndices[c]) + ", ";
      pinMode(csdtiIndices[c], OUTPUT);
    }
    Serial.println("Set  " + String(numPins) + " pins as output: " + confMsg + ".");
  }
  else
  {
    String msg = "Error: Unknown command: ";
    msg.concat(newCommandText);
    Serial.println(msg);
    return false;
  }
}

void setup()
{
  Serial.begin(9600);
  // send blink sequence indinputCommandating that right firmware is running
  pinMode(13, OUTPUT);
  sendStartBlinkSequence();
  for (int c = 0; c < csdtiIndicesCount; c++)
  {  // set all required pins to output mode
    pinMode(csdtiIndices[c], OUTPUT);
  }
  Wire.begin();
  pinMode(m_signal_pin, INPUT);
}

// send blink sequence indinputCommandating that right firmware is running
void sendStartBlinkSequence()
{
  digitalWrite(13, HIGH);
  delay(1000);
  digitalWrite(13, LOW);
  delay(1000);
  digitalWrite(13, HIGH);
  delay(200);
  digitalWrite(13, LOW);
  delay(200);
}

void processBinaryData()
{  // processes binary data and sends them to the specified output pins
  for (int chunk = 0; chunk < numberOfChunks; chunk++)
  {
    String output = "";
    int curIIndex = 0;
    int byteIndex = 0;
    int bitIndex = 0;
    int curIndex = 0;
    for (byteIndex = 0; byteIndex < bytesPerChunk; byteIndex++)
    {
      for (bitIndex = 0; bitIndex < 8 && curIIndex < csdtiIndicesCount; bitIndex++, curIIndex++)
      {
        curIndex = chunk * bytesPerChunk + byteIndex;
        if ((binaryData[curIndex] >> bitIndex) & 1)
        {
          //output += String(curIIndex) + ":1 ";
          
          // apply to corresponding output (clock, sync, din, tilt, countS
          digitalWrite(csdtiIndices[curIIndex], HIGH);
          if (bitIndex == 0)
          {
            digitalWrite(13, HIGH);
            //delay(200);
          }
        }
        else
        {
          output += String(curIIndex) + ":0 ";
          digitalWrite(csdtiIndices[curIIndex], LOW);
          if (bitIndex == 0)
          {
            digitalWrite(13, LOW);
            //delay(200);
          }
        }
      }
    }
    delay(clockSpeedMilliseconds);
    //Serial.println(chunk);
    //Serial.println(output);
  }
}

//Calc Checksum for SHT31
static uint8_t SHT3X_CalcCrc(uint8_t data[], uint8_t nbrOfBytes)
{
   uint8_t bitmask; // bit mask
   uint8_t crc = 0xFF; // calculated checksum
   uint8_t byteCtr; // byte counter

   // calculates 8-Bit checksum with given polynomial
   for(byteCtr = 0; byteCtr < nbrOfBytes; byteCtr++)
   {
     crc ^= (data[byteCtr]);
     for(bitmask = 8; bitmask > 0; --bitmask)
     {
       if(crc & 0x80)
         crc = (crc << 1) ^ POLYNOMIAL;
       else crc = (crc << 1);
     }
   }
   return crc;
}

//Calculate temperature in Celsius for TSIC716
double calc_Celsius(uint16_t *temperature16)
{
  double temp_val16=*temperature16;
  double celsius=0;
  //temp_val16=((*temperature16 * 250L) >> 8) - 500; 
  celsius=temp_val16/16383*70-10;
  return celsius;
}

//Get temperature of TSIC716
uint8_t getTemperature(uint16_t *temp_value)
{
  uint16_t strobelength = 0;
  uint16_t strobetemp = 0;
  uint8_t dummy = 0;
  uint16_t timeout = 0; // max value for timeout is set in .h file
  while (TSIC_HIGH)
  {  // wait until start bit starts
    timeout++;
    delayMicroseconds(10);
    Cancel();
  }
  
  strobelength = 0;
  timeout = 9900;   // max value for timeout is set in .h file
  while (TSIC_LOW)
  {    // wait for rising edge
    strobelength++;
    timeout++;
    delayMicroseconds(10);
    Cancel();
  }
  for (uint8_t i=0; i<9; i++)
  {
    // Wait for bit start
    timeout = 0;
    while (TSIC_HIGH)
    { // wait for falling edge
      timeout++;
      Cancel();
    }
    // Delay strobe length
    timeout = 0;
    dummy = 0;
    strobetemp = strobelength;
    while (strobetemp--)
    {
      timeout++;
      dummy++;
      delayMicroseconds(10);
      Cancel();
    }
    *temp_value <<= 1;
    // Read bit
    if (TSIC_HIGH)
    {
      *temp_value |= 1;
    }
    // Wait for bit end
    timeout = 0;
    while (TSIC_LOW)
    {    // wait for rising edge
      timeout++;
      Cancel();
    }
  }
  
  return 1;
}

//check parity of TSIC716
uint8_t checkParity(uint16_t *temp_value)
{
  uint8_t parity = 0;

  for (uint8_t i = 0; i < 9; i++)
  {
    if (*temp_value & (1 << i))
      parity++;
  }
  if (parity % 2)
  {
    Serial.println("Error: wrong parity");       // wrong parity
    return 0;
  }
  *temp_value >>= 1;          // delete parity bit
  return 1;
}

void loop()
{
  // check if we have a complete command
  if (commandComplete)
  {
    Serial.println(inputCommand);
    startTime = 0;
    endTime = 0;
    startTime = micros();
    parseCommand(inputCommand);
    endTime = micros();
    Serial.println("execution time: ");
    Serial.println(endTime - startTime);
    commandComplete = false;
    inputCommand = "";
  }
  if (binaryDataAvailable)
  { // sendbinarydata 2  4\n1a2b3c4d5e6f7g8h is taking data in chunks of 2 byte and sending bit1 to pin[1], ..., total length is 10 chunks
    // process and send binary data out
    // free binary data memory
    Serial.println("processing binary data");
    //Serial.println(binaryData);
    startTime = micros();
    processBinaryData();
    binaryDataAvailable = false;
    currentlyBinaryMode = false;
    //free(binaryData);
    endTime = micros();
    Serial.println("time [microseconds]:");
    Serial.println((endTime-startTime));
  }
}

void serialEvent()
{
  while (Serial.available())
  {
    // get the new byte:
    char inChar = (char)Serial.read();

    /*
      check if we are in text or binary mode for reading
      in text mode we read until \n
      in binary mode we read until we found indicated number of bytes or time is over
    */
    
    // if the incoming character is a newline and we are not in binary mode, set a flag
    // so the main loop can do something about it:
    if (!currentlyBinaryMode)
    {
      // add it to the inputString:
      inputCommand += inChar;
    }
    else
    {
      // if we are in binary mode 
      //bytesPerChunk = 0;
      //numberOfChunks = 0;
      //currentlyBinaryMode = false;

      // append binary data
      binaryData[binaryByteCount] = (byte)inChar;
      //Serial.println(String((int)binaryData[binaryByteCount]));
      //Serial.println(numberOfChunks * bytesPerChunk);

      // check if we read sufficient number of bytes already, if so -> finish this command and let main loop deal with processing it
      if(++binaryByteCount >= numberOfChunks * bytesPerChunk)
      {
        binaryDataAvailable = true;
        currentlyBinaryMode = false;
      }      
    }

    if (inChar == '\n' && !currentlyBinaryMode)
    {
      // check if we have to switch to binary mode
      String newCommandText = inputCommand;
      newCommandText.toLowerCase();
      
      // todo: move this to parse function.... does not belong here... split code into separate files to keep maintainable 
      if (newCommandText.startsWith("sendbinarydata"))
      { // switch to binary mode
        // sendbinarydata 2  4 \n1a2b3c4d5e6f7g8h is taking data in chunks of 2 byte and sending bit1 to pin[1], ..., total length is 4 chunks
        int whitespaceIndex = 0;
        String textPar = "";
        int intPar = 0;
        whitespaceIndex = newCommandText.indexOf(" ", whitespaceIndex) + 1;
        textPar = newCommandText.substring(whitespaceIndex);
        intPar = textPar.toInt();
        bytesPerChunk = intPar;
        
        whitespaceIndex = newCommandText.indexOf(" ", whitespaceIndex) + 1;
        textPar = newCommandText.substring(whitespaceIndex);
        intPar = textPar.toInt();
        numberOfChunks = intPar;
        currentlyBinaryMode = true;
        binaryByteCount = 0;
        inputCommand = "";

        free(binaryData);
        binaryData = new char[numberOfChunks * bytesPerChunk];
        if (binaryData == NULL)
        {
          Serial.println("could not allocate memory!");
          currentlyBinaryMode = false;
        }
        else
        {
          Serial.println("allocated memory");
          Serial.println((int)binaryData);
        }
        startTime = micros();
      }
      else
      { // not binary mode -> parse and execute via main loop
        commandComplete = true;
      }
    }
  }
}

