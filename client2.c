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

	printf("GO!!\n");
	time_t t = time(NULL);
	int nframes=0;

	write(sockfd,"get\n",4);
	read_completely(sockfd, buffer, 4);
	int framelen = ((buffer[0]*256+buffer[1])*256+buffer[2])*256+buffer[3];
	if (framelen > sizeof(buffer)) suicide("buffer too small");
	read_completely(sockfd, buffer, framelen);
	char key;
	bool go=true;
	bool new_vals=true;
	
	#define N_VALUES 11
	int curr_val=0;
	float val[N_VALUES];
	float step[N_VALUES] = {0.05,0.05,0.0,0.0,10,10,10,10,0.002,0.002,0.002};
	Mat map1A[3], map2A[3], map1B[3], map2B[3];
	
	float &k1=val[0], &k2=val[1], &p1=val[2], &p2=val[3];
	float &x1=val[4], &y1=val[5], &x2=val[6], &y2=val[7];
	float* col=&val[8];
	float c1,c2,c1_,c2_;

	k1=0.5;
	k2=0.1;
	p1=0.0;
	p2=0.0;
	c1=1280/2;
	c2=768/2;
	c1_=960/2;
	c2_=1080/2;
	x1=x2=y1=y2=0.0;
	col[0]=-0.01;
	col[1]=0.0;
	col[2]=0.01;

	while ((key = waitKey(1)) != 'x')
	{
		switch (key)
		{
			case 'g': go=!go; break;
			case 'w': val[curr_val]+=step[curr_val]; new_vals=true; break;
			case 's': val[curr_val]-=step[curr_val]; new_vals=true; break;
			case 'a': curr_val = (curr_val-1+N_VALUES) % N_VALUES; break;
			case 'd': curr_val = (curr_val+1) % N_VALUES; break;
		}

		if (new_vals)
		{
			printf("val = { %f", val[0]);
			for (int i=1; i< N_VALUES; i++)
				printf(", %f", val[i]);
			printf("}\n");


			Mat camera_matrix = Mat::eye(3,3,CV_32FC1);
			camera_matrix.at<float>(0,0)=1000;
			camera_matrix.at<float>(1,1)=1000;
			camera_matrix.at<float>(0,2)=c1+x1+x2;
			camera_matrix.at<float>(1,2)=c2+y1+y2;
			for  (int i=0; i<3; i++)
			{
				Mat camera_matrix_clone = camera_matrix.clone();
				camera_matrix_clone.at<float>(0,0)*=(1.+col[i]);
				camera_matrix_clone.at<float>(1,1)*=(1.+col[i]);
				camera_matrix_clone.at<float>(0,2)=c1_+x1;
				camera_matrix_clone.at<float>(1,2)=c2_+y1;
				initUndistortRectifyMap(camera_matrix, Vec4f(k1,k2,p1,p2), Mat::eye(3,3,CV_32F), camera_matrix_clone, Size(960,1080), CV_32FC1, map1A[i], map2A[i]);
			}

			camera_matrix.at<float>(0,0)=1000;
			camera_matrix.at<float>(1,1)=1000;
			camera_matrix.at<float>(0,2)=c1-x1-x2;
			camera_matrix.at<float>(1,2)=c2+y1+y2;
			for  (int i=0; i<3; i++)
			{
				Mat camera_matrix_clone = camera_matrix.clone();
				camera_matrix_clone.at<float>(0,0)*=(1.+(i-1)/100.);
				camera_matrix_clone.at<float>(1,1)*=(1.+(i-1)/100.);
				camera_matrix_clone.at<float>(0,2)=c1_-x1;
				camera_matrix_clone.at<float>(1,2)=c2_+y1;
				initUndistortRectifyMap(camera_matrix, Vec4f(k1,k2,p1,p2), Mat::eye(3,3,CV_32F), camera_matrix_clone, Size(960,1080), CV_32FC1, map1B[i], map2B[i]);
			}

			new_vals=false;
		}

		if (go) {
			write(sockfd,"get\n",4);
			read_completely(sockfd, buffer, 4);
			int framelen = ((buffer[0]*256+buffer[1])*256+buffer[2])*256+buffer[3];
			if (framelen > sizeof(buffer)) suicide("buffer too small");
			read_completely(sockfd, buffer, framelen);
		}

		//Mat dingens=Mat::eye(100,100,CV_8UC1) * 244;
		Mat dingens(768,1280,CV_8UC3, buffer);
		for (int i=0; i< 1280; i+=50)
			dingens.col(i)=Vec3b(255,192,128);
		for (int i=0; i< 768; i+=50)
			dingens.row(i)=Vec3b(255,192,128);
		
		
		Mat lefteye, righteye, zeuch2;
		//remap(dingens, zeuch, map1, map2, INTER_LINEAR);

		Mat colors[3];
		Mat colors2[3];

		split(dingens, colors);
		
		for (int i=0; i<3; i++)
			remap(colors[i], colors2[i], map1A[i], map2A[i], INTER_LINEAR);

		merge(colors2, 3, lefteye);
		
		split(dingens, colors);
		
		for (int i=0; i<3; i++)
			remap(colors[i], colors2[i], map1B[i], map2B[i], INTER_LINEAR);

		merge(colors2, 3, righteye);
		hconcat(lefteye, righteye ,zeuch2);

		imshow("dingens",zeuch2);

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
