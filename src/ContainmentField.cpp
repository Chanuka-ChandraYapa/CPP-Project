#include "../include/ContainmentField.h"
#include "../include/Particle.h"
#include "../include/Config.h"
#include <cmath>
#include <algorithm>

ContainmentField::ContainmentField(const Config& config)
    : size(config.field_size), fieldStrength(config.initial_strength), decayRate(config.initial_decay_rate), GRID_SIZE(config.field_grid_size), fieldEnergy(0.0), forceStrength(config.force_strength) {
    initializeField();
}

ContainmentField::~ContainmentField() {
    
}

void ContainmentField::initializeField() {
    fieldData.resize(GRID_SIZE * GRID_SIZE, 0.0);  // Now 2D grid
}

double ContainmentField::getContainmentForce(const Particle& particle) const {
    double x = particle.getX();
    double y = particle.getY();
    double halfSize = size / 2.0;

    if (std::abs(x) >= halfSize || std::abs(y) >= halfSize) {
        return 0.0; // No force outside or exactly on the boundary edge
    }

    // Calculate the shortest distance from the particle to any of the four edges
    double minDistToEdge = std::min(halfSize - std::abs(x), halfSize - std::abs(y));

    // Ensure minDistToEdge is not negative due to floating point issues if x/y are extremely close to halfSize
    minDistToEdge = minDistToEdge;

    // Calculate the force magnitude
    double forceMagnitude = forceStrength * fieldStrength * (minDistToEdge / halfSize);

    // Clamp the force to be non-negative just in case
    return forceMagnitude;
}

bool ContainmentField::isParticleContained(const Particle& particle) const {
    double x = particle.getX();
    double y = particle.getY();
    
    return std::abs(x) < size/2 && std::abs(y) < size/2;
}

void ContainmentField::update(double dt) {
    std::lock_guard<std::mutex> lock(fieldMutex);
    for (size_t i = 0; i < fieldData.size(); ++i) {
        fieldData[i] *= (1.0 - decayRate * dt);
    }
}

void ContainmentField::setFieldStrength(double strength) {
    fieldStrength = strength;
}

double ContainmentField::getFieldStrength() const {
    return fieldStrength;
}

void ContainmentField::setDecayRate(double rate) {
    std::lock_guard<std::mutex> lock(fieldMutex);
    decayRate = rate;
}

double ContainmentField::getDecayRate() const {
    std::lock_guard<std::mutex> lock(fieldMutex);
    return decayRate;
}

double ContainmentField::getSize() const {
    return size;
}

double ContainmentField::getFieldEnergy() const {
    std::lock_guard<std::mutex> lock(fieldMutex);
    return fieldEnergy;
} 