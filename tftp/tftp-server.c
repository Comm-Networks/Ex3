#include "tftp_protocol.h"


data_packet data_pkt;
error_packet err_pkt;
ack_packet ack_pkt;
rw_packet rw_pkt;
struct sockaddr client_addr;
int fd=0;
int sock=0;


int send_data(u_short opcode, int size_to_send) {
	switch (opcode) {
	case ACK:
		printf("Block: %d\n", ack_pkt.block_num);
		return sendto(sock, &ack_pkt, size_to_send, 0, &client_addr, sizeof(client_addr));

	case DATA:
		return sendto(sock, &data_pkt, size_to_send, 0, &client_addr, sizeof(client_addr));

	case ERROR:
		return sendto(sock, &err_pkt, size_to_send, 0, &client_addr, sizeof(client_addr));

	default:
		return -1;
	}
}


/*
 * handler for SIGINT and SIGTERM signals.
 */
void handle_termination(int signum) {
	if (fd>0){
		close(fd);
	}
	if (sock>0){
		close(sock);
	}
	exit(EXIT_SUCCESS);
}

void fillErorMessage(error_packet * err_pkt,u_short err_code,char* msg){
	printf("Err Begin\n");
	err_pkt->err_code=err_code;
	err_pkt->opcode=ntohs(ERROR);
	if (msg!=NULL){
		memcpy(err_pkt->err_msg,msg,MAX_DATA_SIZE);
	}
	else {
		memset(err_pkt->err_msg,0,MAX_DATA_SIZE);
	}
	printf("Err End\n");
}


int main(int argc,char** argv){


	struct addrinfo  hints;
	struct addrinfo * my_addr , *rp;
	socklen_t slen;
	char* port = DEFAULT_PORT;
	int last_ack_blk=0;
	int last_data_blk=1;
	int ret_val=0;
	int client_connected=0;
	short new_packet=0;
	char buffer[sizeof(rw_packet)];
	u_short opcode;
	struct timeval timeout;
	double term_timeout;
	struct stat st_buf;
	short final_data_block=0;
	struct sigaction term_sa;
	int size_to_send;


	sigemptyset(&term_sa.sa_mask);
	term_sa.sa_handler = &handle_termination;
	if (sigaction(SIGINT,&term_sa,NULL)==-1 || sigaction(SIGTERM,&term_sa,NULL)){
		printf("Error using sigaction to handle terminating signal.%s\n",strerror(errno));
	}

	err_pkt.zero_byte = 0;

	timeout.tv_sec = 20;
	timeout.tv_usec = 0;

	term_timeout=(double) clock() / CLOCKS_PER_SEC;

	// Main loop.
	while (1) {
		term_timeout= (double) clock() / CLOCKS_PER_SEC - term_timeout;
		//no new packet was recieved for 60 sec. can close the client.
		if (term_timeout>60){
			if (fd>0){
				close(fd);
				fd=-1;
			}
			printf("timeout exceeded\n");
			close(sock);
			sock=-1;
			client_connected=0;
			continue;
		}

		//no client is connected - open a new socket and bind.
		if (!client_connected){
			// Obtain address(es) matching host/port
			memset(&hints,0,sizeof(struct addrinfo));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_flags = AI_ADDRCONFIG;
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

				// Setting socket timeout.
				if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
				    perror("Error setting timeout\n");
				}

				if (bind(sock,rp->ai_addr,rp->ai_addrlen)!=-1){
					struct sockaddr_in *ipv4 = (struct sockaddr_in *)rp->ai_addr;
					char ipAddress[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &(ipv4->sin_addr), ipAddress, INET_ADDRSTRLEN);
					printf("Addr: %s\n", ipAddress);
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
			printf("Connected!!\n");
			client_connected=1;
		}
		slen=sizeof(client_addr);
		/*Buffer's size is the len of the longest packet type.
		  If the recieved packet's size is smaller => we recieve the bytes we need. */

		memset(buffer, 0, sizeof(rw_packet));
		ret_val = recvfrom(sock,buffer,sizeof(buffer),0,&client_addr,&slen);
		if (ret_val<0){
			printf("Recieving failed.Discconeting client: %s.\n",strerror(errno));
			continue;
//			fillErorMessage(&err_pkt,OTHER,strerror(errno));
//			opcode=ERROR;
		}
		else{
			printf("Recieved a msg: ");
			memcpy(&opcode,buffer,sizeof(u_short));
			opcode = htons(opcode);
			//check recieved packet type and prepare the output message.
			switch (opcode){
			case (WRQ):
					printf("WRQ\n");
					memcpy(&(rw_pkt),&(buffer),sizeof(rw_packet));
					new_packet=1;
					printf("File: %s\n", rw_pkt.file_name);
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
						size_to_send = sizeof(error_packet);
						err_pkt.opcode=ntohs(opcode);
						err_pkt.err_code=FILE_EXIST;
						printf("Here\n");
						memset(err_pkt.err_msg,0,MAX_DATA_SIZE);//no need for a message here, code is enough.
						printf("Heree\n");
					}
					//file does not exist. we can open a new file.
					else {
						fd = open(rw_pkt.file_name, O_RDWR | O_TRUNC | O_CREAT,S_IRWXU | S_IRWXG | S_IRWXO);
						if (fd<0){
							printf("Failed opening the new file.%s.\n",strerror(errno));
							opcode=(u_short) ERROR;
							fillErorMessage(&err_pkt,OTHER,strerror(errno));
						}
						else{
							opcode=(u_short)ACK;
							size_to_send = sizeof(ack_packet);
							ack_pkt.block_num=0;
							ack_pkt.opcode=ntohs(opcode);
						}
					}
			break;
			case (RRQ):
					printf("RRQ\n");
					memcpy(&(rw_pkt),&(buffer),sizeof(rw_packet));
					new_packet=1;
					if ((ret_val=stat(rw_pkt.file_name,&st_buf))<0){
						if (errno==ENOENT){
							opcode=(u_short)ERROR;
							size_to_send = sizeof(error_packet);
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
					fd = open(rw_pkt.file_name, O_RDONLY, S_IRWXU | S_IRWXG | S_IRWXO);
					if (fd<0){

						printf("Failed opening file.%s.\n",strerror(errno));
					}
					//set the data msg.
					opcode=(u_short)DATA;
					data_pkt.block_num=ntohs(last_data_blk);
					data_pkt.opcode=ntohs(opcode);
					ret_val=read(fd,data_pkt.data,MAX_DATA_SIZE);
					size_to_send = sizeof(data_packet) - (MAX_DATA_SIZE - ret_val);
					if (ret_val<0){
						printf("Error reading from file.%s\n",strerror(errno));
						close(fd);
						fillErorMessage(&err_pkt,OTHER,strerror(errno));
					}
					//checks if final data block recieved.
					final_data_block = ret_val<MAX_DATA_SIZE ? 1 :0;
			break;
			case (DATA):
					printf("DATA\n");
					memset(data_pkt.data, 0, MAX_DATA_SIZE);
					memcpy(&(data_pkt),&(buffer),sizeof(data_packet));
					data_pkt.block_num = htons(data_pkt.block_num);
					printf("1 - %d | %d\n", data_pkt.block_num, last_ack_blk);
					//the last ack packet we sent was recievd. this is the response a data packet.
					if (data_pkt.block_num==last_ack_blk+1){
						printf("2 - %d\n", fd);
						new_packet=1;
						ret_val=write(fd,data_pkt.data,MAX_DATA_SIZE);
						printf("3 - %d | %d\n", ret_val, sizeof(data_pkt.data));
						if (ret_val<0){
							printf("4\n");
							opcode=(u_short) ERROR;
							if (ENOSPC==errno){
								fillErorMessage(&err_pkt,FULL_DISK,NULL);
							}
							else {
								fillErorMessage(&err_pkt,OTHER,strerror(errno));
							}
							printf("5\n");
							printf("Error writing to file.%s\n",strerror(errno));
							close(fd);
							printf("6\n");
						}
						opcode= (u_short) ACK;
						size_to_send = sizeof(ack_packet);
						printf("7\n");
						//prepraring the ack reply msg.
						ack_pkt.block_num=ntohs(++last_ack_blk);
						printf("8\n");
						ack_pkt.opcode=ntohs(opcode);
						printf("9\n");
						if (ret_val<MAX_DATA_SIZE){
							printf("10\n");
							final_data_block=1;
							close(fd);
						}
						printf("11\n");
					}
					else {
						printf("12\n");
						new_packet=0;
					}


					break;
			case (ACK):
					printf("ACK\n");
					memcpy(&(ack_pkt),&(buffer),sizeof(ack_packet));
					//we recieved a new ack packet.
					ack_pkt.block_num = htons(ack_pkt.block_num);
					if (ack_pkt.block_num == last_data_blk) {
						new_packet=1;
						/*final data block was sent by the server and now we got the final ack.
						we can close the file and the connection.*/
						if (final_data_block){
//							// Sending final ACK.
//							opcode = (u_short) ACK;
//							ack_pkt.block_num = ntohs(last_ack_blk);
//							ack_pkt.opcode = ntohs(opcode);
//							break;
							close(fd);
							close(sock);
							client_connected=0;
							continue;
						}
						data_pkt.block_num = ntohs(++last_data_blk);
						data_pkt.opcode = ntohs((u_short)DATA);
						ret_val=read(fd,data_pkt.data,MAX_DATA_SIZE);
						//in case of failure - send error message to the client and close the file.
						if (ret_val<0){
							printf("Error reading from file.%s\n",strerror(errno));
							close(fd);
							fillErorMessage(&err_pkt,OTHER,strerror(errno));
						}
						else {
							//checks if final data block read from file.
							final_data_block = ret_val<MAX_DATA_SIZE ? 1 :0;

							printf("Final: %d | %d\n", sizeof(data_pkt.data), strlen(data_pkt.data));
						}
					}
					else {
						new_packet=0;
					}
					opcode=(u_short)DATA;
					size_to_send = sizeof(data_packet) - (MAX_DATA_SIZE - ret_val);
			break;
			default:
				printf("Unknown!!!!!!\n");
				break;
			}
			if (new_packet){
				term_timeout=(double)clock()/CLOCKS_PER_SEC;
			}
		}

		ret_val = send_data(opcode, size_to_send);
		if (ret_val<0){
			if (fd>0){
				close(fd);
				fd=-1;
			}
			printf("Error sending message to client: %s. The client is now discoonected\n.",strerror(errno));
			close(sock);
			client_connected=0;
			continue;
		}

		if (opcode==ERROR){
			//Premature Termination
			if (fd>0){
				close(fd);
				fd=-1;
			}
			close(sock);
			sock=-1;
			client_connected=0; //we disconnected the client after error message was sent.
			continue;
		}
		//normal termination
		else if (opcode==ACK && final_data_block){
			//finished writing all data to file and sent the client the final ack msg.
			close(fd);
			close(sock);
			sock=-1;
			client_connected=0;
			continue;

		}
	}
	close(sock);
	return (ret_val==-1)? EXIT_FAILURE : EXIT_SUCCESS;
}

