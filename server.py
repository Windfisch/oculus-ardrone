import libardrone.libardrone as libardrone
import pygame
import cv2
import os
import socket
import sys
import threading
import time
import struct

def putStatusText(img, text, pos, activated):
    cv2.putText(img, text, pos, cv2.FONT_HERSHEY_PLAIN, 1, (0,0,255) if activated else (127,127,127), 2 if activated else 1)

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

                while True:
                    data = connection.recv(16)
                    if data:
                        if data=="get\n":
                            lock.acquire()
                            framestr = global_frame.tostring()
                            lenframestr=len(global_framestr)
                            connection.sendall(struct.pack(">i",lenframestr)+framestr+struct.pack("@dddd", global_phi, global_theta, global_psi, global_batt));
                            lock.release()
                        elif data[0:3] == "fly" and data[-1]=="\n":
                            values = data[3:-1].split()
                            lock.acquire()
                            global_cmd_x = float(values[0])
                            global_cmd_y = float(values[1])
                            global_cmd_z = float(values[2])
                            global_cmd_rot = float(values[3])
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

no_flight = False

try:
    pygame.init()
    pygame.joystick.init()
    js=pygame.joystick.Joystick(0)
    js.init()
    js_angle_shift = 0.0
except:
    print "no joystick! disabling flight controls"
    no_flight = True

manual_override_xy = True
manual_override_z = True
manual_override_rot = True


drone = libardrone.ARDrone(True, True)
drone.reset()


serverthread=ServerThread()
lock=threading.Lock()


writer = cv2.VideoWriter("flight.avi",cv2.VideoWriter_fourcc(*'MP42'),25,(1280,720),1)
logfile = open("flight.log", "w")


global_phi = 0.
global_theta = 0.
global_psi = 0.
global_batt = 0.
global_frame =  None

serverthread.start()
while True:
    if no_flight == False:
        btn_leftshoulder =  js.get_button(4) or js.get_button(6)
        btn_rightshoulder = js.get_button(5) or js.get_button(7)
        btn_thumb = js.get_button(0) or js.get_button(1) or js.get_button(2) or js.get_button(3)
        btn_all = js.get_button(0) and js.get_button(1) and js.get_button(2) and js.get_button(3)
        btn_readjust = js.get_button(10)

        if btn_thumb:
            drone.land()
            manual_override_xy = True
            manual_override_z = True
            manual_override_rot = True
        if btn_leftshoulder and btn_rightshoulder and js.get_button(10):
            drone.takeoff()
            manual_override_xy = True
            manual_override_z = True
            manual_override_rot = True
        if btn_all:
            drone.reset()
            manual_override_xy = True
            manual_override_z = True
            manual_override_rot = True
        if btn_readjust:
            js_angle_shift = drone.navdata.get(0, dict()).get('psi',0)

        rel_angle = (drone.navdata.get(0, dict()).get('psi',0) - js_angle_shift)/180.*math.pi

        js_x = js.get_axis(0)
        js_y = js.get_axis(1)
        js_z = -js.get_axis(4)
        js_rot = js.get_axis(3)
        js_radius =  math.sqrt(js_x**2 + js_y**2)
        if btn_leftshoulder==0:
            js_x, js_y =  ( js_x * cos(rel_angle) + js_y * sin(rel_angle) )   ,  ( -js_x * sin(rel_angle) + js_y * cos(rel_angle) )
        
        js_hover = (btn_rightshoulder==0 and (js_radius <= 0.01))

        if (js_radius > OVERRIDE_THRESHOLD): manual_override_xy = True
        if (abs(js_z) > OVERRIDE_THRESHOLD): manual_override_z = True
        if (abs(js_rot) > OVERRIDE_THRESHOLD): manual_override_rot = True

        if manual_override_xy:
            actual_hover, actual_x, actual_y = js_hover, js_x, js_y
        else:
            actual_hover, actual_x, actual_y = global_cmd_hover, global_cmd_x, global_cmd_y

        if manual_override_z:
            actual_z = js_z
        else:
            actual_z = global_cmd_z

        if manual_override_rot:
            actual_rot = js_rot
        else:
            actual_rot = global_cmd_rot

        drone.move_freely( not actual_hover  , actual_x, actual_y, actual_z, actual_rot) 
    
    
    
    lock.acquire()
    rawimg = drone.get_image()
    global_frame = cv2.cvtColor(rawimg, cv2.COLOR_BGR2RGB)
    global_phi = drone.navdata.get(0, dict()).get('phi',1337)
    global_theta = drone.navdata.get(0, dict()).get('theta',1337)
    global_psi = drone.navdata.get(0, dict()).get('psi',1337)
    global_batt = drone.navdata.get(0, dict()).get('batt',1337)
    lock.release()

    smallframe = cv2.resize(global_frame, (640,480))
    cv2.rectangle(smallframe, (0,0), (640,30), (255,255,255), -1)
    cv2.putText(smallframe, "override", (0,20), cv2.FONT_HERSHEY_PLAIN, 1, (255,0,0))
    putStatusText(smallframe, "XY", (100,20), manual_override_xy)
    putStatusText(smallframe, "height", (200,20), manual_override_z)
    putStatusText(smallframe, "rotation", (300,20), manual_override_rot)
    #cv2.putText(smallframe, "XY", (100,20), cv2.FONT_HERSHEY_PLAIN, 1, (0,0,255) if manual_override_xy else (127,127,127))
    #cv2.putText(smallframe, "height", (250,20), cv2.FONT_HERSHEY_PLAIN, 1, (0,0,255)   f manual_override_z else (127,127,127))
    #cv2.putText(smallframe, "rotation", (400,20), cv2.FONT_HERSHEY_PLAIN, 1, (0,0,255) if manual_override_rot else (127,127,127))
    cv2.imshow("frame", smallframe)
    writer.write(global_frame)
    logfile.write(str(global_phi)+"\t"+str(global_theta)+"\t"+str(global_psi)+"\n")
    logfile.flush()



    key = cv2.waitKey(10) & 0xFF

    if key == ord("t"):
        drone.trim()

    if key == ord("z"):
        drone.set_max_vz(750.0000)
        drone.set_max_rotspeed(1.0)
        drone.set_max_angle(0.1)
        print "slow"
    elif key == ord("a"):
        drone.set_max_vz(10000.0000)
        drone.set_max_rotspeed(10)
        drone.set_max_angle(0.6)
        print "fast"
    
    if key == ord("1"):
        manual_override_xy = False
    elif key == ord("2"):
        manual_override_z = False
    elif key == ord("3"):
        manual_override_rot = False


