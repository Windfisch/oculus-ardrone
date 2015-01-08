#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <time.h>

#include <opencv2/opencv.hpp>
#include "lib.h"
#include "ringbuf.h"

#define PI 3.141593654

using namespace cv;

#define SOCKETPATH "/home/flo/uds_socket"


const int width = 1280, height = 720;

float fixup_range(float a, float low, float upp)
{
	float tot=upp-low;
	while (a < low) a+=tot;
	while (a>= upp) a-=tot;
	return a;
}

float fixup_angle(float a)
{
	return fixup_range(a,-180,180);
}

void calc_undistort_maps(float px_per_deg, int width, int height, Mat& map1, Mat& map2)
{
	Mat camera_matrix(3,3,CV_32FC1);
	camera_matrix.at<float>(0,0)=1.0; //fx
	camera_matrix.at<float>(1,1)=1.0; //fy
	camera_matrix.at<float>(2,2)=1.0; // 1
	camera_matrix.at<float>(0,2)=1280/2.; //cx
	camera_matrix.at<float>(1,2)=720/2.; //cy
	Mat camera_matrix2 = camera_matrix.clone();
	camera_matrix2.at<float>(0,2)=width/2.; //cx
	camera_matrix2.at<float>(1,2)=height/2.; //cy

	float px_per_rad = px_per_deg * PI / 180.;

	Matx<float,1,5> dist_coeffs(-px_per_rad*px_per_rad/3.f, px_per_rad*px_per_rad*px_per_rad*px_per_rad/5.f, 0.f, 0.f, -px_per_rad*px_per_rad*px_per_rad*px_per_rad*px_per_rad*px_per_rad/7.f);

	initUndistortRectifyMap(camera_matrix, dist_coeffs, Mat(), camera_matrix2, Size(width,height), CV_32FC1, map1, map2);
}

int main(int argc, const char** argv)
{
	DroneConnection drone(SOCKETPATH);
	navdata_t navdata;

	Mat white(Size(1280,720), CV_8UC3, Scalar(255,255,255));
	Mat map1, map2;
	calc_undistort_maps(80/1280., 1280,720, map1, map2);

	float scale_factor = 0.2;
	float diag = sqrt(1280*1280+720*720);
	float px_per_deg = diag / 92.;

	int virtual_canvas_width = 360. * px_per_deg;
	int virtual_canvas_height = 90. * px_per_deg;

	int real_canvas_extra_width = sqrt(1280*1280+720*720)*scale_factor/2 + 2;
	int real_canvas_width = virtual_canvas_width * scale_factor  +  2*real_canvas_extra_width;
	int real_canvas_height = virtual_canvas_height * scale_factor;


	int total_x = 100, total_y = 00;
	float total_rot = 0.0;

	Mat frame, gray, oldgray;
	Mat screencontent(real_canvas_height,real_canvas_width, CV_8UC3);
	Mat screencontent_(real_canvas_height,real_canvas_width, CV_8UC3);
	Mat screencontent_mask(real_canvas_height,real_canvas_width, CV_8UC3);

	#define RINGBUF_SIZE 10
	ModuloRingbuffer ringbuf_x(RINGBUF_SIZE, -virtual_canvas_width/2, virtual_canvas_width/2);
	Ringbuffer ringbuf_y(RINGBUF_SIZE);
	ModuloRingbuffer ringbuf_a(RINGBUF_SIZE, -180,180);
	ModuloRingbuffer ringbuf_phi(RINGBUF_SIZE, -180,180);
	ModuloRingbuffer ringbuf_psi(RINGBUF_SIZE, -180,180);
	ModuloRingbuffer ringbuf_theta(RINGBUF_SIZE, -180,180);


	for (int i=0; i<160;i++)
	{
		Mat frame_;
		drone.get(frame_, &navdata);
		remap(frame_, frame, map1, map2, INTER_LINEAR);
		cvtColor(frame, oldgray, COLOR_BGR2GRAY);
	}

	while (waitKey(1) != 'x')
	{
		Mat frame_;
		drone.get(frame_, &navdata);
		
		//for (int i=0; i<1280; i+=50) frame_.col(i)=Scalar(0,255,255);
		//for (int i=0; i<720; i+=50) frame_.row(i)=Scalar(0,255,255);

		remap(frame_, frame, map1, map2, INTER_LINEAR);
		cvtColor(frame, gray, COLOR_BGR2GRAY);

		imshow("dingens",frame);

		Mat mat = estimateRigidTransform(gray, oldgray, false);

		float angle; int shift_x, shift_y;
		if (mat.total() > 0)
		{
			angle = atan2(mat.at<double>(0,1), mat.at<double>(0,0)) / PI * 180.;
			shift_x = mat.at<double>(0,2) - width/2 + (mat.at<double>(0,0)*width/2 + mat.at<double>(0,1)*height/2);
			shift_y = mat.at<double>(1,2) - height/2 + (mat.at<double>(1,0)*width/2 + mat.at<double>(1,1)*height/2);
		}
		else
		{
			angle = 0;
			shift_x = 0;
			shift_y = 0;
			printf("no mat!\n");
		}

		total_x += shift_x;
		total_y += shift_y;
		total_rot = fixup_angle(total_rot+angle);

		ringbuf_x.put(total_x);
		ringbuf_y.put(total_y);
		ringbuf_a.put(total_rot);
		ringbuf_phi.put(navdata.phi);
		ringbuf_psi.put(navdata.psi);
		ringbuf_theta.put(navdata.theta);

		double xdiff = fixup_range( ringbuf_x.get() - px_per_deg*ringbuf_psi.get(), -virtual_canvas_width/2, virtual_canvas_width/2);
		double ydiff = ringbuf_y.get() + px_per_deg*ringbuf_theta.get();
		double adiff = fixup_angle(ringbuf_a.get() - (-ringbuf_phi.get()));
		
		//if (fabs(xdiff) < px_per_deg) xdiff = 0.0;
		//if (fabs(ydiff) < px_per_deg) ydiff = 0.0;
		//if (fabs(adiff) < 2) adiff = 0.0;

		xdiff*=0.3;
		ydiff*=0.3;
		adiff*=0.3;
		total_x = fixup_range(total_x - xdiff, -virtual_canvas_width/2, virtual_canvas_width/2);
		total_y = total_y - ydiff;
		total_rot = fixup_angle(total_rot - adiff);
		ringbuf_x.add(-xdiff);
		ringbuf_y.add(-ydiff);
		ringbuf_a.add(-adiff);




		printf("sh:  %i\t%i\t%f\n", shift_x, shift_y, angle);
		printf("tot: %i\t%i\t%f\n", total_x, total_y, total_rot);

		Mat rotmat = getRotationMatrix2D(Point2f(width/2,height/2), total_rot, scale_factor);
		printf("dingskram %i\n", rotmat.type());
		rotmat.at<double>(0,2) += total_x*scale_factor - width/2  + real_canvas_width/2;
		rotmat.at<double>(1,2) += total_y*scale_factor - height/2 + real_canvas_height/2;

		warpAffine(frame, screencontent_    , rotmat, Size(real_canvas_width, real_canvas_height));
		warpAffine(white, screencontent_mask, rotmat, Size(real_canvas_width, real_canvas_height));

		threshold(screencontent_mask, screencontent_mask, 254, 255, THRESH_BINARY);
		erode(screencontent_mask, screencontent_mask, Mat::ones(2,2, CV_8U));
		
		Mat screencontent_mask2;
		erode(screencontent_mask, screencontent_mask2, Mat::ones(30,200, CV_8U));
		
		screencontent = (screencontent & (~screencontent_mask2)) + (screencontent_ & screencontent_mask2);
		Mat screencontent_displayed = (screencontent & (~screencontent_mask)) + (screencontent_ & screencontent_mask);

		printf("%i/%i\n", screencontent.size().width, screencontent.size().height);
		if (total_x > 0)
			screencontent.colRange(0, (2*real_canvas_extra_width)) = screencontent.colRange( real_canvas_width - 2*real_canvas_extra_width, real_canvas_width);
		else
			
			screencontent.colRange( real_canvas_width - 2*real_canvas_extra_width, real_canvas_width) = screencontent.colRange(0, (2*real_canvas_extra_width));

		imshow("screencontent", screencontent_displayed);

		oldgray = gray.clone();


	}
	
	return 0;
}
