import socket
import argparse
import sys
import time

threshold = 1

# Least squares https://stackoverflow.com/questions/5083465/fast-efficient-least-squares-fit-algorithm-in-c
def leastSquares(y):
    sumxy, sumy, sumy2 = 0, 0, 0,
    sumx = 435
    sumx2 = 8555

    for i in range(30):
        sumy += y[i]
        sumy2 += y[i] * y[i]
        sumxy += i * y[i]

    denom = 67425
    
    return (30 * sumxy - sumx * sumy) / denom


def updateSensorValues(sv, id, value, sock):
    if(len(sv[id]) == 30):
        slope = leastSquares(sv[id])
        if(slope > threshold):
            # Send Valve control message
            sock.send(id.encode('utf-8') + b'\n')
            # print(f"sent data : {id.encode('utf8')}")
            
        sv[id].pop(0) # remove oldest value from the list
    else:
        sv[id].append(value)

def recv(sock):
    data = sock.recv(1)
    buf = b""
    while data.decode("utf-8") != "\n":
        buf += data
        data = sock.recv(1)
    return buf

def main(ip, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((ip, port))

    sensorValues = {}
    for i in range(256):
        sensorValues[str(i)] = []

    data = recv(sock)

    while data.decode("utf-8") != "Border Router": 
        data = recv(sock)
        # print(data.decode("utf-8"))

    while True:
        try:
            data = recv(sock)
            value = data.decode("utf-8").split(";")[0]
            id = data.decode("utf-8").split(";")[1]
            
            updateSensorValues(sensorValues, id, int(value), sock)
        except KeyboardInterrupt:
            break
        except:
            continue



if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", dest="ip", type=str)
    parser.add_argument("--port", dest="port", type=int)
    args = parser.parse_args()

    main(args.ip, args.port)

