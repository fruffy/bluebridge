include "shared.thrift"

service RemoteMemTest {
	/**
	 * Test network connectivity with a ping
	 */
	void ping(),

	/**
	 * Request memory of a certain size
	 */
	binary allocate_mem(1:i32 size) throws (1:shared.CallException ouch),

	/**
	 * Ask the server to read memory from a pointer
	 */
	void read_mem(1:binary pointer) throws (1:shared.CallException ouch),

	/**
	 * Request the server to write a message to a pointer
	 */
	void write_mem(1:binary pointer, 2:string message) throws (1:shared.CallException ouch),

	/**
	 * Request the server to free memory
	 * Throws an exception if the server 
	 */
	void free_mem(1:binary pointer) throws (1:shared.CallException ouch),
}