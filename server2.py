import cv2
import os
import socket
import sys
import time
import struct






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



while True:
    # Wait for a connection
    print >>sys.stderr, 'waiting for a connection'
    connection, client_address = sock.accept()
    try:
        print >>sys.stderr, 'connection from', client_address
        
        cap = cv2.VideoCapture("flight.avi")
        logfile = open("flight.log", "r")
        while True:
            data = connection.recv(16)
            if data:
                if data=="get\n":
                    status, frame = cap.read()
                    values = logfile.readline().split()
                    phi = float(values[0])
                    theta = float(values[1])
                    psi = float(values[2])
                    batt = 100.0

                    framestr = frame.tostring()
                    #cv2.imshow("server", frame)
                    #cv2.waitKey(1)
                    lenframestr=len(framestr)
                    connection.sendall(struct.pack(">i",lenframestr)+framestr+struct.pack("@dddd", phi, theta, psi, batt));
                elif data[0:3] == "fly" and data[-1]=="\n":
                    values = data[3:-1].split()
                    print "fly ",values
            else:
                print >>sys.stderr, 'no more data from', client_address
                break
    except:
        print "Dingens!!11!1!!!"
    finally:
        # Clean up the connection
        connection.close()

