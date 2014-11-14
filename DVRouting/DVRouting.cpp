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


int DVRouting::enllist(RoutInfoManagLL* q, Info x)
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


int DVRouting::dellist(RoutInfoManagLL* q, char cond_id[])
{
	boolean findflag = false;
	InfoNodeType* x = q->front->next;
	InfoNodeType* y = q->front;

	while (x != NULL)
	{
		if (strcmp(x->info.ID, cond_id))
		{
			findflag = true;
			break;
		}
		y = x;
		x = x->next;
	}
	if (!findflag) return -1;
	else
	{
		y->next = x->next; // update front pointer
		if (x->next == NULL) q->rear = q->front;
		delete x;		       // free the node
		return 0;
	}

}


DVRouting::Info* DVRouting::lookupllist(RoutInfoManagLL* q, char cond_id[])
{
	boolean findflag = false;
	InfoNodeType* x = q->front->next;
	while (x != NULL)
	{
		if (strcmp(x->info.ID, cond_id))
		{
			findflag = true;
			break;
		}
		x = x->next;
	}
	if (!findflag) return NULL;
	else return &(x->info);
}

int DVRouting::enRouter_table(RoutingTable* routing_table, RoutingInfo entry)
{
	if (routing_table->entry_count >= MaxnumOfRouter) return -1;
	else
	{
		routing_table->RInfo[routing_table->entry_count] = entry;
		routing_table->entry_count += 1;
		return 0;
	}
}

int DVRouting::deRouter_table(RoutingTable* routing_table, char cond_DestRID[])
{
	int i, j;
	boolean findflag = false;
	for (i = 0; i < routing_table->entry_count; i++)
	{
		if (strcmp(routing_table->RInfo[i].DestRID, cond_DestRID) == 0)
		{
			findflag = true;
			break;
		}
	}

	if (!findflag) return -1;
	else
	{
		for (j = i; j < routing_table->entry_count - 1; j++)
		{
			routing_table->RInfo[j] = routing_table->RInfo[j + 1];
		}
		routing_table->entry_count -= 1;
		return 0;
	}
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

DVRouting::RoutingInfo DVRouting::makeRoutingInfo(char SourRID[], char DestRID[], int cost, int numOfHops, char nextRID[])
{
	RoutingInfo routing_info;
	memcpy(routing_info.SourRID, SourRID, sizeof(SourRID));
	memcpy(routing_info.DestRID, DestRID, sizeof(DestRID));
	routing_info.cost = cost;
	routing_info.numOfHops = numOfHops;
	memcpy(routing_info.nextRID, nextRID, sizeof(nextRID));
	return routing_info;
}

DWORD WINAPI DVRouting::router_proc(thread_arg* my_arg)
{
	int time_start = clock(), i, index, index2, retval;
	routerMsg* recv_router_msg = new routerMsg;
	routerMsg* send_router_msg = new routerMsg;
	RoutingTable routing_table;
	RoutingInfo routing_info;
	routing_table.entry_count = 0;
	
	// initialize router
	my_arg->info->sock = my_arg->ptr->init_router(my_arg->info);
	enllist(&rout_info_manag, *(my_arg->info)); // add new router into management queue
	cout << "active" << endl;

	//initialize routing table
	routing_info = makeRoutingInfo(my_arg->info->ID, my_arg->info->ID, 0, 0, my_arg->info->ID);
	my_arg->ptr->enRouter_table(&routing_table, routing_info);


	// select
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
				if (recv_router_msg->msg_count > 0)
				{
					// update routing_table
					// update routing info with neighbors if allowed
					index2 = lookupRouter_table(routing_table, recv_router_msg->RInfo[0].SourRID);

					for (i = 0; i < recv_router_msg->msg_count; i++)
					{
						if (recv_router_msg->RInfo[i].DestRID == my_arg->info->ID)
						{
							if (index2 == -1)
							{
								routing_info = makeRoutingInfo(my_arg->info->ID, recv_router_msg->RInfo[i].SourRID, \
									recv_router_msg->RInfo[i].cost, recv_router_msg->RInfo[i].numOfHops, \
									recv_router_msg->RInfo[i].SourRID);

								retval = my_arg->ptr->enRouter_table(&routing_table, routing_info);
								if (retval == -1)
								{
									#ifdef DEBUG
									cout << "routing table is full ! exit..." << endl;
									exit(0);
									#endif
									return -1;
								}
							}
							else
							{
								routing_table.RInfo[index2].cost = recv_router_msg->RInfo[i].cost;
								routing_table.RInfo[index2].numOfHops = recv_router_msg->RInfo[i].numOfHops;
							}
							break;
						}
					}

					
					index2 = lookupRouter_table(routing_table, recv_router_msg->RInfo[0].SourRID);
					if (index2 == -1)
					{
						#ifdef DEBUG
						cout << "routing error ! exit..." << endl;
						exit(0);
						#endif
						return -1;
					}

					// update other routing info
					for (i = 0; i < recv_router_msg->msg_count; i++)
					{
						int index = lookupRouter_table(routing_table, recv_router_msg->RInfo[i].DestRID);

						if (index == -1)
						{
							// update routing_table
							routing_info = makeRoutingInfo(my_arg->info->ID, recv_router_msg->RInfo[i].DestRID, \
								recv_router_msg->RInfo[i].cost + routing_table.RInfo[index2].cost, \
								recv_router_msg->RInfo[i].numOfHops + 1, \
								recv_router_msg->RInfo[i].SourRID);

							retval = my_arg->ptr->enRouter_table(&routing_table, routing_info);
							if (retval == -1)
							{
								#ifdef DEBUG
								cout << "routing table is full ! exit..." << endl;
								exit(0);
								#endif
								return -1;
							}
						}
						else
						{
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
				
				else
				{
					// periodically keep alive
					// to do...

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

	delete recv_router_msg;
	delete send_router_msg;
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
	Info *s_info, *d_info;
	sockaddr remote_addr;
	int remote_addr_len = sizeof(remote_addr);
	s_info = lookupllist(&rout_info_manag, s_RID);
	d_info = lookupllist(&rout_info_manag, d_RID);
	getsockname(d_info->sock, &remote_addr, &remote_addr_len); // get remote address
	retval = sendto(s_info->sock, (char*)rmsg, sizeof(rmsg), 0, &remote_addr, remote_addr_len);
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

