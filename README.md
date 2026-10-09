# 🤖 Self-Balancing Bot

An Arduino-based two-wheeled self-balancing robot built around an ESP32 DevKit V1.

The robot uses an MPU6050 inertial measurement unit to estimate its tilt angle. A PID controller processes the tilt error and produces motor correction commands. Two L298N motor drivers control geared DC motors, while quadrature encoders provide wheel-speed feedback for synchronization and movement correction.

This project was developed by four BEI students of the 2080 batch as a sixth-semester college minor project.

---

## 👥 Project Team

| Team member | Student ID | Profiles |
|---|---|---|
| **Aadesh Dahal** | `PU080BEI002` | [![Facebook](https://img.shields.io/badge/Facebook-Profile-0A66C2?logo=linkedin&logoColor=white)](https://www.facebook.com/aadesh.dahal.1145) |
| **Nishchal Pokhrel** | `PU080BEI0027` | [![LinkedIn](https://img.shields.io/badge/LinkedIn-Profile-0A66C2?logo=linkedin&logoColor=white)](https://www.linkedin.com/in/nischal-pokhrel-571a1b2b6/) |
| **Prashan Chenta Rai** | `PU080BEI030` | [![LinkedIn](https://img.shields.io/badge/LinkedIn-Profile-0A66C2?logo=linkedin&logoColor=white)](https://www.linkedin.com/in/prashanrai/) |
| **Yogesh Khadka** | `PU080BEI048` | [![Facebook](https://img.shields.io/badge/Facebook-Profile-0A66C2?logo=linkedin&logoColor=white)](https://www.facebook.com/yogesh.khadka.3386585) |

We worked together as a team to design, assemble, program, test, and document this self-balancing robot.

### Academic Information

| Item | Details |
|---|---|
| Project type | Minor Project |
| Semester | Sixth semester |
| Batch | 2080 |
| Program | Bachelor of Electronics and Information Engineering (BEI) |

---

## 📸 Project Preview

Project photographs and screenshots are available in [`Project_Resources/Images`](Project_Resources/Images).

The images show the robot, its wheel configurations, and parts of the development and testing process.

---

## ✨ Main Features

- Two-wheeled self-balancing system
- ESP32-based real-time control
- MPU6050 angle and gyroscope measurements
- Complementary-filter angle estimation
- PID-based balance control
- Encoder-based wheel-speed feedback
- Left/right motor speed synchronization
- Automatic motor trimming
- Position-hold correction
- L298N motor-driver control
- Wi-Fi access point
- Web-based telemetry and PID tuning dashboard
- FreeRTOS-based control task
- Separate motor symmetry test sketch

---

## 🧰 Hardware Used

| Component | Specification or purpose |
|---|---|
| ESP32 DevKit V1 | Main controller for sensor reading, PID calculation, PWM generation, and communication |
| MPU6050 IMU | Measures acceleration and angular velocity for tilt estimation |
| L298N motor drivers | Two motor-driver modules used to control the left and right motors |
| Geared DC motors | 12 V, approximately 150 RPM, with quadrature encoders |
| 3S Li-ion battery | Main power source for the robot |
| Buck converter | Regulates voltage for the required electronics and motor supply |
| Robot chassis and wheels | Mechanical structure of the self-balancing robot |

---

## 💻 Software and Libraries

| Software or library | Purpose |
|---|---|
| Arduino IDE | Writing, compiling, and uploading the code |
| ESP32 board support | Allows Arduino IDE to program the ESP32 |
| `Wire` | I2C communication with the MPU6050 |
| `WiFi` | Creates the robot's Wi-Fi access point |
| `WebServer` | Provides the web interface |
| FreeRTOS | Supports task scheduling and real-time control |
| WebSockets by Markus Sattler | Enables real-time dashboard communication |

---

## 🧠 How the Robot Works

The main control process is:

```text
MPU6050 sensor
      ↓
Tilt angle and gyro readings
      ↓
Complementary filter
      ↓
PID controller
      ↓
Motor correction command
      ↓
L298N motor drivers
      ↓
Encoder feedback
      ↓
Continuous balancing adjustment
```

The MPU6050 detects whether the robot is leaning forward or backward. The sensor readings are combined to estimate the robot's tilt. The PID controller then calculates the correction required to move the wheels and bring the robot back toward its balance point.

The encoder motors provide wheel-speed feedback. This feedback is used for motor synchronization, movement correction, and reducing unwanted drift.

The ESP32 also provides a Wi-Fi web interface for viewing live telemetry and adjusting selected control parameters.

---

## 🎛️ PID Controller

The robot uses a proportional-integral-derivative controller to calculate the motor correction required to reduce tilt error.

The documented tuned gains are:

| Parameter | Value | Purpose |
|---|---:|---|
| `Kp` | `221.55` | Responds to the current tilt error |
| `Ki` | `0.002` | Responds to accumulated tilt error |
| `Kd` | `4.65` | Responds to the rate of change of the error |

These values are defined in:

```text
Self_Balancing_Bot/Self_Balancing_Bot.ino
```

The values may need to be adjusted when using different motors, batteries, chassis designs, wheel sizes, or sensor mounting positions.

> 📝 The robot's control loop is designed around a 10 ms sampling interval and was observed to run at approximately 113.2 Hz during testing.

---

## 📁 Repository Structure

```text
Minor Project (BEI080)/
├── README.md
├── .gitignore
├── Self_Balancing_Bot/
│   ├── Self_Balancing_Bot.ino
│   ├── config.h
│   ├── balance_pid.cpp
│   ├── balance_pid.h
│   ├── motor_control.cpp
│   ├── motor_control.h
│   ├── mpu_sensor.cpp
│   ├── mpu_sensor.h
│   ├── web_interface.cpp
│   ├── web_interface.h
│   └── Motor_symmetry_test/
└── Project_Resources/
    ├── Data/
    ├── Images/
    ├── Reports/
    └── Archive/
```

### Important folders

| Folder | Description |
|---|---|
| `Self_Balancing_Bot/` | Main Arduino source code |
| `Motor_symmetry_test/` | Separate sketch for testing motor direction and symmetry |
| `Project_Resources/Data/` | CSV files collected during testing and PID tuning |
| `Project_Resources/Images/` | Robot photographs and project screenshots |
| `Project_Resources/Reports/` | Reports and presentation files |
| `Project_Resources/Archive/` | Project backup archive |

For the complete installation list, see [`DEPENDENCIES.md`](DEPENDENCIES.md).

---

## 🚀 How to Run the Project

This project is designed to be used with the Arduino IDE.

### 1. Open the Arduino sketch

Open this folder in Arduino IDE:

```text
Self_Balancing_Bot/
```

Then open:

```text
Self_Balancing_Bot.ino
```

The `.cpp` and `.h` files in the same folder are used by the main sketch.

### 2. Select the ESP32 board

In Arduino IDE:

1. Connect the ESP32 board to your computer.
2. Go to **Tools → Board**.
3. Select the appropriate ESP32 board.
4. Select the correct USB port under **Tools → Port**.

### 3. Install the required library

Install the WebSockets library by **Markus Sattler** through the Arduino IDE Library Manager.

The `Wire`, `WiFi`, `WebServer`, and FreeRTOS functionality is provided by the Arduino and ESP32 board environments.

### 4. Check the configuration

Before uploading, review:

```text
Self_Balancing_Bot/config.h
```

This file contains:

- ESP32 pin assignments
- MPU6050 I2C pins
- Motor-driver pins
- Encoder pins
- Motor-direction settings
- PID limits
- Wi-Fi settings
- Balance and safety parameters

### 5. Upload the program

After selecting the board and port, click the **Upload** button in Arduino IDE.

> ⚠️ Keep the robot's wheels raised during the first motor test. This helps prevent unexpected movement and makes troubleshooting safer.

---

## 🔌 Hardware and Wiring

The current ESP32 pin assignments are defined in:

```text
Self_Balancing_Bot/config.h
```

The main connections include:

- MPU6050 using I2C
- L298N motor-driver input and enable pins
- Quadrature encoder signals
- Shared ground between the controller and connected modules

> ⚠️ Always disconnect the battery before changing motor, driver, sensor, or encoder wiring.

---

## 📊 Experimental Data

The CSV files in [`Project_Resources/Data`](Project_Resources/Data) contain data collected during robot testing and PID tuning.

The data may include experiments related to:

- PID gain changes
- Robot stability
- Motor synchronization
- Encoder readings
- Telemetry measurements
- Balance performance

Some filenames describe the tuning values used during an experiment. The data is included to preserve the development and testing history of the project.

---

## 📄 Reports and Presentations

Reports and presentation files are available in [`Project_Resources/Reports`](Project_Resources/Reports).

These resources contain project documentation, presentation material, and supporting reference documents.

---

## 🗃️ Backup Archive

A backup copy of the project is available in [`Project_Resources/Archive`](Project_Resources/Archive).

The backup is kept for reference and recovery purposes. The source files in `Self_Balancing_Bot/` are the files intended for normal development.

---

## ⚠️ Safety Notes

A self-balancing robot can move unexpectedly when powered on.

Please follow these precautions:

- Keep the wheels raised during the first upload and motor tests.
- Keep fingers, wires, and loose clothing away from the wheels.
- Check the motor direction before attempting to balance the robot.
- Use a stable power supply.
- Disconnect the battery before modifying the wiring.
- Test the robot in an open area.
- Be ready to disconnect power if the robot behaves unexpectedly.

---

## 🔧 Project Status

This project was developed as a sixth-semester college minor project by four BEI students of the 2080 batch.

The repository is intended for:

- Learning and experimentation
- Documentation
- Future improvements
- Rebuilding the project
- Studying PID-based balancing systems

---

## 📚 Future Improvements

Possible future improvements include:

- Better automatic PID tuning
- Improved battery monitoring
- More organized experiment naming
- Wireless firmware updates
- Improved web-interface design
- Mobile-friendly controls
- Additional safety limits
- More detailed wiring diagrams
- Externally hosted video demonstrations

---

## 👋 Thanks for Visiting

Thank you for checking out our project!

If this project helps you learn about self-balancing robots, PID control, Arduino development, or ESP32 systems, we hope you find the code and project resources useful.

⭐ Feel free to explore the repository and build on the ideas presented here.
