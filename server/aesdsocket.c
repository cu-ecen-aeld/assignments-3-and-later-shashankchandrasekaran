/*
 * filename: aesdsocket.c
 * Created by: Shashank Chandrasekaran 
 * Description: C file for data packet transmission between Server and client
 * Date: 26-Feb-2023
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>


int socket_fd,accept_return; //Socket fd and client connection
int status; //Daemon return status

//Structure for bind and accept
struct addrinfo hints;
struct addrinfo *servinfo;
struct sockaddr_in client_addr;

char *store_data=NULL; //To store data from receiver


//Signal handler
void signal_handler(int sig)
{
	if(sig==SIGINT)
	{
		syslog(LOG_INFO,"Caught SIGINT, leaving");
	}
	else if(sig==SIGTERM)
	{
		syslog(LOG_INFO,"Caught SIGTERM, leaving");
	}
	
	unlink("/var/tmp/aesdsocketdata.txt"); //Deletes a file
	
	//Close socket and client connection
	close(socket_fd);
	close(accept_return);
	
	exit(0); //Exit success 
}


int main(int argc, char *argv[])
{
	int getaddr,bind_status,listen_status; //Variables to get address for bind, return value of bind and listen
	ssize_t rec_status=1; //Return of recv function
	socklen_t size=sizeof(struct sockaddr); 
	char buff[100]; //temporary buffer to store receiver data
	int fd_status;
	ssize_t write_status; //Retur of write function
	int send_status; //Return of send function
	int i, total_bytes=0,packet_bytes_total=0; //Store total bytes received in a packet and total bytes received in a connection
	bool write_flag=false; 
	
	openlog(NULL,LOG_PID, LOG_USER); //To setup logging with LOG_USER
	
	
	//To start a daemon process
	if((argc>1) && strcmp(argv[1],"-d")==0)
	{
		if(daemon(0,0)==-1)
		{
			syslog(LOG_ERR, "Couldn't enter daemon mode");
			exit(1);
		}
	}
	
	//To check if a signal is received
	if(signal(SIGINT,signal_handler)==SIG_ERR)
	{
		syslog(LOG_ERR,"SIGINT failed");
		exit(2);
	}
	if(signal(SIGTERM,signal_handler)==SIG_ERR)
	{
		syslog(LOG_ERR,"SIGTERM failed");
		exit(3);
	}
	
	//create socket
	socket_fd=socket(PF_INET, SOCK_STREAM, 0);
	if(socket_fd==-1)
	{
		syslog(LOG_ERR, "Couldn't create a socket");
		exit(4);
	}
	
	//Get server address
	hints.ai_flags=AI_PASSIVE; //Set this flag before passing to function
	getaddr=getaddrinfo(NULL,"9000",&hints,&servinfo);
	if(getaddr !=0)
	{
		syslog(LOG_ERR, "Couldn't get the server's address");
		exit(5);
	}
	
	//Bind
	bind_status=bind(socket_fd,servinfo->ai_addr,sizeof(struct sockaddr));
	if(bind_status==-1)
	{
		freeaddrinfo(servinfo); //Free the memory before exiting
		syslog(LOG_ERR, "Binding failed");
		exit(6);
	}
	
	freeaddrinfo(servinfo); //Free this memory before next operations
	
	fd_status=open("/var/tmp/aesdsocketdata.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO); //To create a file and give permissions to all
	if (fd_status==-1)
	{
		syslog(LOG_ERR, "The file could not be created/found");
		exit(7);
	}
	close(fd_status);
	
	//Infinite loop for server-client connections
	while(1)
	{
	
	//Listen
	listen_status=listen(socket_fd,20); //Max queue of connections set to 20
	if(listen_status==-1)
	{
		syslog(LOG_ERR, "Listening to the connections failed");
		exit(8);
	}
	
	//accept
	accept_return=accept(socket_fd,(struct sockaddr *)&client_addr,&size);
	if(accept_return==-1)
	{
		syslog(LOG_ERR, "Connection could not be accepted");
		exit(9);
	}
	else
	{
		syslog(LOG_INFO,"Accepts connection from %s",inet_ntoa(client_addr.sin_addr));
		printf("Accepts connection from %s\n",inet_ntoa(client_addr.sin_addr));
	}
	
	
	store_data = (char*)malloc(100); //Allocate a memory
	if(store_data==NULL)
	{
		syslog(LOG_ERR, "Memory couldn't be allocated");
		exit(10);
	}
	
	memset(store_data,0,100); //Reset the temporary buffer
	
	write_flag=false;
	
	//Loop to receive data packet
	while(!write_flag)
	{
	
	rec_status=recv(accept_return,buff,100,0); //Receive data packets from client
	if(rec_status==-1)
	{
		syslog(LOG_ERR, "Error in reception of data packets from client");
		exit(11);
	}
	
	
	for(i=0;i<100;i++)
	{
		if(buff[i]=='\n')
		{
			i++;
			write_flag=true;
			break;
		}	
		
	}
	total_bytes+=i; //Total bytes in a data packet
	store_data=(char *)realloc(store_data,total_bytes); //Realloc memory
	if(store_data==NULL)
	{
		syslog(LOG_ERR, "Reallocation of memory failed");
		exit(12);
	}
	memcpy(store_data+total_bytes-i,buff,i); //Copy data from temporary buffer to memory
	memset(buff,0,100); //Reset the temporary buffer
	}
	
	fd_status = open("/var/tmp/aesdsocketdata.txt", O_APPEND | O_WRONLY); //Open the file in writeonly and append mode
	if(fd_status==-1)
	{
		syslog(LOG_ERR, "Could not open the file");
		exit(13);
	}
		
	
	write_status= write(fd_status,store_data,total_bytes); //Writes to the file
	if(write_status!=total_bytes)
	{
		syslog(LOG_ERR, "Could not write total bytes to the file");
		exit(14);
	}
	close(fd_status);
	
	packet_bytes_total += total_bytes; //Total bytes of a connection
		
	int op_fd=open("/var/tmp/aesdsocketdata.txt",O_RDONLY); //Open file to read only
	if(op_fd==-1)
	{
		syslog(LOG_ERR, "Could not open the file to read");
		exit(15);
	}
		
	char read_arr[packet_bytes_total]; //Create a temporary buffer to read file's contents
	memset(read_arr,0,packet_bytes_total);  //Reset the buffer
		
	int rd_status=read(op_fd,&read_arr,packet_bytes_total); //Read the file and store contents in the buffer
	if(rd_status==-1)
	{
		syslog(LOG_ERR, "Could not read bytes from the file");
		exit(16);
	}
	else if(rd_status < packet_bytes_total) 
	{
		syslog(LOG_ERR, "The bytes read do not match the bytes requested to be read");
		exit(17);
	}
		
	//Send data packet to the client 
	send_status=send(accept_return,&read_arr,packet_bytes_total,0);
	if(send_status==-1)
	{
		syslog(LOG_ERR, "The data packets couldn't be send to the client");
		exit(18);
	}
		
	close(op_fd); //Close file after reading
	free(store_data); //Free the buffer
	syslog(LOG_ERR,"Closed connection with %s\n",inet_ntoa(client_addr.sin_addr));
	total_bytes =0; //Reset total bytes before sending new packet
	}

	closelog(); //Close syslog
	return 0;
}
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
	
	
