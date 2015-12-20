//------------------------------------------------------
//------------------------------------------------------
//                     FoxyPI v1
//                (C) 2015 FoxyLab
//              http://acdc.foxylab.com
//              License: GNU GPL v3.0
//------------------------------------------------------
//------------------------------------------------------
//LCD libraries
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <JeeLib.h> // Low power functions library
//AVR
#include <avr/io.h>
#include <avr/power.h>
#include <util/delay.h>

//bits clear
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
//bits set
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

//LCD pins
Adafruit_PCD8544 display = Adafruit_PCD8544(3, 4, 5, 9, 10);

//Arduino pins
//D02 - btn
//D03 - CLK
//D04 - Din
//D05 - DC
//D06 - AIN0
//D07 - AIN1
//D08 - gen
//D09 - CE
//D10 - RST
//D11 - beep
//D13 - LED

//constants
//------------ pins
const int genPin = 8; //gen pin
const int btnPin = 2; //btn pin
const int ain0Pin = 6; //AIN0 pin
const int ain1Pin = 7; //AIN1 pin
const int ledPin = 13; //LED pin
const int beepPin = 11; //beep pin
//------------ delays
const unsigned int PULSE_US = 250; //pulse length, us
const unsigned int DELAY_US = 5; //delay time, us
const unsigned int PAUSE_MS = 20;  //pause length, ms
const unsigned int BLINK_MS = 250; //blink length, ms
const unsigned int BEEP_MS = 1000; //beep length, ms

const int BLINK_TIMES = 3;//init blink times
//PWM
const byte BEEP_ON = 128;//PWM beep on
const byte BEEP_OFF = 0;//PWM beep off
//LCD
const byte CONTRAST = 50;//LCD contrast
const byte TEXT_SIZE_SMALL = 1;//LCD text size (small)
const byte TEXT_SIZE_BIG = 3;//LCD text size (big)

const int PROTECT = 3;
const int AVG_COUNTS = 20;
const int ZERO_COUNTS = 4000;

//------------ btn

//vars
int count;
int i;
int zero = 0;
int counter;//counter
float count_avg;
int btnState = 0;
boolean zeroing = false;
unsigned int zero_count = 0;
int delta = 0;//diff

ISR(WDT_vect) { Sleepy::watchdogEvent(); } // Setup the watchdog

boolean target = false;

void setup()   {
  Serial.begin(9600);
  pinMode(genPin, OUTPUT);//gen pin -> out
  pinMode(btnPin, INPUT_PULLUP); //btn pin <- in
  digitalWrite(btnPin, HIGH); //btn pin pull-up en
  pinMode(ledPin, OUTPUT);//LED pin -> out
  digitalWrite(ledPin,LOW);//LED off
  //beep
  pinMode(beepPin, OUTPUT);//beep pin -> out
  analogWrite(beepPin, BEEP_OFF);//the duty cycle: between 0 (always off) and 255 (always on)  490 (500) Hz
  //comparator init
  pinMode(ain0Pin, INPUT);//AIN0 -> input
  pinMode(ain1Pin, INPUT);//AIN1 -> input
  ACSR = 0b00000000;
  ADCSRA = 0b10000000;
  ADCSRB = 0b00000000;
  //zeroing init
  zero_count = ZERO_COUNTS;
  zero = 0;
  zeroing = true;
  target = false;
  display.begin();//LCD init
  display.clearDisplay();//LCD clear
  display.display();
  //contrast set
  display.setContrast(CONTRAST);
  //text color set
  display.setTextColor(BLACK);
  //text size set
  display.setTextSize(TEXT_SIZE_SMALL);
  //welcome message
  display.println("FoxyPI v1");
  display.println("(C) FoxyLab"); 
  display.display();
  //blink $ beep 3 times
  for (i = 0 ; i < BLINK_TIMES ; i++)
  {
    digitalWrite(ledPin,HIGH);//LED on
    analogWrite(beepPin, BEEP_ON);//beep on
    delay(BLINK_MS);
    digitalWrite(ledPin,LOW);//LED off
    analogWrite(beepPin, BEEP_OFF);//beep off
    delay(BLINK_MS);  
  }
  power_adc_disable();//ADC disable
  power_spi_disable();//SPI disable
  power_twi_disable();//Two Wire Interface disable
  power_usart0_disable();//USART0 disable
  display.clearDisplay();//LCD clear
  display.display();
  display.setCursor(0, 0);
  display.println("Zeroing");
  display.display();
}


void loop() 
{
  while (true) //infinite loop
  { 
    noInterrupts();//interrupts disable
    count = 0;
    //pulse
    PORTB |= 0b00000001; //1
    delayMicroseconds(PULSE_US);
    PORTB &= ~0b00000001; //0
    //delay time
    delayMicroseconds(DELAY_US);
    //low wait
    do
    {
    }
    while ((ACSR & 0b00100000) != 0);
    //hi wait
    do
    {
      count=count+1;
    }
    while ((ACSR & 0b00100000) == 0);
    interrupts();//interrupts enable

    //zeroing
    if (zeroing) {
      if (count>zero) {
        zero = count;//new zero value
      }
      zero_count--;//zero counter decrement
      if (zero_count == 0) {
        //display
        display.clearDisplay();//LCD clear
        display.display();
        display.setCursor(0, 0);
        display.print("Zero: ");
        display.println(zero);
        display.display();
        //beep
        digitalWrite(ledPin,HIGH);//LED on
        analogWrite(beepPin, BEEP_ON);
        delay(BEEP_MS);
        digitalWrite(ledPin,LOW);//LED off
        analogWrite(beepPin, BEEP_OFF);
        zeroing = false;//zeroing O.K. 
        counter = 0;//average counter reset
        count_avg = 0;
      }
      else {
        display.setCursor(0, 15);
        //*********
        for (i = 0 ; i < ( ((float) ((float) (ZERO_COUNTS-zero_count)/ZERO_COUNTS)))*10 ; i++)
        {
          display.print("*");  
        }
        display.display();
      }
    }
    else {
      delta = count - zero;
      //target check
      if (count > (zero+PROTECT)) {
        //target is found!!!
        target = true;
        digitalWrite(ledPin,HIGH);//LED on
        analogWrite(beepPin, BEEP_ON);//beep on
        //delta indication
        //???
      }
      else
      {
        //target not found
        target = false;
        digitalWrite(ledPin,LOW);//LED off
        analogWrite(beepPin, BEEP_OFF);//beep off
      }

      //avg calculation
      count_avg = (count_avg*counter+count)/(++counter);
      if (counter == AVG_COUNTS) {
        //avg display
        display.clearDisplay();//LCD clear
        display.display();
        display.setCursor(0, 0);
        display.setTextSize(TEXT_SIZE_BIG);
        display.println(((int) count_avg));
        display.display();
        count_avg = 0;//avg reset
        counter = 0;//counter reset
            
      }
    }
    
    //pause between pulses
    if (target) {
        delay(PAUSE_MS);  
      }
      else {
        Sleepy::loseSomeTime(PAUSE_MS);
    }
    
    //zeroing pushbutton check
    btnState = digitalRead(btnPin);//btn reading
    if ((btnState == 0) && (zeroing == false)) {
      //button is pressed  
      zero_count = ZERO_COUNTS;
      zero = 0;
      zeroing = true;
      target = false;
      display.clearDisplay();//LCD clear
      display.display();
      display.setCursor(0, 0);
      display.setTextSize(TEXT_SIZE_SMALL);
      display.println("Zeroing");
      display.display();
      digitalWrite(ledPin,LOW);//LED off
      analogWrite(beepPin, BEEP_OFF);//beep off
    }

    //next pulse
  }
}

