/*
 * =============================================================================
 *
 *       Filename:  protocol_getRoomnum.c
 *
 *    Description:  太川房号获取协议
 *
 *        Version:  1.0
 *        Created:  2019-01-02 15:22:47
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
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "timer.h"
#include "stateMachine.h"
#include "protocol_getRoomnum.h"

/* ---------------------------------------------------------------------------*
 *                        macro define
 *----------------------------------------------------------------------------*/
#ifdef DBG_GET_ROOM
    #define DBG_P( ...  ) \
    do { \
           printf("[GETRM DEBUG]"); \
        printf( __VA_ARGS__  );  \
    }while(0)
#else
    #define DBG_P( ...  )
#endif

#define NELEMENTS(array)        /* number of elements in an array */ \
	            (sizeof (array) / sizeof ((array) [0]))
#define LOADFUNC(func) \
do {\
	if (in->func)\
		priv->func = in->func;\
	else\
		priv->func = func##Default;\
} while (0)

#define PROTOCOL_VERSION   1.0

// 中控主机获取设备地址信息
typedef struct
{
	unsigned int ID;
	unsigned int Size;
	unsigned int Type;		// 0x8000
	unsigned int LiDevID;   // 设备编号低位
	unsigned int HiDevID;   // 设备编号高位
	unsigned int GetType;   // 获取内容 0取期号 1取栋号 2取单元号 3取层号 4取房号 5取分机号
	union {
		struct  {
			char QH[8];     // 期号，当GetType=0时忽略该字段
			char DH[8];
			char DYH[8];
			char CH[8];
			char FH[8];
		} room_num;
		char room_array[40];
	} room_num;
} TGetDevAddrInfo;

//小区信息数据结构
typedef struct
{
	char Code[4];
	char Caption[4];
} TXCInfo;

typedef struct
{
	unsigned int ID;
	unsigned int Size;
	unsigned int Type;		// 0x8001
	unsigned int LiDevID;
    unsigned int HiDevID;
    unsigned int RetType;
    unsigned int PacketCnt;
    unsigned int PacketIdx;
    unsigned int RecCnt;
	TXCInfo XCInfo[1];
} TRetDevAddrInfo;

//中控主机获取设备配置信息
typedef struct {
	unsigned int ID;
	unsigned int Size;
	unsigned int Type;	// 0x8002
	union {
		struct  {
			char QH[8];     // 期号，当GetType=0时忽略该字段
			char DH[8];
			char DYH[8];
			char CH[8];
			char FH[8];
		} room_num;
		char room_array[40];
	} room_num;
    char FJH[8];
} TGetConfigInfo;

//管理中心返回设备配置信息
typedef struct
{
	unsigned int ID;
	unsigned int Size;
	unsigned int Type;	// TP_GETCONFIGADDR_EXT1=0x800A,	//扩展室内机申请房号命令
	union {
		struct{
			char QH[8];
			char DH[8];		    //栋号
			char DYH[8];		//单元号
			char CH[8];		    //层号
			char FH[8];		    //房号
			char FJH[8];		//分机号
		} room_num;
		char room_array[48];
	};
	char IP[16];
	char Mask[16];
	char GateWay[16];
	char RoomName[32];
	char RoomNumber[32];
	int CenterCnt;          //中心数量
	TDeviceInf CenterDev[4];
	char HMKIP[16];                 //户门口机IP
	char ULifeAddr[32];
	char CommunityAddr[32];
	char DDNS1Addr[32];
	char DDNS2Addr[32];
	unsigned short SystemCode;
	unsigned short UserCode;
	unsigned int CallBySrv;
	char MasterIP[16];              //主机IP地址
	char LifeCtrlIP[16];            //电梯控制器IP地址
	int DoorCnt;                    //门口机数量
	TDeviceInf DoorPhone[1];
} TRetCenterAddrInfo_Ext;

// 接收的包类型
enum {
	TYPE_QH,	//期号
	TYPE_DH,	//栋号
	TYPE_DYH,	//单元号
	TYPE_CH,	//层号
	TYPE_FH,	//房号
	TYPE_FJH,	//分机号
};

typedef struct _GetRoomNumPriv {
	Timer *timer_search_center;
	Timer *timer_get_room;
	StMachine *st_machine;
	char *ip, *mask;
	char *master_ip;
	int port;
	char temp_ip[16];
	char center_ip[16];
	int packet_id;
	int cmd_get_dev_info;
	int cmd_get_config_info;
	uint64_t dev_id;
	int max_rows,max_columns;
	int rooms_num;
	RoomData *rooms;
	RoomData *disp_rooms;
	unsigned int cast_count; // 广播次数
	TGetDevAddrInfo room_inf;
	char centers[MAX_CENTER_NUM][16];

	int page_now;
	int select_index;
}GetRoomNumPriv;

// 状态机
enum {
	EVENT_START_GET_RM,		//设置房号
	EVENT_START_GET_PH,
	EVENT_START_CALL,
	EVENT_GET_RM,			//获取房号
	EVENT_CALL,			//获取房号
	EVENT_END,		//结束获取房号
};
enum {
	ST_IDLE,				//空闲状态
	ST_BROARDCAST,			//广播状态
	ST_SELECT_CENTER,		//选择管理中心状态
	ST_GET_CENTER,			//获取管理中心状态
	ST_GET_INFO,			//获取房号状态
	ST_GET_CALL_LIST,		//
	ST_CALL,				//
};
enum {
	DO_BROARDCAST,			//广播
	DO_SELECT_CENTER,		//选择管理中心
	DO_GET_INFO,			//选择房号
	DO_END,					//结束
	DO_GET_CENTER,
	DO_GET_CALL_LIST,
	DO_CALL,
};
typedef struct _STMData {
	int index;
	GetRoomNumCenterDmks *data;
    GetRoomNum *p_get_room_num;
}STMData;

typedef struct _STMDo {
    int action;
    int (*proc)(GetRoomNum *,STMData *data);
}STMDo;
/* ---------------------------------------------------------------------------*
 *                  extern variables declare
 *----------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*
 *                  internal functions declare
 *----------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*
 *                      variables define
 *----------------------------------------------------------------------------*/
GetRoomNum *get_room_num = NULL;
static STMData *stm_data = NULL;
static StateTable stm_getroomnum_state[] = {
	{EVENT_START_GET_RM,ST_IDLE,			ST_BROARDCAST,		DO_BROARDCAST},
	{EVENT_START_GET_RM,ST_BROARDCAST,		ST_BROARDCAST,		DO_BROARDCAST},

	{EVENT_START_GET_PH,ST_IDLE,			ST_GET_CENTER,		DO_GET_CENTER},

	{EVENT_START_CALL,	ST_IDLE,			ST_GET_CALL_LIST,	DO_GET_CALL_LIST},

	{EVENT_GET_RM,		ST_BROARDCAST,		ST_SELECT_CENTER,	DO_SELECT_CENTER},
	{EVENT_GET_RM,		ST_GET_CENTER,		ST_SELECT_CENTER,	DO_SELECT_CENTER},
	{EVENT_GET_RM,		ST_SELECT_CENTER,	ST_GET_INFO,		DO_GET_INFO},
	{EVENT_GET_RM,		ST_GET_INFO,		ST_GET_INFO,		DO_GET_INFO},

	{EVENT_CALL,		ST_GET_CALL_LIST,	ST_CALL,			DO_CALL},

	{EVENT_END,			ST_IDLE,			ST_IDLE,			DO_END},
	{EVENT_END,			ST_BROARDCAST,		ST_IDLE,			DO_END},
	{EVENT_END,			ST_SELECT_CENTER,	ST_IDLE,			DO_END},
	{EVENT_END,			ST_GET_CENTER,		ST_IDLE,			DO_END},
	{EVENT_END,			ST_GET_INFO,		ST_IDLE,			DO_END},
	{EVENT_END,			ST_GET_CALL_LIST,	ST_IDLE,			DO_END},
	{EVENT_END,			ST_CALL,			ST_IDLE,			DO_END},
};

/* ---------------------------------------------------------------------------*/
/**
 * @brief printRoomNum 打印房号数据，调试使用
 *
 * @param This
 */
/* ---------------------------------------------------------------------------*/
static void printRoomNum(GetRoomNum *This)
{
#ifdef DBG_GET_ROOM
	int i,j;
	printf("index:%d\n",This->priv->select_index);
	for (i=0; i<This->priv->max_rows; i++) {
		for (j=0; j<This->priv->max_columns; j++) {
			printf("[%s]", (This->priv->disp_rooms + i*This->priv->max_columns + j)->name);
		}
		printf("\n");
	}
#endif
}
static int getDispRooms(GetRoomNum *This,RoomData **data)
{
	*data = This->priv->disp_rooms;
	return This->priv->select_index;	
}
static void clearRooms(GetRoomNum *This)
{
	if (This->priv->rooms)
		free(This->priv->rooms);
	This->priv->rooms_num = 0;
	This->priv->disp_rooms = This->priv->rooms = NULL;
}
/* ---------------------------------------------------------------------------*/
/**
 * @brief getBroadcastIP 获得广播域
 *
 * @param psrc_ip 原IP
 * @param pMask	 子网掩码
 *
 * @returns 广播IP
 */
/* ---------------------------------------------------------------------------*/
const char * getBroadcastIP(const char *psrc_ip,const char *pMask)
{
    static char ip[16];
    unsigned int src_ip,des_ip,mask;
    unsigned char *pIP = (unsigned char *)&des_ip;
    src_ip = inet_addr(psrc_ip);
    des_ip = inet_addr("255.255.255.255");
    mask = inet_addr(pMask);
    if((src_ip & mask)!=(des_ip & mask)) {
           des_ip = (src_ip & mask) | (~mask & 0xFFFFFFFF);
    }
    sprintf(ip,"%d.%d.%d.%d",pIP[0],pIP[1],pIP[2],pIP[3]);
    return ip;
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief fillRooms 填充对应数组房号
 *
 * @param This
 * @param index 数组下标
 * @param name 名字
 * @param ip IP
 * @param type 房号类型
 */
/* ---------------------------------------------------------------------------*/
static void fillRooms(GetRoomNum *This,int index,char *name,char *ip,int type)
{
	strcpy(This->priv->rooms[index].name,name);
	strcpy(This->priv->rooms[index].ip,ip);
	This->priv->rooms[index].type = type;
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief dispBaseInfo 在UI界面显示返回的房号信息列表
 *
 * @param pAddrInfo 返回信息列表
 */
/* ---------------------------------------------------------------------------*/
static void dispBaseInfo(GetRoomNum *This,TRetDevAddrInfo *pAddrInfo)
{
	// 停止定时器
	This->priv->timer_get_room->stop(This->priv->timer_get_room);
	unsigned int i;
	TXCInfo *pXC;
	This->priv->page_now = 0;
	This->priv->select_index = 0;
	pXC = pAddrInfo->XCInfo;
	This->priv->rooms_num = pAddrInfo->RecCnt;
	for(i=0; i < pAddrInfo->RecCnt; i++) {
		fillRooms(This,i,pXC->Code,pXC->Code,pAddrInfo->RetType);
		pXC++;
	}

	This->priv->room_inf.GetType = pAddrInfo->RetType;
	This->priv->disp_rooms = This->priv->rooms;

	if(pAddrInfo->RetType == TYPE_QH) {
		DBG_P("get room step qh\n");
		This->interface->uiUpdateTitle(This,GETROOM_UI_SELECT_QH);
	} else if(pAddrInfo->RetType == TYPE_DH) {
		DBG_P("get room step dh\n");
		This->interface->uiUpdateTitle(This,GETROOM_UI_SELECT_DH);
	} else if(pAddrInfo->RetType == TYPE_DYH) {
		DBG_P("get room step dyh\n");
		This->interface->uiUpdateTitle(This,GETROOM_UI_SELECT_DYH);
	} else if(pAddrInfo->RetType == TYPE_CH) {
		DBG_P("get room step ch\n");
		This->interface->uiUpdateTitle(This,GETROOM_UI_SELECT_CH);
	} else if(pAddrInfo->RetType == TYPE_FH) {
		DBG_P("get room step fh\n");
		This->interface->uiUpdateTitle(This,GETROOM_UI_SELECT_FH);
	} else if(pAddrInfo->RetType == TYPE_FJH) {
		DBG_P("get room step fjh\n");
		This->interface->uiUpdateTitle(This,GETROOM_UI_SELECT_FJH);
	}

	if(pAddrInfo->RecCnt == 1) {// 只有一组时，自动发下一组
		stm_data = (STMData *)This->priv->st_machine->initPara(This->priv->st_machine,
				sizeof(STMData));
		stm_data->index = 0;
		This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_GET_RM,stm_data);
		return;
	} else {
		This->interface->uiUpdate(This);
	}

}

static void savePhoneCallBack(void *arg)
{
	GetRoomNum *This = arg;
	This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_END,0);
	This->interface->uiUpdateTitle(This,GETROOM_UI_SAVE_PHONE_END);
}
/* ---------------------------------------------------------------------------*/
/**
 * @brief savePhonelist 保存户户对讲时的房号
 *
 * @param data
 */
/* ---------------------------------------------------------------------------*/
static void savePhonelist(GetRoomNum *This,char *data)
{
	printf("[%s]\n", __FUNCTION__);
	TRetCenterAddrInfo_Ext *pDeviceinfo = (TRetCenterAddrInfo_Ext*)data;
	clearRooms(This);
	This->interface->uiUpdate(This);
	if ( (strcmp(pDeviceinfo->IP,This->priv->ip) == 0)
   		|| (strcmp(pDeviceinfo->IP,This->priv->master_ip) == 0))	{
		This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_END,0);
		This->interface->uiUpdateTitle(This,GETROOM_UI_FAIL_SAVE_MYSELF_IP);
		return;
	}
	This->interface->uiUpdateTitle(This,GETROOM_UI_SAVING);
	int ret = This->interface->savePhoneNum(This,
			(GetRoomRooms *)pDeviceinfo->room_array,
			pDeviceinfo->IP,
			pDeviceinfo->RoomName,savePhoneCallBack);
	if (ret == 0) {
		This->interface->uiUpdateTitle(This,GETROOM_UI_FAIL_ADDR_EXIST);
	}
}

static void saveRoomnumCallBack(void *arg)
{
	GetRoomNum *This = arg;
	This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_END,0);
	This->interface->uiUpdateTitle(This,GETROOM_UI_SAVE_ROOMNUM_END);
}
/* ---------------------------------------------------------------------------*/
/**
 * @brief saveRoomnum 保存房号
 *
 * @param AData
 */
/* ---------------------------------------------------------------------------*/
static void saveRoomnum(GetRoomNum *This,char *data)
{
	DBG_P("get room step saving..\n" );
	TRetCenterAddrInfo_Ext *pDeviceinfo = (TRetCenterAddrInfo_Ext *)data;
	clearRooms(This);
	This->interface->uiUpdate(This);

	This->interface->uiUpdateTitle(This,GETROOM_UI_SAVING);

	// 保存网络配置
	This->interface->saveNetConfig(This,pDeviceinfo->IP,pDeviceinfo->Mask,pDeviceinfo->GateWay);
	// 保存房号
	This->interface->saveRoomNum(This,(GetRoomRooms *)pDeviceinfo->room_array);
	// 保存房间名称
	This->interface->saveRoomName(This,pDeviceinfo->RoomName);
	// 保存主机IP
	This->interface->saveMasterIp(This,pDeviceinfo->MasterIP);
	// 保存管理中心，门口机，户门口机信息
	GetRoomNumCenterDmks info;
	info.center_cnt = pDeviceinfo->CenterCnt;
	info.centers = pDeviceinfo->CenterDev;
	info.dmk_cnt = pDeviceinfo->DoorCnt;
	info.dmks = pDeviceinfo->DoorPhone;
	info.hdmk_cnt = 1;
	TDeviceInf hdmk;
	strcpy(hdmk.IP,pDeviceinfo->HMKIP);
	info.hdmks = &hdmk;
	This->interface->saveCenterDmks(This,&info,saveRoomnumCallBack);
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief timerSearchCenter3S 获取管理中心超时
 *
 * @param arg
 */
/* ---------------------------------------------------------------------------*/
static void timerSearchCenter3S(void *arg)
{
	GetRoomNum *This = arg;
	This->priv->timer_search_center->stop(This->priv->timer_search_center);

	if (This->priv->rooms_num == 0) { // 未搜索到管理中心
		if (This->priv->cast_count < 5) {
			This->priv->cast_count++;
			This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_START_GET_RM,0);
			return;
		}
		This->interface->uiUpdateTitle(This,GETROOM_UI_FAIL_COMM);
	} else if (This->priv->rooms_num == 1) {// 搜索到一个管理中心
		stm_data = (STMData *)This->priv->st_machine->initPara(This->priv->st_machine,
				sizeof(STMData));
		stm_data->index = 0;
		This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_GET_RM,stm_data);
	} else {
		This->interface->uiUpdateTitle(This,GETROOM_UI_SELECT_CENTER);
	}
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief timerGetRoom3S 获取房号超时
 *
 * @param arg
 */
/* ---------------------------------------------------------------------------*/
static void timerGetRoom3S(void *arg)
{
	GetRoomNum *This = arg;
	This->priv->timer_get_room->stop(This->priv->timer_get_room);
	if (This->priv->rooms_num == 0)  // 未搜索房号
		This->interface->uiUpdateTitle(This,GETROOM_UI_FAIL_COMM);
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief getRoomNumFilter 过滤重复IP
 *
 * @param ABinding
 *
 * @returns
 */
/* ---------------------------------------------------------------------------*/
#if 0
static int getRoomNumFilter(GetRoomNum *This,char *ip)
{
	if (This->priv->center_ip[0] == 0) {
		int i;
		for (i=0; i<MAX_CENTER_NUM; i++) {
			if(strcmp(ip, This->priv->centers[i]) == 0)
				return 0;
			if (This->priv->centers[i][0] == 0) {
				memcpy(&This->priv->centers[i],ip,sizeof(This->priv->centers[0]));
				return 1;
			}
		}
		return 1;
	}
	if (strcmp(This->priv->center_ip,ip) != 0)
		return 0;
	return 1;
}
#endif
/* ---------------------------------------------------------------------------*/
/**
 * @brief cmdSendGetDevInfo 发送获取设备信息命令
 *
 * @param ID 首次发送为0，之后发送+1
 * @param ip 发送目标IP
 */
/* ---------------------------------------------------------------------------*/
static void cmdSendGetDevInfo(GetRoomNum *This,unsigned int id,char *ip)
{
	This->priv->room_inf.ID = id;
	This->priv->room_inf.Size = sizeof(TGetDevAddrInfo);
	This->priv->room_inf.Type = This->priv->cmd_get_dev_info;
	This->priv->room_inf.LiDevID = (unsigned int)This->priv->dev_id;
	This->priv->room_inf.HiDevID = (unsigned int)(This->priv->dev_id >> 32);
	This->interface->udpSend(This,
			ip, This->priv->port,
		   	&This->priv->room_inf,
			This->priv->room_inf.Size);
	if (id) {
		This->priv->rooms_num = 0;
		This->priv->page_now = 0;
		This->priv->select_index = 0;
		memset(This->priv->rooms,0,MAX_ROOM_NUM * sizeof(RoomData));
		This->priv->disp_rooms = This->priv->rooms;
	}
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief udpCmdGetDevInfo 接收UDP端口获取中心，房号协议
 *
 * @param This
 * @param ip IP
 * @param data 数据
 */
/* ---------------------------------------------------------------------------*/
static void udpCmdGetDevInfo(GetRoomNum *This,char *ip,char *data)
{
	DBG_P("TP_RESPDEVADDR:ResPdevAddr():%s,center:%s\n",ip,This->priv->center_ip);
	// if (getRoomNumFilter(This,ip) == 0) {
		// return;
	// }

	TRetDevAddrInfo *pAddrInfo = (TRetDevAddrInfo*)data;

	if (This->priv->center_ip[0] == 0) {
		This->priv->rooms_num++;
		fillRooms(This,This->priv->rooms_num-1,ip,ip,0);
		This->priv->disp_rooms = This->priv->rooms;
		This->interface->uiUpdate(This);
	} else
		dispBaseInfo(This,pAddrInfo);
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief udpCmdSaveConfig 接收UDP端口保存房号协议
 *
 * @param This
 * @param ip
 * @param data
 */
/* ---------------------------------------------------------------------------*/
static void udpCmdSaveConfig(GetRoomNum *This,char *ip,char *data)
{
	DBG_P("TP_RESCONFIGADDR:ResConfigAddr()\n");
	// if (getRoomNumFilter(This,ip) == 0)
		// return;
	This->priv->timer_get_room->stop(This->priv->timer_get_room);
	if (This->type == ICON_TYPE_SETDJ) {
		savePhonelist(This,data);
	} else {
		saveRoomnum(This,data);
	}
}


/* ---------------------------------------------------------------------------*/
/**
 * @brief start 开始获取房号等流程,分配最大房号内存
 *
 * @param This
 * @param type 设置房号，呼叫中心，对讲机，设置户户对讲等
 */
/* ---------------------------------------------------------------------------*/
static void start(GetRoomNum *This,
		GetRoomNumCenterDmks *info,
		int rows,int columns)
{
	This->priv->max_rows = rows;
	This->priv->max_columns = columns;
	This->priv->page_now = 0;
	This->priv->select_index = 0;
	This->priv->cast_count = 0;
	This->priv->rooms_num = 0;
	This->priv->rooms = (RoomData *) calloc (MAX_ROOM_NUM,sizeof(RoomData));
	memset(This->priv->centers, 0, sizeof(This->priv->centers));
	memset(This->priv->center_ip,0,sizeof(This->priv->center_ip));
	memset(&This->priv->room_inf, 0, sizeof(TGetDevAddrInfo));

	if (This->type == ICON_TYPE_SETFH) {
		This->interface->setTempNetIp(This,
				This->priv->ip,This->priv->temp_ip);
		This->interface->uiUpdateTitle(This,GETROOM_UI_SEARCHING_CENTER);
		This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_START_GET_RM,0);
		DBG_P("search center..\n" );
	} else if (This->type == ICON_TYPE_SETDJ) {
		stm_data = (STMData *)This->priv->st_machine->initPara(This->priv->st_machine,
				sizeof(STMData));
		stm_data->data = info;
		This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_START_GET_PH,stm_data);
		DBG_P("get center..\n" );
	} else {
		stm_data = (STMData *)This->priv->st_machine->initPara(This->priv->st_machine,
				sizeof(STMData));
		stm_data->data = info;
		This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_START_CALL,stm_data);
		DBG_P("show call list..\n" );
	}
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief end 结束时调用
 *
 * @param This
 */
/* ---------------------------------------------------------------------------*/
static void end(GetRoomNum *This)
{
	This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_END,0);
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief selectRoom 选择房号
 *
 * @param This
 * @param index
 */
/* ---------------------------------------------------------------------------*/
static void selectRoom(GetRoomNum *This,int index)
{
	if (index >= MAX_ROOM_NUM) {
		printf("err select %d,max:%d\n", index,MAX_ROOM_NUM);
		return;
	}
	stm_data = (STMData *)This->priv->st_machine->initPara(This->priv->st_machine,
			        sizeof(STMData));
	stm_data->index = index;
	if (This->type == ICON_TYPE_SETFH
			|| This->type == ICON_TYPE_SETDJ)
		This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_GET_RM,stm_data);
	else
		This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_CALL,stm_data);
}

static int next(GetRoomNum *This,int offset)
{
	int cur_index = This->priv->select_index + 
		This->priv->page_now * (This->priv->max_rows * This->priv->max_columns);
	if (cur_index < This->priv->rooms_num - 1 + offset -1 ) {
		cur_index += offset;
	} else {
		cur_index = cur_index + offset - This->priv->rooms_num;
	}
	int page_new = cur_index / (This->priv->max_rows * This->priv->max_columns);
	This->priv->select_index = cur_index % (This->priv->max_rows * This->priv->max_columns); 
	if (This->priv->page_now != page_new) {
		This->priv->page_now = page_new;
		This->priv->disp_rooms = This->priv->rooms + 
			This->priv->page_now * (This->priv->max_rows * This->priv->max_columns);
		This->interface->uiUpdate(This);
	}
	return This->priv->select_index;
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief nextPage 刷新下一页房号
 *
 * @param This
 */
/* ---------------------------------------------------------------------------*/
static int nextPage(GetRoomNum *This)
{
	return next(This,This->priv->max_rows * This->priv->max_columns);
}
static int front(GetRoomNum *This,int offset)
{
	int cur_index = This->priv->select_index + 
		This->priv->page_now * (This->priv->max_rows * This->priv->max_columns);
	if (cur_index - offset >= 0 ) {
		cur_index -= offset;
	} else {
		cur_index = This->priv->rooms_num + cur_index - offset;
	}
	int page_new = cur_index / (This->priv->max_rows * This->priv->max_columns);
	This->priv->select_index = cur_index % (This->priv->max_rows * This->priv->max_columns); 
	if (This->priv->page_now != page_new) {
		This->priv->page_now = page_new;
		This->priv->disp_rooms = This->priv->rooms + 
			This->priv->page_now * (This->priv->max_rows * This->priv->max_columns);
		printRoomNum(This);
		This->interface->uiUpdate(This);
	}
	return This->priv->select_index;
}
/* ---------------------------------------------------------------------------*/
/**
 * @brief frontPage 刷新上一页房号
 *
 * @param This
 */
/* ---------------------------------------------------------------------------*/
static int frontPage(GetRoomNum *This)
{
	return front(This,This->priv->max_rows *	This->priv->max_columns);
	// int page_elements = This->priv->max_rows *	This->priv->max_columns;
	// if (This->priv->page_now) {
		// This->priv->page_now--;
		// This->priv->disp_rooms = This->priv->rooms + This->priv->page_now * page_elements;
	// }
}

/* ---------------------------------------------------------------------------*/
/**
 * @brief stmDoBroadcast 广播查询管理中心
 *
 * @param This
 * @param st_data
 *
 * @returns
 */
/* ---------------------------------------------------------------------------*/
static int stmDoBroadcast(GetRoomNum *This,STMData *st_data)
{
	DBG_P("STMDO:%s() \n",__FUNCTION__);
	cmdSendGetDevInfo(This,0,"255.255.255.255");
	This->priv->timer_search_center->start(This->priv->timer_search_center);
    return 0;
}
/* ---------------------------------------------------------------------------*/
/**
 * @brief stmDoSelecCenter 选择一个管理中心，查询期号
 *
 * @param This
 * @param st_data
 *
 * @returns
 */
/* ---------------------------------------------------------------------------*/
static int stmDoSelecCenter(GetRoomNum *This,STMData *st_data)
{
	DBG_P("STMDO:%s(),rooms[%d]:%s\n",__FUNCTION__,
			st_data->index,
			This->priv->disp_rooms[st_data->index].ip);
	strcpy(This->priv->center_ip,This->priv->disp_rooms[st_data->index].ip);
	// 修改网段与管理中心保持一致
	char local_temp_ip[16] = {0};
	strncpy(local_temp_ip,
		   	(char*)getBroadcastIP(This->priv->temp_ip, This->priv->mask),
		   	sizeof(local_temp_ip));
	char *server_ip = (char*)getBroadcastIP(This->priv->center_ip,
			This->priv->mask);
	if(strcmp(local_temp_ip, server_ip) != 0) {
		DBG_P("server_ip:%s,local_ip:%s\n", server_ip,local_temp_ip);
		This->interface->setTempNetIp(This,
				This->priv->center_ip,This->priv->temp_ip);
	}
	This->interface->uiUpdate(This);
	This->interface->uiUpdateTitle(This,GETROOM_UI_GETTING_INFO);
	cmdSendGetDevInfo(This,This->priv->packet_id++,This->priv->center_ip);
	This->priv->timer_search_center->stop(This->priv->timer_search_center);
    return 0;
}
/* ---------------------------------------------------------------------------*/
/**
 * @brief stmDoGetInfoStep 继续查询栋号，房号等
 *
 * @param This
 * @param st_data
 *
 * @returns
 */
/* ---------------------------------------------------------------------------*/
static int stmDoGetInfoStep(GetRoomNum *This,STMData *st_data)
{
	DBG_P("STMDO:%s() \n",__FUNCTION__);
	int type = This->priv->disp_rooms[st_data->index].type;
	char *code = This->priv->disp_rooms[st_data->index].name;
	This->priv->room_inf.GetType = type + 1;
	if (type == TYPE_FJH) {
		TGetConfigInfo room_config;
		room_config.ID = This->priv->packet_id++;
		room_config.Size = sizeof(TGetConfigInfo);
		room_config.Type = This->priv->cmd_get_config_info;
		memcpy(room_config.room_num.room_array, This->priv->room_inf.room_num.room_array,
				sizeof(This->priv->room_inf.room_num));
		memcpy(room_config.FJH, code, 4);
		This->interface->udpSend(This,
				This->priv->center_ip,
				This->priv->port,
				&room_config,
				room_config.Size);
	} else {
		strncpy(&This->priv->room_inf.room_num.room_array[8*type], code, 4);
	}
	This->interface->uiUpdate(This);
	This->interface->uiUpdateTitle(This,GETROOM_UI_GETTING_INFO);
	cmdSendGetDevInfo(This,This->priv->packet_id++,This->priv->center_ip);
	This->priv->timer_get_room->start(This->priv->timer_get_room);

    return 0;
}
/* ---------------------------------------------------------------------------*/
/**
 * @brief stmDoEndGetRoomNum 结束房号获取，释放内存
 *
 * @param This
 * @param st_data
 *
 * @returns
 */
/* ---------------------------------------------------------------------------*/
static int stmDoEndGetRoomNum(GetRoomNum *This,STMData *st_data)
{
	DBG_P("STMDO:%s() \n",__FUNCTION__);
	This->priv->timer_get_room->stop(This->priv->timer_get_room);
	This->priv->timer_search_center->stop(This->priv->timer_search_center);
	if (This->type == ICON_TYPE_SETFH) {
		if(strcmp(This->priv->temp_ip, This->priv->ip) != 0)
			This->interface->resetIp(This);
	}
	clearRooms(This);
    return 0;
}
/* ---------------------------------------------------------------------------*/
/**
 * @brief stmDoGetCenter 获取管理中心信息
 *
 * @param This
 * @param st_data
 *
 * @returns
 */
/* ---------------------------------------------------------------------------*/
static int stmDoGetCenter(GetRoomNum *This,STMData *st_data)
{
	DBG_P("STMDO:%s() \n",__FUNCTION__);
	int i;
	This->priv->rooms_num = st_data->data->center_cnt;
	for (i=0; i<st_data->data->center_cnt; i++) {
		fillRooms(This,i,st_data->data->centers[i].IP,st_data->data->centers[i].IP,0);
	}
	This->priv->disp_rooms = This->priv->rooms;
	if (This->priv->rooms_num == 1) { // 当只有一个管理中心时，自动从此管理中心获取
		stm_data = (STMData *)This->priv->st_machine->initPara(This->priv->st_machine,
				sizeof(STMData));
		stm_data->index = 0;
		This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_GET_RM,stm_data);
	} else {
		This->interface->uiUpdate(This);
		This->interface->uiUpdateTitle(This,GETROOM_UI_SELECT_CENTER);
	}
    return 0;
}

static int stmDoGetCallList(GetRoomNum *This,STMData *st_data)
{
	DBG_P("STMDO:%s() \n",__FUNCTION__);
	int i,total_cnt = 0;
	for (i=0; i<st_data->data->center_cnt; i++) {
		fillRooms(This,i,st_data->data->centers[i].Name,st_data->data->centers[i].IP,0);
	}
	total_cnt = st_data->data->center_cnt;

	for (i=0; i<st_data->data->dmk_cnt; i++) {
		fillRooms(This,i+total_cnt,st_data->data->dmks[i].Name,st_data->data->dmks[i].IP,0);
	}
	total_cnt += st_data->data->dmk_cnt;

	for (i=0; i<st_data->data->hdmk_cnt; i++) {
		fillRooms(This,i+total_cnt,st_data->data->hdmks[i].Name,st_data->data->hdmks[i].IP,0);
	}
	total_cnt += st_data->data->hdmk_cnt;
	This->priv->rooms_num = total_cnt;
	This->priv->disp_rooms = This->priv->rooms;
	if (This->priv->rooms_num == 1) { // 当只有一个数据时，自动呼叫
		stm_data = (STMData *)This->priv->st_machine->initPara(This->priv->st_machine,
				sizeof(STMData));
		stm_data->index = 0;
		This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_CALL,stm_data);
	} else {
		This->interface->uiUpdate(This);
	}
    return 0;
}
static int stmDoCall(GetRoomNum *This,STMData *st_data)
{
	This->interface->call(This,This->priv->disp_rooms[st_data->index].ip);
	This->priv->st_machine->msgPost(This->priv->st_machine,EVENT_END,0);
	return 0;
}

static STMDo stm_state_do[] = {
	{DO_BROARDCAST,    	stmDoBroadcast},
	{DO_SELECT_CENTER, 	stmDoSelecCenter},
	{DO_GET_INFO,  		stmDoGetInfoStep},
	{DO_END,    		stmDoEndGetRoomNum},
	{DO_GET_CENTER,    	stmDoGetCenter},
	{DO_GET_CALL_LIST,  stmDoGetCallList},
	{DO_CALL,    		stmDoCall},
};

static void stmGetRoomNumHandle(StMachine *This,void *data,void *arg)
{
    STMData *st_data = (STMData *)data;
    stm_state_do[This->getCurRun(This)].proc(arg,st_data);
}

static void *stmGetRoomNumThread(void *arg)
{
    GetRoomNum *p = (GetRoomNum *)arg;
    while (1) {
		p->priv->st_machine->run(p->priv->st_machine);
        usleep(10000);
    }
    pthread_exit(NULL);
}

static void stmThreadCreate(void *arg)
{
    pthread_t m_pthread;
    pthread_attr_t threadAttr1;

    pthread_attr_init(&threadAttr1);
	pthread_attr_setdetachstate(&threadAttr1,PTHREAD_CREATE_DETACHED);
	pthread_create(&m_pthread,&threadAttr1,stmGetRoomNumThread,arg);
	pthread_attr_destroy(&threadAttr1);
}

static void *timerThread(void *arg)
{
	GetRoomNum *p = arg;
    while (1) {
		if (p->priv->timer_search_center)
			p->priv->timer_search_center->handle(p->priv->timer_search_center);
		if (p->priv->timer_get_room)
			p->priv->timer_get_room->handle(p->priv->timer_get_room);
        sleep(1);
    }
    pthread_exit(NULL);
}

static void timerThreadCreate(void *arg)
{
    pthread_t m_pthread;
    pthread_attr_t threadAttr1;

    pthread_attr_init(&threadAttr1);
	pthread_attr_setdetachstate(&threadAttr1,PTHREAD_CREATE_DETACHED);
	pthread_create(&m_pthread,&threadAttr1,timerThread,arg);
	pthread_attr_destroy(&threadAttr1);
}

static void udpSendDefault(struct _GetRoomNum *This,char *ip,int port,void *data,int size)
{
	DBG_P("[%s]\n", __FUNCTION__);
}
static void setTempNetIpDefault(struct _GetRoomNum *This,char *src_ip,char *des_ip)
{
	strcpy(des_ip,src_ip);
	DBG_P("[%s]\n", __FUNCTION__);
}
static void resetIpDefault(struct _GetRoomNum *This)
{
	DBG_P("[%s]\n", __FUNCTION__);
}
static void uiUpdateTitleDefault(struct _GetRoomNum *This,GetRoomNumUIStatus status)
{
	DBG_P("[%s]status:%d\n", __FUNCTION__,status);
}
static 	void uiUpdateDefault(GetRoomNum *This)
{
	printf("[%s]\n", __FUNCTION__);
	printRoomNum(This);
}

static void saveNetConfigDefault(struct _GetRoomNum *This,char *ip,char *netmask,char *gateway)
{
	printf("[%s]ip:%s,mask:%s,gateway:%s\n", __FUNCTION__,
			ip,netmask,gateway);
}
static void saveRoomNumDefault(struct _GetRoomNum *This,GetRoomRooms *rooms)
{
	printf("[%s]\n", __FUNCTION__);
}
static void saveRoomNameDefault(struct _GetRoomNum *This,char *name)
{
	printf("[%s]name:%s\n", __FUNCTION__,name);
}
static void saveMasterIpDefault(struct _GetRoomNum *This,char *ip)
{
	printf("[%s]ip:%s\n", __FUNCTION__,ip);
}
static void saveCenterDmksDefault(struct _GetRoomNum *This,GetRoomNumCenterDmks *info,void (*callBack)(void *))
{
	printf("[%s]\n", __FUNCTION__);
}
static int savePhoneNumDefault(struct _GetRoomNum *This,GetRoomRooms *rooms,char *ip,char *name,void (*callBack)(void *))
{
	printf("[%s]ip:%s,name:%s\n", __FUNCTION__,ip,name);
	return 1;
}
static void callDefault(struct _GetRoomNum *This,char *ip)
{
	printf("[%s]ip:%s\n", __FUNCTION__,ip);
}
static void loadInterface(GetRoomNumInterface *priv,GetRoomNumInterface *in)
{
	LOADFUNC(udpSend);
	LOADFUNC(setTempNetIp);
	LOADFUNC(resetIp);
	LOADFUNC(uiUpdateTitle);
	LOADFUNC(uiUpdate);
	LOADFUNC(saveNetConfig);
	LOADFUNC(saveRoomNum);
	LOADFUNC(saveRoomName);
	LOADFUNC(saveMasterIp);
	LOADFUNC(saveCenterDmks);
	LOADFUNC(savePhoneNum);
	LOADFUNC(call);
}

GetRoomNum * getRoomNumCreate(GetRoomNumInterface *interface,
		char *ip, char *mask, int port,
		char *master_ip,
		uint64_t dev_id,
		int cmd_get_dev_info,
		int cmd_get_config_info)
{
	GetRoomNum * This = (GetRoomNum*) calloc (1,sizeof(GetRoomNum));
	This->priv = (GetRoomNumPriv *) calloc (1,sizeof(GetRoomNumPriv));
	This->interface = (GetRoomNumInterface *) calloc (1,sizeof(GetRoomNumInterface));
	This->priv->packet_id = (unsigned)time(NULL);
	This->priv->ip = ip;
	This->priv->port = port;
	This->priv->mask = mask;
	This->priv->master_ip = master_ip;
	This->priv->dev_id = dev_id;
	This->priv->cmd_get_dev_info = cmd_get_dev_info;
	This->priv->cmd_get_config_info = cmd_get_config_info;
	This->priv->st_machine = stateMachineCreate(ST_IDLE,
			stm_getroomnum_state,
			NELEMENTS(stm_getroomnum_state),
			0, stmGetRoomNumHandle,This);

	This->priv->timer_search_center = timerCreate(LAYER_TIME_1S * 3,timerSearchCenter3S,This);
	This->priv->timer_get_room = timerCreate(LAYER_TIME_1S * 3,timerGetRoom3S,This);

	This->start = start;
	This->end = end;
	This->select = selectRoom;
	This->udpCmdGetDevInfo = udpCmdGetDevInfo;
	This->udpCmdSaveConfig = udpCmdSaveConfig;
	This->nextPage = nextPage;
	This->frontPage = frontPage;
	This->next= next;
	This->front= front;
	This->printRoomNum = printRoomNum;
	This->getDispRooms = getDispRooms;

	loadInterface(This->interface,interface);
	stmThreadCreate(This);
	timerThreadCreate(This);

	return This;
}

