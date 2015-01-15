client2: client2.cpp lib.cpp
	g++ -std=c++11 -g -pg -nopie client2.cpp lib.cpp -lglfw -lGLEW -lGLU -lGL `pkg-config --libs opencv` -lm  -o client2
client: client.c
	gcc client.c -lX11 -lXi -lXmu -lglut -lGL -lGLU -lm  -o client
