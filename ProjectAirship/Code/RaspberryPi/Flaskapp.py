from flask import Flask, request, jsonify, Response
import time
import threading
import cv2
import uart

#import camera libraries
from picamera2 import Picamera2



"""
use curl to send post commands for testing
curl -X POST -H "Content-Type: application/json" -d "{\"left\": 166, \"right\": 150, \"stop\": 2, \"target_altitude\": 25.1}" http://192.168.4.1:5000/control
curl -X POST -d "command=left" http://192.168.4.1:5000/command 

"""

app = Flask(__name__)

# --- PI Camera and video generation
camera = Picamera2()

# #camera config --- uncomment when camera is plugged in
camera_config = camera.create_preview_configuration(main={"size": (640, 480), "format": "RGB888"})
camera.configure(camera_config)
camera.start()

# ------ Info for distance calculation using pinhole formula: distance = real width * focal length / pixel width ----- 
# Known real-world object width in cm 
# TODO: adjust based on landing pad 
KNOWN_WIDTH_CM = 30.0 

# Approximate focal length in pixels ( describes how zoomed in the camera is )
FOCAL_LENGTH_PX = 530.0  # known estimate based on pi camera 

# --- Generate Frames ---
def generate_frames():

    while True:
        # get singular video frame
        vidFrame =  camera.capture_array()

        #convert to BGR for cv2
        frameBGR = cv2.cvtColor(vidFrame, cv2.COLOR_RGB2BGR)

        # ----- START OF COMPUTER VISION LOGIC -----
        gray = cv2.cvtColor(frameBGR, cv2.COLOR_BGR2GRAY)  # convert to grayscale
        gray = cv2.GaussianBlur(gray, (5, 5), 0)  # small blur to reduce noise
        
        edges = cv2.Canny(gray, 50, 150)  # edge detection + light clean up v
        kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3)) #smooth out small gaps in edges (better contour detection)
        edges = cv2.morphologyEx(edges, cv2.MORPH_CLOSE, kernel, iterations=1) #smooth out small gaps in edges (better contour detection)
        

         # --- Find contours
        contours, _ = cv2.findContours(edges.copy(), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        flat_ground_detected = False
        estimated_distance_cm = None # initialize so its always defined
        H, W = frameBGR.shape[:2] 
        
        # dynamic thresholds based on image size
        min_area = 0.01 * (H * W) # ignore small areas (< 1% of image)
        
        for cnt in contours:
            area = cv2.contourArea(cnt)
            if area < min_area:
                continue
            
            # Convexity/solidity filter to prevent false positives 
            hull = cv2.convexHull(cnt)
            hull_area = cv2.contourArea(hull)
            if hull_area == 0:
                continue
            solidity = area / float(hull_area)
            if solidity < 0.85:
                continue  # reject ragged/holey shapes

            x, y, w, h = cv2.boundingRect(cnt)
            if w == 0 or h == 0:
                continue

            aspect_ratio = w / float(h)

            # For downward-facing camera, look for roughly square regions (landing pad-like)
            if 0.7 <= aspect_ratio <= 1.3:  # close to square
                cv2.rectangle(frameBGR, (x, y), (x + w, y + h), (0, 255, 0), 2)
                flat_ground_detected = True

                # Estimate distance using pinhole model 
                estimated_distance_cm = (KNOWN_WIDTH_CM * FOCAL_LENGTH_PX) / float(max(w, h))
                print(f"Landing pad detected. Estimated distance: {estimated_distance_cm:.2f} cm")

                break  # stop after first valid pad found

        # --- Overlay feedback (for landing pad and flat ground) ---
        if flat_ground_detected:
            label = (
                f"Landing Pad Detected - Dist: {estimated_distance_cm:.1f} cm"
                if estimated_distance_cm else "Landing Pad Detected"
            )
            cv2.putText(frameBGR, label, (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
        else:
            cv2.putText(frameBGR, "Searching for Landing Pad...",
                        (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)

        # ----- END OF COMPUTER VISION LOGIC -----

        #JPG encoding
        ret, buffer = cv2.imencode('.jpg', frameBGR)

        #split frame into bytes
        frameBytes = buffer.tobytes()

        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frameBytes + b'\r\n')

        #time delay for cpu usage. added for 20fps
        time.sleep(0.1)


@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')


#   --- blimp data and network code

#blimp data to respond to get commands
#add more data points after setup on sensors
blimp_data = {

    # #IMU DATA
    "imu-heading": 0,
    # "Accel_X": 0.0,
    # "Accel_Y": 0.0,
    # "Accel_Z": 0.0,
    # "Mag_X": 0.0,
    # "Mag_Y": 0.0,
    # "Mag_Z": 0.0,
    # "Gyro_X": 0.0,
    # "Gyro_Y": 0.0,
    # "Gyro_Z": 0.0,
    #LIDAR DATA
    "distance": 0,
    #GPS DATA
    "lat": 0.0,
    "lon": 0.0,
    "alt": 0.0,
    "speed": 0.0,
    "climb": 0.0,
    "heading": 0.0,
    "startFlag" : 0,

    #return data from arduino
    "left-motor": 0,
    "right-motor": 0,
    "back-motors": 0,
    "front-motors": 0,
    "battery": 100,
    "current": 0,
    #ultrasonic altitude
    "ultrasonic-altitude": 0,
    "stop": 0,
    "pid-toggle": 0
}



#control commands reiceved from laptop
#add controls
control_commands = {
    "temp": 1234,
    "front-motors": 0,
    "left-motor": 0,  
    "right-motor": 0,
    "back-motors": 0,
    "stop": 0,   
    "Taltitude": 20.0
}

# index route
@app.route('/')
def index():
    return  """
    <html>
    <head>
        <title>Flask app is online</title>
    </head>
    <body>
        <h1>Live Video Feed</h1>
        <img src="/video_feed" width="640" height="480" />
    </body>
    </html>
    """

#get status route
@app.route('/blimp-status', methods=['GET'])
def get_blimp_status():
    #Responds to get commands on the raspberry pi server and responds with json data
    print(f"Sending blimp status: {blimp_data}")
    return jsonify(blimp_data)

#post route - for recieving commands
@app.route('/control', methods=['POST'])
def receive_control_commands():
    #json data in the parameters, from laptop -> post
    
    if request.is_json:
        # get json data
        received_json = request.get_json()

        #update variables in controls object - added key error protection
        control_commands["left-motor"] = received_json.get("frontleft", control_commands["left-motor"])
        control_commands["right-motor"] = received_json.get("frontright", control_commands["right-motor"])
        if blimp_data["pid-toggle"] == 0:
            control_commands["back-motors"] = received_json.get("back", control_commands["back-motors"])
            control_commands["front-motors"] = received_json.get("back", control_commands["front-motors"])
        control_commands["Taltitude"] = received_json.get("Taltitude", control_commands["Taltitude"])
        
        print(f"Received control commands: {received_json}")
        print(f"Updated control state: {control_commands}")

        return jsonify({"current_commands": control_commands}), 200
    else:
        # if the request is not JSON, return an error
        return jsonify({"status": "error", "message": "no json provided"}), 400

# --- Manual Command setup (arrow keys) -----
@app.route('/command', methods=['POST'])
def receive_command():
    cmd = request.form.get('command')
    if cmd:
        print(f"Received command: {cmd}")
        if cmd == "left":               # turn left - speed 50
            control_commands["left-motor"] = 50
            control_commands["right-motor"] = 0
        elif cmd == "right":            # turn right - speed 50  
            control_commands["left-motor"] = 0
            control_commands["right-motor"] = 50
        elif cmd == "forward":          # move forward - both motors at speed 50
            control_commands["left-motor"] = 50
            control_commands["right-motor"] = 50
        elif cmd == "stop-forward":     # stop both motors 
            control_commands["left-motor"] = 0
            control_commands["right-motor"] = 0
        elif cmd == "stop-left":        # stop left motor
            control_commands["left-motor"] = 0
            control_commands["right-motor"] = 0
        elif cmd == "stop-right":       # stop right motor
            control_commands["left-motor"] = 0
            control_commands["right-motor"] = 0
        elif cmd == "start-blimp":
            blimp_data["startFlag"] = 1
            blimp_data["stop"] = 0
        elif cmd == "stop-blimp":
            blimp_data["startFlag"] = 0
            blimp_data["stop"] = 1
            with uart.uart_lock:
                uart.writeArduinoCommmand("stop", "1")
        elif cmd == "pid-toggle":
            blimp_data["pid-toggle"] = 1 - blimp_data["pid-toggle"]  # toggle between 0 and 1
            # if blimp_data["pid-toggle"] == 0:
            #     control_commands["back-motors"] = 0
            #     control_commands["front-motors"] = 0

        return "OK", 200
    return "No command received", 400



#  Run flask app - main
if __name__ == '__main__':
    #start app
    print("starting Flask server for blimp control...")
    print("GET /blimp-status to retrieve data.")
    print("POST /control to send commands (JSON expected).")
    
    #set host to listen on all ip
    app.run(host='0.0.0.0', port=5000)