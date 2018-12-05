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
struct sched_param prio_us, prio_motor, prio_driveForwards, prio_tuch_1, prio_tuch_2;
pthread_t threads[NUM_THREADS];

//ultraSonicSensor
int result, val;
#undef DEBUG
#define US_PORT PORT_4 // For the FW Ultrasonic sensor support, use port 3
#define MOTOR_PORT_L PORT_A
#define MOTOR_PORT_R PORT_B
#define TUCH_SENSOR_PORT_1 PORT_1
#define TUCH_SENSOR_PORT_2 PORT_2
//#define TUCH_SENSOR_PORT_3  PORT_3
//#define TUCH_SENSOR_PORT_4  PORT_4

//motor
int v, f;

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
static void tuchSensor_Init();
void *tuchSensor_Run();
void *driveForwards();

enum commandenum
{
	STOP,
	FORWARD,
	BACK,
	LEFT,
	RIGHT
};
struct order
{
	int urgent_level;
	int duration;
	enum commandenum command;
	int speed;
} order_status;

void firstInEveryThread()
{
	cpu_set_t cpuset; //Must be placed first in each thread function except main
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void order_update(int u, int d, enum commandenum c, int s)
{
	if (u > order_status.urgent_level)
	{
		order_status.urgent_level = u;
		order_status.duration = d;
		order_status.command = c;
		order_status.speed = s;
	}
}

void timespec_add_us(struct timespec *t, long us)
{
	t->tv_nsec += us*1000;
	if(t->tv_nsec > 1000000000) 
	{
		t->tv_nsec = t->tv_nsec - 1000000000; // + ms*1000000
		t->tv_sec += 1;
	}		
}

int main()
{
	ClearTick();

	result = BrickPiSetup();
	printf("BrickPiSetup: %d\n", result);
	if (result)
		return 0;

	BrickPi.Address[0] = 1;
	BrickPi.Address[1] = 2;

	ultraSonicSensor_Init();
	motor_Init();
	tuchSensor_Init();

	result = BrickPiSetupSensors();
	printf("BrickPiSetupSensors: %d\n", result);
	if (!result)
	{

		//pthread
		prio_us.sched_priority = 4;
		prio_motor.sched_priority = 1;
		prio_driveForwards.sched_priority = 1;
		prio_tuch_1.sched_priority = 5;
		//prio_tuch_2.sched_priority = 5;

		pthread_attr_init(&my_attr);
		pthread_attr_setschedpolicy(&my_attr, SCHED_RR);
		pthread_attr_setinheritsched(&my_attr, PTHREAD_EXPLICIT_SCHED);

		//ultrasonic
		pthread_attr_setschedparam(&my_attr, &prio_us);
		pthread_create(&threads[0], &my_attr, ultraSonicSensor_Run, (void *)0);
		
		//motor
		pthread_attr_setschedparam(&my_attr, &prio_motor);
		pthread_create(&threads[1], &my_attr, motor_Run, (void *)1);

		//tuch
		pthread_attr_setschedparam(&my_attr, &prio_tuch_1);
		pthread_create(&threads[2], &my_attr, tuchSensor_Run, (void *)2);

		//forward
		pthread_attr_setschedparam(&my_attr, &prio_driveForwards);
		pthread_create(&threads[3], &my_attr, driveForwards, (void *)3);



//		pthread_attr_setschedparam(&my_attr, &prio_tuch_2);
//		pthread_create(&threads[4], &my_attr, tuchSensor_Run2, (void *)4);
//		pthread_attr_setschedparam(&my_attr, &prio_motor_R);
//		pthread_create(&threads[2], &my_attr, motor_Run2, (void *)2);

		//while true
		while (1)
		{
			result = BrickPiUpdateValues();
			if (!result)
			{
				usleep(10000);
			}
			usleep(10000);
		}

		int i;
		for (i = 0; i < NUM_THREADS; i++)
		{
			pthread_join(threads[i], NULL);
		}

		pthread_attr_destroy(&my_attr);
	}

	return 0;
}

static void ultraSonicSensor_Init()
{

	BrickPi.SensorType[US_PORT] = TYPE_SENSOR_ULTRASONIC_CONT;
}

void *ultraSonicSensor_Run()
{
	firstInEveryThread();
	struct timespec next;

	while (1)
	{

		val = BrickPi.Sensor[US_PORT];
		if (val != 255 && val != 127)
		{
			if(0 <= val && val <= 20)
				order_update(4, 3 ,STOP , 0);
			//printf("UltraSonic Results: %3.1d \n", val);
		}
		timespec_add_us(&next, 1000);
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);
	}
}

static void motor_Init()
{

	BrickPi.MotorEnable[MOTOR_PORT_L] = 1;
	BrickPi.MotorEnable[MOTOR_PORT_R] = 1;
}

void *motor_Run()
{
	firstInEveryThread();
	struct timespec next;
	v = 0;
	f = 1;

	while (1)
	{

		if (order_status.duration > 0)
		{
			order_status.duration--;
			BrickPi.MotorSpeed[MOTOR_PORT_L] = 200;			
			BrickPi.MotorSpeed[MOTOR_PORT_R] = 200;
		}
		else
		{
			BrickPi.MotorSpeed[MOTOR_PORT_L] = 0;
			//BrickPi.MotorSpeed[MOTOR_PORT_R]=0;
			order_status.urgent_level = 0;
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

		timespec_add_us(&next, 1000);
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);
	}
}

static void tuchSensor_Init()
{
	BrickPi.SensorType[TUCH_SENSOR_PORT_1] = TYPE_SENSOR_TOUCH;
	BrickPi.SensorType[TUCH_SENSOR_PORT_2] = TYPE_SENSOR_TOUCH;
	// BrickPi.SensorType[TUCH_SENSOR_PORT_3] = TYPE_SENSOR_TOUCH;
	// BrickPi.SensorType[TUCH_SENSOR_PORT_4] = TYPE_SENSOR_TOUCH;
}

void *tuchSensor_Run()
{
	firstInEveryThread();
	struct timespec next;

	while (1)
	{
		if(BrickPi.Sensor[TUCH_SENSOR_PORT_1] == 1)
		{
			order_update(5, 5, LEFT, 100);
			//printf("Results Tuch Sensor1: %3.1d \n", BrickPi.Sensor[TUCH_SENSOR_PORT_1]);
		}
		if(BrickPi.Sensor[TUCH_SENSOR_PORT_2] == 1)
		{
			order_update(5, 5, RIGHT, 100);
			//printf("Results Tuch Sensor2: %3.1d \n", BrickPi.Sensor[TUCH_SENSOR_PORT_2]);
		}
			timespec_add_us(&next, 1000);
			clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);
	}
}
void *driveForwards()
{
	firstInEveryThread();
	struct timespec next;
	while(1)
	{
		order_update(1, 10, FORWARD, 200);
		timespec_add_us(&next, 1000);
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);
	}	
}


