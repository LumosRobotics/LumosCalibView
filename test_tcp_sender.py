#!/usr/bin/env python3
"""
Simple TCP client to send test data to the PlotView TCP receiver.
Sends timestamped values for real-time visualization testing.
"""

import socket
import time
import json
import math

def send_test_data():
    # Connect to the TCP server
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect(('localhost', 8080))
        print("Connected to TCP server on localhost:8080")
        
        start_time = time.time()
        
        for i in range(200):
            current_time = time.time()
            timestamp = current_time - start_time
            
            # Generate test data: sine wave with some noise
            value = math.sin(timestamp * 2) + 0.1 * math.sin(timestamp * 10)
            
            # Send as simple CSV format: timestamp,value,channel
            message = f"{timestamp:.3f},{value:.3f},0\n"
            sock.send(message.encode('utf-8'))
            
            print(f"Sent: {message.strip()}")
            
            # Send data every 50ms for smooth real-time visualization
            time.sleep(0.05)
            
    except ConnectionRefusedError:
        print("Connection refused. Make sure the Qt application is running.")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()
        print("Connection closed")

def send_json_data():
    """Alternative: send data in JSON format"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect(('localhost', 8080))
        print("Connected to TCP server on localhost:8080 (JSON mode)")
        
        start_time = time.time()
        
        for i in range(100):
            current_time = time.time()
            timestamp = current_time - start_time
            
            # Generate test data
            value = math.cos(timestamp * 1.5) * math.exp(-timestamp * 0.1)
            
            # Send as JSON
            data = {
                "timestamp": timestamp,
                "value": value,
                "channel": 1
            }
            message = json.dumps(data) + "\n"
            sock.send(message.encode('utf-8'))
            
            print(f"Sent JSON: {data}")
            time.sleep(0.1)
            
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    print("TCP Data Sender Test")
    print("1. Simple CSV format")
    print("2. JSON format")
    choice = input("Choose format (1 or 2): ").strip()
    
    if choice == "2":
        send_json_data()
    else:
        send_test_data()