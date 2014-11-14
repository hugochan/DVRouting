// DVRouting.h

#include <stddef.h>

class DVRouting
{
public:
	#define MaxnumOfRouter 10
	typedef struct
	{
		char LinkToID[10];
		int cost;
	}LinkInfo;

	typedef struct // element type
	{
		char ID[10];
		char hostname[16];
		int portnum;
		SOCKET sock; // sock-RID mapping
		boolean life_state;
		LinkInfo link_info[MaxnumOfRouter];
		int link_info_count;
	}Info;
	
	// thread arg
	typedef struct
	{
		DVRouting* ptr;
		Info* info;
	}thread_arg;


	DVRouting();
	void frontend(void);
	DWORD WINAPI router_proc(thread_arg* my_arg);



private:

	// routers
	// linked list
	int broadcast_timeout;

	typedef struct node
	{
		Info info;
		node* next = NULL;
	}InfoNodeType;
	InfoNodeType info_node;

	typedef struct
	{
		InfoNodeType* front;
		InfoNodeType* rear;
	}RoutInfoManagLL;
	RoutInfoManagLL rout_info_manag;


	// Routing Info :
	typedef struct
	{
		char SourRID[10];
		char DestRID[10];
		int cost;
		int numOfHops;
		char nextRID[10];
		boolean updateflag;
	}RoutingInfo;

	// Message format
	typedef struct
	{
		char type;
		RoutingInfo RInfo[MaxnumOfRouter];
		int msg_count;
	}routerMsg;

	// routing table
	typedef struct
	{
		RoutingInfo RInfo[MaxnumOfRouter];
		int entry_count;
	}RoutingTable;



	


	// function
	int enllist(RoutInfoManagLL* q, Info x);
	int dellist(RoutInfoManagLL* q, char cond_id[]);
	Info* lookupllist(RoutInfoManagLL* q, char cond_id[]); // look up for the specific element
	int sendRouterMsg(char s_RID[], char d_RID[], routerMsg* rmsg);
	int recvRouterMsg(SOCKET sock, routerMsg* rmsg);
	SOCKET init_router(Info* info);
	int enRouter_table(RoutingTable* routing_table, RoutingInfo entry);
	int deRouter_table(RoutingTable* routing_table, char cond_DestRID[]);
	int lookupRouter_table(RoutingTable routing_table, char cond_DestRID[]);
	RoutingInfo makeRoutingInfo(char SourRID[], char DestRID[], int cost, int numOfHops, char nextRID[]);
};