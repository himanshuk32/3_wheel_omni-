/*
 * TO DO:
 * Check if IMU giving readings- Timer interrupt to calc loop counter value, if same then rese
 * Limit ang_velocity
 * 6-A,7-B,8-C
 */

#include <Wire.h>
//#include "Arduino.h"

//The following code is for setting up IMU constants


#define mode 2
//#define mode 1
//Mode 1 - Line following + IMU control
//Mode 0 - IMU control + Manual Driving



//**********************************************************************
#define Gyro_Gain_X 0.07 //X axis Gyro gain
#define Gyro_Gain_Y 0.07 //Y axis Gyro gain
#define Gyro_Gain_Z 0.07 //Z axis Gyro gain
#define Gyro_Scaled_X(x) ((x)*ToRad(Gyro_Gain_X)) //Return the scaled ADC raw data of the gyro in radians for second
#define Gyro_Scaled_Y(x) ((x)*ToRad(Gyro_Gain_Y)) //Return the scaled ADC raw data of the gyro in radians for second
#define Gyro_Scaled_Z(x) ((x)*ToRad(Gyro_Gain_Z)) //Return the scaled ADC raw data of the gyro in radians for second
int SENSOR_SIGN[9] = {1,1,1,-1,-1,-1,1,1,1}; //Correct directions x,y,z - gyro, accelerometer, magnetometer

// accelerometer: 8 g sensitivity
// 3.9 mg/digit; 1 g = 256
#define GRAVITY 256  //this equivalent to 1G in the raw data coming from the accelerometer

#define ToRad(x) ((x)*0.01745329252)  // *pi/180
#define ToDeg(x) ((x)*57.2957795131)  // *180/pi

#define M_X_MIN -2292
#define M_Y_MIN -2374
#define M_Z_MIN -2605
#define M_X_MAX +2672
#define M_Y_MAX +2674
#define M_Z_MAX +2255 

#define Kp_ROLLPITCH 0.02
#define Ki_ROLLPITCH 0.00002
#define Kp_YAW 1.2
#define Ki_YAW 0.00002

/*For debugging purposes*/
//OUTPUTMODE=1 will print the corrected data,
//OUTPUTMODE=0 will print uncorrected data of the gyros (with drift)
#define OUTPUTMODE 1

#define PRINT_DCM 0     //Will print the whole direction cosine matrix
#define PRINT_ANALOGS 0 //Will print the analog raw data
#define PRINT_EULER 1   //Will print the Euler angles Roll, Pitch and Yaw
//min: { -2308,  -2293,  -1676}    max: { +2752,  +2620,  +1846}

long timer=0;   //general purpuse timer
long timer_old;
long timer24=0; //Second timer used to print values
int AN[6]; //array that stores the gyro and accelerometer data
int AN_OFFSET[6]={0,0,0,0,0,0}; //Array that stores the Offset of the sensors

int gyro_x;
int gyro_y;
int gyro_z;
int accel_x;
int accel_y;
int accel_z;
int magnetom_x;
int magnetom_y;
int magnetom_z;
float c_magnetom_x;
float c_magnetom_y;
float c_magnetom_z;
float MAG_Heading;

float Accel_Vector[3 ]= {0,0,0}; //Store the acceleration in a vector
float Gyro_Vector[3]= {0,0,0};//Store the gyros turn rate in a vector
float Omega_Vector[3]= {0,0,0}; //Corrected Gyro_Vector data
float Omega_P[3]= {0,0,0};//Omega Proportional correction
float Omega_I[3]= {0,0,0};//Omega Integrator
float Omega[3]= {0,0,0};

// Euler angles
float roll;
float pitch;
float yaw;

float errorRollPitch[3]= {0,0,0};
float errorYaw[3]= {0,0,0};

unsigned int counter=0;
byte gyro_sat=0;
float G_Dt=0.02;    // Integratio n time (DCM algorithm)  We will run the integration loop at 50Hz if possible


//*********************************************************************










//The following code is to set up driving constants/structs

//******************************************************
typedef struct wheels {
  float trans_rpm; //RPM for translational Velocity
  float ang_rpm; //RPM due to angular velocity
  int angle; //Position of tyre from x-axis
  float rpmmax; //Max RPM of wheel
  int pinpwm;//Pin PWM of motor to be connected to
  int pina;//a = 1, b=0 positive
  int pinb;
  int rpm;
} wheel;

const int anglea = 90;
const int angleb = 210;
const int anglec = 330;
const int rpmmax = 300;
const int pinpwma = 7;
const int pinpwmb = 8;
const int pinpwmc = 6;
const int pinaa = 29;
const int pinab = 27;     //ALL WHEELS ARE TRYING TO ROTATE BOT CLOCKWISE WHEN A HIGH AND B LOW
const int pinba = 31;
const int pinbb = 33;
const int pinca = 23;
const int pincb = 25;
const float HeadTheta=54.2;
//*****************************************************************



//The following code is to set up PID constants


//***************************************************


struct gain {
  float kd;
  float kp;
  float ki;
  float required;
  float maxControl;
  float minControl;
  float error;
  float previousError;
  float derivativeError;
  float integralError;
};

//****************************************************************



//This is the init variables for LSA08



//******************************************************************
typedef struct LSA08{
  char Address;
  int Theta;
  int JunctionPin;
  int JunctionCount;
  int OePin;
}LSA;
LSA LSAf={0x01,270,3,0,35},LSAr={0x02,0,2,0,37},LSAl={0x03,180,18,0,39},*LSAArray[3]={&LSAf,&LSAr,&LSAl};

const byte rx = 14;    // Defining pin 0 as Rx
const byte tx = 15;    // Defining pin 1 as Tx
const byte OutputEnable[3] = {35,37,39};
const float LSAlength = 11.1;
const float LSAdistance = 46.18;
const int JucntionPulse[3]={3,2,18};
//int JucntionCount[3]={0};
//char add[3]={0x01,0x02,0x03};
//int Theta[3]={270,0,180};
int Test[3]={6,1,0};
int ActiveSensor=0;
//enum LSA08{LSA08a,LSA08b,LSA08c};
//*******************************************************************



struct gain IMUgain, Linegain[3];
struct gain *pIMUgain = &IMUgain, *pLinegain[3] = {&Linegain[0],&Linegain[1],&Linegain[2]};
const float r = 1;
const float pi = 3.14;
int flag = 0;
int theta;

wheel wheela = {0, 0, anglea, rpmmax, pinpwma, pinaa, pinab,0}, wheelb = {0, 0, angleb, rpmmax, pinpwmb, pinba, pinbb,0}, wheelc = {0, 0, anglec, rpmmax, pinpwmc, pinca, pincb,0};
wheel *pwheela = &wheela, *pwheelb = &wheelb, *pwheelc = &wheelc;
wheels *wheelp[3]={pwheela,pwheelb,pwheelc};
float aspeed=0,bspeed=0,cspeed=0;

//enum LSA08 LSA;

float Linecontrol, IMUcontrol;

void setup() {
  Serial.begin(9600);
  //Serial2.begin(115200);
  Serial3.begin(9600);
  pinMode(LSAArray[0]->JunctionPin,INPUT);
  pinMode(LSAArray[1]->JunctionPin,INPUT);
  pinMode(LSAArray[2]->JunctionPin,INPUT);
  IMUinit();                //Initialise IMU
  SetOffset();              //Take initial readings for offset
  initDriving();
  initLSA(9600,LSAArray[0]->OePin);            //const int minControl = -255;      const int maxControl = 255;
  initLSA(9600,LSAArray[1]->OePin);
  //PIDinit(15,0,0,0,-255,255, pIMUgain);
  PIDinit(13,3,0,0,-255,255, pIMUgain);
  PIDinit(.5,0,0,0,-255,255,pLinegain[0]);
  PIDinit(.5,0,0,0,-255,255,pLinegain[1]);
  clearJunction(LSAArray[0]->Address);
  clearJunction(LSAArray[1]->Address);
  timer=millis();           //save ccurrent time in timer ffor gyro integration
  delay(20);
  counter=0;
  
}
///////////////////Set limit if >90

void loop() {
      if(LSAArray[ActiveSensor]->JunctionCount>Test[ActiveSensor]){
        brakeWheels(wheelp);
        int ps,ns;
        ps=ActiveSensor;
        ActiveSensor^=1;
        //ChangeDir(ps,ActiveSensor);

      }
      else{
      float IMUcontrol=HeadControl(HeadTheta,pIMUgain);
      float Linecontrol=LineControl(LSAArray[ActiveSensor]->OePin,17,35,pLinegain[ActiveSensor]);
      for(int j=0;j<2;j++)
      if(digitalRead(LSAArray[j]->JunctionPin)){
        while(digitalRead(LSAArray[j]->JunctionPin)){
        Serial.print("hello");
        }
        LSAArray[j]->JunctionCount=getJunction(LSAArray[j]->Address);
        
      }
      calcRPM(IMUcontrol,LSAArray[ActiveSensor]->Theta+Linecontrol,rpmmax,wheelp);
      //calcRPM(Linecontrol,90,rpmmax,wheelp);
      Serial.println(" Head: "+String(IMUcontrol)+" Line: "+String(Linecontrol)+" CurrentYaw: "+ String(ToDeg(yaw))+" Junction1: "+String(LSAArray[ActiveSensor]->JunctionCount));
      if(mode==1){
      Serial2.flush();
       if(Serial2.available()>0){
       char data = Serial2.read();
       switch(data){
        case 'w':
          theta=270;
          break;
        case 'a':
          theta=180;
          break;
        case 's':
          theta=90;
          break;
        case 'd':
          theta=0;
          break;
        case 'c':
          CalibrateIMU(pIMUgain);
          break;
          }
          calcRPM(IMUcontrol,theta,rpmmax,wheelp);
          startMotion(wheelp);
       }
       else
          brakeWheels(wheelp);
      }
      else if(mode==2){
       if(Serial.available()>0){
        String data = Serial.readString();
        Serial.print(data);
        if (data == "s")
          flag^=1;
        else if(data== "c")
            CalibrateIMU(pIMUgain);
        else
            theta=atoi(data.c_str());
        }
      if(flag==1){
        Serial.println(" Started ");
        startMotion(wheelp);
        }
      else if(flag==0){
        Serial.println(" Stopped ");
        brakeWheels(wheelp);
        }
      }
     }
}
