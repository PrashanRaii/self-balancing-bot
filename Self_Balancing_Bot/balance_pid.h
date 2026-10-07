#ifndef BALANCE_PID_H
#define BALANCE_PID_H

// Resets the PID loop's internal timer. Call once from setup(),
// after calibrateGyro().
void initBalancePID();

// Runs the balance PID from the current filteredAngle/gyroRate
// and returns the raw (pre-sync, pre-trim) motor command.
float calculateBalancePID();

// Top-level control loop: runs the PID, applies RPM sync + auto
// trim + startup ramp + slew limiting, and drives both motors.
// Call every loop().
void controlMotors();

#endif
