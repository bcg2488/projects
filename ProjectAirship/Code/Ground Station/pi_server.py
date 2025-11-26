from flask import Flask, request, jsonify

app = Flask(__name__)

current_command = None

@app.route('/command', methods=['POST'])
def receive_command():
    global current_command
    cmd = request.form.get('command')
    if cmd:
        current_command = cmd
        print(f"Received command: {cmd}")
        return "OK", 200
    return "No command received", 400

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)