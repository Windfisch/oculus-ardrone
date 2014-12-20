#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>

#define SOCKETPATH "/home/flo/uds_socket"

void die(const char* msg){perror(msg); exit(1);}
void suicide(const char* msg){ fprintf(stderr, "%s\n", msg); exit(1); }

unsigned char buffer[67108864]; // must be unsigned. because reasons -_-

int main()
{

	struct sockaddr_un my_sockaddr;
	my_sockaddr.sun_family=AF_UNIX;
	strcpy(my_sockaddr.sun_path, SOCKETPATH);
	int sockaddrlen = strlen(my_sockaddr.sun_path) + sizeof(my_sockaddr.sun_family);

	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd == -1) die("socket");


	if (connect(sockfd, (struct sockaddr*) &my_sockaddr, sockaddrlen) == -1)
		die("connect");
	
	write(sockfd,"get\n",4);

	printf("%i\n",read(sockfd, buffer, 4));
	printf("%x%x%x%x\n",buffer[0],buffer[1],buffer[2],buffer[3]);
	int framelen = ((buffer[0]*256+buffer[1])*256+buffer[2])*256+buffer[3];
	printf("framelen is %i\n", framelen);
	if (framelen > sizeof(buffer)) suicide("buffer too small");
	read(sockfd, buffer, framelen);

	printf("done reading\n");

	read(sockfd, buffer, 123);

	close(sockfd);
	return 0;
}
