#define _GNU_SOURCE //must be placed first in the file
//...
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

#include "tick.h"
#include "BrickPi.h"

#include <linux/i2c-dev.h>
#include <fcntl.h>

#include <pthread.h>
#define NUM_THREADS 5
#define DELAY 1000000

// Compile Using:
// sudo gcc -o program "Legolab.c" -lrt -lm -lpthread
// Run the compiled program using:
// sudo ./program

//pthread
pthread_attr_t my_attr;
struct sched_param prio_us, prio_motor, prio_driveForwards, prio_tuch_1, prio_randomFunc;
pthread_t threads[NUM_THREADS];

//ultraSonicSensor
int result = 0;
int val = 0;
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


void firstInEveryThread();
void order_update(int u, int d, enum commandenum c, int s);
void timespec_add_us(struct timespec *t, long us);
static void ultraSonicSensor_Init();
void *ultraSonicSensor_Run();
static void motor_Init();
void *motor_Run();
static void tuchSensor_Init();
void *tuchSensor_Run();
void *driveForwards();
void *randomInstuction();


int main()
{
    ClearTick();
    srand(time(NULL));

    result = BrickPiSetup();
    printf("BrickPiSetup: %d\n", result);
    if (result)
    {
        return 0;
    }

    BrickPi.Address[0] = 1;
    BrickPi.Address[1] = 2;

    ultraSonicSensor_Init();
    motor_Init();
    tuchSensor_Init();

    do
    {
        result = BrickPiSetupSensors();
    } while (result == -1);

    printf("BrickPiSetupSensors: %d\n", result);
    if (!result)
    {       

        pthread_attr_init(&my_attr);
        pthread_attr_setschedpolicy(&my_attr, SCHED_RR);
        pthread_attr_setinheritsched(&my_attr, PTHREAD_EXPLICIT_SCHED);

 	//pthread
        prio_us.sched_priority = 1;
        prio_motor.sched_priority = 1;
        prio_driveForwards.sched_priority = 0;
        prio_tuch_1.sched_priority = 1;
        prio_randomFunc.sched_priority = 0;
        /*
        prio_us.sched_priority = 4;
		prio_motor.sched_priority = 3;
		prio_driveForwards.sched_priority = 1;
		prio_tuch_1.sched_priority = 5;
		prio_randomFunc.sched_priority = 5;
		*/

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

        pthread_attr_setschedparam(&my_attr, &prio_randomFunc);
        pthread_create(&threads[4], &my_attr, randomInstuction, (void *)4);

        //while true
        while (1)
        {
            result = BrickPiUpdateValues();
            //order_update(5, 50,FORWARD, 200);
            usleep(1000);
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

void firstInEveryThread()
{
    cpu_set_t cpuset; //Must be placed first in each thread function except main
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

static void ultraSonicSensor_Init()
{

    BrickPi.SensorType[US_PORT] = TYPE_SENSOR_ULTRASONIC_CONT;
}

void *ultraSonicSensor_Run()
{
    firstInEveryThread();
    struct timespec next= {0,0};
    printf("HEJ JAG HETER ULTRA\n");
    while (1)
    {

        val = BrickPi.Sensor[US_PORT];
        if (val != 255 && val != 127)
        {
            if (0 <= val)
            {
                if (val > 10 && val < 30)
                    order_update(4, 1000, BACK, -100);
                printf("UltraSonic Results: %3.1d \n", val);
                //printf("ultra hej\n");
            }
        }
        // Blir 30 millisekunder
        timespec_add_us(&next, 30000); //long)DELAY*0.030
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);
	//usleep(1000);
	//printf("JAG KOR: ULTRA\n");
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
    struct timespec next= {0,0};
    printf("HEJ JAG HETER MOTOR\n");
    v = 0;
    f = 1;

    while (1)
    {
        //printf("motorrrrrrrrrrrrr\n");
        if (order_status.duration > 0)
        	{ 
			printf("motorrrrrrrrrrrrr %d\n", order_status.duration);
			switch (order_status.command)
			{
			case FORWARD:
				BrickPi.MotorSpeed[MOTOR_PORT_L] = order_status.speed;
				BrickPi.MotorSpeed[MOTOR_PORT_R] = order_status.speed;
				break;
			case STOP:
				BrickPi.MotorSpeed[MOTOR_PORT_L] = order_status.speed;
				BrickPi.MotorSpeed[MOTOR_PORT_R] = order_status.speed;
				break;
			case BACK:
				BrickPi.MotorSpeed[MOTOR_PORT_L] = order_status.speed;
				BrickPi.MotorSpeed[MOTOR_PORT_R] = order_status.speed;
				break;
			case LEFT:
				BrickPi.MotorSpeed[MOTOR_PORT_L] = 0;
				BrickPi.MotorSpeed[MOTOR_PORT_R] = order_status.speed;
				break;
			case RIGHT:
				BrickPi.MotorSpeed[MOTOR_PORT_L] = order_status.speed;
				BrickPi.MotorSpeed[MOTOR_PORT_R] = 0;
				break;
			default:
				break;
			}

			order_status.duration--;
		}

        else
        {
            BrickPi.MotorSpeed[MOTOR_PORT_L] = 0;
            BrickPi.MotorSpeed[MOTOR_PORT_R] = 0;
            //order_status.urgent_level = 0;
            //printf("else\n");
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
        //Blir hundra millisekunder
        timespec_add_us(&next, 50000); //DELAY*0.100
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);
	//usleep(1000);
        //printf("JAG KOR: MOTOR\n");
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
    struct timespec next= {0,0};
    printf("HEJ JAG HETER TOUCHSENSOR\n");

    while (1)
    {
        if (BrickPi.Sensor[TUCH_SENSOR_PORT_1] == 1)
        {
            order_update(5, 10000, LEFT, 100);
            printf("Results Tuch Sensor1: %3.1d \n", BrickPi.Sensor[TUCH_SENSOR_PORT_1]);
        }
        if (BrickPi.Sensor[TUCH_SENSOR_PORT_2] == 1)
        {
            order_update(5, 10000, RIGHT, 100);
            printf("Results Tuch Sensor2: %3.1d \n", BrickPi.Sensor[TUCH_SENSOR_PORT_2]);
        }
        // Delay blir tio millisekunder
        timespec_add_us(&next, 10000); //DELAY*0.010
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);
	//usleep(1000);
        //printf("JAG KOR: TOUCHSENSOR\n");
    }
}
void *driveForwards()
{
    firstInEveryThread();
    struct timespec next = {0,0};
    printf("HEJ JAG HETER DRIVE\n");
    while (1)
    {
        order_update(1, 30, FORWARD, 200);
        timespec_add_us(&next, 900000); //(long)DELAY
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);
	//usleep(100000);
        //printf("JAG KOR: DRIVE\n");
    }
}

void order_update(int u, int d, enum commandenum c, int s)
{

    if (u > order_status.urgent_level)
    {
        printf("order update\n");
        order_status.urgent_level = u;
        order_status.duration = d;
        order_status.command = c;
        order_status.speed = s;
    }
}

void timespec_add_us(struct timespec *t, long us)
{
    t->tv_nsec += us * 1000;
    if (t->tv_nsec > 1000000000)
    {
        t->tv_nsec = t->tv_nsec - 1000000000; // + ms*1000000
        t->tv_sec += 1;
    }
}

void *randomInstuction()
{
    firstInEveryThread();
    struct timespec next= {0,0};
    printf("HEJ JAG HETER RANDOM_INSTRUCTION\n");
    while(1){
    	printf("Random Number: %d \n", rand());
	timespec_add_us(&next, 900000); //DELAY*0.010
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);
	//usleep(10000000);
    }
}