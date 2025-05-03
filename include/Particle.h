#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

class Particle {
public:
    Particle(double x, double y, double energy, double radius, double max_energy);
    ~Particle();

    // Position getters and setters
    double getX() const;
    double getY() const;
    void setPosition(double x, double y);

    // Velocity getters and setters
    double getVX() const;
    double getVY() const;
    void setVelocity(double vx, double vy);

    // Energy management
    double getEnergy() const;
    void setEnergy(double energy);
    void addEnergy(double delta);  // Bug: No bounds checking
    double getMaxEnergy() const;

    // Collision handling
    void collide(Particle& other);  // Bug: Incorrect energy transfer
    bool isColliding(const Particle& other) const;

private:
    // Position and velocity
    double x, y;
    double vx, vy;
    
    // Energy level
    double energy;  // Bug: Not atomic, potential race condition
    const double MAX_ENERGY;
    
    // Constants
    const double PARTICLE_RADIUS;
    
    // Mutex for thread safety
    mutable std::mutex particleMutex;  // Bug: Not properly used in all methods
}; 