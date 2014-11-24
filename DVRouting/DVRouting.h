// DVRouting.h

#include <stddef.h>
#pragma pack (1)
class DVRouting
{
public:
	#define MaxnumOfRouter 10
	#define POISONMETRIC 255
	typedef struct
	{
		char LinkToID[10];
		unsigned int cost;
		char hostname[16];
		unsigned int portnum;
		unsigned int keepalive_time; // for keep alive purpose
	}LinkInfo;

	// router info
	typedef struct
	{
		char ID[10];
		char hostname[16];
		unsigned int portnum;
		SOCKET sock; // sock-RID mapping
		bool life_state;
		LinkInfo link_info[MaxnumOfRouter];
		unsigned int link_info_count;
	}Info;
	Info router_info;

	// thread arg
	typedef struct
	{
		DVRouting* ptr;
		Info info;
	}thread_arg;


	DVRouting();
	void frontend(char ID[], unsigned int portnum);
	DWORD WINAPI router_proc(thread_arg my_arg);



private:
	// upating & display & kill flag
	typedef struct
	{
		char update_router_ID[10];
		char update_link_ID[10];
	}UpdateFlag;
	UpdateFlag update_flag;

	char display_flag[10];

	char kill_flag[10];

	unsigned int broadcast_timeout;
	unsigned int keepalive_timeout;

	// Routing Info :
	typedef struct
	{
		char SourRID[10];
		char DestRID[10];
		unsigned int cost;
		unsigned int numOfHops;
		char nextRID[10];
		bool updateflag;
	}RoutingInfo;

	// Message format
	typedef struct
	{
		char type;
		char ID[10];
		RoutingInfo RInfo[MaxnumOfRouter];
		unsigned int msg_count;
	}routerMsg;

	// routing table
	typedef struct
	{
		RoutingInfo RInfo[MaxnumOfRouter];
		unsigned int entry_count;
	}RoutingTable;



	


	// function
	int sendRouterMsg(char d_RID[], routerMsg* rmsg);
	int recvRouterMsg(SOCKET sock, routerMsg* rmsg, sockaddr_in* remote_addr);
	SOCKET init_router(Info* info);
	int enRouter_table(RoutingTable* routing_table, RoutingInfo entry);
	int deRouter_table(RoutingTable* routing_table, char cond_DestRID[]);
	int lookupRouter_table(RoutingTable routing_table, char cond_DestRID[]);
	RoutingInfo makeRoutingInfo(char SourRID[], char DestRID[], unsigned int cost, unsigned int numOfHops, char nextRID[]);
	int lookupLinkInfoArray(LinkInfo link_info[], unsigned int link_info_count, char cond_link_to_ID[]);
	int deInfoArray(LinkInfo link_info[], unsigned int* link_info_count, char cond_link_to_ID[]);
};