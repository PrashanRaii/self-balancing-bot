# 🤖 Self-Balancing Bot

An Arduino-based two-wheeled self-balancing robot built around an ESP32 DevKit V1.

The robot uses an MPU6050 inertial measurement unit to estimate its tilt angle. A PID controller processes the tilt error and produces motor correction commands. Two L298N motor drivers control geared DC motors, while quadrature encoders provide wheel-speed feedback for synchronization and movement correction.

The project also includes a Wi-Fi web interface for live telemetry and PID parameter adjustment.

> 🎓 This repository contains the source code, project resources, experimental data, reports, presentations, and backup files for a college minor project.

---

## 📸 Project Preview

Project images are available in the [`Project_Resources/Images`](Project_Resources/Images) folder.

The folder includes pictures of:

- The self-balancing robot
- Different wheel configurations
- Project testing and development

---

## ✨ Main Features

- Two-wheeled self-balancing system
- ESP32-based control
- MPU6050 angle and gyroscope readings
- PID-based balance control
- Encoder-based wheel feedback
- L298N motor driver control
- Motor speed synchronization
- Automatic motor trimming
- Position-hold correction
- Wi-Fi access point
- Web-based monitoring and control
- Telemetry and experiment data logging
- Separate motor symmetry test sketch

---

## 🧰 Hardware Used

| Component | Specification / purpose |
|---|---|
| ESP32 DevKit V1 | Main controller for sensor reading, PID calculation, PWM generation, and communication |
| MPU6050 IMU | Measures acceleration and angular velocity for tilt estimation |
| L298N motor drivers | Two motor-driver modules used to control the left and right motors |
| Geared DC motors | 12 V, approximately 150 RPM, with quadrature encoders |
| 3S Li-ion battery | Main power source for the robot |
| Buck converter | Regulates the battery voltage for the required electronics and motor supply |
| Robot chassis and wheels | Mechanical structure of the self-balancing robot |

---

## 💻 Software and Libraries

| Software or library | Purpose |
|---|---|
| Arduino IDE | Writing, compiling, and uploading the code |
| ESP32 board support | Allows Arduino IDE to program the ESP32 |
| Wire | I2C communication with the MPU6050 |
| WiFi | Creates the robot's Wi-Fi connection |
| WebServer | Provides the robot's web interface |
| FreeRTOS | Supports ESP32 task and timing features |
| WebSockets by Markus Sattler | Real-time communication with the web interface |

---

## 🧠 How the Robot Works

The control process is approximately:

```text
MPU6050 sensor
      ↓
Tilt angle and gyro readings
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

The MPU6050 detects whether the robot is leaning forward or backward. The PID controller calculates how much correction is needed. The motors then move in the required direction to bring the robot back toward its balance point.

The encoder motors provide additional feedback for speed synchronization and movement correction.

---

## 🎛️ PID Controller

The robot uses a proportional-integral-derivative controller to calculate the motor correction required to reduce its tilt error.

The gains used in the documented final tuning were:

| Parameter | Value | Purpose |
|---|---:|---|
| `Kp` | `221.55` | Responds to the current tilt error |
| `Ki` | `0.002` | Responds to accumulated tilt error |
| `Kd` | `4.65` | Responds to the rate of change of the error |

These values are defined in:

```text
Self_Balancing_Bot/Self_Balancing_Bot.ino
```

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

--- 

### Important folders

| Folder | Description |
|---|---|
| `Self_Balancing_Bot/` | Main Arduino source code |
| `Motor_symmetry_test/` | Separate sketch for testing motor direction and symmetry |
| `Project_Resources/Data/` | CSV files collected during testing and PID tuning |
| `Project_Resources/Images/` | Robot photographs and project screenshots |
| `Project_Resources/Reports/` | Reports and presentations |
| `Project_Resources/Archive/` | Project backup archive |

---

## 🚀 How to Open the Project

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

Install the WebSockets library by **Markus Sattler** using the Arduino IDE Library Manager.

The `Wire`, `WiFi`, `WebServer`, and FreeRTOS functionality is provided by the ESP32 board package or the Arduino/ESP32 environment.

### 4. Check the configuration

Before uploading, review:

```text
Self_Balancing_Bot/config.h
```

This file contains:

- ESP32 pin assignments
- MPU6050 I2C pins
- Motor driver pins
- Encoder pins
- Motor direction settings
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

A separate wiring reference will be added to the project documentation so that the circuit can be rebuilt more easily.

> ⚠️ Always disconnect power before changing motor, driver, sensor, or encoder wiring.

---

## 📊 Experimental Data

The CSV files in [`Project_Resources/Data`](Project_Resources/Data) contain data collected during robot testing and PID tuning.

They may include experiments related to:

- PID gain changes
- Robot stability
- Motor synchronization
- Encoder readings
- Telemetry measurements
- Balance performance

The filenames reflect the experiment settings used at the time. A detailed data description can be added later as the experiments are organized.

---

## 📄 Reports and Presentations

Project reports and presentation files are available in:

[`Project_Resources/Reports`](Project_Resources/Reports)

These resources include:

- Project proposal report
- Final presentation PDF
- Final defense presentation PDF
- Final defense presentation PowerPoint file

---

## 🗃️ Backup Archive

A backup copy of the project is available in:

[`Project_Resources/Archive`](Project_Resources/Archive)

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

This project was developed as a college minor project.

The repository is intended for:

- Learning and experimentation
- Documentation
- Future improvements
- Rebuilding the project
- Studying PID-based balancing systems

The tuning values may need to be adjusted for different motors, batteries, chassis designs, wheel sizes, or sensor mounting positions.

---

## 📚 Future Improvements

Possible future improvements include:

- Better automatic PID tuning
- Improved battery monitoring
- More organized experiment naming
- Wireless firmware updates
- Improved web interface design
- Mobile-friendly controls
- Additional safety limits
- More detailed wiring diagrams
- Video demonstrations hosted externally

---

## 👋 Thanks for Visiting

Thank you for checking out this project!

If you build on this work, improve the balancing algorithm, or find a useful tuning method, feel free to document your changes and share what you learned.

⭐ If this project helps you understand self-balancing robots, PID control, or ESP32 development, consider giving the repository a star.