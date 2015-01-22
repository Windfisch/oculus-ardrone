#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

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


#define PX_PER_DEG 16.0


#define PX_PER_DEG_WINDOWSIZE 3
#define PX_PER_DEG_CANVAS 16
#define CANVAS_XDEG 450
#define CANVAS_YDEG 120
#define CANVAS_WIDTH  CANVAS_XDEG*PX_PER_DEG_CANVAS
#define CANVAS_HEIGHT CANVAS_YDEG*PX_PER_DEG_CANVAS
#define SCREEN_WIDTH  CANVAS_XDEG*PX_PER_DEG_WINDOWSIZE
#define SCREEN_HEIGHT CANVAS_YDEG*PX_PER_DEG_WINDOWSIZE



const char* justDrawASpriteVertexSource =
	"#version 150\n"
	"in vec2 position;\n"
	"in vec2 texcoord;\n"
	"out vec2 Texcoord;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = vec4(position, 0.0, 1.0);\n"
	"	Texcoord = texcoord;\n"
	"}\n";

const char* justDrawASpriteFragmentSource =
	"#version 150\n"
	"uniform sampler2D texVideo;\n"
	"in vec2 Texcoord;\n"
	"out vec4 outColor;\n"
	"void main()\n"
	"{\n"
	"	outColor = texture(texVideo, Texcoord);\n"
	"}\n";

const char* justDrawASpriteFragmentSourceGray =
	"#version 150\n"
	"uniform sampler2D texVideo;\n"
	"in vec2 Texcoord;\n"
	"out vec4 outColor;\n"
	"void main()\n"
	"{\n"
	"	float gray = (texture(texVideo, Texcoord).r+ texture(texVideo, Texcoord).g + texture(texVideo, Texcoord).b)/3.;\n"
	"	if (Texcoord.x < 0.5) gray=1.0-gray;\n"
	"	outColor = vec4(gray,gray,gray,1.0);\n"
	"}\n";


float vertices[] = {
	-1.f,  1.f,  0.0f,0.0f,  // Vertex 1 (X, Y)
	 1.f,  1.f,  1.0f,0.0f,  // Vertex 2 (X, Y)
	 1.f, -1.f, 1.0f,1.0f,  // Vertex 3 (X, Y)

	 1.f, -1.f, 1.0f,1.0f,  // Vertex 3 (X, Y)
	-1.f, -1.f,  0.0f,1.0f,  // Vertex 4 (X, Y)
	-1.f,  1.f,  0.0f,0.0f  // Vertex 1 (X, Y)
};


float quadVertices[] = {
	-1.f, -1.f,  0.4f,0.4f,  // Vertex 1 (X, Y)
	 1.f, -1.f,  0.6f,0.4f,  // Vertex 2 (X, Y)
	 1.f,  1.f, 0.6f,0.6f,  // Vertex 3 (X, Y)

	 1.f,  1.f, 0.6f,0.6f,  // Vertex 3 (X, Y)
	-1.f,  1.f,  0.4f,0.6f,  // Vertex 4 (X, Y)
	-1.f, -1.f,  0.4f,0.4f  // Vertex 1 (X, Y)
};


void calcVerticesRotated(int xshift, int yshift, float angle, float* v)
{
	Point2f pt;
	pt = Point2f( -cos(angle)*1280./2 + sin(angle)*720./2, +sin(angle)*1280./2 + cos(angle)*720./2 );
	v[0]=v[20]=(float) ( pt.x + xshift) / PX_PER_DEG / CANVAS_XDEG * 2;
	v[1]=v[21]=(float) ( pt.y + yshift) / PX_PER_DEG / CANVAS_YDEG * 2;
	v[8]=v[12]=(float) (-pt.x + xshift) / PX_PER_DEG / CANVAS_XDEG * 2;
	v[9]=v[13]=(float) (-pt.y + yshift) / PX_PER_DEG / CANVAS_YDEG * 2;

	pt = Point2f( cos(angle)*1280./2 + sin(angle)*720./2, -sin(angle)*1280./2 + cos(angle)*720./2 );
	v[4] =(float) ( pt.x + xshift) / PX_PER_DEG / CANVAS_XDEG * 2;
	v[5] =(float) ( pt.y + yshift)/ PX_PER_DEG / CANVAS_YDEG * 2;
	v[16]=(float) (-pt.x + xshift)/ PX_PER_DEG / CANVAS_XDEG * 2;
	v[17]=(float) (-pt.y + yshift) / PX_PER_DEG / CANVAS_YDEG * 2;
}


void compileShaderProgram(const GLchar* vertSrc, const GLchar* fragSrc, GLuint& vertexShader, GLuint& fragmentShader, GLuint& shaderProgram)
{
	GLint status;
	char buffer[512];
	
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertSrc, NULL);
	glCompileShader(vertexShader);
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
	glGetShaderInfoLog(vertexShader, 512, NULL, buffer);
	printf("vertex shader log:\n%s\n\n\n", buffer);

	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragSrc, NULL);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);
	glGetShaderInfoLog(fragmentShader, 512, NULL, buffer);
	printf("fragment shader log:\n%s\n\n\n", buffer);

	// assemble shader program
	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glBindFragDataLocation(shaderProgram, 0, "outColor"); // not neccessary
	glLinkProgram(shaderProgram);
}

GLuint justDrawASpriteShaderProgram(GLuint vao, GLuint vbo, bool gray=false)
{
	GLuint vertexShader, fragmentShader, shaderProgram;
	compileShaderProgram(justDrawASpriteVertexSource, gray? justDrawASpriteFragmentSourceGray : justDrawASpriteFragmentSource, vertexShader, fragmentShader, shaderProgram);


	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	
	// set up shaders
	GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
	glEnableVertexAttribArray(posAttrib);
	glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);


	GLint texAttrib = glGetAttribLocation(shaderProgram, "texcoord");
	glEnableVertexAttribArray(texAttrib);
	glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));

	return shaderProgram;
}

GLFWwindow* initOpenGL()
{
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OpenGL", NULL, NULL); // Windowed
	// GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL", glfwGetPrimaryMonitor(), nullptr); // Fullscreen

	glfwMakeContextCurrent(window);

	glewExperimental = GL_TRUE;
	glewInit();
	
	return window;
}







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
	Mat camera_matrix= Mat::zeros(3,3,CV_32FC1);
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

	GLFWwindow* window = initOpenGL();


	GLuint vaoCanvas, vaoQuad;
	glGenVertexArrays(1, &vaoCanvas);
	glGenVertexArrays(1, &vaoQuad);


	GLuint vboCanvas, vboQuad;
	glGenBuffers(1, &vboCanvas);
	glGenBuffers(1, &vboQuad);

	calcVerticesRotated(0,0,PI/2,vertices);

	glBindBuffer(GL_ARRAY_BUFFER, vboCanvas);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, vboQuad);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);




	
	// compile shaders
	GLuint shaderProgram = justDrawASpriteShaderProgram(vaoCanvas, vboCanvas);
	GLuint quadShaderProgram = justDrawASpriteShaderProgram(vaoQuad, vboQuad);


	// texture
	GLuint texVideo;
	glGenTextures(1, &texVideo);
	glBindTexture(GL_TEXTURE_2D, texVideo);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	


// Framebuffer stuff
	GLuint frameBuffer;
	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	
	GLuint texColorBuffer;
	glGenTextures(1, &texColorBuffer);
	glBindTexture(GL_TEXTURE_2D, texColorBuffer);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, CANVAS_WIDTH, CANVAS_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);













	DroneConnection drone(SOCKETPATH);
	navdata_t navdata;

	Mat white(Size(1280,720), CV_8UC3, Scalar(255,255,255));
	Mat map1(Size(width,height), CV_32FC1), map2(Size(width,height), CV_32FC1);
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

	Mat frame(Size(1280,720), CV_8UC3), frame_(Size(1280,720), CV_8UC3), gray, oldgray;
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


	for (int i=0; i<400;i++)
	{
		drone.get(frame_, &navdata);
		remap(frame_, frame, map1, map2, INTER_LINEAR);
		cvtColor(frame, oldgray, COLOR_BGR2GRAY);
	}

	while (waitKey(1) != 'x')
	{
		drone.get(frame_, &navdata);
		
		//for (int i=0; i<1280; i+=50) frame_.col(i)=Scalar(0,255,255);
		//for (int i=0; i<720; i+=50) frame_.row(i)=Scalar(0,255,255);

		remap(frame_, frame, map1, map2, INTER_LINEAR);
		cvtColor(frame, gray, COLOR_BGR2GRAY);








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

		xdiff*=0.0;
		ydiff*=0.0;
		adiff*=0.1;
		total_x = fixup_range(total_x - xdiff, -virtual_canvas_width/2, virtual_canvas_width/2);
		total_y = total_y - ydiff;
		total_rot = fixup_angle(total_rot - adiff);
		ringbuf_x.add(-xdiff);
		ringbuf_y.add(-ydiff);
		ringbuf_a.add(-adiff);













		glBindTexture(GL_TEXTURE_2D, texVideo);
		Mat frame_gl = frame.clone();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame_gl.size().width, frame_gl.size().height, 0, GL_RGB, GL_UNSIGNED_BYTE, frame_gl.ptr<unsigned char>(0));


		calcVerticesRotated(total_x, -total_y,-total_rot*PI/180.,vertices);

		glBindBuffer(GL_ARRAY_BUFFER, vboCanvas);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);


		glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
		glViewport(0,0,CANVAS_WIDTH,CANVAS_HEIGHT);
		glBindVertexArray(vaoCanvas);
		glUseProgram(shaderProgram);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texVideo);
		glDrawArrays(GL_TRIANGLES, 0, 6);


		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0,0,SCREEN_WIDTH,SCREEN_HEIGHT);
		glBindVertexArray(vaoQuad);
		glUseProgram(quadShaderProgram);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texColorBuffer);
		glDrawArrays(GL_TRIANGLES, 0, 6);



		glfwSwapBuffers(window);
		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GL_TRUE);

		imshow("dingens",frame_);

















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
	
	//glDeleteFramebuffers(1, &frameBuffer);

	glfwTerminate();
	return 0;
}
