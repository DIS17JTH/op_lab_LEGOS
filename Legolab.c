#define _GNU_SOURCE //must be placed first in the file
//...
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "tick.h"
#include "BrickPi.h"

#include <linux/i2c-dev.h>
#include <fcntl.h>

#include <pthread.h>
#define NUM_THREADS 5

// Compile Using:
// sudo gcc -o program "Legolab.c" -lrt -lm -lpthread
// Run the compiled program using:
// sudo ./program

//pthread
pthread_attr_t my_attr;
struct sched_param param1, param2, param3, param4, param5;

//ultraSonicSensor
int result,val;
#undef DEBUG
#define US_PORT         PORT_4                       // For the FW Ultrasonic sensor support, use port 3
#define MOTOR_PORT_L	PORT_A
#define MOTOR_PORT_R	PORT_B
#define TUCH_SENSOR_PORT_1  PORT_1
#define TUCH_SENSOR_PORT_2  PORT_2
//#define TUCH_SENSOR_PORT_3  PORT_3
//#define TUCH_SENSOR_PORT_4  PORT_4

//motor
int v,f;

//Bengt
/*
cpu_set_t cpuset; //Must be placed first in each thread function except main
CPU_ZERO(&cpuset);
CPU_SET(0,&cpuset);
pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
*/

static void ultraSonicSensor_Init();
void *ultraSonicSensor_Run();
static void motor_Init();
void *motor_Run();
void *motor_Run2();
static void tuchSensor_Init();
void *tuchSensor_Run();
void *tuchSensor_Run2();
enum commandenum{STOP,FORWARD,BACK,LEFT,RIGHT};
struct order
{
	int urgent_level;
	int duration;
	enum commandenum command;
	int speed;
}order_status;

void firstInEveryThread(){
	cpu_set_t cpuset; //Must be placed first in each thread function except main
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
}


void order_update(int u, int d, enum commandenum c, int s)
{
	if(u>order_status.urgent_level)
	{
	order_status.urgent_level=u;
	order_status.duration=d;
	order_status.command=c;
	order_status.speed=s;
}



int main() {
    ClearTick();

    result = BrickPiSetup();
    printf("BrickPiSetup: %d\n", result);
    if(result)
      return 0;

    BrickPi.Address[0] = 1;
    BrickPi.Address[1] = 2;

    ultraSonicSensor_Init();
    motor_Init();
    tuchSensor_Init();

    result = BrickPiSetupSensors();
    printf("BrickPiSetupSensors: %d\n", result);
    if(!result){

    //pthread
    param1.sched_priority = 1;
    param2.sched_priority = 1;
    param3.sched_priority = 1;
    param4.sched_priority = 1;
    param5.sched_priority = 1;

    pthread_attr_init(&my_attr);
    pthread_attr_setschedpolicy(&my_attr, SCHED_FIFO);
    pthread_attr_setinheritsched(&my_attr, PTHREAD_EXPLICIT_SCHED);

    pthread_t threads[NUM_THREADS];

    pthread_attr_setschedparam(&my_attr, &param1);
    pthread_create(&threads[0], &my_attr, ultraSonicSensor_Run, (void *)0);
    //motor
    pthread_attr_setschedparam(&my_attr, &param2);
    pthread_create(&threads[1], &my_attr, motor_Run, (void *)1);
    pthread_attr_setschedparam(&my_attr, &param3);
    pthread_create(&threads[2], &my_attr, motor_Run2, (void *)2);
    //tuch
    pthread_attr_setschedparam(&my_attr, &param4);
    pthread_create(&threads[3], &my_attr, tuchSensor_Run, (void *)3);
    pthread_attr_setschedparam(&my_attr, &param5);
    pthread_create(&threads[4], &my_attr, tuchSensor_Run2, (void *)4);

    //while true
    while(1){
    	result = BrickPiUpdateValues();
    	if(!result){
		usleep(10000);
    	}
    	usleep(10000);
    }

    int i;
    for(i = 0; i < NUM_THREADS;i++){
	pthread_join(threads[i], NULL);
    }

    pthread_attr_destroy(&my_attr);

  }

  return 0;
}

static void ultraSonicSensor_Init(){

    BrickPi.SensorType[US_PORT] = TYPE_SENSOR_ULTRASONIC_CONT;

}

void *ultraSonicSensor_Run(){
firstInEveryThread();

    while(1){

      	val = BrickPi.Sensor[US_PORT];
        if(val!=255 && val!=127) printf("UltraSonic Results: %3.1d \n", val ); 
	usleep(10000);
    }
}

static void motor_Init(){

  BrickPi.MotorEnable[MOTOR_PORT_L] = 1;
  BrickPi.MotorEnable[MOTOR_PORT_R] = 1;

}

void *motor_Run(){
firstInEveryThread();
  v=0;
  f=1;

    while(1){

		if(order_status.duration>0)
		{
			order_status.duration--;
			BrickPi.MotorSpeed[MOTOR_PORT_L]=200;
		}
		else
		{
			BrickPi.MotorSpeed[MOTOR_PORT_L]=0;
			//BrickPi.MotorSpeed[MOTOR_PORT_R]=0;
			order_status.urgent_level=0;
		}
		/*
		printf("%d\n",v);
		if(f==1) {
			if(++v > 300) f=0;
			BrickPi.MotorSpeed[MOTOR_PORT_L]=200;
			BrickPi.MotorSpeed[MOTOR_PORT_R]=200;
			}
		else{
			if(--v<0) f=1;
			BrickPi.MotorSpeed[MOTOR_PORT_L]=-200;
			BrickPi.MotorSpeed[MOTOR_PORT_R]=-200;
			}
		*/

		usleep(10000);
    }

}

void *motor_Run2(){
firstInEveryThread();
  v=0;
  f=1;

    while(1){
		if(order_status.duration>0)
		{
			order_status.duration--;
			BrickPi.MotorSpeed[MOTOR_PORT_R]=200;
		}
		else
		{
			//BrickPi.MotorSpeed[MOTOR_PORT_L]=0;
			BrickPi.MotorSpeed[MOTOR_PORT_R]=0;
			order_status.urgent_level=0;
		}
		/*
		printf("%d\n",v);
		if(f==1) {
			if(++v > 300) f=0;
			BrickPi.MotorSpeed[MOTOR_PORT_L]=200;
			BrickPi.MotorSpeed[MOTOR_PORT_R]=200;
			}
		else{
			if(--v<0) f=1;
			BrickPi.MotorSpeed[MOTOR_PORT_L]=-200;
			BrickPi.MotorSpeed[MOTOR_PORT_R]=-200;
			}
		*/
		usleep(10000);
    }

}

static void tuchSensor_Init(){
	  BrickPi.SensorType[TUCH_SENSOR_PORT_1] = TYPE_SENSOR_TOUCH;
  	  BrickPi.SensorType[TUCH_SENSOR_PORT_2] = TYPE_SENSOR_TOUCH;
  // BrickPi.SensorType[TUCH_SENSOR_PORT_3] = TYPE_SENSOR_TOUCH;
  // BrickPi.SensorType[TUCH_SENSOR_PORT_4] = TYPE_SENSOR_TOUCH;

}


void *tuchSensor_Run(){
firstInEveryThread();

    while(1){
         printf("Results Tuch Sensor1: %3.1d \n", BrickPi.Sensor[TUCH_SENSOR_PORT_1] );
	 //printf("Results Tuch Sensor2: %3.1d \n", BrickPi.Sensor[TUCH_SENSOR_PORT_2] );
	usleep(10000);
    }
}

void *tuchSensor_Run2(){
firstInEveryThread();

    while(1){
         //printf("Results Tuch Sensor1: %3.1d \n", BrickPi.Sensor[TUCH_SENSOR_PORT_1] );
	 printf("Results Tuch Sensor2: %3.1d \n", BrickPi.Sensor[TUCH_SENSOR_PORT_2] );
	usleep(10000);
    }
}
