# posture-sensor
Thesis project via android studio and arduino.

The Arduino uses an HC-SR04 ultrasonic sensor, SEN-10264 flex sensor, and FSR 406 pressure pads to collect posture data, use fuzzy logic to create membership functions, then pass those values to Thingspeak

The Android app retrieves the data from Thingspeak via the Volley library.
