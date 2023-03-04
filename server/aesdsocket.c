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
#include <errno.h>


int socket_fd,accept_return; //Socket fd and client connection
int status; //Daemon return status
bool handler_status=false;

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
	
	handler_status=true;
	close(socket_fd); //Close socket connection
}


int main(int argc, char *argv[])
{
	int getaddr,bind_status,listen_status; //Variables to get address for bind, return value of bind and listen
	socklen_t size=sizeof(struct sockaddr); 
	int fd_status; //file descriptor while creation
	int send_status; //Return of send function
	int packet_bytes_total=0; //Store total bytes received in a connection
	int file_delstatus, op_fd; //File delete status and file descriptor for open
	//Structure for bind and accept
	struct sockaddr_in client_addr;
	struct addrinfo *servinfo;
	struct addrinfo hints;
	int recv_buff_length=1,recv_bytes=0,newline=0, conclose_status = 0;  //variables for receiving and writing packets to file

	openlog(NULL,LOG_PID, LOG_USER); //To setup logging with LOG_USER
	
	//To check if a signal is received
	if(signal(SIGINT,signal_handler)==SIG_ERR)
	{
		syslog(LOG_ERR,"SIGINT failed");
		exit(1);
	}
	if(signal(SIGTERM,signal_handler)==SIG_ERR)
	{
		syslog(LOG_ERR,"SIGTERM failed");
		exit(2);
	}
	
	//create socket
	socket_fd=socket(PF_INET, SOCK_STREAM, 0);
	if(socket_fd==-1)
	{
		syslog(LOG_ERR, "Couldn't create a socket");
		exit(3);
	}
	
	
	//Get server address
	memset(&hints,0,sizeof(hints));
	hints.ai_flags=AI_PASSIVE; //Set this flag before passing to function
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_STREAM;
	getaddr=getaddrinfo(NULL,"9000",&hints,&servinfo);
	if(getaddr !=0)
	{
		syslog(LOG_ERR, "Couldn't get the server's address");
		exit(4);
	}
	
	//Bind
	bind_status=bind(socket_fd,servinfo->ai_addr,sizeof(struct sockaddr));
	if(bind_status==-1)
	{
		freeaddrinfo(servinfo); //Free the memory before exiting
		syslog(LOG_ERR, "Binding failed");
		exit(5);
	}
	
	freeaddrinfo(servinfo); //Free this memory before next operations
	
   	 //Listen
	listen_status=listen(socket_fd,20); //Max queue of connections set to 20
	if(listen_status==-1)
	{
		syslog(LOG_ERR, "Listening to the connections failed");
		exit(6);
	}
   	 
   	 //Added later
	if((argc>1) && strcmp(argv[1],"-d")==0)
	{
		if(daemon(0,0)==-1)
		{
			syslog(LOG_ERR, "Couldn't enter daemon mode");
			exit(7);
		}
	}
	
	fd_status=open("/var/tmp/aesdsocketdata.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO); //To create a file and give permissions to all
	if (fd_status==-1)
	{
		syslog(LOG_ERR, "The file could not be created/found");
		exit(8);
	}
	close(fd_status);
	
	//Loop for server-client connections
	while(!handler_status)
	{
	//accept
	accept_return=accept(socket_fd,(struct sockaddr *)&client_addr,&size);
	if(accept_return==-1)
	{
		syslog(LOG_ERR, "Connection could not be accepted");
		break;
	}
	else
	{
		syslog(LOG_INFO,"Accepts connection from %s",inet_ntoa(client_addr.sin_addr));
		printf("Accepts connection from %s\n",inet_ntoa(client_addr.sin_addr));
	}
	
	store_data = (char*)malloc(1*sizeof(char));
     	if (store_data == NULL)
		{
         syslog(LOG_ERR,"Memory not available for allocating buffer");
         exit(9);
     	}

		//resetting
     	conclose_status =0;
		recv_buff_length =1;

		//Loop to receive byte from client and write in buffer till \n is found
     	while (!conclose_status && !handler_status)
		{
         newline = 0;
         while((!newline) && (!conclose_status) && (!handler_status))
		 {
             recv_bytes = recv(accept_return,store_data+recv_buff_length-1,1,0);
             if (recv_bytes == 0)
			 {
                 syslog(LOG_INFO,"Connection is closed");
                 recv_buff_length--; 
                 conclose_status = 1;
             }
             else if (recv_bytes == -1)
			 {
                    syslog(LOG_ERR,"Unable to receive bytes from client");
                    exit(10);                 
             }
             else
			 {
				 //Check for newline
                 if (store_data[recv_buff_length - 1] == '\n')
                     newline = 1;
                 else
				 {
                     recv_buff_length ++;
                     store_data = realloc(store_data, (recv_buff_length)*sizeof(char));
                     if (store_data == NULL)
					 {
                         syslog(LOG_ERR,"memory not available to store packet size\r\n");
                         exit(11);
                     }
                 }
             }
         }
		if (newline)
		{   
			ssize_t bytes_written = 0;
			fd_status = open("/var/tmp/aesdsocketdata.txt", O_APPEND | O_WRONLY); //Open the file in writeonly and append mode
			if(fd_status==-1)
			{
				syslog(LOG_ERR, "Could not open the file");
				exit(12);
			}
			while (bytes_written != recv_buff_length)
			{
				bytes_written = write(fd_status, store_data,recv_buff_length);        
				if (bytes_written == -1)
				{
					syslog(LOG_ERR,"Couldn't write the bytes to the file");
						exit(13);
				}
			}
			
			packet_bytes_total+=recv_buff_length; //Store total packet bytes
			recv_buff_length = 1;
			store_data = realloc(store_data, (recv_buff_length)*sizeof(char));
			if (store_data == NULL)
			{
				syslog(LOG_ERR,"Memory not available to reallocate memory for buffer");
					exit(14);
			}
			close(fd_status); 
			op_fd=open("/var/tmp/aesdsocketdata.txt",O_RDONLY); //Open file to read only
			if(op_fd==-1)
			{
				syslog(LOG_ERR, "Could not open the file to read");
				exit(15);
			}

			//Logic to read from file
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
            }
   	 }
   			
	free(store_data); //Free the buffer
	syslog(LOG_ERR,"Closed connection with %s\n",inet_ntoa(client_addr.sin_addr));
	}

	close(accept_return); //Close connection
	file_delstatus=unlink("/var/tmp/aesdsocketdata.txt"); //Deletes a file descriptor
	if(file_delstatus==-1)
	{
		syslog(LOG_ERR, "File could not be closed");
		exit(19);
	}
	closelog(); //Close syslog
	return 0;
}
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
	
	
