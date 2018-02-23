/*
---------Changelog---------
02/23/18:
    -RED LED broke, LED Drivers no longer respond to PWM pulses. Amber LED is now in place of RED LED
    -Crash detection changed from absolute value to tap interrupt for better performance.
    -LED now flashes instead of pulsing
*/


#include <SPI.h> 
#include <Wire.h>
#include <VL6180X.h>
#include <SparkFun_ADXL345.h>
#include <stdbool.h>

extern "C" 
{ 
    #include "utility/twi.h"  // from Wire library, so we can do bus scanning
}

//Multiplexer Addresses
#define TCAADDR1 0x70
#define TCAADDR2 0x74

//Pins to interface with Stepper I/O
#define M_DIR 11
#define M_EN A4
#define M_VELO 10  //9is linked to LED
#define LED_RED 8 //Yellow LED at 8

//Crash Detection Threshold
#define crash_threshold 50

#define PWM_percent(val)  (255*val/100)
//Accelerometer
ADXL345 accelerometer = ADXL345();

VL6180X laser_sensor[4]; //M_sensor_left;M_sensor_right;I_sensor_left;I_sensor_right;
int channel[4] ={4,3,4,3};
uint8_t multiplexer_ID[4]={TCAADDR1,TCAADDR1,TCAADDR2,TCAADDR2};


//Multiplexer selection (multiplexer address, multiplexer channel)  
void tcaselect(uint8_t multiplexer_address, uint8_t i) 
{
    if (i > 7) return;
    Wire.beginTransmission(multiplexer_address);
    Wire.write(1 << i);
    Wire.endTransmission();  
}

void close_plexer(uint8_t multiplexer_ID)
{
    tcaselect(TCAADDR1,0);
    tcaselect(TCAADDR2,0);    
}


/*
Laser initialize loop.
Check VL6180X documentation for register details.
*/
void Laser_initialize()
{
    for (int i=0;i<4;i++)
    {
        close_plexer(multiplexer_ID[i]);
        tcaselect(multiplexer_ID[i],channel[i]);
        laser_sensor[i].init();
        laser_sensor[i].configureDefault();
        laser_sensor[i].writeReg(VL6180X::SYSRANGE__MAX_CONVERGENCE_TIME, 30);
        laser_sensor[i].writeReg16Bit(VL6180X::SYSALS__INTEGRATION_PERIOD, 50);
        laser_sensor[i].setTimeout(500);
        laser_sensor[i].stopContinuous();
        delay(10);    
    }

    delay(300);

    for (int i=0;i<4;i++)
    {
        close_plexer(multiplexer_ID[i]);
        tcaselect(multiplexer_ID[i],channel[i]);
        laser_sensor[i].startInterleavedContinuous(100);
        
    }
}


int Laser_read(int i)
{
    close_plexer(multiplexer_ID[i]);
    tcaselect(multiplexer_ID[i],channel[i]);
    return laser_sensor[i].readRangeContinuousMillimeters();
}

void accelerometer_initialize()
{

  accelerometer.powerOn();                     // Power on the accelerometer345
  accelerometer.setRangeSetting(2);           // Give the range settings "2" for +/-2G range
  accelerometer.setTapDetectionOnXYZ(1,1,0);
  accelerometer.setTapThreshold(75);
  accelerometer.setTapDuration(15);
  accelerometer.setDoubleTapWindow(50);

  accelerometer.singleTapINT(1); 

}


bool crash_detection(int acc_threshold)
{
    byte interrupts = accelerometer.getInterruptSource();

    if(accelerometer.triggered(interrupts, ADXL345_SINGLE_TAP))
    {
        return true;
    }
    
    return false;

/*     int x,y,z;

    accelerometer.readAccel(&x, &y, &z);

    if (x>=acc_threshold || y>=acc_threshold)
    {
        return true;
    }
 */

}


void crash_error()
{
    while (true)
    {
        //TODO blink RED LED
        //Pulse_led(LED_RED,100);
        Flash_led(LED_RED);
    }

}


void Pulse_led(int LED_pin, int max_brightness)
{
for (int brightness = 0; brightness < max_brightness; brightness++) {
      analogWrite(LED_pin, brightness);
      delay(30);
    }
    delay(10);
    // fade the LED on thisPin from brightest to off:
    for (int brightness = max_brightness; brightness >= 0; brightness--) {
      analogWrite(LED_pin, brightness);
      delay(50);
    }

}


void Flash_led(int LED_pin)
{
    digitalWrite(LED_pin,HIGH);
    delay(200);
    digitalWrite(LED_pin,LOW);
    delay(200);
}

void setup()
{
    Wire.begin();//Start wire so i2c transactions can take place
    /*Pin direction on the BWCD
    ---------------------outputs */
    pinMode(M_DIR,OUTPUT);
    pinMode(M_EN,OUTPUT);
    pinMode(M_VELO,OUTPUT);

    digitalWrite(M_EN,LOW); //Keep motor off during setup
    analogWrite(M_VELO,PWM_percent(50)); //Set initial speed to 10%.  10% of whatever that is defined on the motor side
    Serial.begin(115200);
    Laser_initialize();
    //Old_Laser_initialize();
    accelerometer_initialize();
    analogWrite(M_VELO,PWM_percent(100));
    
}
     
  
void loop()
{
    
    if (crash_detection(crash_threshold))
    {
        digitalWrite(M_EN,LOW); //Motor is turned off
        crash_error();
        //TODO:  Add Pulse Red LED to next rev.
    }

    digitalWrite(M_EN,HIGH);
    
    for (int i=0;i<2;i++)
    {        
        if (Laser_read(i) < 50)
        {
            digitalWrite(M_DIR,LOW);
        }
       //Serial.println(Laser_read(i));     
    }

    for (int i=2;i<4;i++)
    {        
        if (Laser_read(i) < 50)
        {
            digitalWrite(M_DIR,HIGH);
        }
       //Serial.println(Laser_read(i));     
    }

}
