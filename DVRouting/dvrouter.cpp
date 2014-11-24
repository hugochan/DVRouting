// dvrouter.cpp

#include <windows.h>
#include <iostream>
#include <string>
#include "DVRouting.h"

using namespace std;

int main(int argc, char* argv[])
{
	if (argc != 3) return -1;

	cout << "#########################################################" << endl;
	cout << "*********************************************************" << endl;
	cout << "******************DVRouting******************************" << endl;
	cout << "*****************************author: Yu Chen & Zhan Shi**" << endl;
	cout << "*********************************************************" << endl;
	cout << "#########################################################" << endl;

	WSAData wsa;
	WSAStartup(0x101, &wsa);
	DVRouting dvr;
	dvr.frontend(argv[1], atoi(argv[2]));

	return 0;
}
