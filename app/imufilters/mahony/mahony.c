//=====================================================================================================
// MahonyAHRS.c
//=====================================================================================================
//
// Madgwick's implementation of Mayhony's AHRS algorithm.
// See: http://www.x-io.co.uk/node/8#open_source_ahrs_and_imu_algorithms
//
// Date            Author            Notes
// 29/09/2011    SOH Madgwick    Initial release
// 02/10/2011    SOH Madgwick    Optimised for reduced CPU load
//
//=====================================================================================================

//---------------------------------------------------------------------------------------------------
// Header files

#include "mahony.h"
#include "c_math.h"

//---------------------------------------------------------------------------------------------------
// Definitions

#define sampleFreq  512.0f      // sample frequency in Hz
#define twoKpDef    (2.0f * 0.5f) // 2 * proportional gain
#define twoKiDef    (2.0f * 0.0f) // 2 * integral gain

//---------------------------------------------------------------------------------------------------
// Variable definitions

static float twoKp = twoKpDef;                                            // 2 * proportional gain (Kp)
static float twoKi = twoKiDef;                                            // 2 * integral gain (Ki)
static float qm0 = 1.0f, qm1 = 0.0f, qm2 = 0.0f, qm3 = 0.0f;              // quaternion of sensor frame relative to auxiliary frame
static float integralFBx = 0.0f,  integralFBy = 0.0f, integralFBz = 0.0f; // integral error terms scaled by Ki

//---------------------------------------------------------------------------------------------------
// Function declarations

float invSqrt(float x);

//====================================================================================================
// Functions

//---------------------------------------------------------------------------------------------------
// AHRS algorithm update

void MahonyUpdate(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz) {
    float recipNorm;
    float q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;  
    float hx, hy, bx, bz;
    float halfvx, halfvy, halfvz, halfwx, halfwy, halfwz;
    float halfex, halfey, halfez;
    float qa, qb, qc;

    // Use IMU algorithm if magnetometer measurement invalid (avoids NaN in magnetometer normalisation)
    if((mx == 0.0f) && (my == 0.0f) && (mz == 0.0f)) {
        MahonyUpdateIMU(gx, gy, gz, ax, ay, az);
        return;
    }

    // Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
    if(!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {

        // Normalise accelerometer measurement
        recipNorm = invSqrt(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        // Normalise magnetometer measurement
        recipNorm = invSqrt(mx * mx + my * my + mz * mz);
        mx *= recipNorm;
        my *= recipNorm;
        mz *= recipNorm;

        // Auxiliary variables to avoid repeated arithmetic
        q0q0 = qm0 * qm0;
        q0q1 = qm0 * qm1;
        q0q2 = qm0 * qm2;
        q0q3 = qm0 * qm3;
        q1q1 = qm1 * qm1;
        q1q2 = qm1 * qm2;
        q1q3 = qm1 * qm3;
        q2q2 = qm2 * qm2;
        q2q3 = qm2 * qm3;
        q3q3 = qm3 * qm3;   

        // Reference direction of Earth's magnetic field
        hx = 2.0f * (mx * (0.5f - q2q2 - q3q3) + my * (q1q2 - q0q3) + mz * (q1q3 + q0q2));
        hy = 2.0f * (mx * (q1q2 + q0q3) + my * (0.5f - q1q1 - q3q3) + mz * (q2q3 - q0q1));
        bx = sqrt(hx * hx + hy * hy);
        bz = 2.0f * (mx * (q1q3 - q0q2) + my * (q2q3 + q0q1) + mz * (0.5f - q1q1 - q2q2));

        // Estimated direction of gravity and magnetic field
        halfvx = q1q3 - q0q2;
        halfvy = q0q1 + q2q3;
        halfvz = q0q0 - 0.5f + q3q3;
        halfwx = bx * (0.5f - q2q2 - q3q3) + bz * (q1q3 - q0q2);
        halfwy = bx * (q1q2 - q0q3) + bz * (q0q1 + q2q3);
        halfwz = bx * (q0q2 + q1q3) + bz * (0.5f - q1q1 - q2q2);  

        // Error is sum of cross product between estimated direction and measured direction of field vectors
        halfex = (ay * halfvz - az * halfvy) + (my * halfwz - mz * halfwy);
        halfey = (az * halfvx - ax * halfvz) + (mz * halfwx - mx * halfwz);
        halfez = (ax * halfvy - ay * halfvx) + (mx * halfwy - my * halfwx);

        // Compute and apply integral feedback if enabled
        if(twoKi > 0.0f) {
            integralFBx += twoKi * halfex * (1.0f / sampleFreq);    // integral error scaled by Ki
            integralFBy += twoKi * halfey * (1.0f / sampleFreq);
            integralFBz += twoKi * halfez * (1.0f / sampleFreq);
            gx += integralFBx;    // apply integral feedback
            gy += integralFBy;
            gz += integralFBz;
        }
        else {
            integralFBx = 0.0f;    // prevent integral windup
            integralFBy = 0.0f;
            integralFBz = 0.0f;
        }

        // Apply proportional feedback
        gx += twoKp * halfex;
        gy += twoKp * halfey;
        gz += twoKp * halfez;
    }

    // Integrate rate of change of quaternion
    gx *= (0.5f * (1.0f / sampleFreq));        // pre-multiply common factors
    gy *= (0.5f * (1.0f / sampleFreq));
    gz *= (0.5f * (1.0f / sampleFreq));
    qa = qm0;
    qb = qm1;
    qc = qm2;
    qm0 += (-qb * gx - qc * gy - qm3 * gz);
    qm1 += (qa * gx + qc * gz - qm3 * gy);
    qm2 += (qa * gy - qb * gz + qm3 * gx);
    qm3 += (qa * gz + qb * gy - qc * gx);

    // Normalise quaternion
    recipNorm = invSqrt(qm0 * qm0 + qm1 * qm1 + qm2 * qm2 + qm3 * qm3);
    qm0 *= recipNorm;
    qm1 *= recipNorm;
    qm2 *= recipNorm;
    qm3 *= recipNorm;
    MAHONY_DEBUG("MahonyUpdate:  qm0(%d), qm1(%d), qm2(%d), qm3(%d)\n",qm0, qm1, qm2, qm3);
}

//---------------------------------------------------------------------------------------------------
// IMU algorithm update

void MahonyUpdateIMU(float gx, float gy, float gz, float ax, float ay, float az) {
    float recipNorm;
    float halfvx, halfvy, halfvz;
    float halfex, halfey, halfez;
    float qa, qb, qc;

    // Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
    if(!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {

        // Normalise accelerometer measurement
        recipNorm = invSqrt(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        // Estimated direction of gravity and vector perpendicular to magnetic flux
        halfvx = qm1 * qm3 - qm0 * qm2;
        halfvy = qm0 * qm1 + qm2 * qm3;
        halfvz = qm0 * qm0 - 0.5f + qm3 * qm3;

        // Error is sum of cross product between estimated and measured direction of gravity
        halfex = (ay * halfvz - az * halfvy);
        halfey = (az * halfvx - ax * halfvz);
        halfez = (ax * halfvy - ay * halfvx);

        // Compute and apply integral feedback if enabled
        if(twoKi > 0.0f) {
            integralFBx += twoKi * halfex * (1.0f / sampleFreq);    // integral error scaled by Ki
            integralFBy += twoKi * halfey * (1.0f / sampleFreq);
            integralFBz += twoKi * halfez * (1.0f / sampleFreq);
            gx += integralFBx;    // apply integral feedback
            gy += integralFBy;
            gz += integralFBz;
        }
        else {
            integralFBx = 0.0f;    // prevent integral windup
            integralFBy = 0.0f;
            integralFBz = 0.0f;
        }

        // Apply proportional feedback
        gx += twoKp * halfex;
        gy += twoKp * halfey;
        gz += twoKp * halfez;
    }

    // Integrate rate of change of quaternion
    gx *= (0.5f * (1.0f / sampleFreq));        // pre-multiply common factors
    gy *= (0.5f * (1.0f / sampleFreq));
    gz *= (0.5f * (1.0f / sampleFreq));
    qa = qm0;
    qb = qm1;
    qc = qm2;
    qm0 += (-qb * gx - qc * gy - qm3 * gz);
    qm1 += (qa * gx + qc * gz - qm3 * gy);
    qm2 += (qa * gy - qb * gz + qm3 * gx);
    qm3 += (qa * gz + qb * gy - qc * gx);

    // Normalise quaternion
    recipNorm = invSqrt(qm0 * qm0 + qm1 * qm1 + qm2 * qm2 + qm3 * qm3);
    qm0 *= recipNorm;
    qm1 *= recipNorm;
    qm2 *= recipNorm;
    qm3 *= recipNorm;
    MAHONY_DEBUG("MahonyUpdateIMU:  qm0(%d), qm1(%d), qm2(%d), qm3(%d)\n",qm0, qm1, qm2, qm3);
}

//---------------------------------------------------------------------------------------------------
// Fast inverse square-root
// See: http://en.wikipedia.org/wiki/Fast_inverse_square_root

float invSqrt(float x) {
    float halfx = 0.5f * x;
    float y = x;
    long i = *(long*)&y;
    i = 0x5f3759df - (i>>1);
    y = *(float*)&i;
    y = y * (1.5f - (halfx * y * y));
    return y;
}

void MahonyComputeAngles(float *roll, float *pitch, float *yaw) {
    *roll = atan2f(qm0 * qm1 + qm2 * qm3, 0.5f - qm1 * qm1 - qm2 * qm2);
    *pitch = asinf(-2.0f * (qm1 * qm3 - qm0 * qm2));
    *yaw = atan2f(qm1 * qm2 + qm0 * qm3, 0.5f - qm2 * qm2 - qm3 * qm3);
    MAHONY_DEBUG("MahonyComputeAngles:  roll(%d), pitch(%d), yaw(%d)\n", roll, pitch, yaw);
}

void MahonyQuaternions(float *q0, float *q1, float *q2, float *q3){
    *q0 = qm0;
    *q1 = qm1;
    *q2 = qm2;
    *q3 = qm3;
}

//====================================================================================================
// END OF CODE
//====================================================================================================
