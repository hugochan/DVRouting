// DVRouting.cpp

#include <iostream>
#include <winsock.h>
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


enum commandName { add, change, display, kill };
const char* command_array[4] = { "add", "change", "display", "kill" };

DWORD __stdcall startMethodInThread(LPVOID arg);


DVRouting::DVRouting()
{
	broadcast_timeout = 10000; // 10s
	keepalive_timeout = 30000; // 30s

	// initialize router_info
	memset(router_info.hostname, 0, sizeof(router_info.hostname));
	memset(router_info.ID, 0, sizeof(router_info.ID));
	router_info.life_state = false;
	router_info.link_info_count = 0;
	router_info.portnum = 0;
	router_info.sock = 0;
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
	DVRouting* class_ptr = my_arg.ptr;
	SOCKET sock;
	sockaddr_in* remote_addr = new sockaddr_in;
	char temp[10];
	// initialize router
	sock = class_ptr->init_router(&my_arg.info);
	if (sock == -1)
	{
		cout << "socket error! exit..." << endl;
		exit(0);
	}
	memcpy(router_info.ID, my_arg.info.ID, sizeof(my_arg.info.ID));
	//memcpy(router_info.hostname, my_arg.info.hostname, sizeof(my_arg.info.hostname));
	router_info.portnum = my_arg.info.portnum;
	router_info.sock = sock;
	router_info.life_state = true;
	itoa(router_info.portnum, temp, 10);
	cout << "Router " + string(router_info.ID) + " (localhost, " + temp + ") is active. Waiting for input and output ..." << endl;

	//initialize routing table
	routing_info = class_ptr->makeRoutingInfo(router_info.ID, router_info.ID, 0, 0, router_info.ID);
	class_ptr->enRouter_table(&routing_table, routing_info);


	// select
	fd_set readfds;
	unsigned long mode = 1;// set nonblocking mode
	const timeval blocking_time{ 0, 100000 };
	FD_ZERO(&readfds);
	retval = ioctlsocket(router_info.sock, FIONBIO, &mode);
	if (retval == SOCKET_ERROR)
	{
		retval = WSAGetLastError();
		#ifdef DEBUG
		cout << "select error ! exit..." << endl;
		closesocket(router_info.sock);
		exit(0);
		#endif
		return -1;
	}



	while (1)
	{	
		// listen new msgs & update routing table
		// select
		FD_SET(router_info.sock, &readfds);
		retval = select(0, &readfds, NULL, NULL, &blocking_time);// noblocking
		if (retval == SOCKET_ERROR)
		{
			retval = WSAGetLastError();
			#ifdef DEBUG
			cout << "select error ! exit..." << endl;
			closesocket(router_info.sock);
			exit(0);
			#endif
			break;
		}

		if (retval = FD_ISSET(router_info.sock, &readfds))
		{
			// recv event
			if (class_ptr->recvRouterMsg(router_info.sock, recv_router_msg, remote_addr) != -1)
			{
				// run Bellman-Ford algorithm
				if (recv_router_msg->msg_count > 0)
				{
					if (recv_router_msg->type == 0) // routing msg
					{
						// update routing_table
						// update routing info with neighbors if allowed
						index2 = lookupRouter_table(routing_table, recv_router_msg->RInfo[0].SourRID);

						for (i = 0; i < recv_router_msg->msg_count; i++)
						{
							if (strcmp(recv_router_msg->RInfo[i].DestRID, router_info.ID) == 0)
							{
								if (index2 == -1) // add new entry
								{
									routing_info = makeRoutingInfo(router_info.ID, recv_router_msg->RInfo[i].SourRID, \
										recv_router_msg->RInfo[i].cost, recv_router_msg->RInfo[i].numOfHops, \
										recv_router_msg->RInfo[i].SourRID);

									retval = class_ptr->enRouter_table(&routing_table, routing_info);
									if (retval == -1)
									{
										#ifdef DEBUG
										cout << "routing table is full ! exit..." << endl;
										closesocket(router_info.sock);
										exit(0);
										#endif
										return -1;
									}
								}
								else // change existing entry
								{
									if (strcmp(routing_table.RInfo[index2].nextRID, recv_router_msg->RInfo[i].SourRID) == 0)
									{
										if ((routing_table.RInfo[index2].cost != recv_router_msg->RInfo[i].cost)) // some issues might ocur
										{
											routing_table.RInfo[index2].cost = recv_router_msg->RInfo[i].cost;
											routing_table.RInfo[index2].numOfHops = recv_router_msg->RInfo[i].numOfHops;
											routing_table.RInfo[index2].updateflag = true;
										}
									}
									else
									{
										if (routing_table.RInfo[index2].cost >= recv_router_msg->RInfo[i].cost)
										{
											routing_table.RInfo[index2].cost = recv_router_msg->RInfo[i].cost;
											routing_table.RInfo[index2].numOfHops = recv_router_msg->RInfo[i].numOfHops;
											memcpy(routing_table.RInfo[index2].nextRID, recv_router_msg->RInfo[i].SourRID, sizeof(recv_router_msg->RInfo[i].SourRID));
											routing_table.RInfo[index2].updateflag = true;
										}
									}
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

							if (index == -1) // add new entry
							{
								#ifdef _POISONREVERSE
								if (recv_router_msg->RInfo[i].cost >= POISONMETRIC)
								{
									continue;
								}
								#endif
								// update routing_table
								routing_info = makeRoutingInfo(router_info.ID, recv_router_msg->RInfo[i].DestRID, \
									recv_router_msg->RInfo[i].cost + routing_table.RInfo[index2].cost, \
									recv_router_msg->RInfo[i].numOfHops + 1, \
									recv_router_msg->RInfo[i].SourRID);

								retval = class_ptr->enRouter_table(&routing_table, routing_info);
								if (retval == -1)
								{
									#ifdef DEBUG
									cout << "routing table is full ! exit..." << endl;
									closesocket(router_info.sock);
									exit(0);
									#endif
									return -1;
								}
							}
							else // change existing entry
							{
								if (strcmp(recv_router_msg->RInfo[i].SourRID, routing_table.RInfo[index].nextRID) == 0 \
									|| recv_router_msg->RInfo[i].cost + routing_table.RInfo[index2].cost < \
									routing_table.RInfo[index].cost)
								{
									// update routing_table
									routing_table.RInfo[index].cost = recv_router_msg->RInfo[i].cost + \
										routing_table.RInfo[index2].cost;
									memcpy(routing_table.RInfo[index].nextRID, recv_router_msg->RInfo[i].SourRID, sizeof(recv_router_msg->RInfo[i].SourRID));
									routing_table.RInfo[index].numOfHops = recv_router_msg->RInfo[i].numOfHops + 1;
									routing_table.RInfo[index].updateflag = true;
								}
							}
						}
					}
					else if (recv_router_msg->type == 1) // direct - link msg
					{
						index = lookupLinkInfoArray(router_info.link_info, router_info.link_info_count, recv_router_msg->RInfo[0].SourRID);
						if (index == -1)
						{
							// add
							if (router_info.link_info_count >= MaxnumOfRouter)
							{
								cout << "no more room for add!" << endl;
								closesocket(router_info.sock);
								exit(0);
							}

							memcpy(router_info.link_info[router_info.link_info_count].LinkToID, recv_router_msg->RInfo[0].SourRID, sizeof(recv_router_msg->RInfo[0].SourRID));
							router_info.link_info[router_info.link_info_count].cost = recv_router_msg->RInfo[0].cost;
							//memcpy(router_info.link_info[router_info.link_info_count].hostname, inet_ntoa(remote_addr->sin_addr), sizeof(inet_ntoa(remote_addr->sin_addr)));
							router_info.link_info[router_info.link_info_count].portnum = ntohs(remote_addr->sin_port);
							router_info.link_info[router_info.link_info_count].keepalive_time = clock();
							router_info.link_info_count += 1;
							cout << "The link " + string(router_info.ID) + "-" + recv_router_msg->RInfo[0].SourRID + " is set!" << endl;
						}
						else
						{
							// change
							if (router_info.link_info[index].cost == recv_router_msg->RInfo[0].cost)
							{
								router_info.link_info[index].keepalive_time = clock();
							}
							else
							{
								router_info.link_info[index].cost = recv_router_msg->RInfo[0].cost;
								router_info.link_info[index].keepalive_time = clock();
								cout << "The cost of link " + string(router_info.ID) + "-" + recv_router_msg->RInfo[0].SourRID + " is changed!" << endl;
							}

						}
					}
				}

				if (recv_router_msg->type != 1)
				{
					// update keepalive time
					index = lookupLinkInfoArray(router_info.link_info, router_info.link_info_count, recv_router_msg->ID);
					if (index == -1) {
						i = 0;
					}

					assert(index != -1);
					router_info.link_info[index].keepalive_time = clock();
				}
			}	
		}

	
		
		// periodically broadcast advertisement to neighbors
		if (clock() - time_start > broadcast_timeout)
		{	
			if (router_info.link_info_count > 0)
			{
				send_router_msg->type = 0; // default
				send_router_msg->msg_count = 0;
				memcpy(send_router_msg->ID, router_info.ID, sizeof(router_info.ID));
				// broadcast changed entries
				for (i = 0, j = 0; i < routing_table.entry_count; i++)
				{
					//if (routing_table.RInfo[i].updateflag == true)
					if (true) // broadcast all the entries
					{
						send_router_msg->RInfo[j] = routing_table.RInfo[i];
						j++;
						routing_table.RInfo[i].updateflag = false;
						
						#ifdef _POISONREVERSE
						if (routing_table.RInfo[i].cost >= POISONMETRIC)
						{
							retval = deRouter_table(&routing_table, routing_table.RInfo[i].DestRID);
							if (retval == -1) {
								retval = 0;
							}
							assert(retval != -1);
						}
						#endif
					}
				}
				send_router_msg->msg_count = j;

				for (i = 0; i < router_info.link_info_count; i++) // traverse neighbors
				{
					class_ptr->sendRouterMsg(router_info.link_info[i].LinkToID, send_router_msg);
				}
			}
			
			time_start = clock();
		}

		// keep alive process: tell whether neighbors are alive
		for (i = 0; i < router_info.link_info_count; i++)
		{
			if (clock() - router_info.link_info[i].keepalive_time > keepalive_timeout)
			{

				// update routing table
				#ifdef _POISONREVERSE
				index2 = lookupRouter_table(routing_table, router_info.link_info[i].LinkToID);
				assert(index2 != -1);
				routing_table.RInfo[index2].cost = POISONMETRIC; // exist one round
				routing_table.RInfo[index2].updateflag = true;
				
				#else
				retval = deRouter_table(&routing_table, router_info.link_info[i].LinkToID);
				if (retval == -1) {
					retval = 0;
				}
				assert(retval != -1);
				#endif

				// remove the link
				if (deInfoArray(router_info.link_info, &(router_info.link_info_count), router_info.link_info[i].LinkToID) == -1)
				{
					#ifdef DEBUG
					cout << "deInfoArray error! exit..." << endl;
					closesocket(router_info.sock);
					exit(0);
					#endif
					return -1;
				}
			}

		}


		// kill
		if (strcmp(kill_flag, router_info.ID) == 0)
		{
			memset(kill_flag, 0, sizeof(kill_flag)); // clear kill_flag
			closesocket(router_info.sock);
			cout<< "Router " + string(router_info.ID) + " is about to be terminated..."<<endl;
			break;
		}

		// display routing table
		if (strcmp(display_flag, router_info.ID) == 0)
		{
			memset(display_flag, 0, sizeof(display_flag)); // clear display_flag
			cout << "The distance vector for Router " + string(router_info.ID) + " is:" << endl;
			for (i = 0; i < routing_table.entry_count; i++)
			{
				cout << routing_table.RInfo[i].SourRID + string(" ") + string(routing_table.RInfo[i].DestRID)\
					+ string(" ") + to_string(routing_table.RInfo[i].cost) + string(" ") + \
					to_string(routing_table.RInfo[i].numOfHops) + string(" ") + \
					routing_table.RInfo[i].nextRID<< endl;
			}
		}



		// update flag: add or change
		if (strcmp(update_flag.update_router_ID, router_info.ID) == 0)
		{

			index = lookupRouter_table(routing_table, update_flag.update_link_ID);
			index2 = lookupLinkInfoArray(router_info.link_info, router_info.link_info_count, update_flag.update_link_ID);
			if (index2 == -1) {
				i = 0;
			}

			assert(index2 != -1);
			if (index == -1) // add new entry to routing_table
			{
				routing_info = makeRoutingInfo(router_info.ID, update_flag.update_link_ID, \
					router_info.link_info[index2].cost, 1, update_flag.update_link_ID);

				retval = class_ptr->enRouter_table(&routing_table, routing_info);
				if (retval == -1)
				{
					#ifdef DEBUG
					cout << "routing table is full ! exit..." << endl;
					closesocket(router_info.sock);
					exit(0);
					#endif
					return -1;
				}
			}
			else // existing entry
			{
				
				if (strcmp(routing_table.RInfo[index].nextRID, update_flag.update_link_ID) == 0) // origin entry was direct link 
				{
					routing_table.RInfo[index].cost = router_info.link_info[index2].cost; // ??
					routing_table.RInfo[index].updateflag = true;
				}
				else
				{
					if (routing_table.RInfo[index].cost >= router_info.link_info[index2].cost)
					{
						routing_table.RInfo[index].cost = router_info.link_info[index2].cost;
						routing_table.RInfo[index].numOfHops = 1;
						memcpy(routing_table.RInfo[index].nextRID, update_flag.update_link_ID, sizeof(update_flag.update_link_ID));
						routing_table.RInfo[index].updateflag = true;
					}
				}
				
			}

			// send direct msg to the specifical neighbor to update link_info & routing_table (changed entry)
			send_router_msg->type = 1; // direct - link msg
			memcpy(send_router_msg->ID, router_info.ID, sizeof(router_info.ID));
			send_router_msg->RInfo[0] = makeRoutingInfo(router_info.ID, update_flag.update_link_ID, \
				router_info.link_info[index2].cost, 1, update_flag.update_link_ID);
			send_router_msg->msg_count = 1;
			sendRouterMsg(update_flag.update_link_ID, send_router_msg);
			
			
			
			// triggered update
			send_router_msg->type = 0; // routing_table msg
			memcpy(send_router_msg->ID, router_info.ID, sizeof(router_info.ID));
			index = lookupRouter_table(routing_table, update_flag.update_link_ID);
			assert(index != -1);
			send_router_msg->RInfo[0] = routing_table.RInfo[index];
			send_router_msg->msg_count = 1;
			sendRouterMsg(update_flag.update_link_ID, send_router_msg);


			// clear flags
			memset(update_flag.update_router_ID, 0, sizeof(update_flag.update_router_ID)); // clear
			memset(update_flag.update_link_ID, 0, sizeof(update_flag.update_link_ID)); // clear
		}
	}

	delete recv_router_msg;
	delete send_router_msg;
	delete remote_addr;
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

int DVRouting::sendRouterMsg(char d_RID[], routerMsg* rmsg)
{
	int retval, index;
	sockaddr_in remote_addr;
	index = lookupLinkInfoArray(router_info.link_info, router_info.link_info_count, d_RID);
	if (index == -1)
	{
		#ifdef DEBUG
		cout << "send msg error ! exit..." << endl;
		closesocket(router_info.sock);
		exit(0);
		#endif
		return -1;
	}

	remote_addr.sin_family = AF_INET;
	//remote_addr.sin_addr.s_addr = inet_addr(router_info.link_info[index].hostname);
	remote_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	remote_addr.sin_port = htons(router_info.link_info[index].portnum);
	retval = sendto(router_info.sock, (char*)rmsg, sizeof(*rmsg), 0, (sockaddr*)&remote_addr, sizeof(remote_addr));
	if (retval < 0)
	{
		#ifdef DEBUG
		cout << "send msg error ! exit..." << endl;
		closesocket(router_info.sock);
		exit(0);
		#endif
		return -1;
	}
	return 0;
}

int DVRouting::recvRouterMsg(SOCKET sock, routerMsg* rmsg, sockaddr_in* remote_addr)
{
	char recvbuf[1024];
	int len, retval;
	int remote_addr_len = sizeof(*remote_addr);
	len = recvfrom(sock, recvbuf, sizeof(recvbuf), 0, (sockaddr*)remote_addr, &remote_addr_len);
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
		closesocket(info->sock);
		exit(0);
		#endif
		return -1;
	}
	return sock;
}


void DVRouting::frontend(char ID[], unsigned int portnum)
{

	char command[100];
	string cmd;
	string sub_cmd;
	string sub_cmd2;
	string sub_cmd3;
	string sub_cmd4;

	char operate[10];
	int i = 0;
	thread_arg arg;



	Info* tempInfo = new Info;
	memcpy((*tempInfo).ID, ID, sizeof(ID));
	memcpy((*tempInfo).hostname, "127.0.0.1", sizeof("127.0.0.1"));
	(*tempInfo).portnum = portnum;

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


	while (router_info.life_state == false);
	delete tempInfo;

	while (1)
	{

		if (_kbhit())// querying keyboard event
		{
			if (cin.getline(command, sizeof(command)))
			{
				cmd = string(command);
			
				if (cmd.substr(0, cmd.find(" ")) == command_array[add])
				{
					sub_cmd = cmd.substr(cmd.find(" ") + 1, cmd.find(" ", cmd.find(" ") + 1) - cmd.find(" ") - 1); //router
					sub_cmd2 = cmd.substr(cmd.find(" ", cmd.find(" ") + 1) + 1, cmd.find(" ", cmd.find(" ", cmd.find(" ") + 1) + 1) - cmd.find(" ", cmd.find(" ") + 1) - 1); // hostname
					sub_cmd3 = cmd.substr(cmd.find(" ", cmd.find(" ", cmd.find(" ") + 1) + 1) + 1, cmd.find(" ", cmd.find(" ", cmd.find(" ", cmd.find(" ") + 1) + 1) + 1) - cmd.find(" ", cmd.find(" ", cmd.find(" ") + 1) + 1) - 1);// port number
					sub_cmd4 = cmd.substr(cmd.find(" ", cmd.find(" ", cmd.find(" ", cmd.find(" ") + 1) + 1) + 1) + 1);// cost
					memcpy(operate, sub_cmd.c_str(), sizeof(sub_cmd.c_str()));


					if (1)
					{
						// update current router
						i = lookupLinkInfoArray(router_info.link_info, router_info.link_info_count, operate);
						if (i != -1) // already exists
						{
							cout << "The link " + string(router_info.ID) + "-" + operate + " already exists! add is not allowed!" << endl;
							continue;
						}
						if (router_info.link_info_count >= MaxnumOfRouter)
						{
							cout << "no more room for add!" << endl;
							continue;
						}

						memcpy(router_info.link_info[router_info.link_info_count].LinkToID, operate, sizeof(operate));
						router_info.link_info[router_info.link_info_count].cost = atoi(sub_cmd4.c_str());
//						memcpy(router_info.link_info[router_info.link_info_count].hostname, sub_cmd2.c_str(), sub_cmd2.length());
						router_info.link_info[router_info.link_info_count].portnum = atoi(sub_cmd3.c_str());
						router_info.link_info[router_info.link_info_count].keepalive_time = clock();
						router_info.link_info_count += 1;

						// update flag
						memcpy(update_flag.update_router_ID, router_info.ID, sizeof(router_info.ID));
						memcpy(update_flag.update_link_ID, operate, sizeof(operate));

						cout << "The link " + string(router_info.ID) + "-" + operate + " is set!" << endl;
					}



				}
				else if (cmd.substr(0, cmd.find(" ")) == command_array[change])
				{
					sub_cmd = cmd.substr(cmd.find(" ") + 1, cmd.find(" ", cmd.find(" ") + 1) - cmd.find(" ") - 1);
					sub_cmd2 = cmd.substr(cmd.find(" ", cmd.find(" ") + 1) + 1);
					memcpy(operate, sub_cmd.c_str(), sizeof(sub_cmd.c_str()));
					
					if (1)
					{
						// update current router
						i = lookupLinkInfoArray(router_info.link_info, router_info.link_info_count, operate);
						if (i == -1)
						{
							cout << "The link " +  string(router_info.ID) + "-" + operate + " is not set!" << endl;
							continue;
						}

						router_info.link_info[i].cost = atoi(sub_cmd2.c_str());

						// update flag
						memcpy(update_flag.update_router_ID,router_info.ID, sizeof(router_info.ID));
						memcpy(update_flag.update_link_ID, operate, sizeof(operate));

						cout << "The cost of link " + string(router_info.ID) + "-" + operate + " is changed!" <<endl;
					}
				}
				else if (cmd.substr(0, cmd.find(" ")) == command_array[display])
				{
					memcpy(display_flag, router_info.ID, sizeof(router_info.ID));
				}
				else if (cmd.substr(0, cmd.find(" ")) == command_array[kill])
				{
					memcpy(kill_flag, router_info.ID, sizeof(router_info.ID));
					break;
				}
				else
				{
					cout << "undefined command..." << endl; 
				}
			}
		}
	}
}
