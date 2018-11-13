#!/usr/bin/env python3

import argparse
import json
import random
import socket
import sys
import time

if (sys.version_info.major < 3
        or (sys.version_info.major == 3 and sys.version_info.minor < 2)):
    print("This script requires Python version >= 3.2")
    sys.exit(1)

MaxResponseSize   = 1024 * 256
SocketReceiveSize = 4096

last_request_id = 0
last_response_id = 0

def send_request(sock, method, params = None):
    global last_request_id
    request_id = last_request_id + 1

    request_object = {
        'jsonrpc': '2.0',
        'id': str(request_id),
        'method': method,
        'params': params
    }

    request_json = json.dumps(request_object, sort_keys=True)
    print("--->", request_json)

    s.sendall(bytes(request_json, 'utf-8'))
    last_request_id = request_id

def receive_response(sock):
    global last_request_id
    global last_response_id

    response = bytes()

    while True:
        try:
            response += s.recv(SocketReceiveSize)
            response_json = json.loads(response.decode('utf-8'))
            print("<---", json.dumps(response_json, sort_keys=True))
            last_response_id = int(response_json["id"])
            assert last_response_id == last_request_id
            break
        except json.decoder.JSONDecodeError:
            if len(response) > MaxResponseSize:
                raise Exception("Maximum response size exceeded.")

    return response_json

if __name__ == "__main__":
    argparser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    argparser.add_argument('host', metavar='<host>', type=str)
    argparser.add_argument('port', metavar='<port>', type=int)

    args = argparser.parse_args()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((args.host, args.port))

        while True:
            # Check Running state
            send_request(s, "getDAQState")
            response = receive_response(s)
            assert response["result"] == "Running"

            print("Stopping DAQ and asserting Idle state")
            # Stop DAQ
            send_request(s, "stopDAQ")
            response = receive_response(s)
            assert response["result"] == True

            # Check Idle state
            send_request(s, "getDAQState")
            response = receive_response(s)
            assert response["result"] == "Idle"

            # sleep() time in seconds
            # rangrange(start, stop [, step])
            sleeptime = random.randrange(0, 6, 1)
            print("Sleeping %d seconds" % (sleeptime))
            time.sleep(sleeptime)

            # Check Idle state again
            send_request(s, "getDAQState")
            response = receive_response(s)
            assert response["result"] == "Idle"

            print("Starting DAQ")
            # Start the DAQ
            send_request(s, "startDAQ")
            response = receive_response(s)
            assert response["result"] == True

            # Check Running state
            send_request(s, "getDAQState")
            response = receive_response(s)
            assert response["result"] in ("Starting", "Running")

            # sleep() time in seconds
            # rangrange(start, stop [, step])
            sleeptime = 5
            print("Waiting %d seconds for startup to complete" % (sleeptime))
            time.sleep(sleeptime)

            # Check Running state
            send_request(s, "getDAQState")
            response = receive_response(s)
            assert response["result"] == "Running"

            # sleep() time in seconds
            # rangrange(start, stop [, step])
            sleeptime = random.randrange(60, 120, 1)
            print("Running for %d seconds" % (sleeptime))
            time.sleep(sleeptime)

    sys.exit(0)