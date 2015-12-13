#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h> // for open flags
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_CLIENTS 6
#define DEFAULT_PORT "6900"
#define MAX_DATA_SIZE 512
#define MODE "octec"
#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERROR 5

#pragma pack(push, 1)
typedef struct {
	u_short opcode;
	u_short block_num;
	u_char data[MAX_DATA_SIZE];
}data_packet;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	u_short opcode;
	u_char file_name[FILENAME_MAX];
	u_char zero_byte=0;
	u_char mode[5];
	u_char zero_byte_2=0;
}rw_packet;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	u_short opcode;
	u_short block_num;
}ack_packet;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	u_short opcode;
	u_char file_name[FILENAME_MAX];
	u_char err_msg[MAX_DATA_SIZE];
	u_char zero_byte=0;
}error_packet;
#pragma pack(pop)

#pragma pack(push,1)
typedef union {
	data_packet data_pkt;
	ack_packet ack_pkt;
	error_packet err_pck;
	rw_packet rw_pck;
}packet_t;
typedef struct {
	u_short opcode; /* tftp opcode*/
	packet_t packet;
}tftp_message;
#pragma pack(pop)



