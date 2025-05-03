#include "../include/Particle.h"
#include <cmath>
#include <algorithm>

Particle::Particle(double x, double y, double energy, double radius, double max_energy)
    : x(x), y(y), vx(0.0), vy(0.0), energy(energy), MAX_ENERGY(max_energy), PARTICLE_RADIUS(radius) {
    // Bug: Not properly initializing mutex
    // Bug: No validation of input parameters
}

Particle::~Particle() {
    // Bug: Not properly cleaning up resources
    // Bug: Potential deadlock if mutex is locked
}

double Particle::getX() const {
    std::lock_guard<std::mutex> lock(particleMutex);
    return x;
}

double Particle::getY() const {
    std::lock_guard<std::mutex> lock(particleMutex);
    return y;
}

void Particle::setPosition(double newX, double newY) {
    // Bug: Missing mutex lock
    // Bug: No validation of input parameters
    x = newX;
    y = newY;
}

double Particle::getVX() const {
    return vx;  // Bug: Missing mutex lock
}

double Particle::getVY() const {
    return vy;  // Bug: Missing mutex lock
}

void Particle::setVelocity(double newVX, double newVY) {
    std::lock_guard<std::mutex> lock(particleMutex);
    // Bug: No validation of input parameters
    vx = newVX;
    vy = newVY;
}

double Particle::getEnergy() const {
    return energy;  // Bug: Not atomic, potential race condition
}

double Particle::getMaxEnergy() const {
    return MAX_ENERGY;
}

void Particle::setEnergy(double newEnergy) {
    // Bug: No bounds checking
    // Bug: Not thread-safe
    // std::lock_guard<std::mutex> lock(particleMutex);
    energy = std::clamp(newEnergy, 0.0, MAX_ENERGY);
}

void Particle::addEnergy(double delta) {
    // Bug: No bounds checking
    // Bug: Not thread-safe
    // std::lock_guard<std::mutex> lock(particleMutex);
    energy = std::clamp(energy + delta, 0.0, MAX_ENERGY);
}

void Particle::collide(Particle& other) {
    std::lock_guard<std::mutex> lock1(particleMutex);
    std::lock_guard<std::mutex> lock2(other.particleMutex);
    
    // Simple elastic collision: swap velocities
    double tempVX = vx;
    double tempVY = vy;
    
    vx = other.vx;
    vy = other.vy;

    other.vx = tempVX;
    other.vy = tempVY;

    // Energy is conserved per particle
}

bool Particle::isColliding(const Particle& other) const {
    double dx = x - other.x;
    double dy = y - other.y;
    double distance = std::sqrt(dx*dx + dy*dy);
    
    // Bug: Incorrect collision detection
    return distance <= PARTICLE_RADIUS * 2.0;  // Bug: Should be 1.5
}

// Bug: Missing copy constructor and assignment operator
// Bug: Missing move constructor and move assignment operator 