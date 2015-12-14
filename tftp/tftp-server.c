#include "tftp_protocol.h"


data_packet data_pkt;error_packet err_pkt;
ack_packet ack_pkt;rw_packet rw_pkt;



void fillErorMessage(error_packet * err_pkt,u_short err_code,char* msg){
	err_pkt->err_code=err_code;
	err_pkt->opcode=ntohs(ERROR);
	err_pkt->err_msg=msg;
}

/*
 * fill bytes buffer with the proper packet.
 */
void fillBuffer(char* buf, u_short opcode){
	int size;
	switch(opcode){
	case(ACK):
			memcpy(buf,&ack_pkt,sizeof(ack_packet));
			size=sizeof(ack_packet);
	break;
	case(DATA):
			memcpy(buf,&data_pkt,sizeof(data_packet));
			size=sizeof(data_packet);
	break;
	case(ERROR):
			memcpy(buf,&err_pkt,sizeof(error_packet));
			size=sizeof(error_packet);
	break;
	default:
		break;
	}
	memset(buf+sizeof(ack_packet),0,sizeof(buf)-size); //setting the rest of the buffer to 0.
}

int main(int argc,char** argv){


	struct addrinfo  hints;
	struct addrinfo * my_addr , *rp, client_addr;

	socklen_t slen;
	const char hostname[];
	char* port = DEFAULT_PORT;
	unsigned int size;
	int fd,sock;
	int ack_blk_num=0;
	int data_blk_num=1;
	int ret_val=0;
	int client_connected=0;
	int offset=0;
	char file_name[] = "new_file";
	short rw_mode=0; //write-1, read-0
	char buffer[sizeof(rw_packet)];
	u_short opcode;
	struct timeval time;
	struct stat st_buf;
	short final_data_block=0;


	// Main loop.
	while (1) {
		//no client is connected - open a new socket and bind.
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
		slen=sizeof(client_addr);
		/*Buffer's size is the len of the longest packet type.
		  If the recieved packet's size is smaller => we recieve the bytes we need. */
		ret_val =recvfrom(sock,buffer,sizeof(buffer),0,&client_addr,&slen);
		if (ret_val<0){
			printf("Recieving failed.Discconeting client: %s.\n",strerror(errno));
			if (fd>0){
				close(fd);
			}
			client_connected=0;
			continue;
		}

		memcpy(&opcode,buffer,sizeof(u_short));
		opcode = htons(opcode);
		//check recieved packet type and prepare the output message.
		switch (opcode){
		case (WRQ):
				memcpy(&(rw_pkt),&(buffer[2]),sizeof(rw_packet));
				rw_mode=1;//write mode

				if ((ret_val=stat(rw_pkt.file_name,&st_buf))<0){
					if (errno!=ENOENT){
						printf("Error while calling stat.%s\n",strerror(errno));
						opcode=(u_short) ERROR;
						fillErorMessage(&err_pkt,OTHER,strerror(errno));
					}
				}
				//file already exist
				if (ret_val>-1){
					opcode=(u_short)ERROR;
					err_pkt.opcode=ntohs(opcode);
					err_pkt.err_code=FILE_EXIST;
					memset(err_pkt.err_msg,0,MAX_DATA_SIZE);//no need for a message here, code is enough.
				}
				//file does not exist. we can open a new file.
				else {
					fd = open(rw_pkt.file_name,O_RDONLY | O_TRUNC | O_CREAT,S_IRWXU | S_IRWXG | S_IRWXO);
					if (fd<0){
						printf("Failed opening the new file.%s.\n",strerror(errno));
						opcode=(u_short) ERROR;
						fillErorMessage(&err_pkt,OTHER,strerror(errno));
					}
					else{
						opcode=(u_short)ACK;
						ack_pkt.block_num=ack_blk_num++;
						ack_pkt.opcode=ntohs(opcode);
					}
				}
		break;
		case (RRQ):
				memcpy(&(rw_pkt),&(buffer[2]),sizeof(rw_packet));
				rw_mode=0;//read mode
				if ((ret_val=stat(rw_pkt.file_name,&st_buf))<0){
					if (errno==ENOENT){
						opcode=(u_short)ERROR;
						err_pkt.err_code=FILE_NOT_FOUND;
						err_pkt.opcode=ntohs(opcode);
						memset(err_pkt.err_msg,0,MAX_DATA_SIZE);//no need for a message here, code is enough.
					}
					else{
						printf("Error while calling stat.%s\n",strerror(errno));
						close(sock);
						break;
					}
				}
				fd = open(rw_pkt.file_name,O_RDWR,S_IRWXU | S_IRWXG | S_IRWXO);
				if (fd<0){

					printf("Failed opening file.%s.\n",strerror(errno));
				}
				//set the data msg.
				opcode=(u_short)DATA;
				data_pkt.block_num=data_blk_num++;
				data_pkt.opcode=ntohs(opcode);
				ret_val=read(fd,data_pkt.data,MAX_DATA_SIZE);
				if (ret_val<0){
					printf("Error reading from file.%s\n",strerror(errno));
					close(fd);
					fillErorMessage(&err_pkt,OTHER,strerror(errno));
				}
				//checks if final data block recieved.
				final_data_block = ret_val<MAX_DATA_SIZE ? 1 :0;
		break;
		case (DATA):
				memcpy(&(data_pkt),&(buffer[2]),sizeof(data_packet));
				ret_val=write(fd,data_pkt.data,MAX_DATA_SIZE);
				if (ret_val<0){
					printf("Error writing to file.%s\n",strerror(errno));
					close(fd);
					opcode=(u_short) ERROR;
					fillErorMessage(&err_pkt,OTHER,strerror(errno));
				}
				//prepraring the ack reply msg.
				opcode= (u_short) ACK;
				ack_pkt.block_num=ack_blk_num++;
				ack_pkt.opcode=ntohs(opcode);
				if (ret_val<MAX_DATA_SIZE){
					final_data_block=1;
				}
		break;
		case (ACK):
				memcpy(&(ack_pkt),&(buffer[2]),sizeof(ack_packet));
				opcode=(u_short)DATA;
				data_pkt.block_num=data_blk_num++;
				data_pkt.opcode=ntohs(opcode);
				ret_val=read(fd,data_pkt.data,MAX_DATA_SIZE);
				//in case of failure - send error message to the client and close the file.
				if (ret_val<0){
					printf("Error reading from file.%s\n",strerror(errno));
					close(fd);
					fillErorMessage(&err_pkt,OTHER,strerror(errno));
				}
				else {
					//checks if final data block recieved.
					final_data_block = ret_val<MAX_DATA_SIZE ? 1 :0;
				}
		break;
		default:
			break;
		}
		fillBuffer(buffer,opcode);
		ret_val=sendto(sock,buffer,sizeof(buffer),0,&client_addr,sizeof(client_addr));
		if (ret_val<0){
			//TODO: how to deal with fail of send. should I try to send error again?
		}

		//TODO  if error message, after sending close client.
		//TODO  need to take care of finish of read/write from/to file.
	}
	close(sock);
	return (ret_val==-1)? EXIT_FAILURE : EXIT_SUCCESS;
}

