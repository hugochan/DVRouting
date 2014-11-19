// DVRouting.cpp

#include <iostream>
#include <winsock.h>
#include <windows.h>
#include <string.h>
#include <string>
#include <conio.h>
#include <time.h>
#include <assert.h>
#include "DVRouting.h"

#pragma comment(lib,"wsock32.lib")
#pragma pack (1)


using namespace std;
#define DEBUG
//#define _POISONREVERSE


enum commandName { dvrouter, add, change, display, kill };
const char* command_array[7] = { "dvrouter", "add", "change", "display", "kill" };

DWORD __stdcall startMethodInThread(LPVOID arg);


DVRouting::DVRouting()
{
	broadcast_timeout = 10000; // 10s
	keepalive_timeout = 30000; // 30s
	rout_info_manag.front = &info_node;
	rout_info_manag.rear = &info_node;
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
	bool findflag = false;
	InfoNodeType* x = q->front->next;
	InfoNodeType* y = q->front;

	while (x != NULL)
	{
		if (strcmp(x->info.ID, cond_id) == 0)
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
		if (x->next == NULL) q->rear = y;
		delete x;		       // free the node
		return 0;
	}

}


DVRouting::Info* DVRouting::lookupllist(RoutInfoManagLL* q, char cond_id[])
{
	bool findflag = false;
	InfoNodeType* x = q->front->next;
	while (x != NULL)
	{
		if (strcmp(x->info.ID, cond_id) == 0)
		{
			findflag = true;
			break;
		}
		x = x->next;
	}
	if (!findflag) return NULL;
	else return &(x->info);
}

int DVRouting::lookupLinkInfoArray(LinkInfo link_info[], unsigned int link_info_count, char cond_link_to_ID[])
{
	bool findflag = false;
	int i;
	for (i = 0; i < link_info_count; i++)
	{
		if (strcmp(link_info[i].LinkToID, cond_link_to_ID) == 0)
		{
			findflag = true;
			break;
		}
	}
	if (!findflag) return -1;
	else return i;
}

int DVRouting::deInfoArray(LinkInfo link_info[], unsigned int* link_info_count, char cond_link_to_ID[])
{
	int i, j;
	bool findflag = false;
	for (i = 0; i < *link_info_count; i++)
	{
		if (strcmp(link_info[i].LinkToID, cond_link_to_ID) == 0)
		{
			findflag = true;
			break;
		}
	}

	if (!findflag) return -1;
	else
	{
		for (j = i; j < *link_info_count - 1; j++)
		{
			link_info[j] = link_info[j + 1];
		}
		*link_info_count -= 1;
		return 0;
	}
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
	bool findflag = false;
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
	bool findflag = false;
	for (i = 0; i < routing_table.entry_count; i++)
	{
		if (strcmp(routing_table.RInfo[i].DestRID, cond_DestRID) == 0)
		{
			findflag = true;
			break;
		}
	}
	if (!findflag) return -1;
	else return i;
}

DVRouting::RoutingInfo DVRouting::makeRoutingInfo(char SourRID[], char DestRID[], unsigned int cost, unsigned int numOfHops, char nextRID[])
{
	RoutingInfo routing_info;
	memcpy(routing_info.SourRID, SourRID, sizeof(SourRID));
	memcpy(routing_info.DestRID, DestRID, sizeof(DestRID));
	routing_info.cost = cost;
	routing_info.numOfHops = numOfHops;
	memcpy(routing_info.nextRID, nextRID, sizeof(nextRID));
	routing_info.updateflag = true;
	return routing_info;
}

DWORD WINAPI DVRouting::router_proc(thread_arg my_arg)
{
	int time_start = clock(), i, j, index, index2, retval;
	routerMsg* recv_router_msg = new routerMsg;
	routerMsg* send_router_msg = new routerMsg;
	RoutingTable routing_table;
	RoutingInfo routing_info;
	routing_table.entry_count = 0;
	Info* info;
	DVRouting* class_ptr = my_arg.ptr;

	// initialize router
	my_arg.info.sock = class_ptr->init_router(&my_arg.info);
	my_arg.info.link_info_count = 0;
	retval = enllist(&rout_info_manag, my_arg.info); // add new router into management queue
	if (retval == -1)
	{
		cout << "fatal error! exit..." << endl;
		#ifdef DEBUG
		cout << "rout_info_manag enllist error!" << endl;
		#endif
		closeAllSocket(&rout_info_manag);
		exit(0);
	}

	info = lookupllist(&rout_info_manag, my_arg.info.ID); // important! update my_arg.info
	cout << "active" << endl;

	//initialize routing table
	routing_info = makeRoutingInfo(info->ID, info->ID, 0, 0, info->ID);
	class_ptr->enRouter_table(&routing_table, routing_info);


	// select
	fd_set readfds;
	unsigned long mode = 1;// set nonblocking mode
	const timeval blocking_time{ 0, 100000 };
	FD_ZERO(&readfds);
	retval = ioctlsocket(info->sock, FIONBIO, &mode);
	if (retval == SOCKET_ERROR)
	{
		retval = WSAGetLastError();
		#ifdef DEBUG
		cout << "select error ! exit..." << endl;
		closeAllSocket(&rout_info_manag);
		exit(0);
		#endif
		return -1;
	}



	while (1)
	{	
		// listen new msgs & update routing table
		// select
		FD_SET(info->sock, &readfds);
		retval = select(0, &readfds, NULL, NULL, &blocking_time);// noblocking
		if (retval == SOCKET_ERROR)
		{
			retval = WSAGetLastError();
			#ifdef DEBUG
			cout << "select error ! exit..." << endl;
			closeAllSocket(&rout_info_manag);
			exit(0);
			#endif
			break;
		}

		if (retval = FD_ISSET(info->sock, &readfds))
		{
			// recv event
			if (class_ptr->recvRouterMsg(info->sock, recv_router_msg) != -1)
			{
				// run Bellman-Ford algorithm
				if (recv_router_msg->msg_count > 0)
				{
					// update routing_table
					// update routing info with neighbors if allowed
					index2 = lookupRouter_table(routing_table, recv_router_msg->RInfo[0].SourRID);

					for (i = 0; i < recv_router_msg->msg_count; i++)
					{
						if (strcmp(recv_router_msg->RInfo[i].DestRID, info->ID) == 0)
						{
							if (index2 == -1)
							{
								routing_info = makeRoutingInfo(info->ID, recv_router_msg->RInfo[i].SourRID, \
									recv_router_msg->RInfo[i].cost, recv_router_msg->RInfo[i].numOfHops, \
									recv_router_msg->RInfo[i].SourRID);

								retval = class_ptr->enRouter_table(&routing_table, routing_info);
								if (retval == -1)
								{
									#ifdef DEBUG
									cout << "routing table is full ! exit..." << endl;
									closeAllSocket(&rout_info_manag);
									exit(0);
									#endif
									return -1;
								}
							}
							else
							{
								routing_table.RInfo[index2].cost = recv_router_msg->RInfo[i].cost;
								routing_table.RInfo[index2].numOfHops = recv_router_msg->RInfo[i].numOfHops;
								routing_table.RInfo[index2].updateflag = true;
							}
							break;
						}
					}

					
					index2 = lookupRouter_table(routing_table, recv_router_msg->RInfo[0].SourRID);
					if (index2 == -1) {
						i = 0;
					}
					assert(index2 != -1);

					// update other routing info
					for (i = 0; i < recv_router_msg->msg_count; i++)
					{
						int index = lookupRouter_table(routing_table, recv_router_msg->RInfo[i].DestRID);

						if (index == -1)
						{
							// update routing_table
							routing_info = makeRoutingInfo(info->ID, recv_router_msg->RInfo[i].DestRID, \
								recv_router_msg->RInfo[i].cost + routing_table.RInfo[index2].cost, \
								recv_router_msg->RInfo[i].numOfHops + 1, \
								recv_router_msg->RInfo[i].SourRID);

							retval = class_ptr->enRouter_table(&routing_table, routing_info);
							if (retval == -1)
							{
								#ifdef DEBUG
								cout << "routing table is full ! exit..." << endl;
								closeAllSocket(&rout_info_manag);
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
								routing_table.RInfo[index].updateflag = true;
							}
						}
					}
				}

				// update keepalive time
				index = lookupLinkInfoArray(info->link_info, info->link_info_count, recv_router_msg->ID);
				if (index == -1) {
					i = 0;
				}

				assert(index != -1);
				info->link_info[index].keepalive_time = clock();
			}	
		}

	
		
		// periodically broadcast advertisement to neighbors
		if (clock() - time_start > broadcast_timeout)
		{	
			if (info->link_info_count > 0)
			{
				send_router_msg->type = 0; // default
				send_router_msg->msg_count = 0;
				memcpy(send_router_msg->ID, info->ID, sizeof(info->ID));
				// broadcast changed entries
				for (i = 0, j = 0; i < routing_table.entry_count; i++)
				{
					if (routing_table.RInfo[i].updateflag == true)
					{
						send_router_msg->RInfo[j] = routing_table.RInfo[i];
						j++;
						routing_table.RInfo[i].updateflag = false;
					}
				}
				send_router_msg->msg_count = j;

				for (i = 0; i < info->link_info_count; i++) // traverse neighbors
				{
					class_ptr->sendRouterMsg(info->ID, info->link_info[i].LinkToID, send_router_msg);
				}
			}
			
			time_start = clock();
		}

		// keep alive process: tell whether neighbors are alive
		for (i = 0; i < info->link_info_count; i++)
		{
			if (clock() - info->link_info[i].keepalive_time > keepalive_timeout)
			{

				// update routing table
				#ifdef _POISONREVERSE
				index2 = lookupRouter_table(routing_table, info->link_info[i].LinkToID);
				assert(index2 != -1);
				routing_table.RInfo[index2].cost = POISONMETRIC;
				routing_table.RInfo[index2].updateflag = true;
				#else
				retval = deRouter_table(&routing_table, info->link_info[i].LinkToID);
				if (retval == -1) {
					i = 0;
				}
				assert(retval != -1);
				#endif

				// remove the link
				if (deInfoArray(info->link_info, &(info->link_info_count), info->link_info[i].LinkToID) == -1)
				{
					#ifdef DEBUG
					cout << "deInfoArray error! exit..." << endl;
					closeAllSocket(&rout_info_manag);
					exit(0);
					#endif
					return -1;
				}
			}

		}


		// kill
		if (strcmp(kill_flag, info->ID) == 0)
		{
			memset(kill_flag, 0, sizeof(kill_flag)); // clear kill_flag
			closesocket(info->sock);
			if (dellist(&rout_info_manag, info->ID) == -1)
			{
				cout << "kill error! exit..." << endl;
				closeAllSocket(&rout_info_manag);
				exit(0);
			}
			break;
		}

		// display routing table
		if (strcmp(display_flag, info->ID) == 0)
		{
			memset(display_flag, 0, sizeof(display_flag)); // clear display_flag
			cout << "The distance vector for Router " + string(info->ID) + " is:" << endl;
			for (i = 0; i < routing_table.entry_count; i++)
			{
				cout << routing_table.RInfo[i].SourRID + string(" ") + string(routing_table.RInfo[i].DestRID)\
					+ string(" ") + to_string(routing_table.RInfo[i].cost) + string(" ") + \
					to_string(routing_table.RInfo[i].numOfHops) + string(" ") + \
					routing_table.RInfo[i].nextRID<< endl;
			}
		}



		// update flag: add or change
		if (strcmp(update_flag.update_router_ID, info->ID) == 0)
		{

			index = lookupRouter_table(routing_table, update_flag.update_link_ID);
			index2 = lookupLinkInfoArray(info->link_info, info->link_info_count, update_flag.update_link_ID);
			if (index2 == -1) {
				i = 0;
			}

			assert(index2 != -1);
			if (index == -1) // add
			{
				routing_info = makeRoutingInfo(info->ID, update_flag.update_link_ID, \
					info->link_info[index2].cost, 1, update_flag.update_link_ID);

				retval = class_ptr->enRouter_table(&routing_table, routing_info);
				if (retval == -1)
				{
					#ifdef DEBUG
					cout << "routing table is full ! exit..." << endl;
					closeAllSocket(&rout_info_manag);
					exit(0);
					#endif
					return -1;
				}
			}
			else // change
			{
				routing_table.RInfo[index].cost = info->link_info[index2].cost;
				routing_table.RInfo[index].updateflag = true;
			}
			memset(update_flag.update_router_ID, 0, sizeof(update_flag.update_router_ID)); // clear
			memset(update_flag.update_link_ID, 0, sizeof(update_flag.update_link_ID)); // clear
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
	my_arg->ptr->router_proc(*my_arg);
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
	if (d_info == NULL) return -1; // most likely the peer router has been killed
	getsockname(d_info->sock, &remote_addr, &remote_addr_len); // get remote address


	retval = sendto(s_info->sock, (char*)rmsg, sizeof(*rmsg), 0, &remote_addr, remote_addr_len);
	if (retval < 0)
	{
		#ifdef DEBUG
		cout << "send msg error ! exit..." << endl;
		closeAllSocket(&rout_info_manag);
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
		closeAllSocket(&rout_info_manag);
		exit(0);
		#endif
		return -1;
	}
	return sock;
}

int DVRouting::closeAllSocket(RoutInfoManagLL* q)
{
	InfoNodeType* x = q->front->next;
	while (x != NULL)
	{
		closesocket(x->info.sock);
		x = x->next;
	}
	return 0;
}

void DVRouting::frontend(void)
{

	char command[100];
	string cmd;
	string sub_cmd;
	string sub_cmd2;
	string sub_cmd3;
	string sub_cmd4;
	Info *currentR = NULL, *operationR = NULL;
	char operate[10];
	int i = 0;
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

					currentR = lookupllist(&rout_info_manag, (char*)sub_cmd.c_str());
					if (currentR != NULL) // already active, used for checkout
					{
						cout << "checkout to " + sub_cmd << endl;
						continue;
					}

					Info* tempInfo = new Info;
					memcpy((*tempInfo).ID, sub_cmd.c_str(), sizeof(sub_cmd.c_str()));
					memcpy((*tempInfo).hostname, "127.0.0.1", sizeof("127.0.0.1"));
					(*tempInfo).portnum = atoi(sub_cmd2.c_str());
					(*tempInfo).life_state = true;

					arg = { this, *tempInfo };

					DWORD dwThreadId;
					HANDLE hThread;
					hThread = CreateThread(
						NULL,                        // no security attributes 
						0,                           // use default stack size  
						startMethodInThread,                  // thread function 
						&arg,                // argument to thread function 
						0,                           // use default creation flags 
						&dwThreadId);                // returns the thread identifier
					
					currentR = lookupllist(&rout_info_manag, (char*)sub_cmd.c_str()); // point to real addr
					while (currentR == NULL)
					{
						currentR = lookupllist(&rout_info_manag, (char*)sub_cmd.c_str());
					}
					delete tempInfo;
				}
				else if (cmd.substr(0, cmd.find(" ")) == command_array[add])
				{
					if (currentR == NULL)
					{
						cout << "add is not allowed!" << endl;
						continue;
					}
					sub_cmd = cmd.substr(cmd.find(" ") + 1, cmd.find(" ", cmd.find(" ") + 1) - cmd.find(" ") - 1); //router
					sub_cmd2 = cmd.substr(cmd.find(" ", cmd.find(" ") + 1) + 1, cmd.find(" ", cmd.find(" ", cmd.find(" ") + 1) + 1) - cmd.find(" ", cmd.find(" ") + 1) - 1); // hostname
					sub_cmd3 = cmd.substr(cmd.find(" ", cmd.find(" ", cmd.find(" ") + 1) + 1) + 1, cmd.find(" ", cmd.find(" ", cmd.find(" ", cmd.find(" ") + 1) + 1) + 1) - cmd.find(" ", cmd.find(" ", cmd.find(" ") + 1) + 1) - 1);// port number
					sub_cmd4 = cmd.substr(cmd.find(" ", cmd.find(" ", cmd.find(" ", cmd.find(" ") + 1) + 1) + 1) + 1);// cost
					memcpy(operate, sub_cmd.c_str(), sizeof(sub_cmd.c_str()));
					operationR = lookupllist(&rout_info_manag, operate);
					if (operationR == NULL)
					{
						cout << sub_cmd + "is not alive!" << endl;
						continue;
					}

					if ((*(operationR)).life_state)
					{
						// update current router
						i = lookupLinkInfoArray((*currentR).link_info, (*currentR).link_info_count, operate);
						if (i != -1) // already exists
						{
							cout << "link " + string((*currentR).ID) + "-" + operate + " already exists! add is not allowed!" << endl;
							continue;
						}
						if ((*currentR).link_info_count >= MaxnumOfRouter)
						{
							cout << "no more room for add!" << endl;
							continue;
						}

						memcpy(&((*currentR).link_info[(*currentR).link_info_count].LinkToID), operate, sizeof(operate));
						(*currentR).link_info[(*currentR).link_info_count].cost = atoi(sub_cmd4.c_str());
						(*currentR).link_info[(*currentR).link_info_count].keepalive_time = clock();
						(*currentR).link_info_count += 1;

						// update operate router
						i = lookupLinkInfoArray((*operationR).link_info, (*operationR).link_info_count, (*currentR).ID);
						if (i != -1) // it depends !!!
						{
							cout << "link " + string(operate) + "-" + (*currentR).ID + " already exists! add is not allowed!" << endl;
							closeAllSocket(&rout_info_manag);
							exit(0);
						}

						if ((*operationR).link_info_count >= MaxnumOfRouter)
						{
							cout << "no more room for add!" << endl;
							closeAllSocket(&rout_info_manag);
							exit(0);
						}
						memcpy(&((*operationR).link_info[(*operationR).link_info_count].LinkToID), (*currentR).ID, sizeof((*currentR).ID));
						(*operationR).link_info[(*operationR).link_info_count].cost = atoi(sub_cmd4.c_str());
						(*operationR).link_info[(*operationR).link_info_count].keepalive_time = clock();
						(*operationR).link_info_count += 1;

						// update flag
						memcpy(update_flag.update_router_ID, (*currentR).ID, sizeof((*currentR).ID));
						memcpy(update_flag.update_link_ID, (*operationR).ID, sizeof((*operationR).ID));

					}
					else
					{
						cout << sub_cmd + "is not alive!" << endl;
					}


				}
				else if (cmd.substr(0, cmd.find(" ")) == command_array[change])
				{
					if (currentR == NULL)
					{
						cout << "change is not allowed!" << endl;
						continue;
					}
					sub_cmd = cmd.substr(cmd.find(" ") + 1, cmd.find(" ", cmd.find(" ") + 1) - cmd.find(" ") - 1);
					sub_cmd2 = cmd.substr(cmd.find(" ", cmd.find(" ") + 1) + 1);
					memcpy(operate, sub_cmd.c_str(), sizeof(sub_cmd.c_str()));
					operationR = lookupllist(&rout_info_manag, operate);
					if (operationR == NULL)
					{
						cout << sub_cmd + "is not alive!" << endl;
						continue;
					}

					if ((*(operationR)).life_state)
					{
						// update current router
						i = lookupLinkInfoArray((*currentR).link_info, (*currentR).link_info_count, operate);
						if (i == -1)
						{
							cout << "link " +  string((*currentR).ID) + "-" + operate + " is not set!" << endl;
							continue;
						}

						(*currentR).link_info[i].cost = atoi(sub_cmd2.c_str());

						// update operate router
						i = lookupLinkInfoArray((*operationR).link_info, (*operationR).link_info_count, (*currentR).ID);
						if (i == -1) // it depends !!!
						{
							cout << "link " + string(operate) + "-" + (*currentR).ID + " is not set!" << endl;
							closeAllSocket(&rout_info_manag);
							exit(0);
						}

						(*operationR).link_info[i].cost = atoi(sub_cmd2.c_str());

						// update flag
						memcpy(update_flag.update_router_ID, (*currentR).ID, sizeof((*currentR).ID));
						memcpy(update_flag.update_link_ID, (*operationR).ID, sizeof((*operationR).ID));
					}
					else
					{
						cout << sub_cmd + "is not alive!" << endl;
					}
				}
				else if (cmd.substr(0, cmd.find(" ")) == command_array[display])
				{
					if (currentR == NULL)
					{
						cout << "display is not allowed!" << endl;
						continue;
					}
					memcpy(display_flag, (*currentR).ID, sizeof((*currentR).ID));
				}
				else if (cmd.substr(0, cmd.find(" ")) == command_array[kill])
				{
					if (currentR == NULL)
					{
						cout << "kill is not allowed!" << endl;
						continue;
					}
					memcpy(kill_flag, (*currentR).ID, sizeof((*currentR).ID));
					currentR = NULL;
				}
				else
				{
					cout << "undefined command..." << endl; 
				}
			}
		}
	}
	delete currentR;
	delete operationR;
}



int main(int argc, char**argv)
{
	cout << "#########################################################" << endl;
	cout << "*********************************************************" << endl;
	cout << "******************DVRouting******************************" << endl;
	cout << "*****************************author: Yu Chen & Zhan Shi**" << endl;
	cout << "*********************************************************" << endl;
	cout << "#########################################################" << endl;

	WSAData wsa;
	WSAStartup(0x101, &wsa);
	DVRouting dvr;
	dvr.frontend();
	
	return 0;
}

