// farmodbuscpp.cpp : Modbus farm demonstration and unit tests
//

#include "stdafx.h"
#include "cFarmodbus.h"
#include "Serial.h"

	// construct the modbus farm
	raven::farmodbus::cFarmodbus theModbusFarm;


void TestStation()
{
	// construct a test station
	// ( production code should NOT do this! )
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

void ReaderThread()
{
	// Give each thread its own register to read
	static int next_reg_to_read = 0;		// This is NOT thread specific
	next_reg_to_read++;
	static boost::thread_specific_ptr< int > reg_to_read;
	if( ! reg_to_read.get() ) {
		// first time called by this thread
		// construct test element to be used in all subsequent calls from this thread
		reg_to_read.reset( new int);
	}
	*reg_to_read = next_reg_to_read;

	unsigned short value;
	for( int k = 0; k < 100; k++ ) {

		// read register #5
		raven::farmodbus::error error = theModbusFarm.Query( value, 0, *reg_to_read );
		if( error != raven::farmodbus::OK ) {
			printf("Modbus read error #%d register %d\n", 
				error,*reg_to_read );
		} else {
			printf("Successful read, register %d = %d\n",
				*reg_to_read,value);
		}

		Sleep(100);
	}

}

void WriterThread()
{
	// Give each thread its own register to read
	static int next_reg_to_read = 0;		// This is NOT thread specific
	next_reg_to_read += 10;
	static boost::thread_specific_ptr< int > reg_to_read;
	if( ! reg_to_read.get() ) {
		// first time called by this thread
		// construct test element to be used in all subsequent calls from this thread
		reg_to_read.reset( new int);
	}
	*reg_to_read = next_reg_to_read;

	unsigned short valuebuf[5];
	for( int k = 0; k < 100; k++ ) {
		valuebuf[0] = 10;
		valuebuf[1] = 11;
		valuebuf[2] = 12;
		valuebuf[3] = 13;
		valuebuf[4] = 14;
		raven::farmodbus::error error = theModbusFarm.Write( 0, *reg_to_read, 5, valuebuf );
		if( error != raven::farmodbus::OK ) {
			printf("Modbus write error #%d\n", error );
			// We are expecting NYIs - abort on anything else
			if( error != raven::farmodbus::NYI ) {
				printf("UNEXPECTED ERROR\n");
				exit(1);
			}
		} else {
			printf("Successful write, AFAIK\n");
		}
		Sleep(100);
	}


}


void TestThreadSafety()
{
	printf("Thread safety test begin ...\n");

	// Do an initial read and allow the polling to settle down
	unsigned short value[5];
	theModbusFarm.Query( value, 0, 1, 5 );
	Sleep(2000);
	printf("Polling started\n");

	boost::thread_group g;

	g.create_thread( &ReaderThread );
	g.create_thread( &ReaderThread );
	g.create_thread( &WriterThread );
	g.create_thread( &WriterThread );

	g.join_all();

	printf("Thread safety test end \n");

}

int _tmain(int argc, _TCHAR* argv[])
{

	raven::set::cRunWatch::Start();

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

	TestThreadSafety();

	// Do 10 write/reads at 1 Hz
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

		// test write
		valuebuf[0] = 10;
		valuebuf[1] = 11;
		valuebuf[2] = 12;
		valuebuf[3] = 13;
		valuebuf[4] = 14;
		error = theModbusFarm.Write( station, 10, 5, valuebuf );
		if( error != raven::farmodbus::OK ) {
			printf("Modbus write error #%d\n", error );
		} else {
			printf("Successful write, AFAIK\n");
		}

		Sleep(1000);
	}

	raven::farmodbus::cFarmodbus ModbusFarm2;
	if( ModbusFarm2.Query( value, 1, 1 ) != raven::farmodbus::not_singleton ) {
		printf("ERROR: Failed to enforce singleton\n");
		return 1;
	}

	raven::set::cRunWatch::Report();

	// station unit tests
	TestStation();




	return 0;
}

