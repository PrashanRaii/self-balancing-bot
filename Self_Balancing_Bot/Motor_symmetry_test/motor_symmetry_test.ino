#include <Arduino.h>

// Same wiring as the balancing project.
const int LEFT_ENA = 25;
const int LEFT_IN1 = 26;
const int LEFT_IN2 = 27;
const int RIGHT_ENA = 14;
const int RIGHT_IN3 = 4;
const int RIGHT_IN4 = 5;
const int LEFT_C1 = 18;
const int LEFT_C2 = 19;
const int RIGHT_C1 = 23;
const int RIGHT_C2 = 13;
const float COUNTS_PER_REV = 1215.0f;
const bool RIGHT_REVERSE = true;

volatile long leftCount = 0;
volatile long rightCount = 0;
volatile uint8_t leftPrevious = 0;
volatile uint8_t rightPrevious = 0;
const int8_t encoderTable[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void IRAM_ATTR leftISR()
{
    uint8_t state = (digitalRead(LEFT_C1) << 1) | digitalRead(LEFT_C2);
    leftCount += encoderTable[(leftPrevious << 2) | state];
    leftPrevious = state;
}

void IRAM_ATTR rightISR()
{
    uint8_t state = (digitalRead(RIGHT_C1) << 1) | digitalRead(RIGHT_C2);
    rightCount += encoderTable[(rightPrevious << 2) | state];
    rightPrevious = state;
}

void setLeftMotor(int pwm)
{
    digitalWrite(LEFT_IN1, HIGH);
    digitalWrite(LEFT_IN2, LOW);
    ledcWrite(LEFT_ENA, pwm);
}

void setRightMotor(int pwm)
{
    digitalWrite(RIGHT_IN3, RIGHT_REVERSE ? LOW : HIGH);
    digitalWrite(RIGHT_IN4, RIGHT_REVERSE ? HIGH : LOW);
    ledcWrite(RIGHT_ENA, pwm);
}

void setMotors(int leftPWM, int rightPWM)
{
    if (leftPWM > 0)
        setLeftMotor(leftPWM);
    else
    {
        ledcWrite(LEFT_ENA, 0);
        digitalWrite(LEFT_IN1, LOW);
        digitalWrite(LEFT_IN2, LOW);
    }

    if (rightPWM > 0)
        setRightMotor(rightPWM);
    else
    {
        ledcWrite(RIGHT_ENA, 0);
        digitalWrite(RIGHT_IN3, LOW);
        digitalWrite(RIGHT_IN4, LOW);
    }
}

void stopMotors()
{
    ledcWrite(LEFT_ENA, 0);
    ledcWrite(RIGHT_ENA, 0);
    digitalWrite(LEFT_IN1, LOW);
    digitalWrite(LEFT_IN2, LOW);
    digitalWrite(RIGHT_IN3, LOW);
    digitalWrite(RIGHT_IN4, LOW);
}

void setup()
{
    Serial.begin(115200);
    pinMode(LEFT_IN1, OUTPUT);
    pinMode(LEFT_IN2, OUTPUT);
    pinMode(RIGHT_IN3, OUTPUT);
    pinMode(RIGHT_IN4, OUTPUT);
    pinMode(LEFT_C1, INPUT_PULLUP);
    pinMode(LEFT_C2, INPUT_PULLUP);
    pinMode(RIGHT_C1, INPUT_PULLUP);
    pinMode(RIGHT_C2, INPUT_PULLUP);
    ledcAttach(LEFT_ENA, 20000, 8);
    ledcAttach(RIGHT_ENA, 20000, 8);
    leftPrevious = (digitalRead(LEFT_C1) << 1) | digitalRead(LEFT_C2);
    rightPrevious = (digitalRead(RIGHT_C1) << 1) | digitalRead(RIGHT_C2);
    attachInterrupt(digitalPinToInterrupt(LEFT_C1), leftISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(LEFT_C2), leftISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(RIGHT_C1), rightISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(RIGHT_C2), rightISR, CHANGE);
    stopMotors();
    Serial.println("MOTOR SYMMETRY TEST");
    Serial.println("Wheels must be lifted. Battery power is required.");
    Serial.println("Testing BOTH, LEFT ONLY, RIGHT ONLY at PWM 70, 80, 90.");
    Serial.println("Each level runs for 5 seconds. Wheels must stay lifted.");
}

void loop()
{
    static const int pwmTests[] = {70, 80, 90};
    static const char *modes[] = {"BOTH", "LEFT_ONLY", "RIGHT_ONLY"};
    static int modeIndex = 0;
    static int pwmIndex = 0;
    static unsigned long testStart = millis();
    static long previousLeft = 0;
    static long previousRight = 0;
    static unsigned long previousTime = millis();

    unsigned long now = millis();
    unsigned long elapsed = now - testStart;
    if (elapsed >= 5000)
    {
        stopMotors();
        Serial.println("Changing test phase...");
        pwmIndex++;
        if (pwmIndex >= 3)
        {
            pwmIndex = 0;
            modeIndex++;
        }
        if (modeIndex >= 3)
        {
            Serial.println("TEST COMPLETE. Turn off battery power.");
            while (true)
            {
                stopMotors();
                delay(1000);
            }
        }
        testStart = now;
        previousTime = now;
        noInterrupts();
        previousLeft = leftCount;
        previousRight = rightCount;
        interrupts();
    }

    int pwm = pwmTests[pwmIndex];
    int leftPWM = (modeIndex == 2) ? 0 : pwm;
    int rightPWM = (modeIndex == 1) ? 0 : pwm;
    setMotors(leftPWM, rightPWM);

    if (now - previousTime >= 500)
    {
        float seconds = (now - previousTime) / 1000.0f;
        long currentLeft, currentRight;
        noInterrupts();
        currentLeft = leftCount;
        currentRight = rightCount;
        interrupts();
        float leftRPM = ((currentLeft - previousLeft) / COUNTS_PER_REV) / seconds * 60.0f;
        float rightRPM = ((currentRight - previousRight) / COUNTS_PER_REV) / seconds * 60.0f;
        Serial.print("MODE=");
        Serial.print(modes[modeIndex]);
        Serial.print(" | PWM=");
        Serial.print(pwm);
        Serial.print(" | LEFT_RPM=");
        Serial.print(-leftRPM, 2);
        Serial.print(" | RIGHT_RPM=");
        Serial.print(rightRPM, 2);
        Serial.print(" | LEFT_COUNTS=");
        Serial.print(-(currentLeft - previousLeft));
        Serial.print(" | RIGHT_COUNTS=");
        Serial.println(currentRight - previousRight);
        previousLeft = currentLeft;
        previousRight = currentRight;
        previousTime = now;
    }
}
