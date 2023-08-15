import numpy as np
import cv2
from mss import mss
from PIL import Image
import time
import websocket

def convert(x):
    ''' convert 16 bit int x into two 8 bit ints, coarse and fine.
    '''
    c = (x >> 8) & 0xff # The value of x shifted 8 bits to the right, creating coarse.
    f = x & 0xff # The remainder of x / 256, creating fine.
    return c, f

def SendData(data, ws):
    ''' Send data to ESP32 Server via WEB Socket.
        input: The data needed to be sent
    '''
    # Set JPEG compression quality to 90
    jpeg_quality = 65
    encode_params = [int(cv2.IMWRITE_JPEG_QUALITY), jpeg_quality]

    # Convert the 24-bit BGR image to JPEG with the specified quality
    _, jpeg_image = cv2.imencode('.jpg', data, params=encode_params)
    jpeg_image_bytes = jpeg_image.tobytes()
     # Save the JPEG image to a file
    with open('output.jpg', 'wb') as file:
        file.write(jpeg_image_bytes)
    #3. Send it to ESP32 Server via WEB Socket.
    ws.send("1")
    #ws.send_binary(data)q
    # ws.send_binary(data[0:10240])
    # ws.send_binary(data[10240:20481])
    # ws.send_binary(data[20481:30721])
    # ws.send_binary(data[30721:40960])
    # ws.send_binary(jpeg_image_bytes)
    if len(jpeg_image_bytes) > 10240:
        start = 0
        while start < len(jpeg_image_bytes):
            ws.send_binary(jpeg_image_bytes[start:start+10240])
            start += 10240       
    else:
        ws.send_binary(jpeg_image_bytes) 
    print("Bytes Sent: ", len(jpeg_image_bytes))
    ws.send("0")

bounding_box = {'top': 0, 'left': 0, 'width': 1920, 'height': 1080}

sct = mss()
frame  = 0
time_start = time.time()
ws = websocket.WebSocket()
ws.connect("ws://" + "192.168.1.3" + ":81/")

while True:
    #get frame
    sct_img = sct.grab(bounding_box)
    timenow = time.time()
    frame += 1
    
    #process Frame
    img = cv2.resize(np.array(sct_img), (160, 128), interpolation = cv2.INTER_AREA)
    # img = cv2.cvtColor(np.array(img), cv2.COLOR_BGR2RGB)

    # resizedimg = img.astype(np.uint16)
    # B5 = (resizedimg[...,0]>>3).astype(np.uint16) << 11
    # G6 = (resizedimg[...,1]>>2).astype(np.uint16) << 5
    # R5 = (resizedimg[...,2]>>3).astype(np.uint16)
    # RGB565 = R5 | G6 | B5

    # RGB565_flat = RGB565.flatten()

    # Eight_BitData = np.zeros(RGB565_flat.size * 2, dtype=np.uint8)
    # Eight_BitData[::2] = RGB565_flat >> 8  # Upper 8 bits
    # Eight_BitData[1::2] = RGB565_flat & 0xFF  # Lower 8 bits

    #Send Data
    SendData(img, ws)

    
    
    cv2.imshow('screen',img)
    if frame > 100:
        print("FPS: ", frame/(timenow-time_start))
        frame = 0
        time_start = time.time()

    if (cv2.waitKey(1) & 0xFF) == ord('q'):
        cv2.destroyAllWindows()
        break



