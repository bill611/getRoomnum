/*
 * =============================================================================
 *
 *       Filename:  protocol_getRoomnum.h
 *
 *    Description:  太川房号获取协议
 *
 *        Version:  1.0
 *        Created:  2019-01-02 15:22:10
 *       Revision:  none
 *
 *         Author:  xubin
 *        Company:  Taichuan
 *
 * =============================================================================
 */
#ifndef _TC_PROTOCOL_GETROOMNUM_H
#define _TC_PROTOCOL_GETROOMNUM_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define DBG_GET_ROOM

#define MAX_CENTER_NUM	8	// 最大管理中心
#define MAX_ROOM_NUM	256	// 最大房号信息

#include <stdint.h>
	typedef enum _IconTableType{
		ICON_TYPE_SETFH, // 设置房号
		ICON_TYPE_SETDJ, // 设置户户对讲房号
		ICON_TYPE_CALL_CENTER, // 呼叫管理中心
		ICON_TYPE_CALL_DMK, // 呼叫门口机
		ICON_TYPE_CALL_CENTER_AND_DMK, //呼叫管理中心或门口机
	} IconTableType;

	typedef struct{
		char QH[8];			//期号
		char DH[8];		    //栋号
		char DYH[8];		//单元号
		char CH[8];		    //层号
		char FH[8];		    //房号
		char FJH[8];		//分机号
	} GetRoomRooms;

	// 反馈UI状态
	typedef enum _GetRoomNumUIStatus {
		GETROOM_UI_SEARCHING_CENTER,	// 正在搜索管理中心
		GETROOM_UI_GETTING_INFO,		// 正在获取房号信息
		GETROOM_UI_SELECT_CENTER,		// 请选择管理中心
		GETROOM_UI_SELECT_QH,			// 请选择期号
		GETROOM_UI_SELECT_DH,			// 请选择栋号
		GETROOM_UI_SELECT_DYH,			// 请选择单元号
		GETROOM_UI_SELECT_CH,			// 请选择层号
		GETROOM_UI_SELECT_FH,			// 请选择房号
		GETROOM_UI_SELECT_FJH,			// 请选择分机号
		GETROOM_UI_SAVING,				// 正在保存
		GETROOM_UI_SAVE_PHONE_END,		// 保存对讲房号成功
		GETROOM_UI_SAVE_ROOMNUM_END,	// 保存房号成功
		GETROOM_UI_FAIL_COMM,			// 通讯超时
		GETROOM_UI_FAIL_SAVE_MYSELF_IP, // 不能保存自己IP
		GETROOM_UI_FAIL_ADDR_EXIST,		// 房号已存在
	}GetRoomNumUIStatus;

	typedef struct _RoomData {
		char name[32];	// 每个房号的名称
		char ip[16];	// 每个房号的IP
		int  type;		// 房号类型
	}RoomData;		// 内部房号结构数组

	typedef struct {
		char IP[16];
		char Name[32];
	}TDeviceInf;

	typedef struct {
		int center_cnt;			// 管理中心数量
		int dmk_cnt;			// 门口机数量
		int hdmk_cnt;			// 户门口机数量
		TDeviceInf *centers;
		TDeviceInf *dmks;
		TDeviceInf *hdmks;
	}GetRoomNumCenterDmks; // 传入管理中心，门口机，户门口机数据

	struct _GetRoomNumPriv;
	struct _GetRoomNumInterface;
	typedef struct _GetRoomNum {
		struct _GetRoomNumPriv *priv;
		struct _GetRoomNumInterface *interface;
		IconTableType type;

		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 开始进入房号设置流程，调用此函数之前，直接调用区分使用类型
		 *  get_room_num->type = xxx;
		 *
		 * @param 
		 * @param info	除了房号ICON_TYPE_SETFH外，其余都需传入对应房号消息
		 * @param rows	房号行
		 * @param columns 房号列
		 */
		/* ---------------------------------------------------------------------------*/
		void (*start)(struct _GetRoomNum *,
				GetRoomNumCenterDmks *info,
			   	int rows,int columns);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 结束时调用此函数结束房号流程，释放内存
		 *
		 * @param 
		 */
		/* ---------------------------------------------------------------------------*/
		void (*end)(struct _GetRoomNum *);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 选择下标为index的数组，进入下一步流程
		 *
		 * @param 
		 * @param index 下标
		 */
		/* ---------------------------------------------------------------------------*/
		void (*select)(struct _GetRoomNum *,int index);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief int 下一页
		 *
		 * @return 返回当前页被选择的房号下标 
		 */
		/* ---------------------------------------------------------------------------*/
		int (*nextPage)(struct _GetRoomNum *);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief int 上一页
		 *
		 * @return 返回当前页被选择的房号下标 
		 */
		/* ---------------------------------------------------------------------------*/
		int (*frontPage)(struct _GetRoomNum *);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief int 下N项 ，N=offset
		 *
		 * @param offset 下标移动距离
		 *
		 * @return 返回当前页被选择的房号下标 
		 */
		/* ---------------------------------------------------------------------------*/
		int (*next)(struct _GetRoomNum *,int offset);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief int 上N项，N=offset
		 *
		 * @param offset 下标移动距离
		 *
		 * @return 返回当前页被选择的房号下标 
		 */
		/* ---------------------------------------------------------------------------*/
		int (*front)(struct _GetRoomNum *,int offset);

		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief UDP协议接收，获取设备信息
		 *
		 * @param 
		 * @param ip
		 * @param data
		 */
		/* ---------------------------------------------------------------------------*/
		void (*udpCmdGetDevInfo)(struct _GetRoomNum *,char *ip,char *data);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief UDP协议接收，保存最终设备信息
		 *
		 * @param 
		 * @param ip
		 * @param data
		 */
		/* ---------------------------------------------------------------------------*/
		void (*udpCmdSaveConfig)(struct _GetRoomNum *,char *ip,char *data);

		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 获取UI显示房号的数组信息
		 *
		 * @param 
		 * @param data 内部房号结构数组
		 *
		 * @return 返回当前页被选择的房号下标 
		 */
		/* ---------------------------------------------------------------------------*/
		int (*getDispRooms)(struct _GetRoomNum *,RoomData **data);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 打印RoomData结构,用于调试
		 *
		 * @param 
		 */
		/* ---------------------------------------------------------------------------*/
		void (*printRoomNum)(struct _GetRoomNum *);

	}GetRoomNum;

	typedef struct _GetRoomNumInterface {
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 使用UDP底层发送协议
		 *
		 * @param
		 * @param ip 目的IP
		 * @param port 目的端口
		 * @param data 数据
		 * @param size 数据大小
		 * @param enable_call_back 是否使用回调函数
		 */
		/* ---------------------------------------------------------------------------*/
		void (*udpSend)(struct _GetRoomNum *,char *ip,int port,void *data,int size);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 设置临时IP，用于保持与不同网段管理中心一致
		 *
		 * @param 
		 * @param src_ip 原IP
		 * @param des_ip 修改后IP
		 */
		/* ---------------------------------------------------------------------------*/
		void (*setTempNetIp)(struct _GetRoomNum *,char *src_ip,char *des_ip);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 复位IP，用于设置临时IP后，退出房号设置，恢复原有IP
		 *
		 * @param 
		 */
		/* ---------------------------------------------------------------------------*/
		void (*resetIp)(struct _GetRoomNum *);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 更新获取房号步骤标题
		 *
		 * @param 
		 * @param status
		 */
		/* ---------------------------------------------------------------------------*/
		void (*uiUpdateTitle)(struct _GetRoomNum *,GetRoomNumUIStatus status);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief UI更新房号
		 *
		 * @param 
		 */
		/* ---------------------------------------------------------------------------*/
		void (*uiUpdate)(struct _GetRoomNum *);

		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 保存网络设置
		 *
		 * @param 
		 * @param ip IP 
		 * @param netmask 子网掩码
		 * @param gateway 网关
		 */
		/* ---------------------------------------------------------------------------*/
		void (*saveNetConfig)(struct _GetRoomNum *,char *ip,char *netmask,char *gateway);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 保存房号信息
		 *
		 * @param 
		 * @param rooms 具体房号
		 */
		/* ---------------------------------------------------------------------------*/
		void (*saveRoomNum)(struct _GetRoomNum *,GetRoomRooms *rooms); 
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 保存房号名称
		 *
		 * @param 
		 * @param name
		 */
		/* ---------------------------------------------------------------------------*/
		void (*saveRoomName)(struct _GetRoomNum *,char *name);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 保存主机IP
		 *
		 * @param 
		 * @param ip
		 */
		/* ---------------------------------------------------------------------------*/
		void (*saveMasterIp)(struct _GetRoomNum *,char *ip);
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 保存关联关联中心，门口机，户门口机等信息
		 *
		 * @param 
		 * @param info
		 * @param callBack 保存后的回调函数，异步保存，但需要等待回调调用完成
		 */
		/* ---------------------------------------------------------------------------*/
		void (*saveCenterDmks)(struct _GetRoomNum *,GetRoomNumCenterDmks *info,void (*callBack)(void *arg)); 
		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 保存户户对讲
		 *
		 * @param 
		 * @param rooms
		 * @param ip
		 * @param name
		 * @param callBack 保存后的回调函数，异步保存，但需要等待回调调用完成
		 */
		/* ---------------------------------------------------------------------------*/
		int (*savePhoneNum)(struct _GetRoomNum *,GetRoomRooms *rooms,char *ip,char *name,void (*callBack)(void *arg)); 

		/* ---------------------------------------------------------------------------*/
		/**
		 * @brief 户户对讲时,选择后呼叫接口
		 *
		 * @param 
		 * @param ip
		 */
		/* ---------------------------------------------------------------------------*/
		void (*call)(struct _GetRoomNum *,char *ip);
	}GetRoomNumInterface;

	extern GetRoomNum * getRoomNumCreate(GetRoomNumInterface *interface,
			char *ip, char *mask, int port,
			char *master_ip,
			uint64_t dev_id,
			int cmd_get_dev_info,
			int cmd_get_config_info);
	extern GetRoomNum *get_room_num;
#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif
