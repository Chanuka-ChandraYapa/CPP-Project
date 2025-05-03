#include "../include/Particle.h"
#include <cmath>
#include <algorithm>

Particle::Particle(double x, double y, double energy, double radius, double max_energy,
                 double mass, double charge, double restitution, ParticleType type)
    : x(x), y(y), vx(0.0), vy(0.0), energy(energy), MAX_ENERGY(max_energy), PARTICLE_RADIUS(radius),
      mass(mass), charge(charge), restitution(restitution), type(type) {
    // Optionally validate input parameters
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
    std::lock_guard<std::mutex> lock(particleMutex);
    // Bug: No validation of input parameters
    x = newX;
    y = newY;
}

double Particle::getVX() const {
    std::lock_guard<std::mutex> lock(particleMutex);
    return vx;
}

double Particle::getVY() const {
    std::lock_guard<std::mutex> lock(particleMutex);
    return vy;
}

void Particle::setVelocity(double newVX, double newVY) {
    std::lock_guard<std::mutex> lock(particleMutex);
    // Bug: No validation of input parameters
    vx = newVX;
    vy = newVY;
}

double Particle::getEnergy() const {
    return energy;
}

double Particle::getMass() const {
    return mass;
}

double Particle::getCharge() const {
    return charge;
}

double Particle::getRestitution() const {
    return restitution;
}

ParticleType Particle::getType() const {
    return type;
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
    std::lock(particleMutex, other.particleMutex);
    std::lock_guard<std::mutex> lock1(particleMutex, std::adopt_lock);
    std::lock_guard<std::mutex> lock2(other.particleMutex, std::adopt_lock);

    // Compute normal vector
    double dx = other.x - x;
    double dy = other.y - y;
    double dist = std::sqrt(dx * dx + dy * dy);
    if (dist == 0.0) return; // Prevent division by zero
    double nx = dx / dist;
    double ny = dy / dist;

    // Relative velocity
    double dvx = other.vx - vx;
    double dvy = other.vy - vy;
    double relVel = dvx * nx + dvy * ny;
    if (relVel > 0) return; // Already separating

    // Combined restitution (elasticity)
    double e = std::min(restitution, other.restitution);

    // 1D collision impulse (along normal)
    double m1 = mass;
    double m2 = other.mass;
    double impulse = -(1 + e) * relVel / (1/m1 + 1/m2);

    // Update velocities
    vx -= (impulse / m1) * nx;
    vy -= (impulse / m1) * ny;
    other.vx += (impulse / m2) * nx;
    other.vy += (impulse / m2) * ny;

    // Energy transfer (for inelastic, some energy lost)
    double keBefore = 0.5 * m1 * (vx*vx + vy*vy) + 0.5 * m2 * (other.vx*other.vx + other.vy*other.vy);
    // Optionally, for inelastic, reduce total energy
    if (e < 1.0) {
        double lostFraction = 1.0 - e;
        double lostEnergy = lostFraction * keBefore;
        double newTotal = keBefore - lostEnergy;
        // Scale velocities to match new total kinetic energy
        double scale = std::sqrt(newTotal / keBefore);
        vx *= scale;
        vy *= scale;
        other.vx *= scale;
        other.vy *= scale;
    }
    // Optionally, update internal energy if needed (not just velocity)
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