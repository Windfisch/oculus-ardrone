#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <time.h>

#include "lib.h"

using namespace cv;

#define BUFSIZE 67108864

static void die(const char* msg){perror(msg); exit(1);}
static void suicide(const char* msg){ fprintf(stderr, "%s\n", msg); exit(1); }

static int read_completely(int fd, void* buf, size_t len)
{
	size_t n_read;
	for (n_read = 0; n_read < len;)
	{
		size_t tmp = read(fd, buf, len-n_read);
		n_read+=tmp;
		buf = ((char*)buf)+tmp;
	}
	return n_read;
}


DroneConnection::DroneConnection(const char* sockpath)
{
	struct sockaddr_un my_sockaddr;
	buffer = new unsigned char[BUFSIZE];

	my_sockaddr.sun_family=AF_UNIX;
	strcpy(my_sockaddr.sun_path, sockpath);
	int sockaddrlen = strlen(my_sockaddr.sun_path) + sizeof(my_sockaddr.sun_family);

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd == -1) die("socket");


	if (connect(sockfd, (struct sockaddr*) &my_sockaddr, sockaddrlen) == -1)
		die("connect");
}

DroneConnection::~DroneConnection()
{
	close(sockfd);
	delete [] buffer;
}

void DroneConnection::get(Mat& frame, navdata_t* nav)
{
	write(sockfd,"get\n",4);

	read_completely(sockfd, buffer, 4);
	int framelen = ((buffer[0]*256+buffer[1])*256+buffer[2])*256+buffer[3];
	if (framelen + sizeof(navdata_t) > BUFSIZE) suicide("buffer too small");
	
	read_completely(sockfd, buffer, framelen+sizeof(navdata_t));
	
	const navdata_t* navdata = (navdata_t*)(buffer + framelen);
	memcpy(nav, navdata, sizeof(*navdata));

	frame = Mat(720,1280,CV_8UC3, buffer);
}
