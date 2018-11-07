//=====================================================================================================
// MahonyAHRS.h
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
#ifndef MahonyAHRS_h
#define MahonyAHRS_h

//#define MAHONY_DEBUG_ON
//#define MAHONY_ERROR

#if defined(MAHONY_DEBUG_ON)
  static const char log_prefix[] = "MAHONY: ";
  #define MAHONY_DEBUG(format, ...) dbg_printf("%s"format"\n", log_prefix, ##__VA_ARGS__)
#else
  #define MAHONY_DEBUG(...)
#endif
#if defined(MAHONY_ERROR)
  #define MAHONY_ERR(format, ...) NODE_ERR("%s"format"\n", log_prefix, ##__VA_ARGS__)
#else
  #define MAHONY_ERR(...)
#endif
//----------------------------------------------------------------------------------------------------
// Variable declaration

static float twoKp;                // 2 * proportional gain (Kp)
static float twoKi;                // 2 * integral gain (Ki)
static float qm0, qm1, qm2, qm3;   // quaternion of sensor frame relative to auxiliary frame
static float roll, pitch, yaw;
static float invSampleFreq;
static float integralFBx,  integralFBy, integralFBz; // integral error terms scaled by Ki

//---------------------------------------------------------------------------------------------------
// Function declarations

void MahonyUpdate(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz);
void MahonyUpdateIMU(float gx, float gy, float gz, float ax, float ay, float az);
void MahonyComputeAngles(float *roll, float *pitch, float *yaw);
void MahonyQuaternions(float *q0, float *q1, float *q2, float *q3);

#endif
//=====================================================================================================
// End of file
//=====================================================================================================
