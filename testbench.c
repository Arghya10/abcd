#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// AHIR release utilities
//
#include <pthreadUtils.h>
#include <Pipes.h>
#include <pipeHandler.h>

/*
// These will wait.
#ifndef SW
#include "vhdlCStubs.h"
#else
#include "aa_c_model.h"
#endif
*/

// includes the header.
#define PACKET_LENGTH_IN_WORDS  64

typedef struct _TbConfig {

	// if 1, input port 1 will be fed by data
	//   else input port 1 will be unused.
	int input_port_1_active;

	// if random dest is set, then
	// input port 1 can send data to either
	// output port 1 or output port 2.
	int input_port_1_random_dest_flag;

	// if random_dest_flag is 0, then
	// input port 1 writes only to
	// this destination port (provided
	// it is either 1 or 2).
	int input_port_1_destination_port;

	int input_port_2_active;
	// see comments above.
	int input_port_2_random_dest_flag;
	int input_port_2_destination_port;

} TbConfig;
TbConfig tb_config;

void input_port_core(int port_id)
{
	uint32_t send_buffer[PACKET_LENGTH_IN_WORDS];	

	int i;
	for(i = 0; i < PACKET_LENGTH_IN_WORDS; i++)//i=1
	{
		send_buffer[i] = i;
	}

	// odd sequence id means from port 1 even sequence id means from port 2.
	uint8_t seq_id = (port_id == 1) ? 1 : 0;
	while(1)
	{
		int dest_port =  -1;//??
		if(port_id == 1)
		{
			dest_port = 	
				(tb_config.input_port_1_random_dest_flag ? ((rand() & 0x1)+1) :  
					tb_config.input_port_1_destination_port);
		}
		else if(port_id == 2)
		{
			dest_port = 	
				(tb_config.input_port_2_random_dest_flag ? ((rand() & 0x1)+1) :  
					tb_config.input_port_2_destination_port);
		}

		if((dest_port == 1) || (dest_port == 2))
		{
			send_buffer[0] = (dest_port << 24) | (64 << 8) | seq_id;//64
			if(port_id == 1)
				write_uint32_n ("in_data_1", send_buffer, PACKET_LENGTH_IN_WORDS);
			else
				write_uint32_n ("in_data_2", send_buffer, PACKET_LENGTH_IN_WORDS);

			// increment by 2
			seq_id += 2;
		}
	}
}

void input_port_1_sender ()
{
	input_port_core(1);
}
DEFINE_THREAD(input_port_1_sender);//??

void input_port_2_sender ()
{
	input_port_core(2);
}
DEFINE_THREAD(input_port_2_sender);//??

void output_port_core(int port_id)
{
	while(1)
	{
		uint32_t packet[PACKET_LENGTH_IN_WORDS];

		if(port_id == 1)
			read_uint32_n ("out_data_1", packet, PACKET_LENGTH_IN_WORDS);
		else
			read_uint32_n ("out_data_2", packet, PACKET_LENGTH_IN_WORDS);
		

		fprintf(stderr,"\nRx at output port %d from input port %d\n", 
				port_id, (packet[0] & 0x1) + 1);

		// check integrity of the packet.
		//
		// check the destination?
		//
		int I; 
		int err = 0;
		for(I=1; I < PACKET_LENGTH_IN_WORDS; I++)
		{
			if (packet[I] != I)
			{
				fprintf(stderr,"\nError: packet[%d]=%d, expected %d.\n",
					I, packet[I], I);
				err = 1;
			}
		}

		if(err)
			break;
	}

}

void output_port_1_receiver ()
{
	output_port_core(1);
}
DEFINE_THREAD(output_port_1_receiver);

void output_port_2_receiver ()
{
	output_port_core(2);
}
DEFINE_THREAD(output_port_2_receiver);

int main(int argc, char* argv[])
{

#ifdef SW
	init_pipe_handler();
	//start_daemons (stdout,0);
#endif
	// test configuration setup.
	//  both input ports active, send
	//  randomly to output ports.
	tb_config.input_port_1_active = 1;
	tb_config.input_port_1_random_dest_flag = 1;
	tb_config.input_port_1_destination_port = -1;

	tb_config.input_port_2_active = 1;
	tb_config.input_port_2_random_dest_flag = 1;
	tb_config.input_port_2_destination_port = -1;

	// 
	// start the receivers
	// 
	PTHREAD_DECL(output_port_1_receiver);
	PTHREAD_CREATE(output_port_1_receiver);

	PTHREAD_DECL(output_port_2_receiver);
	PTHREAD_CREATE(output_port_2_receiver);

	// start the senders.
	PTHREAD_DECL(input_port_1_sender);
	PTHREAD_CREATE(input_port_1_sender);

	PTHREAD_DECL(input_port_2_sender);
	PTHREAD_CREATE(input_port_2_sender);
	

	// wait on the two output threads
	PTHREAD_JOIN(output_port_1_receiver);
	PTHREAD_JOIN(output_port_2_receiver);

	return(0);
}
