#include <Arduino.h>
#include <Wire.h>
#include "mpu_sensor.h"
#include "config.h"

// ============================================================
// MPU LOW LEVEL + BOOT CALIBRATION + COMPLEMENTARY FILTER
// ============================================================
// Everything here except the three functions in mpu_sensor.h is
// private to this file -- no other module needs raw accel/gyro
// counts or the gyro bias.

static int16_t accelX = 0, accelY = 0, accelZ = 0;
static int16_t gyroX = 0, gyroY = 0, gyroZ = 0;

static float gyroOffset = 0.0f; // calibrated at boot, see calibrateGyro()
static float accelYOffset = 0.0f;
static float accelZOffset = 0.0f;
static unsigned long lastMPUTime = 0;

static void writeMPU(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

static bool readMPU()
{
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0)
        return false;

    uint8_t count = Wire.requestFrom(MPU_ADDR, 14, true);
    if (count < 14)
        return false;

    accelX = ((int16_t)Wire.read() << 8) | Wire.read();
    accelY = ((int16_t)Wire.read() << 8) | Wire.read();
    accelZ = ((int16_t)Wire.read() << 8) | Wire.read();

    Wire.read();
    Wire.read(); // temperature, discarded

    gyroX = ((int16_t)Wire.read() << 8) | Wire.read();
    gyroY = ((int16_t)Wire.read() << 8) | Wire.read();
    gyroZ = ((int16_t)Wire.read() << 8) | Wire.read();

    return true;
}

bool initMPU()
{
    // 400 kHz I2C -> faster reads -> lower loop latency.
    Wire.begin(MPU_SDA, MPU_SCL, 400000);
    delay(100);

    writeMPU(0x6B, 0x00); // wake MPU
    delay(100);

    writeMPU(0x1C, 0x00); // accel range +-2g
    writeMPU(0x1B, 0x00); // gyro range +-250 deg/s
    writeMPU(0x1A, 0x03); // digital low pass filter

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x75);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 1, true);

    if (Wire.available())
    {
        uint8_t id = Wire.read();
        if (id == 0x68)
            return true;
    }
    return false;
}

// Robot must be still while this runs, but it does not need to be held at a
// particular angle. The upright angle is fixed by balanceSetpoint.
void calibrateGyro()
{
    Serial.println("Keep the robot still. Starting in 3 seconds...");
    delay(STARTUP_DELAY_MS);
    Serial.println("Calibrating gyro...");

    const int samples = 1500;
    double sumGyro = 0.0;
    int valid = 0;

    for (int i = 0; i < samples; i++)
    {
        if (readMPU())
        {
            sumGyro += gyroX;
            valid++;
        }
        delay(1);
    }

    if (valid > 0)
    {
        gyroOffset = (float)(sumGyro / valid);
    }

    accelYOffset = ACCEL_Y_OFFSET;
    accelZOffset = ACCEL_Z_OFFSET;
    filteredAngle = balanceSetpoint;

    Serial.print("Gyro offset (raw counts): ");
    Serial.println(gyroOffset, 2);
    Serial.println("Accelerometer reference: fixed");
    Serial.print("Balance setpoint: ");
    Serial.println(balanceSetpoint, 3);

    // Matches the original setup()'s explicit reset, so the very
    // first updateMPU() call after calibration gets a clean dt.
    lastMPUTime = micros();
}

void updateMPU()
{
    unsigned long now = micros();
    float dt = (now - lastMPUTime) / 1000000.0f;
    lastMPUTime = now;

    if (dt <= 0.0f || dt > 0.05f)
        dt = 0.005f;

    if (!readMPU())
    {
        mpuOK = false;
        mpuFailStreak++;
        return;
    }

    mpuOK = true;
    mpuFailStreak = 0;

    // --------------------------------------------------------
    // ACCELEROMETER ANGLE (pitch from Y/Z), sign-correctable
    // --------------------------------------------------------
    float calibratedAccelY = (float)accelY - ACCEL_Y_OFFSET;
    float calibratedAccelZ = (float)accelZ - ACCEL_Z_OFFSET;
    accelAngle = atan2f(calibratedAccelY, calibratedAccelZ) * 180.0f / PI;
    accelAngle *= ANGLE_SIGN;

    // --------------------------------------------------------
    // GYROSCOPE RATE, bias-corrected using calibrated offset
    // --------------------------------------------------------
    gyroRate = ((float)gyroX - gyroOffset) / 131.0f;
    gyroRate *= ANGLE_SIGN;

    // --------------------------------------------------------
    // COMPLEMENTARY FILTER
    // Positive angle = forward tilt, negative = backward tilt.
    // Because both accelAngle and gyroRate carry the same
    // ANGLE_SIGN convention, forward and backward tilt are
    // handled symmetrically.
    // --------------------------------------------------------
    filteredAngle = filterAlpha * (filteredAngle + gyroRate * dt) + (1.0f - filterAlpha) * accelAngle;
}
