#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <openhmd.h>
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


#define PX_PER_DEG_CANVAS 16
#define CANVAS_XDEG 450
#define CANVAS_YDEG 120
#define CANVAS_WIDTH  CANVAS_XDEG*PX_PER_DEG_CANVAS
#define CANVAS_HEIGHT CANVAS_YDEG*PX_PER_DEG_CANVAS
#define SCREEN_WIDTH  (1920/2)
#define SCREEN_HEIGHT (1080/2)
#define EYE_WIDTH (SCREEN_WIDTH/2)
#define EYE_HEIGHT SCREEN_HEIGHT
#define EYE_XDEG 30


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


//(1*tan(oeffnungswinkel/2) , 0 , 1) soll auf (XRES, YRES/2) mappen

//M*p = (alphax*tan(w/2)+XRES/2, YRES/2, 1)
//also XRES/2 / tan(w/2) = alphax


const char* drawOnCanvasFragmentSource =
	"#version 150\n"
	"uniform sampler2D texVideo;\n"
	"uniform float cam_yaw;\n"
	"uniform float cam_pitch;\n"
	"uniform float cam_roll;\n"
	"const float CAM_XRES=1280;\n"
	"const float CAM_YRES=720;\n"
	"const float CAM_XDEG=70/180.*3.141592654;\n"
	"const float CAM_FX=CAM_XRES/2.0 / tan(CAM_XDEG/2.0);\n"
	"const float margin_thickness=100;\n"
	"const mat3 cam_cal = transpose(mat3(CAM_FX, 0, CAM_XRES/2,    0, CAM_FX, CAM_YRES/2,     0,0,1));\n"
	"const mat3 math_to_opencv = transpose(mat3(0,1,0,   0,0,1,   -1,0,0));\n"
	"in vec2 Texcoord;\n"
	"out vec4 outColor;\n"
	"void main()\n"
	"{\n"
	"	// cam_rot rotates a pixel FROM world TO cam frame\n"
	"	mat3 cam_rot = transpose(mat3(1,0,0,  0,cos(cam_roll),sin(cam_roll), 0,-sin(cam_roll),cos(cam_roll))) * transpose(mat3(cos(cam_pitch),0,-sin(cam_pitch), 0,1,0, sin(cam_pitch),0,cos(cam_pitch))) * transpose(mat3(cos(cam_yaw),sin(cam_yaw),0,-sin(cam_yaw),cos(cam_yaw),0,0,0,1));\n"
//	"	mat3 cam_rot = transpose(mat3(1,0,0,  0,1,0,  0,0,1));\n"
	"	// Texcoord.xy is yaw/pitch in the unit sphere\n"
	"	vec3 point_in_world_frame = vec3( cos(Texcoord.x)*cos(Texcoord.y), sin(Texcoord.x)*cos(Texcoord.y), -sin(Texcoord.y) );\n"
//	"	if ((0.2< abs(point_in_world_frame.z)) && (abs(point_in_world_frame.z) < 0.3)) outColor=vec4(1,1,1,1); else outColor=vec4(0,0,0,1); return;"
	"	vec3 point_in_cam_frame = cam_rot * point_in_world_frame;\n"
	"	vec3 point_in_cam_pic_uniform = cam_cal * math_to_opencv * point_in_cam_frame;\n"
	"	vec2 point_in_cam_pic = point_in_cam_pic_uniform.xy / point_in_cam_pic_uniform.z;\n"
	"	if ( point_in_cam_pic_uniform.z < 0 && \n"
	"	     (0 <= point_in_cam_pic.x && point_in_cam_pic.x < CAM_XRES) && \n"
	"	     (0 <= point_in_cam_pic.y && point_in_cam_pic.y < CAM_YRES) ) \n"
	"	{\n"
	"		outColor.rgb = texture(texVideo, vec2(1.0,1.0)-point_in_cam_pic/vec2(CAM_XRES,CAM_YRES)).bgr;\n"
	"		float xmarg = min(  (min(point_in_cam_pic.x, CAM_XRES-point_in_cam_pic.x)/margin_thickness), 1.0);\n"
	"		float ymarg = min(  (min(point_in_cam_pic.y, CAM_YRES-point_in_cam_pic.y)/margin_thickness), 1.0);\n"
	"		outColor.a=xmarg*ymarg;\n"
	"	}\n"
	//"		outColor = vec4(point_in_cam_pic/vec2(CAM_XRES,CAM_YRES),-point_in_cam_pic_uniform.z/1000,1);\n"
	"	else\n"
	"		outColor = vec4(0.0,0.0,0.0,0.00);"
	"	float xxx = Texcoord.x/3.141592654*180/10+100;"
	"	float yyy = Texcoord.y/3.141592654*180/10+100;"
	"	if ( (abs(xxx- int(xxx)) < 0.03) || (abs(yyy- int(yyy)) <.03)) outColor = vec4(0.5,0.5,0.5,1);"
	"}\n";


const char* drawFromCanvasFragmentSource =
	"#version 150\n"
	"uniform sampler2D texVideo;\n"
	"uniform float eye_yaw;\n"
	"uniform float eye_pitch;\n"
	"uniform float eye_roll;\n"
	"const float aspect_ratio=1280./720.;\n"
	"const float horiz_field_of_view=80/180.*3.141592654;\n"
	"const float CAM_FX=1/2.0 / tan(horiz_field_of_view/2.0);\n"
	"const mat3 eye_cal_inv = transpose(mat3(1/CAM_FX, 0, -1/2/CAM_FX,    0, 1/CAM_FX, -1/aspect_ratio/2/CAM_FX,     0,0,1));\n"
	"const mat3 opencv_to_math = mat3(0,1,0,   0,0,1,   -1,0,0);\n"
	"in vec2 Texcoord;\n"
	"out vec4 outColor;\n"
	"void main()\n"
	"{\n"
	//"	vec3 point_in_eye_frame = opencv_to_math * eye_cal_inv * vec3( Texcoord.x,  Texcoord.y/aspect_ratio, 1 );\n"
	"	vec3 point_in_eye_frame = opencv_to_math *  vec3( vec2(Texcoord.x-0.5, (Texcoord.y-0.5)/aspect_ratio) /CAM_FX  , 1);"
	"	// eye_rot_inv rotates a pixel FROM eye TO world frame\n"
	"	mat3 eye_rot_inv = transpose( transpose(mat3(1,0,0,  0,cos(eye_roll),sin(eye_roll), 0,-sin(eye_roll),cos(eye_roll))) * transpose(mat3(cos(eye_pitch),0,-sin(eye_pitch), 0,1,0, sin(eye_pitch),0,cos(eye_pitch))) * transpose(mat3(cos(eye_yaw),sin(eye_yaw),0,-sin(eye_yaw),cos(eye_yaw),0,0,0,1)) );\n"
	"	vec3 point_in_world_frame = eye_rot_inv * point_in_eye_frame;\n"
	"	float yaw = atan( point_in_world_frame.y , point_in_world_frame.x );\n"
	"	float pitch = atan(-point_in_world_frame.z, sqrt(pow(point_in_world_frame.x,2)+pow(point_in_world_frame.y,2)));\n"
	//"	outColor = vec4( 10*yaw/2/3.1415+0.5, 10*pitch/3.1415+0.5,0.5,1);\n"
	"	outColor = texture(texVideo, vec2( yaw/2/3.141593654+0.5, pitch/3.141592654+0.5 ));\n"
	//"	outColor = texture(texVideo, Texcoord);\n"
	//"	outColor = vec4(Texcoord,0,1.0);\n"
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

const char* oculusVertexSource =
	"#version 150\n"
	"in vec2 position;\n"
	"out vec2 Screencoord;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = vec4(position, 0.0, 1.0);\n"
	"	Screencoord = position;\n"
	"}\n";
const char* oculusFragmentSource =
	"#version 150\n"
	"uniform sampler2D texVideo;\n"
	"in vec2 Screencoord;\n"
	"out vec4 outColor;\n"
	"const vec2 LeftLensCenter = vec2(0.2, 0.);\n"
	"const vec2 RightLensCenter = vec2(-0.2, 0.);\n"
	//"const vec4 HmdWarpParam   = vec4(1, 0, 0, 0);\n"
	"const vec4 HmdWarpParam   = vec4(1, 0.2, 0.1, 0);\n"
	"const float aberr_r = 0.97;\n"
	"const float aberr_b = 1.03;\n"
//	"const float aberr_r = 0.985;\n"
//	"const float aberr_b = 1.015;\n"
	"void main()\n"
	"{\n"
	"	vec2 LensCenter = Screencoord.x < 0 ? LeftLensCenter : RightLensCenter;\n"
	"	float x = (Screencoord.x > 0? Screencoord.x : (Screencoord.x+1))*2 -1;\n" // between -1 and 1
	"	float y = (Screencoord.y);\n"
	"	vec2 theta = (vec2(x,y) - LensCenter);\n"
	"	float rSq = theta.x*theta.x+theta.y*theta.y;\n"
	"	vec2 rvector = theta * (HmdWarpParam.x + HmdWarpParam.y * rSq +"
	"		HmdWarpParam.z * rSq * rSq + HmdWarpParam.w * rSq * rSq * rSq);\n"
	"	vec2 loc_r = (aberr_r * rvector + LensCenter)/vec2(2,2)+vec2(0.5,0.5);\n"
	"	vec2 loc_g = (      1 * rvector + LensCenter)/vec2(2,2)+vec2(0.5,0.5);\n"
	"	vec2 loc_b = (aberr_b * rvector + LensCenter)/vec2(2,2)+vec2(0.5,0.5);\n"
	"\n"
	"	float rval = texture(texVideo, loc_r).b;\n"
	"	float gval = texture(texVideo, loc_g).g;\n"
	"	float bval = texture(texVideo, loc_b).r;\n"
	"	outColor = vec4(rval,gval,bval,1.0);\n"
	"}\n";
const char* oculusDummyFragmentSource =
	"#version 150\n"
	"uniform sampler2D texVideo;\n"
	"in vec2 Screencoord;\n"
	"out vec4 outColor;\n"
	"void main()\n"
	"{\n"
	"	outColor = texture(texVideo, Screencoord/vec2(2,2)+vec2(0.5,0.5));\n"
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
	-1.f,  1.f,  -PI,PI/2,  // Vertex 1 (X, Y)
	 1.f,  1.f,  PI,PI/2,  // Vertex 2 (X, Y)
	 1.f, -1.f, PI,-PI/2,  // Vertex 3 (X, Y)

	 1.f, -1.f, PI,-PI/2,  // Vertex 3 (X, Y)
	-1.f, -1.f,  -PI,-PI/2,  // Vertex 4 (X, Y)
	-1.f,  1.f,  -PI,PI/2  // Vertex 1 (X, Y)
};

float vertices2[] = {
	-1.f,  1.f,  1.f,0.f,  // Vertex 1 (X, Y)
	 1.f,  1.f,  0.f,0.f,  // Vertex 2 (X, Y)
	 1.f, -1.f,  0.f,1.f,  // Vertex 3 (X, Y)

	 1.f, -1.f,  0.f,1.f,  // Vertex 3 (X, Y)
	-1.f, -1.f,  1.f,1.f,  // Vertex 4 (X, Y)
	-1.f,  1.f,  1.f,0.f   // Vertex 1 (X, Y)
};

/*float vertices2[] = {
	-1.f,  1.f,  0.f,1.f,  // Vertex 1 (X, Y)
	 1.f,  1.f,  1.f,1.f,  // Vertex 2 (X, Y)
	 1.f, -1.f,  1.f,0.f,  // Vertex 3 (X, Y)

	 1.f, -1.f,  1.f,0.f,  // Vertex 3 (X, Y)
	-1.f, -1.f,  0.f,0.f,  // Vertex 4 (X, Y)
	-1.f,  1.f,  0.f,1.f   // Vertex 1 (X, Y)
};*/

float wholescreenVertices[] = {
	-1.f, -1.f,  // Vertex 1 (X, Y)
	 1.f, -1.f,  // Vertex 2 (X, Y)
	 1.f,  1.f,  // Vertex 3 (X, Y)

	 1.f,  1.f,  // Vertex 3 (X, Y)
	-1.f,  1.f,  // Vertex 4 (X, Y)
	-1.f, -1.f   // Vertex 1 (X, Y)
};

float quadVertices[] = {
	-1.f, -1.f,  0.f,0.f,  // Vertex 1 (X, Y)
	 1.f, -1.f,  1.f,0.f,  // Vertex 2 (X, Y)
	 1.f,  1.f, 1.f,1.f,  // Vertex 3 (X, Y)

	 1.f,  1.f, 1.f,1.f,  // Vertex 3 (X, Y)
	-1.f,  1.f,  0.f,1.f,  // Vertex 4 (X, Y)
	-1.f, -1.f,  0.f,0.f  // Vertex 1 (X, Y)
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

void calcVerticesRotated2(float xshift, float yshift, float angle, float* v)
{
	v+=2;
	Point2f pt;
	float xd = EYE_XDEG;
	float yd = -EYE_XDEG * EYE_HEIGHT / EYE_WIDTH;
	pt = Point2f( -cos(angle)*xd/2 + sin(angle)*yd/2, +sin(angle)*xd/2 + cos(angle)*yd/2 );
	v[0]=v[20]=(float) ( pt.x + xshift) / CANVAS_XDEG + 0.5;
	v[1]=v[21]=(float) ( pt.y + yshift) / CANVAS_YDEG + 0.5;
	v[8]=v[12]=(float) (-pt.x + xshift) / CANVAS_XDEG + 0.5;
	v[9]=v[13]=(float) (-pt.y + yshift) / CANVAS_YDEG + 0.5;

	pt = Point2f( cos(angle)*xd/2 + sin(angle)*yd/2, -sin(angle)*xd/2 + cos(angle)*yd/2 );
	v[4] =(float) ( pt.x + xshift) / CANVAS_XDEG + 0.5;
	v[5] =(float) ( pt.y + yshift) / CANVAS_YDEG + 0.5;
	v[16]=(float) (-pt.x + xshift) / CANVAS_XDEG + 0.5;
	v[17]=(float) (-pt.y + yshift) / CANVAS_YDEG + 0.5;
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

GLuint newCanvasShaderProgram(GLuint vao, GLuint vbo)
{
	GLuint vertexShader, fragmentShader, shaderProgram;
	compileShaderProgram(justDrawASpriteVertexSource, drawOnCanvasFragmentSource, vertexShader, fragmentShader, shaderProgram);


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
GLuint newEyeShaderProgram(GLuint vao, GLuint vbo)
{
	GLuint vertexShader, fragmentShader, shaderProgram;
	compileShaderProgram(justDrawASpriteVertexSource, drawFromCanvasFragmentSource, vertexShader, fragmentShader, shaderProgram);


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

GLuint newOculusShaderProgram(GLuint vao, GLuint vbo)
{
	GLuint vertexShader, fragmentShader, shaderProgram;
	compileShaderProgram(oculusVertexSource, oculusDummyFragmentSource, vertexShader, fragmentShader, shaderProgram);


	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	
	// set up shaders
	GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
	glEnableVertexAttribArray(posAttrib);
	glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), 0);


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

void genFramebuffer(GLuint& frameBuffer, GLuint& texColorBuffer, int w, int h, bool wrap)
{
	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	
	glGenTextures(1, &texColorBuffer);
	glBindTexture(GL_TEXTURE_2D, texColorBuffer);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap?GL_REPEAT:GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap?GL_REPEAT:GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);
}


int init_ohmd(ohmd_context** ctx_, ohmd_device** hmd_)
{
	ohmd_context* ctx = ohmd_ctx_create();

	// Probe
	int num_devices = ohmd_ctx_probe(ctx);
	if(num_devices < 0)
	{
		printf("device probing failed: %s\n", ohmd_ctx_get_error(ctx));
		return -1;
	}

	printf("i've got %d devices\n\n", num_devices);

	ohmd_device* hmd = ohmd_list_open_device(ctx, 0);
	
	if(!hmd)
	{
		printf("open failed! %s\n", ohmd_ctx_get_error(ctx));
		return -1;
	}

	// Print hardware information for the opened device
	int ivals[2];
	ohmd_device_geti(hmd, OHMD_SCREEN_HORIZONTAL_RESOLUTION, ivals);
	ohmd_device_geti(hmd, OHMD_SCREEN_VERTICAL_RESOLUTION, ivals + 1);
	printf("resolution:         %i x %i\n", ivals[0], ivals[1]);

	*ctx_=ctx;
	*hmd_=hmd;

	return 0;
}



int main(int argc, const char** argv)
{

	GLFWwindow* window = initOpenGL();


	GLuint vaoCanvas, vaoEye, vaoQuad, vaoWholescreenQuad;
	glGenVertexArrays(1, &vaoCanvas);
	glGenVertexArrays(1, &vaoEye);
	glGenVertexArrays(1, &vaoQuad);
	glGenVertexArrays(1, &vaoWholescreenQuad);


	GLuint vboCanvas, vboEye, vboQuad, vboWholescreenQuad;
	glGenBuffers(1, &vboCanvas);
	glGenBuffers(1, &vboEye);
	glGenBuffers(1, &vboQuad);
	glGenBuffers(1, &vboWholescreenQuad);

	glBindBuffer(GL_ARRAY_BUFFER, vboCanvas);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, vboEye);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices2), vertices2, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, vboQuad);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, vboWholescreenQuad);
	glBufferData(GL_ARRAY_BUFFER, sizeof(wholescreenVertices), wholescreenVertices, GL_STATIC_DRAW);




	
	// compile shaders
	GLuint shaderProgram = justDrawASpriteShaderProgram(vaoCanvas, vboCanvas);
	GLuint quadShaderProgram = justDrawASpriteShaderProgram(vaoQuad, vboQuad);
	GLuint oculusShaderProgram = newOculusShaderProgram(vaoWholescreenQuad, vboWholescreenQuad);
	GLuint drawOnCanvasProgram = newCanvasShaderProgram(vaoCanvas, vboCanvas);
	GLint uniCamYaw = glGetUniformLocation(drawOnCanvasProgram, "cam_yaw");
	GLint uniCamPitch = glGetUniformLocation(drawOnCanvasProgram, "cam_pitch");
	GLint uniCamRoll = glGetUniformLocation(drawOnCanvasProgram, "cam_roll");
	GLuint drawFromCanvasProgram = newEyeShaderProgram(vaoEye, vboEye);
	GLint uniEyeYaw = glGetUniformLocation(drawFromCanvasProgram, "eye_yaw");
	GLint uniEyePitch = glGetUniformLocation(drawFromCanvasProgram, "eye_pitch");
	GLint uniEyeRoll = glGetUniformLocation(drawFromCanvasProgram, "eye_roll");

	// texture
	GLuint texVideo;
	glGenTextures(1, &texVideo);
	glBindTexture(GL_TEXTURE_2D, texVideo);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	


// Framebuffer stuff
	GLuint canvasFB, canvasTex;
	genFramebuffer(canvasFB, canvasTex, CANVAS_WIDTH, CANVAS_HEIGHT, true);

	GLuint eyeFB, eyeTex;
	genFramebuffer(eyeFB, eyeTex, EYE_WIDTH, EYE_HEIGHT, false);







glEnable (GL_BLEND);
glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);





	ohmd_context* my_ohmd_context;
	ohmd_device* my_ohmd_device;
	if (init_ohmd(&my_ohmd_context, &my_ohmd_device)) return 1;





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

	#define RINGBUF_SIZE 4
	ModuloRingbuffer ringbuf_x(RINGBUF_SIZE, -virtual_canvas_width/2, virtual_canvas_width/2);
	Ringbuffer ringbuf_y(RINGBUF_SIZE);
	ModuloRingbuffer ringbuf_a(RINGBUF_SIZE, -180,180);
	ModuloRingbuffer ringbuf_phi(RINGBUF_SIZE, -180,180);
	ModuloRingbuffer ringbuf_psi(RINGBUF_SIZE, -180,180);
	ModuloRingbuffer ringbuf_psi2(40, -180,180);
	ModuloRingbuffer ringbuf_theta2(40, -180,180);
	ModuloRingbuffer ringbuf_theta(RINGBUF_SIZE, -180,180);


	#define DELAY_SIZE 6
	Ringbuffer delay_phi(DELAY_SIZE);   // should delay sensor data by ~0.2sec
	Ringbuffer delay_psi(DELAY_SIZE);   // that's the amount the video lags behind
	Ringbuffer delay_theta(DELAY_SIZE); // the sensor data


	for (int i=0; i<400;i++)
	{
		drone.get(frame_, &navdata);
		remap(frame_, frame, map1, map2, INTER_LINEAR);
		cvtColor(frame, oldgray, COLOR_BGR2GRAY);
	}

	char key;
	int adjust_phi=10;
	while ((key=waitKey(1)) != 'x')
	{
		printf("\033[H");


		drone.get(frame_, &navdata);
		delay_phi.put(navdata.phi);
		delay_psi.put(navdata.psi);
		delay_theta.put(navdata.theta);
		navdata.phi = delay_phi.front();
		navdata.psi = delay_psi.front();
		navdata.theta = delay_theta.front();

		navdata.phi = fixup_angle(navdata.phi + adjust_phi);

		if (key=='q') adjust_phi++;
		if (key=='w') adjust_phi--;
		
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

		total_x +=  cos(total_rot*PI/180.)*shift_x + sin(total_rot*PI/180.)*shift_y;
		total_y += -sin(total_rot*PI/180.)*shift_x + cos(total_rot*PI/180.)*shift_y;
		total_rot = fixup_angle(total_rot+angle);

		ringbuf_x.put(total_x);
		ringbuf_y.put(total_y);
		ringbuf_a.put(total_rot);
		ringbuf_phi.put(navdata.phi);
		ringbuf_psi.put(navdata.psi);
		ringbuf_psi2.put(navdata.psi);
		ringbuf_theta2.put(navdata.theta);
		ringbuf_theta.put(navdata.theta);

		double xdiff = fixup_range( ringbuf_x.get() - px_per_deg*ringbuf_psi.get(), -virtual_canvas_width/2, virtual_canvas_width/2);
		double ydiff = ringbuf_y.get() + px_per_deg*ringbuf_theta.get();
		double adiff = fixup_angle(ringbuf_a.get() - (-ringbuf_phi.get()));
		
		//if (fabs(xdiff) < px_per_deg) xdiff = 0.0;
		//if (fabs(ydiff) < px_per_deg) ydiff = 0.0;
		//if (fabs(adiff) < 2) adiff = 0.0;

		xdiff*=0.02;
		ydiff*=0.02;
		adiff*=0.5;
		total_x = fixup_range(total_x - xdiff, -virtual_canvas_width/2, virtual_canvas_width/2);
		total_y = total_y - ydiff;
		total_rot = fixup_angle(total_rot - adiff);
		ringbuf_x.add(-xdiff);
		ringbuf_y.add(-ydiff);
		ringbuf_a.add(-adiff);
		
		//total_x = navdata.psi * px_per_deg;
		//total_y = - navdata.theta * px_per_deg;
		//total_rot = -navdata.phi;









		ohmd_ctx_update(my_ohmd_context);

		float quat[4],quat_[4];
		ohmd_device_getf(my_ohmd_device, OHMD_ROTATION_QUAT, quat_);
		quat[0]=quat_[0];
		quat[1]=quat_[1];
		quat[2]=quat_[3];
		quat[3]=quat_[2];
		
		float oculus_yaw = atan2( 2.0* (quat[1]*quat[2]+quat[0]*quat[3]), (quat[0]*quat[0]+quat[1]*quat[1]-quat[2]*quat[2]-quat[3]*quat[3]) );
		float oculus_pitch = asin(2.0*(quat[0]*quat[2]-quat[1]*quat[3]));
		float oculus_roll = -atan2(2.0*(quat[2]*quat[3]+quat[0]*quat[1]), -(quat[0]*quat[0]-quat[1]*quat[1]-quat[2]*quat[2]+quat[3]*quat[3]));

		printf("oculus yaw, pitch, roll = \t%f\t%f\t%f\n", oculus_yaw*180/PI, oculus_pitch*180/PI, oculus_roll*180/PI);









		glBindTexture(GL_TEXTURE_2D, texVideo);
		Mat frame_gl = frame.clone();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame_gl.size().width, frame_gl.size().height, 0, GL_RGB, GL_UNSIGNED_BYTE, frame_gl.ptr<unsigned char>(0));


		calcVerticesRotated2(ringbuf_psi2.get(),10,0,quadVertices);

		glBindBuffer(GL_ARRAY_BUFFER, vboQuad);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);


		glBindFramebuffer(GL_FRAMEBUFFER, canvasFB);
		glViewport(0,0,CANVAS_WIDTH,CANVAS_HEIGHT);
		glBindVertexArray(vaoCanvas);
		glUseProgram(drawOnCanvasProgram);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texVideo);
		glUniform1f(uniCamYaw,(float)total_x/virtual_canvas_width*2*PI);
		glUniform1f(uniCamPitch,-(float)total_y/px_per_deg/180.*PI);
		glUniform1f(uniCamRoll,-total_rot/180.*PI);
		glDrawArrays(GL_TRIANGLES, 0, 6);


		glBindFramebuffer(GL_FRAMEBUFFER, eyeFB);
		glViewport(0,0,EYE_WIDTH,EYE_HEIGHT);
		glBindVertexArray(vaoEye);
		glUseProgram(drawFromCanvasProgram);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, canvasTex);
		/*glUniform1f(uniEyeYaw,3.1415+ringbuf_psi2.get()/180.*PI);
		glUniform1f(uniEyePitch,-ringbuf_theta2.get()/180.*PI);
		//glUniform1f(uniEyeYaw,3.1415+(float)total_x/virtual_canvas_width*2*PI);
		//glUniform1f(uniEyePitch,(float)total_y/px_per_deg/180.*PI);
		glUniform1f(uniEyeRoll,0);*/
		glUniform1f(uniEyeYaw,oculus_yaw);
		glUniform1f(uniEyePitch,-oculus_pitch);
		glUniform1f(uniEyeRoll,-oculus_roll);
		glDrawArrays(GL_TRIANGLES, 0, 6);


		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0,0,SCREEN_WIDTH, SCREEN_HEIGHT);
		glBindVertexArray(vaoWholescreenQuad);
		glUseProgram(oculusShaderProgram);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, eyeTex);
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
