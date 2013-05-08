/*
 *  Class interface for thread safe, non-blocking access to multiple modbus devices
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

#pragma once

namespace raven {
	class cSerial;
	namespace farmodbus {

		// the port and station handles
	typedef int port_handle_t;
	typedef int station_handle_t;

	/**

	Error return values

	*/
	enum error {
		OK,
		NYI,
		bad_port_handle,
		bad_station_handle,
		port_not_open,
		timed_out,
		bad_register_address,
		not_ready,
	};


	// port details
class cPort {

	int myID;
	static int myLastID;
	cSerial* mySerial;

public:
	cPort( cSerial& serial );

	int getID() { return myID; }
	cSerial* getSerial() { return mySerial; }
};

// station details
class cStation {

public:
	cStation( int address, raven::cSerial * serial );

	error Query( 
		unsigned short& value,
		int reg );
	error Query( 
		unsigned short* value,
		int first_reg,
		int reg_count );

	void Poll();

	int getHandle() { return myHandle; }
	int getAddress() { return myAddress; }

	bool CheckPolledRegisters(
		int expected_first,
		int expected_count )
	{
		return ( expected_first == myFirstReg &&
			expected_count == myCount );
	}

private:

	int myHandle;
	static int myLastHandle;
	int myAddress;
	int myFirstReg;
	int myCount;
	error myError;
	raven::cSerial * mySerial;
	unsigned short myValue[255];
	boost::mutex myMutex;

	unsigned short CyclicalRedundancyCheck(
		unsigned char * msg, int len );


};

/**

 A modbus farm

 Provide thread safe, non-blocking access to multiple modbus devices

*/
class cFarmodbus
{
public:

	/**

	Construct modbus farm

	The constructor starts polling.  Until you add some stations
	the polling does nothing, bit is always going on and will do more and more work
	as stations are added.

	*/

	cFarmodbus(void);

	/**

	Add COM port

	@param[out] handle  Use when defining which port a modbus station is connected through
	@param[in]  port    The COM port through which modbus stations can be connected

	@return error

	TODO: Add TCP port ( socket )

	Once a port is added to the modbus farm with some stations
	then polling will start and continue on the port.  NOTHING ELSE
	SHOULD access the port once this begins.

	*/
	error Add( port_handle_t& handle, cSerial& port );

	/**

	Add modbus station

	@param[out] handle Use when requesting access to this station
	@param[in] port_handle Handle to port through which this station is connected
	@param[in] address modbus device address

	@return error

	*/
	error Add( 
		station_handle_t& handle,
		port_handle_t port_handle,
		int address  );

	/**

	Read register

	@param[out] value read from register
	@param[in] station handle
	@param[in] reg register offset to read

	@return error

	The first time that you read a particular register
	the value will not yet have been polled so this will
	return not_ready.
	The register will have been added to the polling list
	and once it has been polled at least once successfully
	this will then work.

	*/
	error Query( 
		unsigned short& value,
		station_handle_t station,
		int reg );
	/**

	Read block of registers

	@param[out] value pointer to buffer long enough to hold values of all registers in block
	@param[in]  station handle
	@param[in] first_reg first register offset to read
	@param[in] reg_count number of registers to read

	@return error
	*/
	error Query(
		unsigned short* value,
		station_handle_t station,
		int first_reg,
		int reg_count );
	/**

	Write value to register

	@param[in] station handle
	@param[in] reg register to write
	@param[in] value to write

	@return error

	*/

	error Write(
		station_handle_t station,
		int reg,
		unsigned short value );
	/**

	Write values to block of registers

	@param[in] station handle
	@param[in] first_reg first register offset to write to
	@param[in] reg_count number of registers to write
	@param[in] value pointer to buffer of values to write

	@return error

	*/
	error WriteBlock(
		station_handle_t station,
		int first_reg,
		int reg_count,
		unsigned short * value );


private:
	std::vector< cPort > myPort;
	std::vector< cStation * > myStation;

	void Poll();
};
	}
}