import libardrone.libardrone as libardrone
import pygame
import cv2
import os
import socket
import sys
import threading
import time

def encode_int(i):
    i = int(i)
    return chr( (i/(2**24))%256) + chr( (i/(2**16))%256 ) +\
            chr( (i/(2**8))%256) + chr(i%256)


class ServerThread(threading.Thread):
    def run(self):
        while True:
            # Wait for a connection
            print >>sys.stderr, 'waiting for a connection'
            connection, client_address = sock.accept()
            try:
                print >>sys.stderr, 'connection from', client_address

                # Receive the data in small chunks and retransmit it
                while True:
                    data = connection.recv(16)
                    if data:
                        if data=="get\n":
                            lock.acquire()
                            framestr = frame.tostring()
                            lenframestr=len(framestr)
                            connection.sendall(encode_int(lenframestr)+framestr);
                            lock.release()
                    else:
                        print >>sys.stderr, 'no more data from', client_address
                        break
            except:
                print "Dingens!!11!1!!!"
            finally:
                # Clean up the connection
                connection.close()





server_address = '/home/flo/uds_socket'
try:
    os.unlink(server_address)
except OSError:
    if os.path.exists(server_address):
        raise

# Create a UDS socket
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

# Bind the socket to the port
print >>sys.stderr, 'starting up on %s' % server_address
sock.bind(server_address)

# Listen for incoming connections
sock.listen(1)


try:
    pygame.init()
    pygame.joystick.init()
    js=pygame.joystick.Joystick(0)
    js.init()
    js_angle_shift = 0.0
except:
    print "meeeeh"


drone = libardrone.ARDrone(True, True)
drone.reset()


serverthread=ServerThread()
lock=threading.Lock()

cap = cv2.VideoCapture("/home/flo/kruschkram/out2.avi")
status, frame = cap.read()


writer = cv2.VideoWriter("flight.avi",cv2.VideoWriter_fourcc(*'MP42'),25,(1280,720),1)
logfile = open("flight.log", "w")




serverthread.start()
while True:
    print "hello world :)"
    lock.acquire()
    rawimg = drone.get_image()
    frame = cv2.cvtColor(rawimg, cv2.COLOR_BGR2RGB)
    phi = drone.navdata.get(0, dict()).get('phi',1337)
    theta = drone.navdata.get(0, dict()).get('theta',1337)
    psi = drone.navdata.get(0, dict()).get('psi',1337)
    lock.release()

    cv2.imshow("frame", frame)
    writer.write(frame)
    logfile.write(str(phi)+"\t"+str(theta)+"\t"+str(psi)+"\n")
    print phi,theta,psi

    cv2.waitKey(50)

