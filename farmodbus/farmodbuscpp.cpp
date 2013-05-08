// farmodbuscpp.cpp : Basic farmodbus sanity test
//

#include "stdafx.h"
#include "cFarmodbus.h"
#include "Serial.h"

int _tmain(int argc, _TCHAR* argv[])
{
	// construct the modbus farm
	raven::farmodbus::cFarmodbus theModbusFarm;
	raven::farmodbus::cFarmodbus::error error;

	// construct the COM port
	raven::cSerial theCOM;

	// tell the modbus farm about the port
	raven::farmodbus::port_handle_t port;
	error = theModbusFarm.Add( port, theCOM );
	if( error != raven::farmodbus::cFarmodbus::OK ) {
		printf("Port add error #%d\n",error);
		return 1;
	}

	// tell the modbus farm about the modbus device with address 1 on the port
	raven::farmodbus::station_handle_t station;
	error = theModbusFarm.Add( station, port, 1 );
	if( error != raven::farmodbus::cFarmodbus::OK ) {
		printf("Station add error #%d\n",error);
		return 1;
	}

	// open the port
	theCOM.Open( "COM4" );

	// read register #5
	unsigned short value;
	error = theModbusFarm.Query( value, station, 5 );
	if( error != raven::farmodbus::cFarmodbus::OK ) {
		printf("Modbus read error #%d\n", error );
	} else {
		printf("Successful read, register 5 = %d\n",value);
	}
	return 0;
}

