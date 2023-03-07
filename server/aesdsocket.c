/*
 * filename: aesdsocket.c
 * Created by: Shashank Chandrasekaran 
 * Description: C file for data packet transmission between Server and client in multithreading along with timestamp
 * Date: 05-Mar-2023
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include "queue.h"

#define TIMER_BUFF_SIZE 	100

int socket_fd, total_packet_bytes =0, g_fd= 0;
bool handler_status = false;
pthread_mutex_t mutex; 


typedef struct thread_nodes{
	pthread_t thread_id; 
	int accept_connection; 
	int file_fd;
	bool thread_complete; 
	SLIST_ENTRY(thread_nodes) entries; 
}thread_nodes_t; 


//Signal handler for sigalarm
void timestamp_handler(int signo)
{
    time_t real_time;
	size_t time_length;
	struct tm cur_time;
	ssize_t bytes_written;
    if(time(&real_time) == -1)
	{
        syslog(LOG_ERR,"Unable to get real time clock value");
        exit (1);
    }

    localtime_r(&real_time,&cur_time); //Converts to local time
    char time_buff[TIMER_BUFF_SIZE];
    time_length = strftime(time_buff,TIMER_BUFF_SIZE,"timestamp:%a, %d %b %Y %T %z\n",&cur_time);
    if (!time_length)
	{
        syslog(LOG_ERR,"Unable to generate timestamp");
        exit(2);
    }

	pthread_mutex_lock(&mutex);
	total_packet_bytes +=time_length; //Add total bytes received by all connections to the file
	bytes_written = write(g_fd, time_buff,time_length);
	pthread_mutex_unlock(&mutex);
	if (bytes_written == -1)
	{
		syslog(LOG_ERR,"Couldn't write bytes to the file");
		exit(3);
	}
}

//Signal Handler for sigint and sigalarm
void signal_handler(int sig)
{
	if(sig==SIGINT)
		syslog(LOG_INFO,"Caught SIGINT, leaving");
	else if(sig==SIGTERM)
		syslog(LOG_INFO,"Caught SIGTERM, leaving");

	handler_status = true; 
	close(socket_fd);
}

//Function to initialize socket
void socket_init(void)
{
    struct addrinfo hints, *servinfo;
	memset(&hints,0,sizeof(hints));
	hints.ai_flags=AI_PASSIVE;
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_STREAM;
	if(getaddrinfo(NULL,"9000",&hints,&servinfo) !=0) 
	{
		syslog(LOG_ERR, "server address cannot be found");
		exit(4);
	}

    socket_fd=socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd==-1) 
	{
		syslog(LOG_ERR, "Socket cannot be created\n");
		exit(5);
	}
	
	if(bind(socket_fd,servinfo->ai_addr,sizeof(struct sockaddr)) == -1)
	{
		syslog(LOG_ERR, "Unable to bind");
		freeaddrinfo(servinfo); 			
		close(socket_fd);
		exit(6);
	}
	freeaddrinfo(servinfo); 				
}

//write data packet to file
int write_pdata_file(int file_fd, int accept_connection)
{
	pthread_mutex_lock(&mutex);
	lseek(file_fd, 0, SEEK_SET);
	char send_buffer[total_packet_bytes]; 			 
	if(read(file_fd,&send_buffer,total_packet_bytes)==-1) 
	{  
		syslog(LOG_ERR, "Cannot read the file");
		return -1;
	}	
	pthread_mutex_unlock(&mutex); 
	
	//Send data packet to the client 
	if(send(accept_connection,&send_buffer,total_packet_bytes,0)==-1) 
	{
		syslog(LOG_ERR, "Unable to send the bytes to the socket");
		return -1;
	} 
	return 0;
}

//Threads packet transfers
void* thread_packet_data(void* thread_in_action) 
{
	char *temp_buff = NULL; 
	int bytes_per_packet =1;
	int recv_status =0,new_line =0;
	int conn_close =0;

	if(thread_in_action == NULL)
  		exit(7);

	//logic to receive packets from client
	thread_nodes_t *t_node_params= (thread_nodes_t *)thread_in_action;
	temp_buff = (char*)malloc(sizeof(char));
	if (temp_buff == NULL) 
	{
		syslog(LOG_ERR,"Couldn't allocate memory to store packets");
		t_node_params->thread_complete = true;
		close(t_node_params->accept_connection); 
		exit(8);
	}	
	while (!conn_close && !handler_status)
	{
		new_line = 0;
		while((!new_line) && (!conn_close) && (!handler_status))
		{
			recv_status = recv(t_node_params -> accept_connection,temp_buff+bytes_per_packet-1,1,0);
			if (recv_status == -1)
			{
				syslog(LOG_ERR,"Cannot receive bytes");
				t_node_params->thread_complete= true;
				close(t_node_params->accept_connection); 
				exit(9);
			}
			else if (recv_status == 0)
			{	    
				bytes_per_packet--;  
				conn_close = 1;       
			}
			else
			{
				if (temp_buff[bytes_per_packet-1] == '\n')
					new_line = 1;
				else
				{
					bytes_per_packet ++;
					temp_buff = realloc(temp_buff, (bytes_per_packet)*sizeof(char)); //allocate
					if (temp_buff == NULL)
					{
						syslog(LOG_ERR,"Couldn't allocate more memory");
						t_node_params->thread_complete= true;
						close(t_node_params->accept_connection); 
						exit(10);
					}
				}
			}
		}
		
		//Check for new line
		if (new_line) 
		{
			pthread_mutex_lock(&mutex); 
			if(write(t_node_params -> file_fd, temp_buff,bytes_per_packet) < bytes_per_packet) 
			{
				syslog(LOG_ERR,"Cannot write bytes to the file");
				t_node_params->thread_complete= true;
				close(t_node_params->accept_connection); 
				exit(11);
			}
			total_packet_bytes+=bytes_per_packet; //Accumulate total packet bytes received till now
			pthread_mutex_unlock(&mutex); 
			bytes_per_packet = 1; 	   				
			temp_buff = realloc(temp_buff, (bytes_per_packet)*sizeof(char)); 
			if (temp_buff == NULL)
			{
				syslog(LOG_ERR,"Cannot reallocate memory");
				t_node_params->thread_complete= true;
				close(t_node_params->accept_connection); 
				exit(12);
			}

			//Read bytes from file and send to socket
			if (write_pdata_file(t_node_params->file_fd,t_node_params->accept_connection)==-1)
			{
				t_node_params->thread_complete= true;
				close(t_node_params->accept_connection); 
				exit(13);
			}
		}
	}
	close(t_node_params->accept_connection); 
	t_node_params->thread_complete = true;  
	free(temp_buff);   
	return thread_in_action;
}

//Thread creation and cleanup tasks
void threads_tasks(int file_fd)
{
	int accept_connection=0,total_connection =0 ;
	socklen_t address_length=sizeof(struct sockaddr); 
	struct sockaddr_in client_address;
	thread_nodes_t *thread_node=NULL; 
	thread_nodes_t *active_thread = NULL;
	thread_nodes_t *store = NULL;

	SLIST_HEAD(slisthead,thread_nodes) head = SLIST_HEAD_INITIALIZER(head);
    SLIST_INIT(&head);

	//Connections accepted till a signal handler is called
	while (!handler_status)
	{
		accept_connection = accept(socket_fd,(struct sockaddr *)&client_address,&address_length);
		if(accept_connection==-1) 
		{
			syslog(LOG_ERR, "Connection cannot be accepted");
			break;
		}
		else 
		{
			thread_node = (struct thread_nodes *)malloc(sizeof(struct thread_nodes));
			thread_node-> thread_complete = false;									
			thread_node-> accept_connection = accept_connection;
			thread_node-> file_fd = file_fd; 
			syslog(LOG_INFO,"Accepts connection from %s",inet_ntoa(client_address.sin_addr));
			if(pthread_create(&thread_node -> thread_id, NULL, thread_packet_data, thread_node) != 0)
			{
				 syslog(LOG_ERR, "Unable to create thread");
				 break; 
			} 
			if(!total_connection)
			{
				SLIST_INIT(&head);
				SLIST_INSERT_HEAD(&head, thread_node, entries); 
			}
			else
				SLIST_INSERT_HEAD(&head, thread_node, entries); 
			
			total_connection++;
			SLIST_FOREACH_SAFE(active_thread, &head, entries, store)
			{
				if(active_thread -> thread_complete)
				{
					 pthread_join(active_thread->thread_id,NULL);//Cleanup of thread
                     SLIST_REMOVE(&head, active_thread, thread_nodes, entries);
                     free(active_thread);
                     total_connection--;
				}
			}  
		}
	}

 	//Wait until threads finish execution
  	SLIST_FOREACH_SAFE(active_thread, &head, entries, store)
	{
         pthread_kill(active_thread->thread_id,SIGINT);
         pthread_join(active_thread->thread_id,NULL);
         SLIST_REMOVE(&head, active_thread, thread_nodes, entries);
         free(active_thread);
         total_connection--;
    }
	close(file_fd);
	close(accept_connection);
}

//Main subroutine
int main(int argc, char *argv[])
{ 	
	struct itimerval timer_count;
	openlog(NULL,LOG_PID, LOG_USER); //To setup logging with LOG_USER

	// Initialize mutex for threads and timestamp
	pthread_mutex_init(&mutex, NULL);

	socket_init();

	//Signal handler for sigint and sigterm
	if(signal(SIGINT,signal_handler)==SIG_ERR)
	{
		syslog(LOG_ERR,"SIGINT failed");
		exit(14); 
	}

	if(signal(SIGTERM,signal_handler)==SIG_ERR)
	{
		syslog(LOG_ERR,"SIGTERM failed");
		exit(15); 
	}

	if(listen(socket_fd,20) == -1) 
	{	
		syslog(LOG_ERR, "Cannot listen to clients");
		close(socket_fd);
		exit(16); 
	}	

	//To start a daemon process
	if((argc>1) && strcmp(argv[1],"-d")==0)
	{
		if(daemon(0,0)==-1) 
		{
			syslog(LOG_ERR, "Couldn't enter daemon mode");
			exit(17);
		}
	}

	int file_fd = open("/var/tmp/aesdsocketdata.txt",O_RDWR|O_CREAT|O_APPEND, S_IRWXU|S_IRGRP|S_IROTH);
	if(file_fd==-1)
	{
		syslog(LOG_ERR, "Cannot create file");
		exit(18);
	}
	g_fd = file_fd; 

	//Signal handler for sigalarm
	signal(SIGALRM, timestamp_handler);

	//first delay
	timer_count.it_value.tv_sec = 10;	  
	timer_count.it_value.tv_usec = 0;	  
	//repeated delay
	timer_count.it_interval.tv_sec = 10; 
	timer_count.it_interval.tv_usec = 0; 

	tzset();

	if (setitimer(ITIMER_REAL, &timer_count, NULL)==-1)
	{
		syslog(LOG_ERR,"The timer couldn't be set");
		exit(19);
	}

	threads_tasks(file_fd);

	unlink("/var/tmp/aesdsocketdata.txt"); //Remove file descriptor
	closelog(); //Close syslog
	
	return 0;
}