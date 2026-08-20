#pragma once

#include <array>
#include <cmath>

// Simple Kalman Filter for 3D position/velocity tracking
// State: [x, y, z, vx, vy, vz] (6D)
// Measurement: [x, y, z] (3D position)
template <int StateDim = 6, int MeasDim = 3>
class KalmanFilter {
public:
    using StateVec = std::array<float, StateDim>;
    using MeasVec = std::array<float, MeasDim>;
    using Matrix = std::array<std::array<float, StateDim>, StateDim>;
    using MeasMatrix = std::array<std::array<float, MeasDim>, StateDim>;
    using CovMatrix = Matrix;
    
    KalmanFilter() {
        // Initialize state to zero
        x_.fill(0.0f);
        
        // Initial covariance - high uncertainty
        P_.fill({});
        for (int i = 0; i < StateDim; ++i) {
            P_[i][i] = 1000.0f; // Large initial uncertainty
        }
        
        // Process noise covariance (Q) - tune based on target maneuverability
        Q_.fill({});
        // Position noise (small)
        Q_[0][0] = Q_[1][1] = Q_[2][2] = 0.1f;
        // Velocity noise (larger - targets can accelerate)
        Q_[3][3] = Q_[4][4] = Q_[5][5] = 10.0f;
        
        // Measurement noise covariance (R) - sensor noise
        R_.fill({});
        for (int i = 0; i < MeasDim; ++i) {
            R_[i][i] = 1.0f; // Screen position noise ~1 pixel
        }
        
        // State transition matrix (F) - constant velocity model
        F_.fill({});
        for (int i = 0; i < StateDim; ++i) {
            F_[i][i] = 1.0f;
        }
        // Position += velocity * dt (will be updated in predict with actual dt)
        
        // Measurement matrix (H) - we measure position only
        H_.fill({});
        H_[0][0] = 1.0f; // x
        H_[1][1] = 1.0f; // y
        H_[2][2] = 1.0f; // z
        
        initialized_ = false;
    }
    
    // Initialize with first measurement
    void init(const MeasVec& measurement, float dt = 0.016f) {
        for (int i = 0; i < MeasDim; ++i) {
            x_[i] = measurement[i];
            x_[i + 3] = 0.0f; // Initial velocity unknown
        }
        initialized_ = true;
        last_dt_ = dt;
    }
    
    // Predict step with time delta
    void predict(float dt) {
        if (!initialized_) return;
        
        last_dt_ = dt;
        
        // Update F matrix with current dt
        F_.fill({});
        for (int i = 0; i < StateDim; ++i) {
            F_[i][i] = 1.0f;
        }
        F_[0][3] = dt; // x += vx * dt
        F_[1][4] = dt; // y += vy * dt
        F_[2][5] = dt; // z += vz * dt
        
        // Update Q with dt (process noise scales with dt)
        Q_.fill({});
        float q_pos = 0.1f * dt;
        float q_vel = 10.0f * dt;
        Q_[0][0] = Q_[1][1] = Q_[2][2] = q_pos;
        Q_[3][3] = Q_[4][4] = Q_[5][5] = q_vel;
        
        // x = F * x
        StateVec x_new{};
        for (int i = 0; i < StateDim; ++i) {
            float sum = 0.0f;
            for (int j = 0; j < StateDim; ++j) {
                sum += F_[i][j] * x_[j];
            }
            x_new[i] = sum;
        }
        x_ = x_new;
        
        // P = F * P * F^T + Q
        CovMatrix P_new{};
        // F * P
        CovMatrix FP{};
        for (int i = 0; i < StateDim; ++i) {
            for (int j = 0; j < StateDim; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < StateDim; ++k) {
                    sum += F_[i][k] * P_[k][j];
                }
                FP[i][j] = sum;
            }
        }
        // FP * F^T
        for (int i = 0; i < StateDim; ++i) {
            for (int j = 0; j < StateDim; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < StateDim; ++k) {
                    sum += FP[i][k] * F_[j][k]; // F^T[k][j] = F[j][k]
                }
                P_new[i][j] = sum;
            }
        }
        // Add Q
        for (int i = 0; i < StateDim; ++i) {
            P_new[i][i] += Q_[i][i];
        }
        P_ = P_new;
    }
    
    // Update step with measurement
    void update(const MeasVec& measurement) {
        if (!initialized_) {
            init(measurement);
            return;
        }
        
        // Innovation: y = z - H * x
        MeasVec y{};
        for (int i = 0; i < MeasDim; ++i) {
            float hx = 0.0f;
            for (int j = 0; j < StateDim; ++j) {
                hx += H_[i][j] * x_[j];
            }
            y[i] = measurement[i] - hx;
        }
        
        // Innovation covariance: S = H * P * H^T + R
        std::array<std::array<float, MeasDim>, MeasDim> S{};
        // H * P
        std::array<std::array<float, StateDim>, MeasDim> HP{};
        for (int i = 0; i < MeasDim; ++i) {
            for (int j = 0; j < StateDim; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < StateDim; ++k) {
                    sum += H_[i][k] * P_[k][j];
                }
                HP[i][j] = sum;
            }
        }
        // HP * H^T
        for (int i = 0; i < MeasDim; ++i) {
            for (int j = 0; j < MeasDim; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < StateDim; ++k) {
                    sum += HP[i][k] * H_[j][k];
                }
                S[i][j] = sum + R_[i][j];
            }
        }
        
        // Kalman gain: K = P * H^T * S^-1
        // For simplicity, since H is [I | 0], K is just first MeasDim columns of P * S^-1
        // We'll compute S inverse (3x3 matrix inversion)
        std::array<std::array<float, MeasDim>, MeasDim> S_inv = invert3x3(S);
        
        // K = P * H^T * S^-1
        std::array<std::array<float, MeasDim>, StateDim> K{};
        for (int i = 0; i < StateDim; ++i) {
            for (int j = 0; j < MeasDim; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < MeasDim; ++k) {
                    // P * H^T: P[i][k] since H^T[k][j] = H[j][k] = 1 if j==k<3 else 0
                    sum += P_[i][k] * S_inv[k][j];
                }
                K[i][j] = sum;
            }
        }
        
        // x = x + K * y
        for (int i = 0; i < StateDim; ++i) {
            float sum = 0.0f;
            for (int j = 0; j < MeasDim; ++j) {
                sum += K[i][j] * y[j];
            }
            x_[i] += sum;
        }
        
        // P = (I - K * H) * P
        CovMatrix P_new{};
        // I - K * H
        CovMatrix I_KH{};
        for (int i = 0; i < StateDim; ++i) {
            for (int j = 0; j < StateDim; ++j) {
                float val = (i == j) ? 1.0f : 0.0f;
                for (int k = 0; k < MeasDim; ++k) {
                    val -= K[i][k] * H_[k][j];
                }
                I_KH[i][j] = val;
            }
        }
        // (I - K*H) * P
        for (int i = 0; i < StateDim; ++i) {
            for (int j = 0; j < StateDim; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < StateDim; ++k) {
                    sum += I_KH[i][k] * P_[k][j];
                }
                P_new[i][j] = sum;
            }
        }
        P_ = P_new;
    }
    
    // Get predicted position (after predict, before update)
    MeasVec get_predicted_position() const {
        MeasVec pos{};
        pos[0] = x_[0];
        pos[1] = x_[1];
        pos[2] = x_[2];
        return pos;
    }
    
    // Get current state estimate
    StateVec get_state() const { return x_; }
    
    // Get velocity estimate
    std::array<float, 3> get_velocity() const {
        return {x_[3], x_[4], x_[5]};
    }
    
    bool is_initialized() const { return initialized_; }
    
    // Reset filter
    void reset() {
        initialized_ = false;
        x_.fill(0.0f);
        for (int i = 0; i < StateDim; ++i) {
            P_[i][i] = 1000.0f;
        }
    }
    
    // Tune noise parameters
    void set_process_noise(float pos_noise, float vel_noise) {
        Q_[0][0] = Q_[1][1] = Q_[2][2] = pos_noise;
        Q_[3][3] = Q_[4][4] = Q_[5][5] = vel_noise;
    }
    
    void set_measurement_noise(float noise) {
        for (int i = 0; i < MeasDim; ++i) {
            R_[i][i] = noise;
        }
    }

private:
    // 3x3 matrix inversion using Cramer's rule
    static std::array<std::array<float, MeasDim>, MeasDim> invert3x3(
        const std::array<std::array<float, MeasDim>, MeasDim>& m) {
        std::array<std::array<float, MeasDim>, MeasDim> inv{};
        
        // Calculate determinant
        float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                  - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                  + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        
        if (std::abs(det) < 1e-6f) {
            // Singular - return identity
            for (int i = 0; i < MeasDim; ++i) {
                for (int j = 0; j < MeasDim; ++j) {
                    inv[i][j] = (i == j) ? 1.0f : 0.0f;
                }
            }
            return inv;
        }
        
        float inv_det = 1.0f / det;
        
        inv[0][0] =  (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * inv_det;
        inv[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * inv_det;
        inv[0][2] =  (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inv_det;
        
        inv[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * inv_det;
        inv[1][1] =  (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inv_det;
        inv[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * inv_det;
        
        inv[2][0] =  (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * inv_det;
        inv[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * inv_det;
        inv[2][2] =  (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * inv_det;
        
        return inv;
    }
    
    StateVec x_;           // State vector
    CovMatrix P_;          // Covariance matrix
    Matrix F_;             // State transition matrix
    CovMatrix Q_;          // Process noise covariance
    std::array<std::array<float, MeasDim>, MeasDim> R_; // Measurement noise covariance
    MeasMatrix H_;         // Measurement matrix
    bool initialized_;
    float last_dt_;
};

// Convenience typedef for 3D position/velocity tracking
using Kalman3D = KalmanFilter<6, 3>;