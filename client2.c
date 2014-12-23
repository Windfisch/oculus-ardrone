#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <time.h>

#include <opencv2/opencv.hpp>

using namespace cv;

#define SOCKETPATH "/home/flo/uds_socket"

void die(const char* msg){perror(msg); exit(1);}
void suicide(const char* msg){ fprintf(stderr, "%s\n", msg); exit(1); }



unsigned char buffer[67108864]; // must be unsigned. because reasons -_-

int read_completely(int fd, void* buf, size_t len)
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

int main(int argc, const char** argv)
{
	struct sockaddr_un my_sockaddr;
	my_sockaddr.sun_family=AF_UNIX;
	strcpy(my_sockaddr.sun_path, SOCKETPATH);
	int sockaddrlen = strlen(my_sockaddr.sun_path) + sizeof(my_sockaddr.sun_family);

	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd == -1) die("socket");


	if (connect(sockfd, (struct sockaddr*) &my_sockaddr, sockaddrlen) == -1)
		die("connect");
	
	time_t t = time(NULL);
	int nframes=0;

	while (waitKey(1) != 'x')
	{
		write(sockfd,"get\n",4);
		read_completely(sockfd, buffer, 4);
		int framelen = ((buffer[0]*256+buffer[1])*256+buffer[2])*256+buffer[3];
		if (framelen > sizeof(buffer)) suicide("buffer too small");
		read_completely(sockfd, buffer, framelen);

		//Mat dingens=Mat::eye(100,100,CV_8UC1) * 244;
		Mat dingens(600,1600,CV_8UC3, buffer);
		imshow("dingens",dingens);

		time_t tmp = time(NULL);
		if (tmp!=t)
		{
			printf("%i FPS\n", (int)(nframes / (tmp-t)));
			nframes=0;
			t=tmp;
		}
		nframes++;
	}
	
	close(sockfd);
	return 0;
}
