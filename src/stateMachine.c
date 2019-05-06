/* * =============================================================================
 *
 *       Filename:  StateMachine.c
 *
 *    Description:  状态机
 *
 *        Version:  1.0
 *        Created:  2016-03-09 11:22:34
 *       Revision:  1.0
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
#include <unistd.h>
#include <pthread.h>
#include "stateMachine.h"

/* ---------------------------------------------------------------------------*
 *                  extern variables declare
 *----------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*
 *                  internal functions declare
 *----------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*
 *                        macro define
 *----------------------------------------------------------------------------*/
// #define DBG_MACHINE 1
#if DBG_MACHINE > 0
	#define DBG_P( ... ) printf( __VA_ARGS__ )
#else
	#define DBG_P(...)
#endif

#define MAX_MSG 	5
#define TSK_BUSY    0x80
#define TSK_READY   0x01

typedef struct _TcbStruct{
	int       status;
	int       msgcnt;
	int       msgcur;
	MsgData   msgque[MAX_MSG];
} TcbStruct;

typedef struct _StMachinePriv {
	TcbStruct table;
	StateTable *funcentry;
	void *arg;
	void *data;
	int cur_state;
	int status_run;	
	int table_num;			
	pthread_mutex_t mutex;
}StMachinePriv;
/* ---------------------------------------------------------------------------*
 *                      variables define
 *----------------------------------------------------------------------------*/

static inline void enterCritical(StMachine* This)
{
	pthread_mutex_lock (&This->priv->mutex);	
}

static inline void exitCritical(StMachine* This)
{
	pthread_mutex_unlock (&This->priv->mutex);
}
/* ---------------------------------------------------------------------------*/
/**
 * @brief stmMsgPost 状态机发送事件消息
 *
 * @param This
 * @param msg 消息
 * @param data 附加参数
 */
/* ---------------------------------------------------------------------------*/
static void stmMsgPost(StMachine* This,int msg,void *data)
{
    int pos;
    if (This->priv->table.msgcnt >= MAX_MSG)
		return;

	enterCritical(This);

	pos = This->priv->table.msgcur + This->priv->table.msgcnt;
	if (pos >= MAX_MSG)
		pos -= MAX_MSG;
	This->priv->table.msgque[pos].msg = msg;
	This->priv->table.msgque[pos].data = data;
	This->priv->table.status |= TSK_READY;
	This->priv->table.msgcnt++;

	exitCritical(This);
}


/* ---------------------------------------------------------------------------*/
/**
 * @brief stmMsgRead 状态机读取消息队列
 *
 * @param This
 *
 * @returns 消息队列地址
 */
/* ---------------------------------------------------------------------------*/
static MsgData *stmMsgRead(StMachine *This)
{
    int pos;
    if ((This->priv->table.status & TSK_BUSY) == TSK_BUSY)
		return NULL;
    if (This->priv->table.msgcnt == 0)
		return NULL;
	enterCritical(This);

    pos = This->priv->table.msgcur;
    if (++This->priv->table.msgcur >= MAX_MSG)
        This->priv->table.msgcur = 0;
    This->priv->table.msgcnt--;

	exitCritical(This);
    return &This->priv->table.msgque[pos];
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief stmExecEntry 状态机执行状态判断和转换
 *
 * @param This
 * @param msg 消息
 *
 * @returns 1成功 0失败
 */
/* ---------------------------------------------------------------------------*/
static int stmExecEntry(StMachine *This,MsgData * msg)
{
	int num = This->priv->table_num;
	StateTable *funcentry = This->priv->funcentry;

    for (; num > 0; num--,funcentry++) {
		if (		(msg->msg == funcentry->msg) 
				&& 	(This->priv->cur_state == funcentry->cur_state)) {
			DBG_P("[ST]msg:%d,cur:%d,next:%d,do:%d\n",
					msg->msg,
					funcentry->cur_state,
					funcentry->next_state,
					funcentry->run);
			This->priv->status_run = funcentry->run ;
			This->priv->data = msg->data;
			This->priv->cur_state = funcentry->next_state;
			This->priv->table.status |= TSK_BUSY;
			return 1;
		}
	}
    return 0;
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief stmSchedule 状态机处理
 *
 * @param This
 */
/* ---------------------------------------------------------------------------*/
static void stmSchedule(StMachine *This)
{
    MsgData * msg;

    if ((This->priv->table.status & TSK_BUSY) == TSK_BUSY)
		return ;

	if ((This->priv->table.status & TSK_READY) != TSK_READY)
		return;
	msg = stmMsgRead(This);

	if (msg >= 0) {
		stmExecEntry(This,msg);
	}
	if (This->priv->table.msgcnt == 0) {
		This->priv->table.status &= (~TSK_READY);
	}

}

/* ---------------------------------------------------------------------------*/
/**
 * @brief smtRunMachine 运行状态机 在线程里执行
 *
 * @param This
 */
/* ---------------------------------------------------------------------------*/
static void smtRunMachine(StMachine* This)
{
	stmSchedule(This);

	if ((This->priv->table.status & TSK_BUSY) == TSK_BUSY) {
		This->priv->table.status &= ~TSK_BUSY;
		This->handle(This,This->priv->data,This->priv->arg);
	}

	if (This->priv->data) {
		free(This->priv->data);
		This->priv->data = NULL;
	}
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief stmInitPara 初始化发送状态机消息时带的参数
 *
 * @param This
 * @param size 参数大小
 *
 * @returns 
 */
/* ---------------------------------------------------------------------------*/
static void *stmInitPara(StMachine *This,int size)
{
	void *para = NULL;
	if (size) {
		para = (void *) calloc (1,size);
	}
	return para;
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief stmGetCurrentState 获取状态机当前状态
 *
 * @param This
 *
 * @returns 
 */
/* ---------------------------------------------------------------------------*/
static int stmGetCurrentState(StMachine *This)
{
	return This->priv->cur_state;	
}

static int stmGetCurRun(StMachine *This)
{
	return This->priv->status_run;
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief stmDestroy 销毁状态机
 *
 * @param This
 */
/* ---------------------------------------------------------------------------*/
static void stmDestroy(StMachine **This)
{
	if ((*This)->priv)
		free((*This)->priv);
	if (*This)
		free(*This);
	*This = NULL;	
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief stateMachineCreate 创建状态机
 *
 * @param InitState 初始状态
 * @param state_table 状态机表
 * @param num 状态机表的数量
 * @param id 状态机的ID，区别多个状态机同时运行
 * @param handle 状态机处理，
 *
 * @returns 
 */
/* ---------------------------------------------------------------------------*/
StMachine* stateMachineCreate(int InitState, 
		StateTable *state_table, 
		int num,
		int id,
		void (*handle)(StMachine *This,void *data,void *arg),
		void *arg)
{

	StMachine *This = (StMachine *)calloc(1,sizeof(StMachine));
	This->priv = (StMachinePriv *)calloc(1,sizeof(StMachinePriv));
	This->priv->arg = arg;
	This->priv->funcentry = (StateTable *)state_table;
	This->priv->table_num = num;
	This->priv->cur_state = InitState;
	pthread_mutex_init(&This->priv->mutex, NULL);

	This->id = id;

	This->msgPost = stmMsgPost;
	This->handle = handle;
	This->run = smtRunMachine;
	This->initPara = stmInitPara;
	This->getCurrentstate = stmGetCurrentState;
	This->getCurRun = stmGetCurRun;
	This->destroy = stmDestroy;
	return This;
}

