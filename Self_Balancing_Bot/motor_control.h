#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

// Sets up motor driver pins + PWM channels and stops both motors.
void initMotors();

// Sets up encoder pins and attaches the quadrature-decoding
// interrupt handlers.
void initEncoders();

// Recomputes left/right RPM from the encoder counts (internally
// rate-limited to ~20 Hz). Call every loop.
void calculateRPM();

// Drives the left/right motor at the given signed PWM command
// (-255..255). Handles direction pins, dead-zone mapping, and
// the LEFT_REVERSE/RIGHT_REVERSE wiring correction.
void applyLeftMotor(float command);
void applyRightMotor(float command);

// Immediately stops both motors and resets motor-side state
// (slew memory, start ramp).
void stopMotors();

// Rate-limit a target command against this wheel's last applied
// command, by at most MAX_PWM_CHANGE per call. Each function
// tracks its own wheel's history internally.
float slewLeftCommand(float target);
float slewRightCommand(float target);

// Returns a correction term (in PWM units) that nudges the
// slower wheel to catch up with the faster one, based on
// filtered RPM.
float calculateSyncCorrection();

// Slowly trims left/right output to correct a persistent RPM
// imbalance (e.g. motor-to-motor variation). Call at ~10 Hz.
void updateMotorTrim();

#endif
