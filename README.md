This project explores a small-scale AI-assisted robotic vehicle platform using an ESP32 and Arduino Uno.
The system combines manual RC control, sensor-based safety systems, and AI warnings to experiment with perception and control concepts used in autonomous vehicles.

The vehicle can be driven manually via a Bluetooth game controller, while additional modules provide obstacle detection, lane assist functionality, and AI-based warnings via WiFi.

The goal of this project is to better understand the interaction between:

embedded systems
computer vision / AI modules
sensor-based safety systems
real-time motor control
System Architecture

The system uses a dual microcontroller architecture:

ESP32

Handles:

Bluetooth controller input
WiFi communication
UDP communication with external AI system
ultrasonic obstacle detection
safety warnings
high level vehicle control
Arduino Uno

Handles:

motor control (TB6612 driver)
steering servo control
line tracking sensors
low-level vehicle actuation

Communication between ESP32 and Arduino is done via serial communication.

Features
Manual Driving

The car can be driven using a Bluetooth game controller.

Features include:

differential motor control
steering servo control
deadzone and exponential input smoothing
pivot turning
Ultrasonic Collision Detection

An ultrasonic sensor continuously measures the distance in front of the vehicle.

If an obstacle is detected within 20 cm, a warning is triggered.

This warning activates:

visual feedback via controller LED
safety indication during driving
