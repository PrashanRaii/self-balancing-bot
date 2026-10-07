#include <Arduino.h>
#include "motor_control.h"
#include "config.h"

// ============================================================
// ENCODERS (ISRs + quadrature decode table -- private to this file)
// ============================================================

static volatile uint8_t leftPreviousState = 0;
static volatile uint8_t rightPreviousState = 0;

static const int8_t encoderTable[16] =
    {
        0, -1, 1, 0,
        1, 0, 0, -1,
        -1, 0, 0, 1,
        0, 1, -1, 0};

static void IRAM_ATTR leftEncoderISR()
{
    uint8_t a = digitalRead(LEFT_C1);
    uint8_t b = digitalRead(LEFT_C2);
    uint8_t currentState = (a << 1) | b;
    uint8_t index = (leftPreviousState << 2) | currentState;
    leftEncoderCount += encoderTable[index];
    leftPreviousState = currentState;
}

static void IRAM_ATTR rightEncoderISR()
{
    uint8_t a = digitalRead(RIGHT_C1);
    uint8_t b = digitalRead(RIGHT_C2);
    uint8_t currentState = (a << 1) | b;
    uint8_t index = (rightPreviousState << 2) | currentState;
    rightEncoderCount += encoderTable[index];
    rightPreviousState = currentState;
}

// ============================================================
// RPM (private raw values -- leftRPMFiltered/rightRPMFiltered
// are the shared, externally-visible ones)
// ============================================================

static float leftRPM = 0.0f;
static float rightRPM = 0.0f;
static long previousLeftCount = 0;
static long previousRightCount = 0;
static unsigned long lastRPMTime = 0;

// ============================================================
// PWM SLEW LIMIT (private "last command" memory per wheel)
// ============================================================

static float previousLeftCommand = 0.0f;
static float previousRightCommand = 0.0f;

static int nonlinearPWM(float magnitude, int minimumPWM)
{
    if (magnitude <= MOTOR_COMMAND_DEADBAND)
        return 0;

    // Keep command magnitude proportional to PWM. Applying minimumPWM to
    // every nonzero command made a small PID correction become a sudden
    // high-torque motor burst.
    float normalized = constrain(magnitude / 255.0f, 0.0f, 1.0f);
    float curved = powf(normalized, PWM_CURVE_EXPONENT);
    float pwm = curved * 255.0f;
    // can remove this`?
    //  pwm = minimumPWM + (pwm / 255.0f) * (255 - minimumPWM);

    return constrain((int)roundf(pwm), 0, 255);
}

void initMotors()
{
    pinMode(LEFT_IN1, OUTPUT);
    pinMode(LEFT_IN2, OUTPUT);
    pinMode(RIGHT_IN3, OUTPUT);
    pinMode(RIGHT_IN4, OUTPUT);

    // 20 kHz keeps motor whine inaudible and gives smooth, low-
    // latency torque control on both channels equally.
    ledcAttach(LEFT_ENA, 20000, 8);
    ledcAttach(RIGHT_ENA, 20000, 8);

    stopMotors();
}

void initEncoders()
{
    pinMode(LEFT_C1, INPUT_PULLUP);
    pinMode(LEFT_C2, INPUT_PULLUP);
    pinMode(RIGHT_C1, INPUT_PULLUP);
    pinMode(RIGHT_C2, INPUT_PULLUP);

    leftPreviousState = (digitalRead(LEFT_C1) << 1) | digitalRead(LEFT_C2);
    rightPreviousState = (digitalRead(RIGHT_C1) << 1) | digitalRead(RIGHT_C2);

    attachInterrupt(digitalPinToInterrupt(LEFT_C1), leftEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(LEFT_C2), leftEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(RIGHT_C1), rightEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(RIGHT_C2), rightEncoderISR, CHANGE);

    lastRPMTime = millis();
}

void calculateRPM()
{
    unsigned long now = millis();
    unsigned long elapsed = now - lastRPMTime;
    if (elapsed < 50)
        return;

    lastRPMTime = now;

    long currentLeftCount, currentRightCount;
    noInterrupts();
    currentLeftCount = leftEncoderCount;
    currentRightCount = rightEncoderCount;
    interrupts();

    long leftDifference = currentLeftCount - previousLeftCount;
    long rightDifference = currentRightCount - previousRightCount;

    previousLeftCount = currentLeftCount;
    previousRightCount = currentRightCount;

    float seconds = elapsed / 1000.0f;

    if (seconds > 0.0f && COUNTS_PER_REV > 0.0f)
    {
        leftRPM = ((float)leftDifference / COUNTS_PER_REV) / seconds * 60.0f;
        rightRPM = ((float)rightDifference / COUNTS_PER_REV) / seconds * 60.0f;
    }

    if (fabsf(leftRPM) < 0.5f)
        leftRPM = 0.0f;
    if (fabsf(rightRPM) < 0.5f)
        rightRPM = 0.0f;

    // Exponential smoothing removes encoder-jitter noise from the
    // value actually used for synchronization, so the motors stop
    // "fighting" each other on tiny count fluctuations.
    leftRPMFiltered = RPM_FILTER_ALPHA * leftRPM + (1.0f - RPM_FILTER_ALPHA) * leftRPMFiltered;
    rightRPMFiltered = RPM_FILTER_ALPHA * rightRPM + (1.0f - RPM_FILTER_ALPHA) * rightRPMFiltered;

    rpmDifference = fabsf(leftRPMFiltered) - fabsf(rightRPMFiltered);
}

void applyLeftMotor(float command)
{
    float magnitude = fabsf(command);

    if (magnitude < 1.0f)
    {
        ledcWrite(LEFT_ENA, 0);
        digitalWrite(LEFT_IN1, LOW);
        digitalWrite(LEFT_IN2, LOW);
        leftPWM = 0;
        leftDirection = "STOP";
        return;
    }

    int pwm = nonlinearPWM(magnitude, leftMinPWM);

    bool physicalForward = command > 0.0f;
    if (LEFT_REVERSE)
        physicalForward = !physicalForward;

    if (physicalForward)
    {
        digitalWrite(LEFT_IN1, HIGH);
        digitalWrite(LEFT_IN2, LOW);
        leftDirection = "FORWARD";
    }
    else
    {
        digitalWrite(LEFT_IN1, LOW);
        digitalWrite(LEFT_IN2, HIGH);
        leftDirection = "BACKWARD";
    }

    ledcWrite(LEFT_ENA, pwm);
    leftPWM = pwm;
}

void applyRightMotor(float command)
{
    float magnitude = fabsf(command);

    if (magnitude < 1.0f)
    {
        ledcWrite(RIGHT_ENA, 0);
        digitalWrite(RIGHT_IN3, LOW);
        digitalWrite(RIGHT_IN4, LOW);
        rightPWM = 0;
        rightDirection = "STOP";
        return;
    }

    int pwm = nonlinearPWM(magnitude, rightMinPWM);

    bool physicalForward = command > 0.0f;
    if (RIGHT_REVERSE)
        physicalForward = !physicalForward;

    if (physicalForward)
    {
        digitalWrite(RIGHT_IN3, HIGH);
        digitalWrite(RIGHT_IN4, LOW);
        rightDirection = "FORWARD";
    }
    else
    {
        digitalWrite(RIGHT_IN3, LOW);
        digitalWrite(RIGHT_IN4, HIGH);
        rightDirection = "BACKWARD";
    }

    ledcWrite(RIGHT_ENA, pwm);
    rightPWM = pwm;
}

void stopMotors()
{
    ledcWrite(LEFT_ENA, 0);
    ledcWrite(RIGHT_ENA, 0);

    digitalWrite(LEFT_IN1, LOW);
    digitalWrite(LEFT_IN2, LOW);
    digitalWrite(RIGHT_IN3, LOW);
    digitalWrite(RIGHT_IN4, LOW);

    leftPWM = 0;
    rightPWM = 0;
    leftDirection = "STOP";
    rightDirection = "STOP";

    previousLeftCommand = 0.0f;
    previousRightCommand = 0.0f;

    motorsStarted = false;
}

float slewLeftCommand(float target)
{
    float difference = target - previousLeftCommand;
    if (difference > MAX_PWM_CHANGE)
        target = previousLeftCommand + MAX_PWM_CHANGE;
    else if (difference < -MAX_PWM_CHANGE)
        target = previousLeftCommand - MAX_PWM_CHANGE;
    previousLeftCommand = target;
    return target;
}

float slewRightCommand(float target)
{
    float difference = target - previousRightCommand;
    if (difference > MAX_PWM_CHANGE)
        target = previousRightCommand + MAX_PWM_CHANGE;
    else if (difference < -MAX_PWM_CHANGE)
        target = previousRightCommand - MAX_PWM_CHANGE;

    previousRightCommand = target;
    return target;
}

float calculateSyncCorrection()
{
    float leftSpeed = fabsf(leftRPMFiltered);
    float rightSpeed = fabsf(rightRPMFiltered);
    float difference = leftSpeed - rightSpeed;

    rpmDifference = difference;

    float correction = difference * syncGain;
    correction = constrain(correction, -MAX_SYNC_CORRECTION, MAX_SYNC_CORRECTION);

    return correction;
}

void updateMotorTrim()
{
    float leftSpeed = fabsf(leftRPMFiltered);
    float rightSpeed = fabsf(rightRPMFiltered);
    float difference = leftSpeed - rightSpeed;

    if (leftSpeed < 5.0f || rightSpeed < 5.0f)
        return;
    if (fabsf(difference) > 30.0f)
        return;

    if (difference < -1.0f)
    {
        leftMotorTrim += TRIM_LEARNING_RATE;
        rightMotorTrim -= TRIM_LEARNING_RATE;
    }
    else if (difference > 1.0f)
    {
        rightMotorTrim += TRIM_LEARNING_RATE;
        leftMotorTrim -= TRIM_LEARNING_RATE;
    }

    leftMotorTrim = constrain(leftMotorTrim, -MAX_MOTOR_TRIM, MAX_MOTOR_TRIM);
    rightMotorTrim = constrain(rightMotorTrim, -MAX_MOTOR_TRIM, MAX_MOTOR_TRIM);
}
