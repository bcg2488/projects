from flask import Flask, request, jsonify, Response
import time
import cv2
from picamera2 import Picamera2

app = Flask(__name__)

camera = Picamera2()
camera_config = camera.create_preview_configuration(main={"size": (640, 480), "format": "RGB888"})
camera.configure(camera_config)
camera.start()

# ------ Info for distance calculation using pinhole formula: distance = real width * focal length / pixel width ----- 
# Known real-world object width in cm 
# TODO: adjust based on landing pad 
KNOWN_WIDTH_CM = 30.0 

# Approximate focal length in pixels ( describes how zoomed in the camera is )
FOCAL_LENGTH_PX = 615.0  # known estimate based on pi camera 



def generate_frames():
    while True:
        vidFrame = camera.capture_array()  # capture raw RGB frame
        frameBGR = cv2.cvtColor(vidFrame, cv2.COLOR_RGB2BGR)  #  convert to OpenCV format

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
      

        ret, buffer = cv2.imencode('.jpg', frameBGR)  #encode to JPEG
        frameBytes = buffer.tobytes()  # convert to bytes

        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frameBytes + b'\r\n')  # stream frame

        time.sleep(0.05)  #   throttle FPS


# --- Flask Video Stream Route ---
@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

# --- Index HTML Page ---
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
#-------
# --- Sensor Data and Control Endpoints ---
#blimp_data = {
#    "battery_level": 100,
    # IMU
#    "Accel_X": 0.0, "Accel_Y": 0.0, "Accel_Z": 0.0,
#    "Mag_X": 0.0, "Mag_Y": 0.0, "Mag_Z": 0.0,
#    "Gyro_X": 0.0, "Gyro_Y": 0.0, "Gyro_Z": 0.0,
#    # LIDAR
#    "distance": 0,
    # GPS
#    "lat": 0.0, "lon": 0.0, "alt": 0.0,
#    "speed": 0.0, "climb": 0.0, "heading": 0.0
#}

#control_commands = {
#    "motor_speed_left": 0,
#    "motor_speed_right": 0,
#    "motor_speed_A": 0,
#    "motor_speed_B": 0,
#    "stop": 0,
#    "target_altitude": 0.0
#}

#@app.route('/blimp-status', methods=['GET'])
#def get_blimp_status():
#    print(f"Sending blimp status: {blimp_data}")
#    return jsonify(blimp_data)

#@app.route('/control', methods=['POST'])
#def receive_control_commands():
#    if request.is_json:
#        received_json = request.get_json()
#        control_commands["motor_speed_left"] = received_json.get("motor_speed_left", control_commands["motor_speed_left"])
#        control_commands["motor_speed_right"] = received_json.get("motor_speed_right", control_commands["motor_speed_right"])
#        control_commands["stop"] = received_json.get("stop", control_commands["stop"])
#        control_commands["target_altitude"] = received_json.get("target_altitude", control_commands["target_altitude"])
#        print(f"Received control commands: {received_json}")
#        print(f"Updated control state: {control_commands}")
#        return jsonify({"current_commands": control_commands}), 200
#    else:
#        return jsonify({"status": "error", "message": "no json provided"}), 400

#@app.route('/command', methods=['POST'])
#def receive_command():
#    cmd = request.form.get('command')
#    if cmd:
#        print(f"Received command: {cmd}")
#        if cmd == "left":
#            control_commands["motor_speed_left"] = 50
#            control_commands["motor_speed_right"] = 0
#        elif cmd == "right":
#            control_commands["motor_speed_left"] = 0
#            control_commands["motor_speed_right"] = 50
#        elif cmd == "forward":
#            control_commands["motor_speed_left"] = 50
#            control_commands["motor_speed_right"] = 50
#        elif cmd == "stop":
#            control_commands["motor_speed_left"] = 0
#            control_commands["motor_speed_right"] = 0
#        return "OK", 200
#    return "No command received", 400

# --- Run Flask App ---
if __name__ == '__main__':
    print("starting Flask server for blimp control...")
    print("GET /blimp-status to retrieve data.")
    print("POST /control to send commands (JSON expected).")
    app.run(host='0.0.0.0', port=5000)