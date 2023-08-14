import socket
import numpy as np
import cv2
from mss import mss
import time

# Path to the image file you want to send
image_path = r"C:\Users\nisch\OneDrive - UBC\UBC_UAS\2023\WindowStreamer\Python\output.jpg"

# ESP32 (or other device) IP and port
UDP_IP_ADDRESS = "192.168.1.3"  # Replace with your ESP32's IP address
UDP_PORT = 1234                # Replace with your chosen port number

#UDP transmission stuff
UDP_SEDNING_SIZE = 1460
Header = [0x00]
Header = np.array(Header, dtype=np.uint8)
Footer = [0xFF]
Footer = np.array(Footer, dtype=np.uint8)


def convert(x):
    ''' convert 16 bit int x into two 8 bit ints, coarse and fine.
    '''
    c = (x >> 8) & 0xff # The value of x shifted 8 bits to the right, creating coarse.
    f = x & 0xff # The remainder of x / 256, creating fine.
    return c, f

"""
Send Data Function

Sends the binary data with a header and footer to the ESP32.
Data is sent in chunks.
"""
def sendUDP(data, sock):
    # Send the header
    sock.sendto(Header, (UDP_IP_ADDRESS, UDP_PORT))
    # Send the data in chunks
    for i in range(0, len(data), UDP_SEDNING_SIZE):
        sock.sendto(data[i:i+UDP_SEDNING_SIZE], (UDP_IP_ADDRESS, UDP_PORT))
        time.sleep(0.0001)
    # Send the footer  
    sock.sendto(Footer, (UDP_IP_ADDRESS, UDP_PORT))

def CreateUDPSocket():
    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Bind the socket to a specific address and port (optional)
    sock.bind(("", 0))  # Bind to any available local address and port

    print("UDP Socket created and bound to address: ", sock.getsockname())
    return sock


# main function
def main():
    #image stuff
    bounding_box = {'top': 0, 'left': 0, 'width': 1920, 'height': 1080}
    frame  = 0
    time_start = time.time()
    sct = mss()

    # Create a UDP socket
    sock = CreateUDPSocket()

    while True:
        #get frame
        sct_img = sct.grab(bounding_box)
        timenow = time.time()
        frame += 1

        # print("Got a Frame")

        #process Frame
        img = cv2.resize(np.array(sct_img), (160, 128), interpolation = cv2.INTER_AREA)
        img = cv2.cvtColor(np.array(img), cv2.COLOR_BGR2RGB)

        resizedimg = img.astype(np.uint16)
        B5 = (resizedimg[...,0]>>3).astype(np.uint16) << 11
        G6 = (resizedimg[...,1]>>2).astype(np.uint16) << 5
        R5 = (resizedimg[...,2]>>3).astype(np.uint16)
        RGB565 = R5 | G6 | B5

        RGB565_flat = RGB565.flatten()

        Eight_BitData = np.zeros(RGB565_flat.size * 2, dtype=np.uint8)
        Eight_BitData[::2] = RGB565_flat >> 8  # Upper 8 bits
        Eight_BitData[1::2] = RGB565_flat & 0xFF  # Lower 8 bits

        # print("Converted to 8 bit")

        #send frame
        sendUDP(Eight_BitData, sock)
        

        # print("Sent Frame")

        cv2.imshow('screen',img)
        if frame > 100:
            print("FPS: ", frame/(timenow-time_start))
            frame = 0
            time_start = time.time()

        if (cv2.waitKey(1) & 0xFF) == ord('q'):
            cv2.destroyAllWindows()
            break
    
    # Close the socket
    sock.close()

if __name__ == "__main__":
    main()
