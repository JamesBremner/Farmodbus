// farmodbus_testTCP.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "stdafx.h"
#include "cFarmodbus.h"

	// construct the modbus farm
	raven::farmodbus::cFarmodbus theModbusFarm;


int _tmain(int argc, _TCHAR* argv[])
{
	raven::set::cRunWatch::Start();

	//-------------------------------
	// Initialize socket library
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD( 2, 2 );
	WSAStartup( wVersionRequested, &wsaData );

	//--------------------------------
	// Declare and initialize variables.
	char* ip = "127.0.0.1";
	const int myPort = 27016;
	char port[10];
	sprintf_s(port,9,"%d",myPort);
	struct addrinfo aiHints;
	struct addrinfo *aiList = NULL;
	int retVal;

	//--------------------------------
	// Setup the hints address info structure
	// which is passed to the getaddrinfo() function
	memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = AF_INET;
	aiHints.ai_socktype = SOCK_STREAM;
	aiHints.ai_protocol = IPPROTO_TCP;

	//--------------------------------
	// Call getaddrinfo(). If the call succeeds,
	// the aiList variable will hold a linked list
	// of addrinfo structures containing response
	// information about the host
	if ((retVal = getaddrinfo(ip, port, &aiHints, &aiList)) != 0) {
		printf( "getaddrinfo() failed.\n");
		return 1;
	}

	//--------------------------------
	// Connect to socket
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo *ptr;
	ptr = aiList;
	ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, 
		ptr->ai_protocol);
	if (ConnectSocket == INVALID_SOCKET) {
		//printf("Error at socket(): %ld\n", WSAGetLastError());
		printf( "Invalid scocket");
		return 1;
	}
	if( connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen) ) {
		//printf("Error at socket(): %ld\n", WSAGetLastError());
		static char msg[200];
		sprintf_s(msg,199,"Error at socket(): %d",WSAGetLastError());
		printf("%s\n",msg);
		return 1;
	}

	// tell the modbus farm about the port
	raven::farmodbus::port_handle_t hport;
	raven::farmodbus::error error;
	error = theModbusFarm.Add( hport, ConnectSocket );
	if( error != raven::farmodbus::OK ) {
		printf("Port add error #%d\n",error);
		return 1;
	}

	// tell the modbus farm about the modbus device with address 1 on the port
	raven::farmodbus::station_handle_t station;
	error = theModbusFarm.Add( station, hport, 1 );
	if( error != raven::farmodbus::OK ) {
		printf("Station add error #%d\n",error);
		return 1;
	}

	// start and allow some time for the polling to settle down
	unsigned short value;
	error = theModbusFarm.Query( value, station, 1 );
	printf("Starting polling...");
	Sleep(2000);

	// read register
	error = theModbusFarm.Query( value, station, 1 );
	if( error != raven::farmodbus::OK ) {
		printf("Modbus read error #%d\n", error );
	} else {
		printf("Successful read, register 1 = %d\n",value);
	}

	Sleep(10000);

	raven::set::cRunWatch::Report();

	return 0;
}

