#pragma once
#include <Arduino.h>

class SimpleKalmanFilter {
public:
  SimpleKalmanFilter(float mea_e, float est_e, float q) {
    _err_measure = mea_e;
    _err_estimate = est_e;
    _q = q;
  }

  float updateEstimate(float mea) {
    float denom = _err_estimate + _err_measure;
    if (denom < 1e-10f) return mea; // prevent division by zero
    _kalman_gain = _err_estimate / denom;
    _current_estimate = _last_estimate + _kalman_gain * (mea - _last_estimate);
    _err_estimate =  (1.0f - _kalman_gain) * _err_estimate + fabsf(_last_estimate - _current_estimate) * _q;
    _last_estimate = _current_estimate;

    return _current_estimate;
  }

  void setMeasurementError(float mea_e) {
    _err_measure = mea_e;
  }

  void setEstimateError(float est_e) {
    _err_estimate = est_e;
  }

  void setProcessNoise(float q) {
    _q = q;
  }

  float getEstimateError() {
    return _err_estimate;
  }
  
  // Reset filter to a new known position (teleport)
  void setEstimate(float est) {
      _last_estimate = est;
      _current_estimate = est;
      _err_estimate = _err_measure; // Reset error to initial state
  }

private:
  float _err_measure;
  float _err_estimate;
  float _q;
  float _current_estimate = 0;
  float _last_estimate = 0;
  float _kalman_gain = 0;
};
