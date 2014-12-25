client2: client2.c
	g++ client2.c `pkg-config --libs opencv` -lm  -o client2
client: client.c
	gcc client.c -lX11 -lXi -lXmu -lglut -lGL -lGLU -lm  -o client
