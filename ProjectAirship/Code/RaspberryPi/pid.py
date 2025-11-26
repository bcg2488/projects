import time
import threading
import queue

# --- Initialize PID controller state on startup ---
def init_pid(Kp, Ki, Kd, setpoint, output_limits=(0, 100)):
    """
    initialize pid controller parameters.
    kp - proportional gain
    ki - integral gain
    kd - derivative gain
    setpoint - desired value
    output_limits - tuple of (min, max) output limits
    returns a dictionary containing pid parameters
    """
    pid = {
        'Kp': Kp,
        'Ki': Ki,
        'Kd': Kd,
        'setpoint': setpoint,
        'output_limits': output_limits,
        'integral': 0.0,
        'prev_error': 0.0,
        'last_time': None,
        'prev_output': 0.0
    }
    return pid


# --- set altitude ---
def set_altitude(pid, altitude):
    pid['setpoint'] = altitude


# --- Updates the PID controller state and returns the output ---
def update_pid(pid, current_altitude):
    # --- calculate PID output ---
    now = time.time()
    
    # Starting case with no previous last_time
    if pid['last_time'] is None:
        pid['last_time'] = now
        return pid['prev_output']

    dt = now - pid['last_time']
    if dt <= 0:
        return pid['prev_output']  # avoid division by zero

    error = pid['setpoint'] - current_altitude

    pid['integral'] += error * dt
    derivative = (error - pid['prev_error']) / dt

    # --- PID output calculation ---
    output = (pid['Kp'] * error) + (pid['Ki'] * pid['integral']) + (pid['Kd'] * derivative)
    
    if output < pid['output_limits'][0]:
        output = pid['output_limits'][0]
    elif output > pid['output_limits'][1]:
        output = pid['output_limits'][1]

    # Update state
    pid['prev_error'] = error
    pid['last_time'] = now
    pid['prev_output'] = output

    return output


if __name__ == '__main__':
    # === pid initialization ===
    pid = init_pid(Kp=1.0, Ki=0.1, Kd=0.05, setpoint=40.0, output_limits=(0, 50))
    # === end pid initialization ===

    system_value = 0.0
    i = 0
    print("starting pid simulation loop...")
    while True:

        if i > 30:
            set_altitude(pid, 4)
        
        output = update_pid(pid, system_value)
        system_value += (output - system_value) * 0.1

        print(f"setpoint: {pid['setpoint']:.2f} | value: {system_value:.2f} | output: {output:.2f}")

        i += 1
        time.sleep(0.5)
