#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================
// COMPILE-TIME CONFIGURATION
// Wiring, hardware constants, and fixed tuning limits.
// Edit these to match YOUR wiring / hardware.
// ============================================================

// ---------------- MPU6050 ----------------
#define MPU_ADDR 0x68
#define MPU_SDA 21
#define MPU_SCL 22

// ---------------- LEFT MOTOR DRIVER ----------------
#define LEFT_ENA 25
#define LEFT_IN1 26
#define LEFT_IN2 27

// ---------------- RIGHT MOTOR DRIVER ----------------
#define RIGHT_ENA 14
#define RIGHT_IN3 4
#define RIGHT_IN4 5

// ---------------- LEFT / RIGHT ENCODER ----------------
#define LEFT_C1 18
#define LEFT_C2 19
#define RIGHT_C1 23
#define RIGHT_C2 13

// ---------------- MOTOR DIRECTION ----------------
// LEFT = normal, RIGHT = electrically reversed (mirror mounting)
// so BOTH PHYSICAL WHEELS rotate the SAME physical direction.
const bool LEFT_REVERSE = false;
const bool RIGHT_REVERSE = true;

// The balance correction was observed to drive toward the tilt, so the
// complete balance command polarity must be inverted.
const float MOTOR_DIRECTION_SIGN = -1.0f;

// The current MPU mounting reports backward tilt with the opposite
// polarity required by the motor controller, so invert both angle and
// gyro-rate signs together.
const float ANGLE_SIGN = -1.0f;

// Fixed accelerometer offsets. Measure these once if needed and keep the
// robot's mechanical upright angle in balanceSetpoint instead of learning
// it from the hand position at every boot.
const float ACCEL_Y_OFFSET = 0.0f;
const float ACCEL_Z_OFFSET = 0.0f;
const unsigned long STARTUP_DELAY_MS = 3000;

// Encoder counts per one output-shaft (wheel) revolution, after
// x4 quadrature decoding. Measured directly on this robot (same
// value on both wheels, as expected for a matched gearbox pair).
const float COUNTS_PER_REV = 1215.0f;

// ---------------- WIFI ----------------
const char *const WIFI_SSID = "BalanceRobot";
const char *const WIFI_PASSWORD = "12345678";

// ---------------- BALANCE PID LIMITS ----------------
const float PID_LIMIT = 300.0f;

// Cut motor power after the robot leaves its recoverable range. Keep this
// only slightly above the largest angle that the current tuning can catch.
const float FALLEN_ANGLE_LIMIT = 30.0f;

// ---------------- RPM SYNCHRONIZATION ----------------
const float MAX_SYNC_CORRECTION = 35.0f;
const float RPM_FILTER_ALPHA = 0.4f; // smoothing for RPM used in sync

// Encoder signs that convert each raw reading into the same physical
// forward direction. Based on the current wiring/data: left is normal,
// right is reversed because the right motor is mirror-mounted.
const float LEFT_ENCODER_PHYSICAL_SIGN = -1.0f;
const float RIGHT_ENCODER_PHYSICAL_SIGN = 1.0f;

// Per-motor output correction for unequal gearbox/motor torque. Reduce
// the stronger wheel slightly instead of changing balance PID gains.
const float RIGHT_MOTOR_SCALE = 0.99f;
const float LEFT_MOTOR_SCALE = 0.95f;
// Outer velocity damping: subtracts wheel motion from the angle PID
// command so the robot slows instead of continuing to roll after a tilt.
const float VELOCITY_DAMPING_GAIN = 0.40f;

// Near-upright quiet zone. Inside this small angle/rate window, the
// controller can stop both motors instead of constantly toggling them
// around dead-zone and minimum PWM.
const float UPRIGHT_ANGLE_DEADBAND = 0.45f;   // degrees
const float UPRIGHT_RATE_DEADBAND = 4.0f;     // deg/s
const float UPRIGHT_VELOCITY_DEADBAND = 6.0f; // wheel RPM (average)

// Commands below this are ignored before motor mapping. Keeping this
// above tiny noise-level PID outputs avoids chatter from min-PWM jumps.
const float MIN_BALANCE_COMMAND = 20.0f;

// Do not turn sensor noise into motor motion. Above this threshold, PWM
// follows the controller command directly instead of jumping to a large
// minimum PWM value.
const float MOTOR_COMMAND_DEADBAND = 20.0f;

// Ignore gyro bias/noise near zero so D cannot create a constant one-way
// command while the robot is momentarily still.
const float GYRO_RATE_DEADBAND = 1.5f;
const float MAX_D_TERM = 100.0f;

// ---------------- AUTOMATIC MOTOR TRIM ----------------
const float MAX_MOTOR_TRIM = 25.0f;
const float TRIM_LEARNING_RATE = 0.015f;

// ---------------- MOTOR START RAMP ----------------
const unsigned long START_RAMP_TIME = 180;

// ---------------- PWM SLEW LIMIT ----------------
// Maximum command change per second. A time-based limit is stable even
// when the ESP32 loop rate changes.
const float MAX_PWM_CHANGE = 2.0f;
// const float MAX_PWM_CHANGE_PER_SECOND = 900.0f;

// Nonlinear compensation for motor dead-zone. Values below 1.0 boost
// low commands while preserving the requested direction and max PWM.
const float PWM_CURVE_EXPONENT = 1.0f;

// ============================================================
// POSITION-HOLD CONTROL
// ============================================================
// Prevents linear drift by gently pulling the robot back when it
// accumulates forward/backward motion. Tuneable via web interface.
const float MAX_POSITION_CORRECTION = 100.0f;  // max PWM correction
const float POSITION_HOLD_FILTER_ALPHA = 0.3f; // responsiveness of filtered position
const float MAX_POSITION_ERROR = 10000.0f;     // clamp accumulated position
const float POSITION_DECAY = 0.85f;            // stronger decay: 15% per iteration prevents unbounded growth

// ============================================================
// SHARED RUNTIME STATE
// Defined once in the main .ino; every module reads/writes
// these through this header. Grouped by which module owns the
// *writing* of each value.
// ============================================================

// ---- web-tunable PID / filter parameters ----
extern float Kp, Ki, Kd;
extern float balanceSetpoint;
extern float filterAlpha;
extern float syncGain;
extern float positionHoldGain; // position-hold feedback gain (0 = disabled)
extern int leftMinPWM, rightMinPWM;
extern bool systemEnabled;

// ---- MPU / angle estimate (written by mpu_sensor) ----
extern float accelAngle;
extern float gyroRate;
extern float filteredAngle;
extern bool mpuOK;
extern uint8_t mpuFailStreak;

// ---- encoders (written by motor_control ISRs) ----
extern volatile long leftEncoderCount;
extern volatile long rightEncoderCount;

// ---- RPM, filtered (written by motor_control) ----
extern float leftRPMFiltered;
extern float rightRPMFiltered;
extern float rpmDifference;
extern float velocityFeedback;
extern float positionFeedback;

// ---- automatic motor trim (written by motor_control) ----
extern float leftMotorTrim;
extern float rightMotorTrim;

// ---- motor output status (written by motor_control) ----
extern int leftPWM, rightPWM;
extern String leftDirection, rightDirection;

// ---- balance PID output, for telemetry (written by balance_pid) ----
extern float pidOutput;
extern float pidProportional;
extern float pidIntegralOutput;
extern float pidDerivativeOutput;

// ---- startup ramp (shared between balance_pid and motor_control) ----
extern bool motorsStarted;
extern unsigned long motorStartTime;

#endif
