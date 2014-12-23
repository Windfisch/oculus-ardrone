import cv2
import os
import socket
import sys

def encode_int(i):
    i = int(i)
    return chr( (i/(2**24))%256) + chr( (i/(2**16))%256 ) +\
            chr( (i/(2**8))%256) + chr(i%256)


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
        cap = cv2.VideoCapture("/home/flo/outvid2.avi")

        # Receive the data in small chunks and retransmit it
        while True:
            data = connection.recv(16)
            if data:
                if data=="get\n":
                    status, frame = cap.read()
                    framestr = frame.tostring()
                    lenframestr=len(framestr)
                    connection.sendall(encode_int(lenframestr)+framestr);
                    #cv2.imshow("img",frame)
                    #cv2.waitKey(1)
            else:
                print >>sys.stderr, 'no more data from', client_address
                break
    except:
        print "Dingens!!11!1!!!"
            
    finally:
        # Clean up the connection
        connection.close()

while True:
    status, frame = cap.read()
    cv2.imshow("img",frame)
    cv2.waitKey(20)

