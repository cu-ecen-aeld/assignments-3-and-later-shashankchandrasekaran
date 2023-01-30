#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>


int main(int argc, char *argv[])
{
	int fd_status;
	ssize_t write_status;
	ssize_t buffer_length;
	char *filepath = argv[1];
	char *filestr = argv[2];
	openlog(NULL,0, LOG_USER); //To setup logging with LOG_USER

	//Check if the number of arguments do not match
	if(argc!=3)
	{
		syslog(LOG_ERR,"The number of arguments entered - %d are invalid. Correct number of arguments required are 2",(argc-1)); //Log the error
		printf("Arguments entered are invalid\nThe number of arguments needed is 2\n Argument 1 is file path and argument 2 is string to be filled in the file\n");
		return 1;
	}
	
	fd_status=open(filepath, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO); //To open/create a file and give permissions to all
	
	//To check the case where file is not created
	if (fd_status==-1)
	{
		syslog(LOG_ERR, "The file cannot be found/ does not exist");
		printf("The file doesn't exist\n");
		return 1;
	}
	
	syslog(LOG_DEBUG, "Writing %s to %s", filestr, filepath); //Log if file successfully created
	
	buffer_length = strlen(filestr); //Calculates the number of bytes to be input in the file
	write_status= write(fd_status,filestr,buffer_length); //Writes to the file
	
	//To check if the number of bytes input is equal to number of bytes written
	if(write_status!=buffer_length)
	{
		syslog(LOG_ERR, "Not enough space in the file to write the string");
		printf("String not written in the file due to inadequate space in the file\n");
		return 1;
	}
	
	close(fd_status); //Close the file 
	return 0;
}
		
	
		
	
	


