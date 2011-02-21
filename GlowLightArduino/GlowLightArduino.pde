// Defines for use with Arduino functions
#define clockpin 13 // CL
#define enablepin 10 // BL
#define latchpin 9 // XL
#define datapin 11 // SI

// Defines for direct port access
#define CLKPORT PORTB
#define ENAPORT PORTB
#define LATPORT PORTB
#define DATPORT PORTB
#define CLKPIN 5
#define ENAPIN 2
#define LATPIN 1
#define DATPIN 3

// Set pins to outputs and initial states
void setup() 
{
  pinMode(datapin, OUTPUT);
  pinMode(latchpin, OUTPUT);
  pinMode(enablepin, OUTPUT);
  pinMode(clockpin, OUTPUT);

  digitalWrite(latchpin, LOW);
  digitalWrite(enablepin, LOW);

  SPCR = (1 << SPE) | (1 << MSTR) | (0 << SPR1) | (0 << SPR0);

  // Start serial communication
  Serial.begin(9600);
}

// Variables for communication
unsigned long briteCommand;

int briteMode;
int briteBlue;
int briteRed;
int briteGreen;

// Load values into SPI register
void briteSendPacket() 
{
  if (briteMode == B01) 
  {
    briteRed = 120;
    briteGreen = 120;
    briteBlue = 120;
  }

  SPDR = (briteMode << 6) | (briteBlue >> 4);

  while (!(SPSR & (1 << SPIF)));

  SPDR = (briteBlue << 4) | (briteRed >> 6);

  while (!(SPSR & (1 << SPIF)));

  SPDR = (briteRed << 2) | (briteGreen >> 8);

  while (!(SPSR & (1 << SPIF)));

  SPDR = briteGreen;

  while (!(SPSR & (1 << SPIF)));
}

// Latch values into PWM registers
void briteLatch() 
{
  delayMicroseconds(1);  

  LATPORT += (1 << LATPIN);

  delayMicroseconds(1);

  LATPORT &= ~(1 << LATPIN);
}

// Define number of ShiftBrite modules
#define LEDS 16

// Create LED value storage array
int channels[LEDS][3] = { 0 };

// Send all array values to chain
void writeBriteArray() 
{
  // Write to PWM control registers
  briteMode = B00; 

  for (int i = 0; i < LEDS; i++) 
  {
    briteRed = channels[i][0] & 1023;
    briteGreen = channels[i][1] & 1023;
    briteBlue = channels[i][2] & 1023;
    briteSendPacket();
  }

  briteLatch();

  // Write to current control registers
  briteMode = B01; 

  for (int z = 0; z < LEDS; z++) 
    briteSendPacket();

  briteLatch();
}

// Serial communication.
#define STATE_SYNC 1
#define STATE_COUNT 2
#define STATE_COLOR 3
#define STATE_ERROR 4

// Current serial state.
int commState = STATE_SYNC;

// Packet handshake.
int commHandshake[4] = { 0 };

// Colors.
int commColorCount;
int commColorPointer;
int commColors[LEDS * 3] = { 0 };

// Synchronize colors with ShiftBrites.

void syncColors()
{
  for (int i = 0, p = 0; i < LEDS; ++i)
  {
    int r = commColors[p++] * 4;
    int g = commColors[p++] * 4;
    int b = commColors[p++] * 4;
    
    channels[i][0] = r;
    channels[i][1] = g;
    channels[i][2] = b;
  }
}

bool firstLoop = true;

// Main loop.
void loop() 
{
  if (firstLoop)
  {
    // Reset LEDs.
    writeBriteArray();
    firstLoop = false;
  }   
  
  if (commState == STATE_SYNC)
  {
    if (Serial.available())
    {
      commHandshake[0] = commHandshake[1];
      commHandshake[1] = commHandshake[2];
      commHandshake[2] = commHandshake[3];
      commHandshake[3] = Serial.read();

      if (commHandshake[0] == 'G' && commHandshake[1] == 'L' && commHandshake[2] == 'O' && commHandshake[3] == 'W')
      {
        commHandshake[0] = commHandshake[1] = commHandshake[2] = commHandshake[3] == 0;

        commState = STATE_COUNT;
      }
    }
  }
  if (commState == STATE_COUNT)
  {
    if (Serial.available())
    {
      commColorCount = Serial.read() * 3;
      commColorPointer = 0;

      if (commColorCount != 48)
        commState = STATE_ERROR;
      else
        commState = STATE_COLOR;
    }
  }
  if (commState == STATE_COLOR)
  {
    if (Serial.available())
    {
      commColors[commColorPointer++] = Serial.read();

      if (commColorPointer == commColorCount)
      {
        syncColors();
        
        commState = STATE_SYNC;
      }
    }
  }

  writeBriteArray();
  
  delay(1);
}
