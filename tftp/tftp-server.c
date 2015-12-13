#include "tftp_protocol.h"

int main(int argc,char** argv){


	struct addrinfo  hints;
	struct addrinfo * my_addr , *rp, client_addr;

	socklen_t slen =sizeof(client_addr);
	const char hostname[];
	char* port = DEFAULT_PORT;
	unsigned int size;
	int fd,sock;
	int max_fd;
	int i;
	int ret_val=0;
	int player_turn =0; // player 0 or 1
	int client_connected=0;
	char file_name[] = "new_file";
	tftp_message recv_data,sent_data;
	short rw_mode=0; //write-1, read-0

	struct timeval time;


	// Main loop.
	while (1) {
		if (!client_connected){
			// Obtain address(es) matching host/port
			memset(&hints,0,sizeof(struct addrinfo));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_flags = AI_PASSIVE;
			hints.ai_protocol = 0;
			int status = getaddrinfo(NULL,port,&hints,&my_addr);
			if (status!=0){
				printf("getaddrinfo error: %s\n", strerror(status));
				return EXIT_FAILURE;
			}


			// loop through all the results and connect to the first we can
			for (rp=my_addr ; rp!=NULL ; rp=rp->ai_next){
				if (rp->ai_family!=AF_INET) {
					continue;
				}
				//opening a new socket
				sock = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
				if (sock==-1) {
					continue;
				}
				if (bind(sock,rp->ai_addr,rp->ai_addrlen)!=-1){
					break ; //successfuly binded
				}
				close(sock);
			}

			// No address succeeded
			if (rp == NULL) {
				fprintf(stderr, "Server:failed to bind\n");
				close(sock);
				freeaddrinfo(my_addr);
				client_connected=0;
				continue;
			}
			freeaddrinfo(my_addr);
			client_connected=1;
		}
		ret_val =recvfrom(sock,&recv_data,sizeof(recv_data),&client_addr,&slen);
		if (ret_val<0){
			printf("Recieving failed.Discconeting client: %s.\n",strerror(errno));
			close(sock);
			if (fd>0){
				close(fd);
			}
			client_connected=0;
			continue;
		}
		switch (recv_data.opcode){
		case (WRQ):
				rw_mode=1;//write mode
				fd = open(recv_data.packet.rw_pck.file_name,O_RDWR | O_TRUNC | O_CREAT,S_IRWXU | S_IRWXG | S_IRWXO);
				if (fd<0){
					printf("Failed opening the new file.%s.\n",strerror(errno));
				}
				sent_data.opcode=ACK;
				sent_data.packet.ack_pkt.block_num=1;
				sent_data.packet.ack_pkt.opcode=ACK;
		break;
		}

	}
}
