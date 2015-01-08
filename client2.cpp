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

int main(int argc, const char** argv)
{
	DroneConnection drone(SOCKETPATH);
	navdata_t navdata;

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
	Mat screencontent(real_canvas_height,real_canvas_width, CV_32FC3);

	#define RINGBUF_SIZE 10
	ModuloRingbuffer ringbuf_x(RINGBUF_SIZE, -virtual_canvas_width/2, virtual_canvas_width/2);
	Ringbuffer ringbuf_y(RINGBUF_SIZE);
	ModuloRingbuffer ringbuf_a(RINGBUF_SIZE, -180,180);
	ModuloRingbuffer ringbuf_phi(RINGBUF_SIZE, -180,180);
	ModuloRingbuffer ringbuf_psi(RINGBUF_SIZE, -180,180);
	ModuloRingbuffer ringbuf_theta(RINGBUF_SIZE, -180,180);


	for (int i=0; i<160;i++)
	{
		drone.get(frame, &navdata);
		cvtColor(frame, oldgray, COLOR_BGR2GRAY);
	}

	while (waitKey(1) != 'x')
	{
		drone.get(frame, &navdata);

		imshow("dingens",frame);

		cvtColor(frame, gray, COLOR_BGR2GRAY);
		Mat mat = estimateRigidTransform(gray, oldgray, false);
		printf("_____ %i\n", mat.type());

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

		warpAffine(frame, screencontent, rotmat, Size(real_canvas_width, real_canvas_height));

		printf("%i/%i\n", screencontent.size().width, screencontent.size().height);
		if (total_x > 0)
			screencontent.colRange(0, (2*real_canvas_extra_width)) = screencontent.colRange( real_canvas_width - 2*real_canvas_extra_width, real_canvas_width);
		else
			
			screencontent.colRange( real_canvas_width - 2*real_canvas_extra_width, real_canvas_width) = screencontent.colRange(0, (2*real_canvas_extra_width));

		imshow("screencontent", screencontent);

		oldgray = gray.clone();


	}
	
	return 0;
}
