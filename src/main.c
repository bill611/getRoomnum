#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include "queue.h"
#include "protocol_getRoomnum.h"
#include "UDPServer.h"

typedef struct
{
    unsigned int ID;
    unsigned int Size;
    unsigned int Type;
}COMMUNICATION;
struct _RmData{
	char cmd;
	int index;
};
static Queue *queue;
static void udpSend(GetRoomNum *This,
        char *ip,int port,void *data,int size)
{
	udp_server->AddTask(udp_server,
			ip,port,data,
			size,1,0,NULL,NULL);
}
static void uiUpdate(GetRoomNum *This)
{
	struct _RmData data;
	This->printRoomNum(This);
	printf("input cmd q s r n u:");
	char buf ;
	do {
		buf = getchar();	
		if (buf != '\n')
			data.cmd = buf;
		else
			break;
	}while(buf != EOF);
	printf("cmd:%c input index :",data.cmd);
	do {
		buf = getchar();	
		if (buf != '\n')
			data.index = buf - '0';
		else
			break;
	}while(buf != EOF);
	queue->post(queue,&data);	
}
static GetRoomNumInterface get_room_num_interface = {
    .udpSend = udpSend,
    .uiUpdate = uiUpdate,
};

/* ---------------------------------------------------------------------------*/
/**
 * @brief udpKillTaskCondition 根据条件删除任务
 *
 * @param arg1 udp数据
 * @param arg2 传入参数
 *
 * @returns 删除0失败 1成功
 */
/* ---------------------------------------------------------------------------*/
static int udpKillTaskCondition(void *arg1,void *arg2)
{
    int id = *(int *)arg2;
    COMMUNICATION * pHead = (COMMUNICATION*)arg1;
    if(pHead->ID==id)
        return 1;

    return 0;
}
uint64_t getMs(void)
{
    struct  timeval    tv;
    gettimeofday(&tv,NULL);
    return ((tv.tv_usec / 1000) + tv.tv_sec  * 1000 );
}
/* ---------------------------------------------------------------------------*/
/**
 * @brief udpProtocolFilter 接收协议前过滤重复协议以及应答回复
 *
 * @param ABinding
 * @param AData
 *
 * @returns
 */
/* ---------------------------------------------------------------------------*/
static int udpProtocolFilter(SocketHandle *ABinding,SocketPacket *AData)
{
    typedef struct _PacketsID {
        char IP[16];
        uint32_t id;
        uint64_t dwTick;        //时间
    }PacketsID;

	static PacketsID packets_id[10] = {0};
    static int packet_pos = 0;
	uint64_t dwTick;
	int i;

    // 收到无效字节
    if (AData->Size < 4)
        return 0;

	// 收到4个字节为对方返回的应答包,即ID
	if (AData->Size == 4) {
        udp_server->KillTaskCondition(udp_server,
                udpKillTaskCondition,AData->Data);
		return 0;
	}

    // 收到无效字节
    if (AData->Size < sizeof(COMMUNICATION))
        return 0;

	// 收到其他协议则回复相同ID
    udp_server->SendBuffer(udp_server,ABinding->IP,ABinding->Port,
            AData->Data,4);

	dwTick = getMs();
    // 过滤相同IP发送的多次重复命令
	for (i=0; i<10; i++) {
        if (strcmp(ABinding->IP,packets_id[i].IP) == 0
                && *(uint32_t*)AData->Data == packets_id[i].id ){
            return 0;
        }
    }
    sprintf(packets_id[packet_pos].IP,"%s",ABinding->IP);
    packets_id[packet_pos].id = *(uint32_t*)AData->Data;
    packets_id[packet_pos++].dwTick = dwTick;
    packet_pos %= 10;
	return 1;
}

static void UDPSocketRead(SocketHandle *ABinding,SocketPacket *AData)
{
	if (udpProtocolFilter(ABinding,AData) == 0)
		return;

	COMMUNICATION * head = (COMMUNICATION *)AData->Data;

#define TP_RESPDEVADDR 0x8001
#define TP_RESCONFIGADDR 0x8003
	switch(head->Type)
	{
		case TP_RESPDEVADDR:			
			get_room_num->udpCmdGetDevInfo(get_room_num,ABinding->IP,AData->Data);
			break;
		case TP_RESCONFIGADDR:			
			get_room_num->udpCmdSaveConfig(get_room_num,ABinding->IP,AData->Data);
			break;
		default:
			break;
	}
}
int main(int argc, char *argv[])
{
    udpServerInit(UDPSocketRead,7800);
	queue = queueCreate("msg",QUEUE_BLOCK,sizeof(struct _RmData));
#define TP_GETDEVADDR  0x8000
#define TP_GETCONFIGADDR_EXT1 0x800A
	get_room_num = getRoomNumCreate(&get_room_num_interface,
			"172.16.1.102", "255.255.0.0", 7800,
			"172.16.1.103",
			90000,
			TP_GETDEVADDR,
			TP_GETCONFIGADDR_EXT1);
start:
	printf("start getting room...\n");
#if 0
	GetRoomNumCenterDmks info;
	memset(&info,0,sizeof(GetRoomNumCenterDmks));
	info.center_cnt = 2;
	info.centers = (TDeviceInf *) calloc(info.center_cnt,sizeof(TDeviceInf));
	strcpy(info.centers[0].IP,"192.168.77.102");
	strcpy(info.centers[1].IP,"172.16.100.102");
	strcpy(info.centers[0].Name,"192.168.77.102");
	strcpy(info.centers[1].Name,"172.16.100.102");
	info.dmk_cnt = 2;
	info.dmks = (TDeviceInf *) calloc(info.dmk_cnt,sizeof(TDeviceInf));
	strcpy(info.dmks[0].IP,"192.168.77.103");
	strcpy(info.dmks[1].IP,"172.16.100.103");
	strcpy(info.dmks[0].Name,"192.168.77.103");
	strcpy(info.dmks[1].Name,"172.16.100.103");
	// strcpy(info.centers[2].IP,"10.0.0.38");
	get_room_num->type = ICON_TYPE_CALL_CENTER_AND_DMK;
	get_room_num->start(get_room_num,&info,4,10);
#else
	get_room_num->type = ICON_TYPE_SETFH;
	get_room_num->start(get_room_num,NULL,4,10);
#endif
	while (1) {
		struct _RmData data;
		queue->get(queue,&data);
		if (data.cmd == 'q') {
wait:
			get_room_num->end(get_room_num);	
			printf("please input s to start\n");	
			scanf("%c",&data.cmd);
			if (data.cmd == 's')
				goto start;
			else
				goto wait;
		} else if (data.cmd == 'r') {
			if (data.index > 0)
				get_room_num->select(get_room_num,data.index - 1);	
		} else if (data.cmd == 'n') {
			get_room_num->next(get_room_num,1);	
			uiUpdate(get_room_num);
		} else if (data.cmd == 'u') {
			get_room_num->front(get_room_num,1);	
			uiUpdate(get_room_num);
		} else if (data.cmd == 96){
		} else {
			printf("wrong cmd :%d \n",data.cmd);
			printf("force input cmd r\n");
			if (data.index > 0)
				get_room_num->select(get_room_num,data.index - 1);	
			// uiUpdate(get_room_num);
		}
		
	}
	return 0;
}
