import cv2

cap = cv2.VideoCapture("/home/flo/outvid2.avi")
while True:
    status, frame = cap.read()
    cv2.imshow("img",frame)
    cv2.waitKey(20)

