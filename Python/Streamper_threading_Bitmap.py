#Streamer File. Multi Threading

#Plan of Action:
#1. Get screenshot of current window.
#2. Resize it to 160x128 and 16bit RGB565.
#3. Send it to ESP32 Server via WEB Socket.

# ScreenshotSavePath = r"C:\Users\nisch\OneDrive\Desktop\SummerProjects2022\WindowsStreamer\python\Image\SC.png"
# ResizedSavePath = r"C:\Users\nisch\OneDrive\Desktop\SummerProjects2022\WindowsStreamer\python\Image\Resized.png"
BufferSize = 160*128*2

startX = 0
startY = 0
width = 1920
height = 1080

IPAddress = "192.168.1.7"

import queue
from PIL import ImageGrab
import threading
from PIL import Image
from mss import mss 
import time
import cv2
import numpy as np
import websocket
mon = {'left': startX, 'top': startY, 'width': width, 'height': height}

q1 = queue.Queue(maxsize=1)       #store the scrrenshot data
q2 = queue.Queue(maxsize=1)       #store the display data

def convert(x):
    ''' convert 16 bit int x into two 8 bit ints, coarse and fine.
    '''
    c = (x >> 8) & 0xff # The value of x shifted 8 bits to the right, creating coarse.
    f = x & 0xff # The remainder of x / 256, creating fine.
    return c, f

def getScreenshot(q1):
    #1. Get screenshot of current window.
    with mss() as sct:
        imgnp = sct.grab(mon)
        imgnp = Image.frombytes('RGB', (imgnp.width, imgnp.height), imgnp.rgb)
    img = cv2.resize(np.array(imgnp), (160, 128), interpolation = cv2.INTER_AREA)
    #save it to the queue
    q1.put(img)



def getimage(q1, q2):
    ''' Get screenshot of current window.
    Deals with Task 2. of the plan of action.
    Get the screenshot, resize it, and save it to a file.
    Then converts the image color data, and returns a bitmap of the image.
    '''
    resizedimg = q1.get()
    R5 = (resizedimg[...,0]>>3).astype(np.uint16) << 11
    G6 = (resizedimg[...,1]>>2).astype(np.uint16) << 5
    B5 = (resizedimg[...,2]>>3).astype(np.uint16)
    RGB565 = B5 | G6 | R5
    # Flatten the RGB565 array
    RGB565_flat = RGB565.flatten()

    # Convert to bytes
    Eight_BitData = np.zeros(RGB565_flat.size * 2, dtype=np.uint8)
    Eight_BitData[::2] = RGB565_flat >> 8  # Upper 8 bits
    Eight_BitData[1::2] = RGB565_flat & 0xFF  # Lower 8 bits
    q2.put(Eight_BitData)
    

def SendData(data, ws, q2):
    ''' Send data to ESP32 Server via WEB Socket.
        input: The data needed to be sent
    '''
    #3. Send it to ESP32 Server via WEB Socket.
    ws.send("1")
    ws.send_binary(data[0:10240])
    ws.send_binary(data[10240:20481])
    ws.send_binary(data[20481:30721])
    ws.send_binary(data[30721:40960])
    ws.send("0")


#####-------------------THREADS-----------------------#####
def ScreenshotThread():
    ''' Thread for getting screenshots. fills up queue q1.
    '''
    #time.sleep(1)
    print("Screenshot Thread Started")
    desiredfps = 100
    while True:
        timestart = time.time()
        #getimage(getScreenshot(), q1)
        getScreenshot(q1)
        timeend = time.time()
        fpsdelay = 1.0/desiredfps - (timeend-timestart)
        if fpsdelay > 0:
            time.sleep(fpsdelay)

def ImageprocessThread():
    """
    Keep processing the images obtained from screenshot thread.
    """ 
    print("waiting for screnshot thread to fill the buffer")
    waittime = 0.1
    time.sleep(waittime)
    print("ImageprocessThread started")
    while True:
        getimage(q1, q2)


def SendDataThread():
    ''' Thread for sending data.
    '''
    #start and setup websocket connection
    ws = websocket.WebSocket()
    ws.connect("ws://" + IPAddress + ":81/")
    print("3 second cooldown")
    time.sleep(0.1)
    #once that is done, we can continously keep getting screenshots and keep sending them to the ESP32 Server.
    print("Display should start now")
    fpscounter = 0
    timestart = time.time()
    desiredfps = 60.0
    while True:
        SendData(q2.get(), ws, q2)
        fpscounter += 1
        if (fpscounter % 30 == 0):
            timeend = time.time()
            print("FPS: ", 30/(timeend - timestart))
            timestart = timeend
        time.sleep(1/desiredfps)


def main():
    ''' Main function.
    '''
    print(dir(websocket))
    print("Program Started")
    #start threads
    thread1 = threading.Thread(target=ScreenshotThread)
    thread2 = threading.Thread(target=SendDataThread)
    thread3 = threading.Thread(target=ImageprocessThread)
    thread1.start()
    thread2.start()
    thread3.start()
    #wait for threads to finish
    #this should never happen
    thread1.join()
    thread2.join()
    thread3.join()
    print("Program stopped")
if __name__ == '__main__':
    main()

    
