#include <Arduino.h>
#include "balance_pid.h"
#include "motor_control.h"
#include "config.h"

// ---- module-private PID state ----
static float pidError = 0.0f;
static float pidIntegral = 0.0f;
static float pidDerivative = 0.0f;
static unsigned long lastPIDTime = 0;

// ---- module-private position-hold state ----
static long lastLeftCount = 0;           // encoder count baseline
static long lastRightCount = 0;          // encoder count baseline
static float accumulatedPosition = 0.0f; // net wheel motion (integrated)
static float filteredPosition = 0.0f;    // smoothed for control
static float positionIntegral = 0.0f;    // integral accumulation for persistent bias correction

void initBalancePID()
{
    lastPIDTime = micros();
    lastLeftCount = 0;
    lastRightCount = 0;
    accumulatedPosition = 0.0f;
    filteredPosition = 0.0f;
    positionIntegral = 0.0f;
}

float calculateBalancePID()
{
    unsigned long now = micros();
    float dt = (now - lastPIDTime) / 1000000.0f;
    lastPIDTime = now;

    if (dt <= 0.0f || dt > 0.03f)
        dt = 0.005f;

    // Positive angle = forward tilt, negative = backward tilt.
    // This error definition is symmetric, so both tilt directions
    // are corrected the same way -- as long as ANGLE_SIGN matches
    // your physical mounting.
    pidError = filteredAngle - balanceSetpoint;

    pidIntegral += pidError * dt;
    pidIntegral = constrain(pidIntegral, -50.0f, 50.0f);

    // Derivative on measurement: the gyro term must oppose the direction
    // of motion. A positive tilt correction should be reduced while the
    // robot is still rotating farther into that tilt.
    float usableGyroRate = fabsf(gyroRate) < GYRO_RATE_DEADBAND ? 0.0f : gyroRate;
    pidDerivative = -usableGyroRate;

    pidProportional = Kp * pidError;
    pidIntegralOutput = Ki * pidIntegral;
    pidDerivativeOutput = constrain(Kd * pidDerivative, -MAX_D_TERM, MAX_D_TERM);
    // Tune the inner balance loop by itself first. Position hold, wheel
    // damping, sync, and trim can mask the pitch response and create a
    // second oscillation when the center of mass is high.
    pidOutput = pidProportional + pidIntegralOutput + pidDerivativeOutput;
    pidOutput = constrain(pidOutput, -PID_LIMIT, PID_LIMIT);

    return pidOutput;
}

void controlMotors()
{
    if (!systemEnabled)
    {
        stopMotors();
        return;
    }

    // Stop if MPU has failed to respond for several consecutive
    // reads -- a single glitch shouldn't kill balancing, but a
    // sustained I2C fault must not leave motors running blind.
    if (!mpuOK || mpuFailStreak > 5)
    {
        stopMotors();
        return;
    }

    if (fabsf(filteredAngle - balanceSetpoint) > FALLEN_ANGLE_LIMIT)
    {
        stopMotors();
        pidIntegral = 0.0f;
        positionIntegral = 0.0f;
        return;
    }

    float balanceOutput = calculateBalancePID();
    balanceOutput *= MOTOR_DIRECTION_SIGN;

    // Keep the balance loop active near upright. Stopping here resets the
    // startup ramp, which delays the next correction and increases sway
    // when the center of mass is high.
    const float angleError = filteredAngle - balanceSetpoint;
    if (fabsf(angleError) < UPRIGHT_ANGLE_DEADBAND &&
        fabsf(gyroRate) < UPRIGHT_RATE_DEADBAND &&
        fabsf(velocityFeedback) < UPRIGHT_VELOCITY_DEADBAND)
    {
        pidIntegral *= 0.9f;
    }

    if (!motorsStarted)
    {
        // Re-arm outer position hold at the real balancing start moment.
        // The robot is often moved by hand after boot calibration, so a
        // boot-time target can become stale and create persistent drive.
        motorsStarted = true;
        motorStartTime = millis();
        lastLeftCount = leftEncoderCount;
        lastRightCount = rightEncoderCount;
        accumulatedPosition = 0.0f;
        filteredPosition = 0.0f;
        positionIntegral = 0.0f;
    }

    // ---- Position-hold control: prevent linear drift using raw encoder counts ----
    // This is more direct and responsive than RPM-based estimation, since encoder
    // counts have no filtering lag. We measure net displacement as the difference
    // between left and right wheel travel.
    long currentLeftCount = leftEncoderCount;
    long currentRightCount = rightEncoderCount;
    long leftDelta = currentLeftCount - lastLeftCount;
    long rightDelta = currentRightCount - lastRightCount;

    // Net motion = average of both wheels. The encoder signs are already
    // baked into the ISR, so we just average the deltas.
    float netMotion = ((float)leftDelta + (float)rightDelta) / 2.0f;

    // Simple integration: just accumulate net motion
    // No decay — let the integral correction handle persistence
    accumulatedPosition += netMotion;

    // Clamp to prevent saturation
    accumulatedPosition = constrain(accumulatedPosition, -MAX_POSITION_ERROR, MAX_POSITION_ERROR);

    // Smooth the accumulated position to reject encoder noise
    filteredPosition = POSITION_HOLD_FILTER_ALPHA * accumulatedPosition +
                       (1.0f - POSITION_HOLD_FILTER_ALPHA) * filteredPosition;

    // Position-hold feedback with integral term to overcome persistent bias:
    // P term: proportional to current position (fast response)
    // I term: accumulates error over time (builds up correction for persistent drift)
    // This is like a mini PID for position hold

    // Accumulate integral - this grows when position error is non-zero
    // Stops growing once correction balances the drift force
    positionIntegral += filteredPosition * 0.025f;                       // very fast accumulation (was 0.01f)
    positionIntegral = constrain(positionIntegral, -25000.0f, 25000.0f); // very high windup limit (was 15000)

    // Scale from encoder counts to PWM
    float scaledPosition = filteredPosition / 40.0f; // proportional term
    float scaledIntegral = positionIntegral / 50.0f; // integral term (2x stronger, was 1/100)

    // Combined correction: P + I
    float positionCorrection = -(scaledPosition + scaledIntegral) * positionHoldGain;
    positionCorrection = constrain(positionCorrection, -MAX_POSITION_CORRECTION, MAX_POSITION_CORRECTION);
    positionFeedback = positionCorrection;

    // Update encoder baselines for next iteration
    lastLeftCount = currentLeftCount;
    lastRightCount = currentRightCount;

    unsigned long elapsed = millis() - motorStartTime;
    float startupFactor = (float)elapsed / (float)START_RAMP_TIME;
    startupFactor = constrain(startupFactor, 0.0f, 1.0f);

    // Apply position correction on top of balance output
    float leftCommand = balanceOutput + positionCorrection;
    float rightCommand = balanceOutput + positionCorrection;

    float syncCorrection = 0.0f;

    if (fabsf(balanceOutput) > 2.0f)
    {
        if (balanceOutput > 0.0f)
        {
            leftCommand = balanceOutput - syncCorrection;
            rightCommand = balanceOutput + syncCorrection;
        }
        else
        {
            leftCommand = balanceOutput + syncCorrection;
            rightCommand = balanceOutput - syncCorrection;
        }
    }

    leftCommand *= startupFactor;
    rightCommand *= startupFactor;
    rightCommand *= RIGHT_MOTOR_SCALE;
    leftCommand *= LEFT_MOTOR_SCALE;

    leftCommand = slewLeftCommand(leftCommand);
    rightCommand = slewRightCommand(rightCommand);

    applyLeftMotor(leftCommand);
    applyRightMotor(rightCommand);
}
