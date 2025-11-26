from hcsr04sensor import sensor
import time

ECHO = 5
TRIG = 6  
measure = sensor.Measurement(TRIG, ECHO)


def getDistanceInCm():
    distance_cm = measure.raw_distance(sample_size=10, sample_wait=0.05)  
    if distance_cm > 900:
        return -1
    else:
        return distance_cm

if __name__ == '__main__':
    while True:
        dist = getDistanceInCm()
        print(f"distance is {dist}")