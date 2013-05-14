/*
 *  Implement thread safe, non-blocking access to multiple modbus devices
 *
 * Copyright (c) 2013 by James Bremner
 * All rights reserved.
 *
 * Use license: Modified from standard BSD license.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation, advertising
 * materials, Web server pages, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by James Bremner. The name "James Bremner" may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "StdAfx.h"
#include "cFarmodbus.h"
#include "Serial.h"

namespace raven {
	namespace farmodbus {

		// keep track of indices( handles )
		// so no duplicates are created
		int cPort::myLastID = 0;
		int cStation::myLastHandle = 0;
		int cFarmodbus::myLastID = 0;

		// The active configuration
		cFarmodbusConfig theConfig;

		cPort::cPort( cSerial& serial )
			: myFlagTCP( false )
		{
			myID = myLastID++;
			mySerial = &serial;
		}
		cPort::cPort( SOCKET s )
			: myFlagTCP( true )
		{
			myID = myLastID++;
			mySocket = s;
		}
		bool cPort::IsOpen()
		{
			if( myFlagTCP ) {
				// assume socket is always open
				return true;
			} else {
				return mySerial->IsOpened();
			}
		}
		/**

  Send data to the port

  @param[in] buffer  pointer to data to be written
  @param[in] size    number of bytes to write

  @return 0 if error

  */
		int cPort::SendData( const unsigned char *msg, int length )
		{
			if( myFlagTCP ) {
				int iResult = send( mySocket,
					(const char* )msg, length, 0 );
				if (iResult == SOCKET_ERROR) {
					return 0;
				}
				return length;
			} else {
				return mySerial->SendData( msg, length );
			}
		}
		/**

	Blocking wait for an amount of data to be ready

  @param[in] len number of bytes required
  @param[in] msec number of milliseconds to wait

  @return 1 if data ready, 0 if timeout

  */
		int cPort::WaitForData( int len, int msec )
		{
			if( myFlagTCP ) {
				int timeout = 0;
				while( ! TCPReadDataWaiting() )
				{
					if( timeout++ > msec ) {
						return 0;
					}
					Sleep(1);
				}
				// TODO  check for length of data waiting
				return 1;

			} else {
				return mySerial->WaitForData( len, msec );
			}

		}
		/**

  True if data available to be read

*/
int cPort::TCPReadDataWaiting( void )
{
		fd_set fds;
		FD_ZERO( &fds );
		FD_SET( mySocket, &fds );
		TIMEVAL timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;
		return ( select( 0, &fds, 0, 0, &timeout ) == 1 );

}
/**

  Read data from port

  @param[in] buffer pointer to location to store data
  @param[in] limit  maximum number of bytes to read

  @return  0 if error, number of bytes read otherwise

*/
		int cPort::ReadData( void *buffer, int limit )
		{
			if( myFlagTCP ) {
				return recv( mySocket, (char*)buffer, limit, 0 );
			} else {
				return mySerial->ReadData( buffer, limit );
			}

		}
		cStation::cStation( 
			int address,
			cPort& port )
			: myAddress( address )
			, myPort( port )
			, myFirstReg( -1 )
			, myError( not_ready )
			, myWriteError( OK )
		{
			myHandle  = myLastHandle++;
		}

		error cStation::Query( 
			unsigned short& value,
			int reg )
		{

			if( 0 > reg || reg > 255 )
				return bad_register_address;

			// prevent other threads from accessing the cached values
			boost::mutex::scoped_lock lock( myMutex );

			// check if we need to extend the registered polled
			if( myFirstReg == -1 ) {
				//first time called
				myFirstReg = reg;
				myCount = 1;
				myError = not_ready;
			} else {
				if( reg < myFirstReg ) {
					myFirstReg = reg;
					myError = not_ready;
				} else if ( reg - myFirstReg + 1 > myCount ) {
					myCount = reg - myFirstReg + 1;
					myError = not_ready;
				}
				
			}


			// check if we have a good value from last poll
			if( myError != OK )
				return myError;

			// value saved from last poll
			value = myValue[ reg ];

			return OK;
		}
		error cStation::Query( 
			unsigned short* value,
			int first_reg,
			int reg_count )
		{
			// prevent other threads from accessing the cached values
			boost::mutex::scoped_lock lock( myMutex );

			// check if we need to extend the registers polled
			if( myFirstReg == -1 ) {
				//first time called
				myFirstReg = first_reg;
				myCount = reg_count;
				myError = not_ready;
			} else {
				int old_first = myFirstReg;
				int old_last  = myFirstReg + myCount - 1;
				int new_first = first_reg;
				int new_last  = first_reg + reg_count - 1;
				if( new_first < old_first ) {

				} else {
					new_first = old_first;
				}
				if( new_last > old_last ) {

				} else {
					new_last = old_last;
				}
				int new_count = new_last - new_first + 1;
				if( new_first < old_first ) {
					myFirstReg = new_first;
					myError = not_ready;
				}
				if( new_count > myCount ) {
					myCount = new_count;
					myError = not_ready;
				}
			}

			// check if we have a good value from last poll
			if( myError != OK )
				return myError;

			for( int k = 0; k < myCount; k++ ) {
				*value++ = myValue[myFirstReg+k];
			}
			return OK;
		}

		void cStation::Poll()
		{
			raven::set::cRunWatch runwatch("cStation::Poll");
			if( ! myPort.IsOpen() ) {
				myError = port_not_open;
				return;
			}

			unsigned char buf[1000];

			// assemble the modbus read command
			int msglen;
			buf[0] = myAddress;
			buf[1] = theConfig.ModbusReadCommand;
			buf[2] = 0;				// max register 255
			buf[3] = myFirstReg;
			buf[4] = 0;
			buf[5] = myCount;
			unsigned short crc = CyclicalRedundancyCheck( buf,6);
			buf[6] = crc >> 8;
			buf[7] = 0xFF & crc;
			msglen = 8;

			// send the query
			myPort.SendData( 
				(const unsigned char *)buf,
				msglen );

			// wait for reply

			/** Wait for data does a 1000Hz poll
			To prevent it using excessive CPU
			do an initial 50ms sleep
			*/
			Sleep(50);
			if( !myPort.WaitForData(
				7,
				6000 ) ) {
					myError = timed_out;
					return;
			}

			// read the reply
			memset(buf,'\0',1000);
			msglen = myPort.ReadData(
				buf,
				999);

			// prevent other threads from accessing the cached values
			boost::mutex::scoped_lock lock( myMutex );


			// decode reply

			/* The values are returned as 
				16 bit integers 
				in 2's complement and
				network byte order ( MSB first )  */

			int iv;
			union  {
				short s;
				unsigned char c[2];
			} v;
			for( int k = 0; k < myCount; k++ ) {
				v.c[0] = buf[4+k*2];
				v.c[1] = buf[3+k*2];

				iv = v.s & 0x7FFF;
				if( v.s & 0x8000 )
					iv -= 32767;
				myValue[k+myFirstReg] = iv;
			}

			//printf("Poll OK Station %d, FirstReg %d, Count %d\n",
			//	myHandle, myFirstReg, myCount );

			myError = OK;

		}

		error cStation::Write( cWriteWaiting& W )
		{
			if( ! myPort.IsOpen() ) {
				myWriteError = port_not_open;
				return port_not_open;
			}
			if( W.getCount() != 1 ) {
				myWriteError = NYI;
				return NYI;
			}

			unsigned char buf[1000];

			// assemble the modbus write command
			int msglen;
			buf[0] = myAddress;
			buf[1] = 6;				// single register read command
			buf[2] = 0;				// max register 255
			buf[3] = W.getFirstReg();
			buf[4] = 0;
			buf[5] = (unsigned char)W.getValue();
			unsigned short crc = CyclicalRedundancyCheck( buf,6);
			buf[6] = crc >> 8;
			buf[7] = 0xFF & crc;
			msglen = 8;

			// send the query
			myPort.SendData( 
				(const unsigned char *)buf,
				msglen );

			// wait for reply

			/** Wait for data does a 1000Hz poll
			To prevent it using excessive CPU
			do an initial 50ms sleep
			*/
			Sleep(50);
			if( !myPort.WaitForData(
				7,
				6000 ) ) {
					myWriteError = timed_out;
					return timed_out;
			}

			// read the reply
			memset(buf,'\0',1000);
			msglen = myPort.ReadData(
				buf,
				999);

			if( buf[1] == 6 )
				return OK;
			if( buf[1] == 86 ) {
				myWriteError = device_exception;
				return device_exception;
			}
			myWriteError = device_error;
			return device_error;

		}


		cFarmodbus::cFarmodbus(void)
		{
			// ensure that the app only creates one of these
			myLastID++;
			if( ! IsSingleton() )
				return;

			// start polling thread
			boost::thread* pThread = new boost::thread(
				boost::bind(
				&cFarmodbus::Poll,		// member function
				this ) );	
		}
		void cFarmodbus::Set( cFarmodbusConfig& config )
		{
			theConfig = config;
		}

		void cFarmodbusConfig::Set( const char* system_name )
		{
			if( std::string( system_name ) == std::string("T3000") ) {

				// Change configuration defaults to T3000 settings
				ModbusReadCommand = 3;
			}
		}

		/**

		The polling thread method.

		This method never returns.
		It should run in its own thread, and should be the ONLY
		code that actually does read/writes on the communication ports

		First it checks the write queue, and performs any write reuests.
		Second it reads all the registers that the application code has requested a read from
		Third it sleeps for 1 second.
		Repeats for ever

		*/
		void cFarmodbus::Poll()
		{
			// for ever
			for( ; ; ) {

				// loop over writes in queue
				while( ! myWriteQueue.empty() ) {

					// pop first write from queue
					cWriteWaiting W = PopWriteFromQueue();
 
					// do write
					myStation[ W.getStation()]->Write( W );

				}

				// loop over stations
				foreach( cStation* station, myStation ) {

					// poll the station
					station->Poll();
				}

				// allow 1 second to elapse between polls
				Sleep(1000);
			}
		}

		/**

		Pop first write from queue

		@return copy of first write that was waiting on the queue

		*/
		cWriteWaiting cFarmodbus::PopWriteFromQueue()
		{
			boost::mutex::scoped_lock lock( myWriteQueueMutex );
			cWriteWaiting W = myWriteQueue.front();
			myWriteQueue.pop();
			return W;
		}

		error cFarmodbus::Add( port_handle_t& handle, ::raven::cSerial& port )
		{ 
			myPort.push_back( cPort( port ) );
			handle = (port_handle_t) myPort.size() - 1;
			return OK;
		}

		error cFarmodbus::Add( port_handle_t& handle, SOCKET port )
		{
			myPort.push_back( cPort( port ) );
			handle = (port_handle_t) myPort.size() - 1;
			return OK;

		}

error 
cFarmodbus::Add(
		station_handle_t& station_handle,
		port_handle_t port_handle,
		int address  )
{
	if( 0 > port_handle || port_handle >= (int) myPort.size() )
		return bad_port_handle;

	/* Construct a new station and store a pointer to it

	The stations will exist for the lifetime of the program

	( if this causes a memory leak, then we will need to add
	  a method to remove the station )

	  Storing a pointer is neccessary because the mutex protecting
	  the cached values makes the station class non-copyable
    */
	myStation.push_back( new cStation( address, 
									myPort[port_handle] ) );

	station_handle = (port_handle_t) myStation.size() - 1;

	return OK;
}


error cFarmodbus::Query(
		unsigned short& value,
		station_handle_t station,
		int reg )
{ 
	raven::set::cRunWatch runwatch("cFarmodbus::Query-1");
	// firewall
	if( ! IsSingleton() )
		return not_singleton;
	if( 0 > station || station >= (int) myStation.size() )
		return bad_station_handle;
	if( 0 > reg || reg > 255 )
		return bad_register_address;

	return myStation[station]->Query( value, reg );

}
error cFarmodbus::Query(
	unsigned short* value,
	station_handle_t station,
	int first_reg,
	int reg_count )
{
	raven::set::cRunWatch runwatch("cFarmodbus::Query-2");
		// firewall
	if( ! IsSingleton() )
		return not_singleton;
	if( 0 > station || station >= (int) myStation.size() )
		return bad_station_handle;
	if( 0 > first_reg || first_reg > 255 )
		return bad_register_address;
	if( first_reg + reg_count - 1 > 255 )
		return bad_register_address;

	return myStation[station]->Query( value, first_reg, reg_count );
}


error cFarmodbus::Write(
			station_handle_t station,
		int first_reg,
		int reg_count,
		unsigned short * value )
 { 
	 		// firewall
	if( ! IsSingleton() )
		return not_singleton;
	if( 0 > station || station >= (int) myStation.size() )
		return bad_station_handle;
	if( 0 > first_reg || first_reg > 255 )
		return bad_register_address;
	if( first_reg + reg_count - 1 > 255 )
		return bad_register_address;

	// Add the write to the end of the write queue
	// This will be executed in the polling thread
	// next time it wakes up

	boost::mutex::scoped_lock lock( myWriteQueueMutex );
	myWriteQueue.push( cWriteWaiting( station, first_reg, reg_count, value ) );

	// return immediatly, with error return from PREVIOUS poll
	return myStation[ station ]->getWriteError(); 
}

error cFarmodbus::Write(
		station_handle_t station,
		int reg,
		unsigned short value )
 {
	 // Convert this to a block write of count 1
	 return Write( station, reg, 1, &value );
}
cWriteWaiting::cWriteWaiting(
		station_handle_t station,
		int first_reg,
		int reg_count,
		unsigned short* value )
		: myStation( station )
		, myFirstReg( first_reg )
		, myCount( reg_count )
{
	// Copy the values to be written into our own attribute

	/* We use an STL vector to store the values.
	This is convenient and safe, requiring the minimum of code
	and almost nothing can go wrong!
	It is not especially efficient.  This class is going to be copied,
	so the vector will be copied also.  If huge numbers of large block
	writes cause a problem, then it might be neccessary to allocate a buffer
	and use a pointer to the buffer, saving the vector copies, but requiring
	careful coding to prevent memory leaks.
	*/
	for( int k = 0; k< myCount; k++ ) {
		myValue.push_back( *value++ );
	}
}
void cWriteWaiting::Print()
{
	printf("Station %d Register %d to %d ( ",
		(int)myStation,myFirstReg,myFirstReg+myCount-1);
	for( int k = 0; k < myCount; k++ ) {
		printf("%d ",myValue[k]);
	}
	printf(" )\n");
}


unsigned short cStation::CyclicalRedundancyCheck(
	unsigned char * msg, int len )
{
	/* Table of CRC values for high–order byte */
	static unsigned char auchCRCHi[] = {
		0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
		0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
		0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
		0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
		0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
		0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
		0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
		0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
		0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
		0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
		0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
		0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
		0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
		0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
		0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
		0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
		0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
		0x40
	} ;
	/* Table of CRC values for low–order byte */
	static unsigned char auchCRCLo[] = {
		0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4,
		0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
		0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD,
		0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
		0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7,
		0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
		0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE,
		0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
		0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2,
		0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
		0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
		0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
		0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91,
		0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
		0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88,
		0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
		0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80,
		0x40
	} ;
	unsigned char uchCRCHi = 0xFF ; /* high byte of CRC initialized */
	unsigned char uchCRCLo = 0xFF ; /* low byte of CRC initialized */
	unsigned uIndex ; /* will index into CRC lookup table */
	while (len--) /* pass through message buffer */
	{
		uIndex = uchCRCHi ^ *msg++ ; /* calculate the CRC */
		uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex] ;
		uchCRCLo = auchCRCLo[uIndex] ;
	}
	return (uchCRCHi << 8 | uchCRCLo) ;
}
	}
}