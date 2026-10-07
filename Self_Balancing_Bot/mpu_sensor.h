#ifndef MPU_SENSOR_H
#define MPU_SENSOR_H

// Initializes the I2C bus and wakes the MPU6050.
// Returns true if the sensor responded with the expected WHO_AM_I.
bool initMPU();

// Measures gyro bias and establishes the initial filtered angle.
// Robot MUST be held still and level while this runs.
void calibrateGyro();

// Reads the MPU6050 and updates the complementary-filter angle
// estimate (accelAngle, gyroRate, filteredAngle). Call every loop.
void updateMPU();

#endif
