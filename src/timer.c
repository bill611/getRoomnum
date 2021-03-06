/*
 * =============================================================================
 *
 *       Filename:  Timer.c
 *
 *    Description:  定时器
 *
 *        Version:  v1.0
 *        Created:  2016-08-08 18:33:01
 *       Revision:  none
 *
 *         Author:  xubin
 *        Company:  Taichuan
 *
 * =============================================================================
 */
/* ---------------------------------------------------------------------------*
 *                      include head files
 *----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "timer.h"
/* ---------------------------------------------------------------------------*
 *                  extern variables declare
 *----------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*
 *                  internal functions declare
 *----------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*
 *                        macro define
 *----------------------------------------------------------------------------*/

typedef struct _TimerPriv {
	timer_t real_id;

	int enable;
	unsigned int speed;
	unsigned int count;
	unsigned int count_old;

	void *arg;
	void (*func)(void *arg);
}TimerPriv;
/* ---------------------------------------------------------------------------*
 *                      variables define
 *----------------------------------------------------------------------------*/

static void timerStart(Timer *This)
{
	if (This->priv->enable) {
		printf("Timer already start\n");
		return;
	}
	This->priv->count_old = This->priv->count = This->getSystemTick();
	This->priv->enable = 1;
}

static void timerStop(Timer *This)
{
	if (This->priv->enable == 0) {
		printf("Timer stopped\n");
		return;
	}
	This->priv->enable = 0;
}

static void timerDestroy(Timer *This)
{
	if (This->priv->real_id)
		This->realTimerDelete(This);
	if (This->priv)
		free(This->priv);
	if (This)
		free(This);
}

static void* timerThread(void *arg)
{
	Timer *This = (Timer *)arg;
	while(1) {
		if (This->priv->enable == 0)
			continue ;
		
		if ((This->priv->count - This->priv->count_old) >= This->priv->speed) {
			if (This->priv->func)
				This->priv->func(This->priv->arg);
			
			This->priv->count_old = This->priv->count;
			// This->priv->count = 0;
		} else {
			// This->priv->count++;
			This->priv->count = This->getSystemTick();
		}
		usleep(10000);
	}
	return NULL;
}

static int timerHandle(Timer *This)
{
	if (This->priv->enable == 0) {
		return 0;
	}
	int ret = 0;
	if ((This->priv->count - This->priv->count_old) >= This->priv->speed) {
		if (This->priv->func) {
			This->priv->func(This->priv->arg);
			ret = 1;
		}
		This->priv->count_old = This->priv->count;
	} else {
		This->priv->count = This->getSystemTick();
	}
	return ret;
}

static unsigned int getSystemTickDefault(void)
{
#if 1
	struct  timeval tv;                                
	gettimeofday(&tv,NULL);                            
	return ((tv.tv_usec / 1000) + tv.tv_sec  * 1000 ); 
#else
	printf("[%s]\n", __FUNCTION__);
	return 0;
#endif
}

static void realTimerCreateDefault(Timer *This,double value,void (*function)(int timerid,int arg))
{
	printf("[%s]\n", __FUNCTION__);
	int result;
	pthread_t m_pthread;					//线程号
	pthread_attr_t threadAttr;				//线程属性

	pthread_attr_init(&threadAttr);		//附加参数

	pthread_attr_setdetachstate(&threadAttr,
			PTHREAD_CREATE_DETACHED);	//设置线程为自动销毁
	result = pthread_create(&m_pthread, &threadAttr, timerThread,This );
	if(result) {
		printf("[%s] pthread failt,Error code:%d\n",__FUNCTION__,result);
	}
	pthread_attr_destroy(&threadAttr);		//释放附加参数

	// timer_create (CLOCK_REALTIME, NULL, &This->priv->real_id);
	// timer_connect (This->priv->real_id, function,0);
	// timer_settime (This->priv->real_id, 0, value, NULL);
}

static void realTimerDeleteDefault(Timer *This)
{
	timer_delete(This->priv->real_id);
	This->priv->real_id = 0;
}

Timer * timerCreate(int speed,void (*function)(void *arg),void *arg)
{
	Timer *This = (Timer *) calloc(1,sizeof(Timer));
	if (!This) {
		printf("timer alloc fail\n");
		return NULL;
	}
	This->priv = (TimerPriv *) calloc(1,sizeof(TimerPriv));
	if (!This->priv){
		printf("timer alloc fail\n");
		free(This);
		return NULL;
	}
	This->priv->speed = speed;
	This->priv->func = function;
	This->priv->arg = arg;

	This->start = timerStart;
	This->stop = timerStop;
	This->destroy = timerDestroy;
	This->handle = timerHandle;
	This->getSystemTick = getSystemTickDefault;

	This->realTimerCreate = realTimerCreateDefault;
	This->realTimerDelete = realTimerDeleteDefault;
	return This;
}
