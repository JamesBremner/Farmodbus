// farmodbuscpp.cpp : Basic farmodbus sanity test
//

#include "stdafx.h"
#include "cFarmodbus.h"
#include "Serial.h"

void TestStation()
{
	raven::farmodbus::cStation station( 1, 0 );

	unsigned short v[255];
	station.Query( v, 1, 2 );
	station.Query( v, 9, 2 );
	if( ! station.CheckPolledRegisters( 1, 10 ) ) {
		printf("Failed TestStation #1\n");
		exit(1);
	}
	station.Query( v, 3, 4 );
	if( ! station.CheckPolledRegisters( 1,10 ) ) {
		printf("Failed TestStation #2\n");
		exit(1);
	}
	station.Query( v, 8, 7 );
	if( ! station.CheckPolledRegisters( 1,14 ) ) {
		printf("Failed TestStation #3\n");
		exit(1);
	}

	raven::farmodbus::cStation station2( 1, 0 );
	station2.Query( v, 8, 7 );
	if( ! station2.CheckPolledRegisters( 8,7 ) ) {
		printf("Failed TestStation #4\n");
		exit(1);
	}
	station2.Query( v, 3, 2 );
	if( ! station2.CheckPolledRegisters( 3,12 ) ) {
		printf("Failed TestStation #5\n");
		exit(1);
	}

}
int _tmain(int argc, _TCHAR* argv[])
{
	// station unit tests
	TestStation();

	// construct the modbus farm
	raven::farmodbus::cFarmodbus theModbusFarm;
	raven::farmodbus::error error;

	// construct the COM port
	raven::cSerial theCOM;

	// tell the modbus farm about the port
	raven::farmodbus::port_handle_t port;
	error = theModbusFarm.Add( port, theCOM );
	if( error != raven::farmodbus::OK ) {
		printf("Port add error #%d\n",error);
		return 1;
	}

	// tell the modbus farm about the modbus device with address 1 on the port
	raven::farmodbus::station_handle_t station;
	error = theModbusFarm.Add( station, port, 1 );
	if( error != raven::farmodbus::OK ) {
		printf("Station add error #%d\n",error);
		return 1;
	}

	// open the port
	theCOM.Open( "COM4" );

	// Do 10 reads at 1 Hz
	unsigned short value;
	for( int k = 0; k < 10; k++ ) {
		// read register #5
		error = theModbusFarm.Query( value, station, 1 );
		if( error != raven::farmodbus::OK ) {
			printf("Modbus read error #%d\n", error );
		} else {
			printf("Successful read, register 1 = %d\n",value);
		}
		error = theModbusFarm.Query( value, station, 5 );
		if( error != raven::farmodbus::OK ) {
			printf("Modbus read error #%d\n", error );
		} else {
			printf("Successful read, register 5 = %d\n",value);
		}
		unsigned short valuebuf[5];
		error = theModbusFarm.Query( valuebuf, station, 1, 5 );
		if( error != raven::farmodbus::OK ) {
			printf("Modbus read error #%d\n", error );
		} else {
			printf("Successful read, registers = ");
			for( int k = 0; k < 5; k++ ) {
				printf("%d ",valuebuf[k]);
			}
			printf("\n");
		}
		Sleep(1000);
	}

	raven::farmodbus::cFarmodbus ModbusFarm2;
	if( ModbusFarm2.Query( value, 1, 1 ) != raven::farmodbus::not_singleton ) {
		printf("ERROR: Failed to enforce singleton\n");
		return 1;
	}

	return 0;
}

