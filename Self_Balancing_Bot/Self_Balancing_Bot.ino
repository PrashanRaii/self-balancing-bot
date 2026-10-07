// ============================================================
// ESP32 TWO-WHEEL SELF BALANCING ROBOT
// MPU6050 + complementary filter (gyro rate used for D-term)
// 2 x L298N, 2 x encoder DC gearmotor
// Balance PID + automatic RPM synchronization + web PID tuning
//
// Code is split across modules -- open config.h first for pins
// and tuning constants, then each module's .h file for what it
// owns:
//   mpu_sensor.*     angle estimation (MPU6050 + complementary filter)
//   motor_control.*  encoders, RPM, motor output, dead-zone, trim
//   balance_pid.*    the PID loop + top-level control orchestration
//   web_interface.*  WiFi AP + live tuning dashboard
//
// This .ino only owns the shared state (declared extern in
// config.h) and the setup()/loop() wiring.
// ============================================================

#include "config.h"
#include "mpu_sensor.h"
#include "motor_control.h"
#include "balance_pid.h"
#include "web_interface.h"

// ---- web-tunable PID / filter parameters ----
float Kp = 221.55f;
float Ki = 0.002f;
float Kd = 4.65f;
float balanceSetpoint = 0.5f;
float filterAlpha = 0.98f;
float syncGain = 0.75f;
float positionHoldGain = 1.0f; // gentle position hold to prevent drift (0.0 = off)
int leftMinPWM = 55;
int rightMinPWM = 55;
bool systemEnabled = true;

// ---- MPU / angle estimate ----
float accelAngle = 0.0f;
float gyroRate = 0.0f;
float filteredAngle = 0.0f;
bool mpuOK = false;
uint8_t mpuFailStreak = 0;

// ---- encoders ----
volatile long leftEncoderCount = 0;
volatile long rightEncoderCount = 0;

// ---- RPM (filtered) ----
float leftRPMFiltered = 0.0f;
float rightRPMFiltered = 0.0f;
float rpmDifference = 0.0f;
float velocityFeedback = 0.0f;
float positionFeedback = 0.0f;

// ---- automatic motor trim ----
float leftMotorTrim = 3.0f;
float rightMotorTrim = -3.5f;

// ---- motor output status ----
int leftPWM = 0;
int rightPWM = 0;
String leftDirection = "STOP";
String rightDirection = "STOP";

// ---- balance PID output (telemetry) ----
float pidOutput = 0.0f;
float pidProportional = 0.0f;
float pidIntegralOutput = 0.0f;
float pidDerivativeOutput = 0.0f;

// ---- startup ramp ----
bool motorsStarted = false;
unsigned long motorStartTime = 0;

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println(" ESP32 TWO MOTOR BALANCING ROBOT");
    Serial.println("========================================");

    initMotors();
    initEncoders();

    Serial.println("Initializing MPU6050...");
    mpuOK = initMPU();
    Serial.println(mpuOK ? "MPU6050: OK" : "MPU6050: ERROR");

    if (mpuOK)
    {
        calibrateGyro(); // robot must be still + level here
    }

    initBalancePID();

    initWebInterface();

    Serial.println("Setup complete.");
    Serial.print("CALIBRATION_STATUS=");
    Serial.print(mpuOK ? "COMPLETE" : "SKIPPED_MPU_ERROR");
    Serial.println(" | Keep robot still only during restart/upload calibration.");
    Serial.println("========================================");
}

// ============================================================
// LOOP  (no blocking delay -- runs as fast as the hardware
// allows; every sub-system below is independently dt-gated)
// ============================================================

void loop()
{
    webInterfaceLoop();

    updateMPU();
    calculateRPM();
    controlMotors();

    static unsigned long lastPrint = 0;
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint >= 5000)
    {
        lastStatusPrint = millis();
        Serial.print("STATUS | CALIBRATION=");
        Serial.print(mpuOK ? "COMPLETE" : "SKIPPED_MPU_ERROR");
        Serial.print(" | MPU=");
        Serial.print(mpuOK ? "OK" : "ERROR");
        Serial.print(" | MPU_FAIL_STREAK=");
        Serial.print(mpuFailStreak);
        Serial.print(" | SYSTEM=");
        Serial.println(systemEnabled ? "ACTIVE" : "STOPPED");
    }

    if (millis() - lastPrint >= 200)
    {
        lastPrint = millis();

        long leftCount, rightCount;
        noInterrupts();
        leftCount = leftEncoderCount;
        rightCount = rightEncoderCount;
        interrupts();

        Serial.print("ANGLE=");
        Serial.print(filteredAngle, 2);
        Serial.print(" | ACC=");
        Serial.print(accelAngle, 2);
        Serial.print(" | LEFT_RPM=");
        Serial.print(leftRPMFiltered, 2);
        Serial.print(" | RIGHT_RPM=");
        Serial.print(rightRPMFiltered, 2);
        Serial.print(" | RPM_DIFF=");
        Serial.print(rpmDifference, 2);
        Serial.print(" | LEFT_PWM=");
        Serial.print(leftPWM);
        Serial.print(" | RIGHT_PWM=");
        Serial.print(rightPWM);
        Serial.print(" | PID=");
        Serial.print(pidOutput, 2);
        Serial.print(" | P_TERM=");
        Serial.print(pidProportional, 2);
        Serial.print(" | I_TERM=");
        Serial.print(pidIntegralOutput, 2);
        Serial.print(" | D_TERM=");
        Serial.print(pidDerivativeOutput, 2);
        Serial.print(" | SETPOINT=");
        Serial.print(balanceSetpoint, 2);
        Serial.print(" | L_TRIM=");
        Serial.print(leftMotorTrim, 2);
        Serial.print(" | R_TRIM=");
        Serial.print(rightMotorTrim, 2);
        Serial.print(" | L_COUNT=");
        Serial.print(leftCount);
        Serial.print(" | R_COUNT=");
        Serial.println(rightCount);
    }
}
