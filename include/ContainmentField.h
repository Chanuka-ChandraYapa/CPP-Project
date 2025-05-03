#pragma once

#include <vector>
#include <memory>
#include <mutex>

struct Config;

class Particle;

class ContainmentField {
public:
    ContainmentField(const Config& config);
    ~ContainmentField();

    // Field properties
    double getSize() const;

    // Particle containment
    bool isParticleContained(const Particle& particle) const;  // Bug: Incorrect bounds checking
    double getContainmentForce(const Particle& particle) const;  // Bug: Incorrect force calculation

    // Field strength
    void setFieldStrength(double strength);
    double getFieldStrength() const;

    // Energy management
    double getFieldEnergy() const;
    void update(double dt);

    // Decay rate
    void setDecayRate(double rate);
    double getDecayRate() const;

private:
    // Field properties
    double size;
    double fieldStrength;
    double fieldEnergy;
    double decayRate;
    const size_t GRID_SIZE;
    std::vector<double> fieldData;
    double forceStrength;

    // Energy pulses
    struct EnergyPulse {
        double x, y;
        double strength;
        double lifetime;
    };
    std::vector<EnergyPulse*> energyPulses;  // Bug: Raw pointers, potential memory leak

    // Thread safety
    mutable std::mutex fieldMutex;  // Bug: Not properly used in all methods

    // Helper methods
    void initializeField();
}; 