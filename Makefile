client2: client2.cpp lib.cpp ringbuf.h
	g++ -std=c++11 -g client2.cpp lib.cpp -lglfw -lGLEW -lGLU -lGL `pkg-config --libs opencv` -lm  -o client2
client: client.c
	gcc client.c -lX11 -lXi -lXmu -lglut -lGL -lGLU -lm  -o client

simple: simple.o contrib/OpenHMD/src/.libs/libopenhmd.a
	gcc -std=gnu99 -g -O2 -o simple simple.o  contrib/OpenHMD/src/.libs/libopenhmd.a -lhidapi-libusb -lrt -lpthread -lm

simple.o: simple.c
	gcc -std=gnu99   -Wall  -DOHMD_STATIC -Icontrib/OpenHMD/include/  -g -O2  -c -o simple.o simple.c

contrib/OpenHMD/src/.libs/libopenhmd.a:
	cd contrib/OpenHMD && ./configure --enable-static=yes && make -j5

clean:
	rm -f simple.o simple

distclean: clean
	cd contrib/OpenHMD && make distclean
