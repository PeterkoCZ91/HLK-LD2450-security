#pragma once
#include <Arduino.h>
#include <cmath>

/**
 * EKF2D - Lightweight 2D Extended Kalman Filter for target tracking
 * State: [x, y, vx, vy] (position + velocity)
 * Measurement: [x, y] (position only, from LD2450)
 *
 * No Eigen, no STL, no dynamic allocation. ~100us per update on ESP32.
 */
class EKF2D {
public:
    EKF2D() { reset(0, 0); }

    void reset(float x0, float y0) {
        // State vector
        _x[0] = x0;   // x position (mm)
        _x[1] = y0;   // y position (mm)
        _x[2] = 0;    // vx velocity (mm/s)
        _x[3] = 0;    // vy velocity (mm/s)

        // Covariance matrix P (4x4) - diagonal init
        memset(_cov, 0, sizeof(_cov));
        _cov[0][0] = 500.0f;   // x uncertainty
        _cov[1][1] = 500.0f;   // y uncertainty
        _cov[2][2] = 1000.0f;  // vx uncertainty (unknown velocity)
        _cov[3][3] = 1000.0f;  // vy uncertainty

        _lastUpdateMs = 0;
        _initialized = false;
    }

    /**
     * Predict + Update in one step
     * @param measX measured X position (mm)
     * @param measY measured Y position (mm)
     * @param nowMs current millis()
     */
    void update(float measX, float measY, unsigned long nowMs) {
        if (!_initialized) {
            _x[0] = measX;
            _x[1] = measY;
            _x[2] = 0;
            _x[3] = 0;
            _lastUpdateMs = nowMs;
            _initialized = true;
            return;
        }

        float dt = (nowMs - _lastUpdateMs) / 1000.0f; // seconds
        _lastUpdateMs = nowMs;

        // Clamp dt to reasonable range
        if (dt <= 0.001f) dt = 0.001f;
        if (dt > 2.0f) dt = 2.0f;

        // === PREDICT ===
        // State prediction: x_pred = F * x
        float x_pred[4];
        x_pred[0] = _x[0] + _x[2] * dt;  // x + vx*dt
        x_pred[1] = _x[1] + _x[3] * dt;  // y + vy*dt
        x_pred[2] = _x[2];                // vx constant
        x_pred[3] = _x[3];                // vy constant

        // Covariance prediction: P_pred = F*P*F' + Q
        // F = [[1,0,dt,0],[0,1,0,dt],[0,0,1,0],[0,0,0,1]]
        float P_pred[4][4];
        predictCovariance(dt, P_pred);

        // === UPDATE ===
        // Innovation: y = z - H*x_pred (H = [[1,0,0,0],[0,1,0,0]])
        float innov[2];
        innov[0] = measX - x_pred[0];
        innov[1] = measY - x_pred[1];

        // Innovation covariance: S = H*P_pred*H' + R
        float S[2][2];
        S[0][0] = P_pred[0][0] + _R[0];
        S[0][1] = P_pred[0][1];
        S[1][0] = P_pred[1][0];
        S[1][1] = P_pred[1][1] + _R[1];

        // Invert 2x2 S matrix
        float det = S[0][0] * S[1][1] - S[0][1] * S[1][0];
        if (fabsf(det) < 1e-10f) {
            // Singular - reset to measurement with zero velocity
            _x[0] = measX;
            _x[1] = measY;
            _x[2] = 0;
            _x[3] = 0;
            // Reinitialize covariance
            memset(_cov, 0, sizeof(_cov));
            _cov[0][0] = _cov[1][1] = 100.0f;
            _cov[2][2] = _cov[3][3] = 50.0f;
            return;
        }
        float invDet = 1.0f / det;
        float S_inv[2][2];
        S_inv[0][0] =  S[1][1] * invDet;
        S_inv[0][1] = -S[0][1] * invDet;
        S_inv[1][0] = -S[1][0] * invDet;
        S_inv[1][1] =  S[0][0] * invDet;

        // Kalman gain: K = P_pred * H' * S_inv (4x2)
        // H' = [[1,0],[0,1],[0,0],[0,0]], so K[:,j] = P_pred[:,0]*S_inv[0][j] + P_pred[:,1]*S_inv[1][j]
        float K[4][2];
        for (int i = 0; i < 4; i++) {
            K[i][0] = P_pred[i][0] * S_inv[0][0] + P_pred[i][1] * S_inv[1][0];
            K[i][1] = P_pred[i][0] * S_inv[0][1] + P_pred[i][1] * S_inv[1][1];
        }

        // State update: x = x_pred + K * innov
        for (int i = 0; i < 4; i++) {
            _x[i] = x_pred[i] + K[i][0] * innov[0] + K[i][1] * innov[1];
        }

        // Covariance update: P = (I - K*H) * P_pred
        // (I - K*H) has K*H only affecting columns 0,1
        float IKH[4][4];
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float kh = 0;
                if (j < 2) kh = K[i][j]; // H is identity for first 2 cols
                IKH[i][j] = ((i == j) ? 1.0f : 0.0f) - kh;
            }
        }
        // P = IKH * P_pred
        float P_new[4][4];
        mat4Mul(IKH, P_pred, P_new);
        memcpy(_cov, P_new, sizeof(_cov));
    }

    // Getters
    float getX()  const { return _x[0]; }
    float getY()  const { return _x[1]; }
    float getVX() const { return _x[2]; }
    float getVY() const { return _x[3]; }
    float getSpeed() const { return sqrtf(_x[2]*_x[2] + _x[3]*_x[3]); }
    bool isInitialized() const { return _initialized; }

    // Set measurement noise (mm^2)
    void setMeasurementNoise(float noiseX, float noiseY) {
        _R[0] = noiseX;
        _R[1] = noiseY;
    }

    // Set process noise (acceleration variance, mm^2/s^4)
    void setProcessNoise(float q) {
        _qAcc = q;
    }

private:
    float _x[4];       // State: [x, y, vx, vy]
    float _cov[4][4];    // Covariance matrix
    float _R[2] = {200.0f, 200.0f}; // Measurement noise (mm^2)
    float _qAcc = 5000.0f;          // Process noise: acceleration variance (mm^2/s^4)
    unsigned long _lastUpdateMs;
    bool _initialized;

    void predictCovariance(float dt, float (&out)[4][4]) {
        float dt2 = dt * dt;
        float dt3 = dt2 * dt / 2.0f;
        float dt4 = dt2 * dt2 / 4.0f;

        // F*P*F' calculation (expanded, avoiding temp matrices)
        // F = [[1,0,dt,0],[0,1,0,dt],[0,0,1,0],[0,0,0,1]]
        float FP[4][4];
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                FP[i][j] = this->_cov[i][j];
                if (i < 2) FP[i][j] += dt * this->_cov[i+2][j];
            }
        }
        // (F*P)*F'
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                out[i][j] = FP[i][j];
                if (j < 2) out[i][j] += dt * FP[i][j+2];
            }
        }

        // Add process noise Q
        // Q models random acceleration: q * [[dt4/4, 0, dt3/2, 0], ...]
        out[0][0] += _qAcc * dt4;
        out[1][1] += _qAcc * dt4;
        out[2][2] += _qAcc * dt2;
        out[3][3] += _qAcc * dt2;
        out[0][2] += _qAcc * dt3;  out[2][0] += _qAcc * dt3;
        out[1][3] += _qAcc * dt3;  out[3][1] += _qAcc * dt3;
    }

    static void mat4Mul(const float (&A)[4][4], const float (&B)[4][4], float (&C)[4][4]) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                C[i][j] = 0;
                for (int k = 0; k < 4; k++) {
                    C[i][j] += A[i][k] * B[k][j];
                }
            }
        }
    }
};
