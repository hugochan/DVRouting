// DVRouting.cpp

#include <iostream>
#include <winsock.h>
#include <windows.h>
#include <string>
#include <conio.h>
#include <time.h>
#include "DVRouting.h"

#pragma comment(lib,"wsock32.lib")


using namespace std;
#define DEBUG



enum commandName { dvrouter, add, change, display, kill };
const char* command_array[7] = { "dvrouter", "add", "change", "display", "kill" };

DWORD __stdcall startMethodInThread(LPVOID arg);


DVRouting::DVRouting()
{
	broadcast_timeout = 10000;
}


int DVRouting::enlqueue(RoutInfoManagLq* q, Info x)
{
	InfoNodeType  *s = new InfoNodeType;
	if (s != NULL)
	{
		s->info = x;
		s->next = NULL;	 // generate new node
		q->rear->next = s;	// add new node in a queue
		q->rear = s;		 // update rear pointer
		return 0;
	}
	else
		return -1;
}


int DVRouting::delqueue(RoutInfoManagLq* q, Info* x)
{
	InfoNodeType  *p;
	if (q->front->next == NULL)
		return -1;
	else
	{
		p = q->front->next;   // get front node
		*x = p->info;			// get element
		q->front->next = p->next; // update front pointer
		if (p->next == NULL)    // if length of queue equals to one
			q->rear = q->front; // update rear pointer
		delete p;		       // free the node
		return 0;
	}
}

int DVRouting::checklqueue(RoutInfoManagLq* q, Info* x)
{
	InfoNodeType  *p;
	if (q->front->next == NULL)
		return -1;
	else
	{
		p = q->front->next;   // get front node
		*x = p->info;			// get element
		return 0;
	}
}

DVRouting::Info DVRouting::lookuplqueue(RoutInfoManagLq* q, char cond_id[])
{

	InfoNodeType* x = q->front->next;
	while (x != NULL)
	{
		if (strcmp(x->info.ID, cond_id))
		{
			break;
		}
		x = x->next;
	}
	return x->info;
}

int DVRouting::lookupRouter_table(RoutingTable routing_table, char cond_DestRID[])
{
	int i;
	for (i = 0; i < routing_table.entry_count; i++)
	{
		if (strcmp(routing_table.RInfo[i].DestRID, cond_DestRID) == 0)
		{
			break;
		}
	}
	return 0;
}

DWORD WINAPI DVRouting::router_proc(thread_arg* my_arg)
{
	int time_start, i;
	routerMsg* recv_router_msg;
	routerMsg* send_router_msg;
	RoutingTable routing_table;
	
	my_arg->info->sock = my_arg->ptr->init_router(my_arg->info);
	enlqueue(&rout_info_manag, *(my_arg->info)); // add new router into management queue
	cout << "active" << endl;

	// select
	int retval, len;
	fd_set readfds;
	unsigned long mode = 1;// set nonblocking mode
	const timeval blocking_time{ 0, 100000 };
	FD_ZERO(&readfds);
	retval = ioctlsocket(my_arg->info->sock, FIONBIO, &mode);
	if (retval == SOCKET_ERROR)
	{
		retval = WSAGetLastError();
		#ifdef DEBUG
		cout << "select error ! exit..." << endl;
		exit(0);
		#endif
		return -1;
	}



	while (1)
	{
		// listen new msgs & update routing table
		// select
		FD_SET(my_arg->info->sock, &readfds);
		retval = select(0, &readfds, NULL, NULL, &blocking_time);// noblocking
		if (retval == SOCKET_ERROR)
		{
			retval = WSAGetLastError();
			#ifdef DEBUG
			cout << "select error ! exit..." << endl;
			exit(0);
			#endif
			break;
		}

		if (retval = FD_ISSET(my_arg->info->sock, &readfds))
		{
			// recv event
			if (my_arg->ptr->recvRouterMsg(my_arg->info->sock, recv_router_msg) != -1)
			{
				// run Bellman-Ford algorithm
				for (i = 0; i < recv_router_msg->msg_count; i++)
				{
					int index = lookupRouter_table(routing_table, recv_router_msg->RInfo[i].DestRID);
					int index2 = lookupRouter_table(routing_table, recv_router_msg->RInfo[i].SourRID);

					if (recv_router_msg->RInfo[i].cost + routing_table.RInfo[index2].cost < \
						routing_table.RInfo[index].cost)
					{
						// update routing_table
						routing_table.RInfo[index].cost = recv_router_msg->RInfo[i].cost + \
							routing_table.RInfo[index2].cost;
						memcpy(routing_table.RInfo[index].nextRID, recv_router_msg->RInfo[i].SourRID, sizeof(recv_router_msg->RInfo[i].SourRID));
						routing_table.RInfo[index].numOfHops = routing_table.RInfo[index2].numOfHops + 1;

					}
					
				}


			}

		}
		
		// periodically broadcast advertisement to neighbors
		if (clock() - time_start > broadcast_timeout)
		{
			send_router_msg->type = 0; // default
			memcpy(send_router_msg->RInfo, routing_table.RInfo, sizeof(routing_table.RInfo));
			send_router_msg->msg_count = routing_table.entry_count;
			for (i = 0; i < my_arg->info->link_info_count; i++) // traverse neighbors
			{
				my_arg->ptr->sendRouterMsg(my_arg->info->ID, my_arg->info->link_info[i].LinkToID, send_router_msg);
			}
			time_start = clock();
		}




	}

	return 0;
}

DWORD __stdcall startMethodInThread(LPVOID arg)
{
	if (!arg)
	{
		return -1;
	}
	DVRouting::thread_arg* my_arg = (DVRouting::thread_arg*)arg;
	my_arg->ptr->router_proc(my_arg);
	return 0;
}

int DVRouting::sendRouterMsg(char s_RID[], char d_RID[], routerMsg* rmsg)
{
	int retval;
	Info s_info, d_info;
	sockaddr remote_addr;
	int remote_addr_len = sizeof(remote_addr);
	s_info = lookuplqueue(&rout_info_manag, s_RID);
	d_info = lookuplqueue(&rout_info_manag, d_RID);
	getsockname(d_info.sock, &remote_addr, &remote_addr_len); // get remote address
	retval = sendto(s_info.sock, (char*)rmsg, sizeof(rmsg), 0, &remote_addr, remote_addr_len);
	if (retval < 0)
	{
		#ifdef DEBUG
		cout << "send msg error ! exit..." << endl;
		exit(0);
		#endif
	}
	return 0;
}

int DVRouting::recvRouterMsg(SOCKET sock, routerMsg* rmsg)
{
	char recvbuf[1024];
	int len, retval;
	sockaddr remote_addr;
	int remote_addr_len = sizeof(remote_addr);
	len = recvfrom(sock, recvbuf, sizeof(recvbuf), 0, &remote_addr, &remote_addr_len);
	if (len <= 0)
	{
		retval = WSAGetLastError();
		if (retval != WSAEWOULDBLOCK && retval != WSAECONNRESET)
		{
			#ifdef DEBUG
			cout << "recv() failed !" << endl;
			#endif
			return -1;
		}
		return -1;
	}
	else
	{
		recvbuf[len] = 0;
		memcpy(rmsg, (routerMsg*)recvbuf, len);
		return 0;
	}
}


SOCKET DVRouting::init_router(Info* info)
{
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	int retval;
	sockaddr_in local_addr;
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
	local_addr.sin_port = htons(info->portnum);
	retval = bind(sock, (sockaddr*)&local_addr, sizeof(local_addr));
	if (retval == SOCKET_ERROR)
	{
		retval = WSAGetLastError();
		#ifdef DEBUG
		cout << "router initialization error ! exit..." << endl;
		exit(0);
		#endif
		return -1;
	}
	return sock;
}


void DVRouting::frontend(void)
{

	char command[100];
	string cmd;
	string sub_cmd;
	string sub_cmd2;
	Info info;
	thread_arg arg;
	while (1)
	{

		if (_kbhit())// querying keyboard event
		{
			if (cin.getline(command, sizeof(command)))
			{
				cmd = string(command);
				if (cmd.substr(0, cmd.find(" ")) == command_array[dvrouter])
				{
					sub_cmd = cmd.substr(cmd.find(" ") + 1, cmd.find(" ", cmd.find(" ") + 1) - cmd.find(" ") - 1);
					sub_cmd2 = cmd.substr(cmd.find(" ", cmd.find(" ") + 1) + 1);

					memcpy(info.ID, "A", sizeof("A"));
					memcpy(info.hostname, "127.0.0.1", sizeof("127.0.0.1"));
					info.portnum = 8001;
					info.life_state = true;


					arg.info = &info;
					arg.ptr = this;

					DWORD dwThreadId;
					HANDLE hThread;
					hThread = CreateThread(
						NULL,                        // no security attributes 
						0,                           // use default stack size  
						startMethodInThread,                  // thread function 
						&arg,                // argument to thread function 
						0,                           // use default creation flags 
						&dwThreadId);                // returns the thread identifier
				}
			}
		}
	}
}


int main(int argc, char**argv)
{
	WSAData wsa;
	WSAStartup(0x101, &wsa);
	DVRouting dvr;
	dvr.frontend();
	
	return 0;
}

