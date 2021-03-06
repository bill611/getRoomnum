/*
 * =============================================================================
 *
 *       Filename:  stateMachine.h
 *
 *    Description:  状态机处理
 *
 *        Version:  1.0
 *        Created:  2016-11-25 14:49:31
 *       Revision:  1.0
 *
 *         Author:  xubin
 *        Company:  Taichuan
 *
 * =============================================================================
 */
#ifndef _STATE_MACHINE_H
#define _STATE_MACHINE_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */
// #define DBG_MACHINE 1
	typedef struct _StateTable {
		// 事件消息

		int msg;

		// 当前状态

		int cur_state;

		// 下一状态

		int next_state;

		// 执行处理

		int run;

	}StateTable;

	typedef struct _MsgData {
		int msg;
		void *data;
	}MsgData;

	struct _StMachinePriv;
	typedef struct _StMachine {
		struct _StMachinePriv *priv;
		int	id;

		void  *(*initPara)(struct _StMachine *,int size);
		int  (*getCurrentstate)(struct _StMachine *);
		int  (*getCurRun)(struct _StMachine *);
		void  (*msgPost)(struct _StMachine *,int msg,void *data);
		void  (*handle)(struct _StMachine *,void *data,void *arg);
		void  (*run)(struct _StMachine *);
		void  (*destroy)(struct _StMachine **);
	}StMachine;

	StMachine* stateMachineCreate(int InitState, 
			StateTable *state_table, 
			int num,
			int id,
			void (*handle)(StMachine *This,void *data,void *arg),
			void *arg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif
