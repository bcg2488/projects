import eventlet
eventlet.monkey_patch()

from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
import requests, threading, time
from threading import Lock

app = Flask(__name__)

socketio = SocketIO(app, cors_allowed_origins="*")  # enable websockets

#PI_IP = "http://127.0.0.1:6000" #mock server for testing
#PI_IP = "http://192.168.4.1:5000/command"  # Replace with your Pi's IP
PI_IP = "http://192.168.4.1:5000"

blimp_location = {"lat":32.731, "long":-97.110}
waypoints = []

data_lock = Lock()

blimp_data = {
    # #"Accel_X": 0, "Accel_Y": 0, "Accel_Z": 0,
    # "Battery": 0, "Current": 0,
    # #"Gyro_X": 0, "Gyro_Y": 0, "Gyro_Z": 0,
    # "heading": 0, "Lidar_Distance": 0,
    # #"Mag_X": 0, "Mag_Y": 0, "Mag_Z": 0,
    # "alt": 0, "climb": 0, "distance": 0,
    # "imu-heading": 0,
    # "backleft-motor": 0, "backright-motor": 0, "heading": 0,
    # "lat": 0, "left-motor": 0, "lon": 0, "right-motor": 0,
    # "speed": 0, "startFlag": 0, "stop": 0,
    # "target_altitude": 0, "target_altitudleft": 0,
    # "value = left": 0
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
    
    "back-motors": 0,
    "left-motor": 0,
    "right-motor": 0,
    "battery": 100,
    "current": 0,
    
    #ultrasonic altitude
    "ultrasonic-altitude": 0,
    "stop": 0
}

@app.route('/')
def index():
    return render_template('index.html')
    # i will be reciveing a json image of the blimp data

@app.route('/command', methods=['POST'])
def command():
    cmd = request.form.get('command')
    if cmd:
        app.logger.info(f"Sending command to Pi: {cmd}")
        try:
            res = requests.post(f"{PI_IP}/command", data={'command': cmd}, timeout=1)
            app.logger.info(f"Response from Pi: {res.status_code}")
        except Exception as e:
            app.logger.error(f"Failed to send to Pi: {e}")

        return '', 204
    return 'No command received', 400

@app.route('/manual', methods=['POST'])
def manual():
    try:
        # Get form data sent from HTML
        frontleft = int(request.form.get("frontleft", 0))
        frontright = int(request.form.get("frontright", 0))
        back = int(request.form.get("back", 0))
        altitude = float(request.form.get("Taltitude", 0.0))

        payload = {"frontleft": frontleft, "frontright": frontright, "back": back, "Taltitude": altitude}

        app.logger.info(f"Sending manual control data: {payload}")

        # Send JSON to Pi
        res = requests.post(f"{PI_IP}/control", json=payload, timeout=1)
        app.logger.info(f"Response from Pi: {res.status_code}")

        return '', 204

    except Exception as e:
        app.logger.error(f"Failed to send manual command to Pi: {e}")
        return 'Error sending manual command', 500
    
@app.route('/send_waypoints', methods=['POST'])
def send_waypoints():
    global waypoints
    try:
        waypoints = request.get_json()
        if not waypoints:
            return 'No waypoints received', 400
        app.logger.info(f"Sending waypoints to Pi: {waypoints}")
        res = requests.post(f"{PI_IP}/waypoints", json=waypoints, timeout=2)
        app.logger.info(f"Response from Pi: {res.status_code}")

        return '', 204
    except Exception as e:
        app.logger.error(f"Failed to send waypoints: {e}")
        return 'Error sending waypoints', 500

@app.route("/blimp_position")
def blimp_position():
    return jsonify({
        "lat": blimp_location["lat"],
        "long": blimp_location["long"]
    })

# Background thread to fetch telemetry and broadcast updates
def telemetry_updater():
    global blimp_data
    last_sent = {}
    while True:
        try:
            res = requests.get(f"{PI_IP}/blimp-status", timeout=3)
            if res.status_code == 200:
                new_data = res.json()
                #print("----new data",new_data)
                
                changed = any(new_data.get(k) != last_sent.get(k) for k in new_data)
                
                if changed:
                    blimp_data.update(new_data)
                    latest = blimp_data.copy()
                    
                    socketio.emit("telemetry_update", latest)
                    last_sent = latest
                    
                    app.logger.info(f"Broadcast telemetry: {latest}")
                    
                # #with data_lock:
                # blimp_data.update(new_data)
                # latest = blimp_data.copy()
                # #print("    ******Updated blimp_data:", latest)
                # socketio.emit("telemetry_update", latest)  # push to clients
                # app.logger.info(f"Broadcast telemetry: {latest}")
            else:
                app.logger.error(f"Failed to fetch telemetry: {res.status_code}")
        except Exception as e:
            app.logger.error(f"Telemetry fetch error: {e}")
        socketio.sleep(0.3)  # adjust update rate as needed

@socketio.on("connect")
def handle_connect():
    print("Browser connected")
    #with data_lock:
    emit("telemetry_update", blimp_data)  # send current data immediately on connect

if __name__ == '__main__':
    socketio.start_background_task(telemetry_updater)
    #threading.Thread(target=telemetry_updater, daemon=True).start()
    socketio.run(app, host="0.0.0.0", port=5000, debug=True)