import socket
import sys

# --- CONFIGURATION ---
# Add all your Android device IPs here
TARGET_IPS = [
    "192.168.1.38", 
    "192.168.1.44",
    "192.168.1.50"
]
PORT = 5005

def send_messages():
    # 1. Get the message from command line
    if len(sys.argv) > 1:
        msg = " ".join(sys.argv[1:])
    else:
        msg = "ALARM"

    # 2. Create the socket (UDP)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # 3. Loop through the IP list and send
    print(f"Sending message: '{msg}'")
    print("-" * 30)
    
    for ip in TARGET_IPS:
        try:
            sock.sendto(msg.encode(), (ip, PORT))
            print(f"[SUCCESS] Sent to {ip}:{PORT}")
        except Exception as e:
            print(f"[FAILED ] Could not send to {ip}: {e}")

    sock.close()
    print("-" * 30)
    print("Done.")

if __name__ == "__main__":
    send_messages()
