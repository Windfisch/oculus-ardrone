#ifndef __LIB_H__
#define __LIB_H__

#include <opencv2/opencv.hpp>

struct navdata_t
{
	double phi;
	double theta;
	double psi;
	double batt;
};


class DroneConnection
{
	public:
		DroneConnection(const char* sockpath);
		~DroneConnection();
		void get(cv::Mat& frame, navdata_t* navdata);
	
	private:
		unsigned char* buffer;
		int sockfd = -1;

};

#endif
