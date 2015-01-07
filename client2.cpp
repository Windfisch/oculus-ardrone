#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <time.h>

#include <opencv2/opencv.hpp>
#include "lib.h"

using namespace cv;

#define SOCKETPATH "/home/flo/uds_socket"

int main(int argc, const char** argv)
{
	DroneConnection drone(SOCKETPATH);

	
	Mat dingens;

	while (waitKey(1) != 'x')
	{
		navdata_t navdata;
		drone.get(dingens, &navdata);

		imshow("dingens",dingens);
	}
	
	return 0;
}
