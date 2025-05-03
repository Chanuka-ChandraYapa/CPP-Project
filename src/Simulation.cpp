#include "../include/Simulation.h"
#include "../include/Config.h"
#include <algorithm>
#include <random>
#include <thread>
#include <iostream> // For debugging

Simulation::Simulation(const Config& config)
    : fieldSize(config.field_size),
      timeStep(config.time_step),
      containmentField(std::make_unique<ContainmentField>(config)) {
    initializeParticles(config);
}

Simulation::~Simulation() {
    stop();
}

void Simulation::initializeParticles(const Config& config) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-fieldSize/2, fieldSize/2);
    std::uniform_real_distribution<> vel_dis(-1.0, 1.0); // Velocity range
    
    particles.clear(); // Ensure vector is empty before initializing
    particles.reserve(config.num_particles); // Reserve space for efficiency
    
    std::uniform_real_distribution<> energy_dis(0.5 * config.initial_energy, 1.5 * config.initial_energy);
    std::uniform_int_distribution<> type_dis(0, 2); // 0:Electron, 1:Proton, 2:Neutron
    for (size_t i = 0; i < config.num_particles; ++i) {
        double energy = energy_dis(gen);
        ParticleType ptype = static_cast<ParticleType>(type_dis(gen));
        double mass = 1.0, charge = 0.0, restitution = 1.0;
        switch (ptype) {
            case ParticleType::Electron:
                mass = 0.00055; charge = -1.0; restitution = 0.95; break;
            case ParticleType::Proton:
                mass = 1.0; charge = 1.0; restitution = 0.9; break;
            case ParticleType::Neutron:
                mass = 1.0; charge = 0.0; restitution = 0.8; break;
            default:
                mass = 1.0; charge = 0.0; restitution = 1.0; break;
        }
        auto particle = std::make_unique<Particle>(
            dis(gen), dis(gen),
            energy,
            config.particle_radius,
            config.max_energy,
            mass, charge, restitution, ptype
        );
        particle->setVelocity(vel_dis(gen), vel_dis(gen));
        particles.push_back(std::move(particle));
    }
    std::cout << "Initialized " << particles.size() << " particles." << std::endl;
}

void Simulation::setContainmentField(std::unique_ptr<ContainmentField> field) {
    containmentField = std::move(field);
}

void Simulation::start() {

    // Single-threaded: no worker threads launched
    std::cout << "Simulation started (single-threaded mode)." << std::endl;
}

void Simulation::stop() {

    std::cout << "Simulation stopped." << std::endl;
}

void Simulation::step() {
    // Bug: Not thread-safe
    updatePositions(timeStep);
    handleCollisions();
    applyForces(timeStep);
    applyRadiationLoss();
    removeEscapedParticles();
}

void Simulation::applyRadiationLoss() {
    constexpr double ENERGY_LOSS_FRACTION = 0.01; // 1% per step
    for (auto& particle : particles) {
        double currentEnergy = particle->getEnergy();
        double loss = currentEnergy * ENERGY_LOSS_FRACTION;
        particle->addEnergy(-loss);
    }
}

void Simulation::addParticle(std::unique_ptr<Particle> particle) {

    particles.push_back(std::move(particle));
}

void Simulation::removeEscapedParticles() {

    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [this](const auto& p) {
                return !containmentField->isParticleContained(*p);
            }
        ),
        particles.end()
    );
}

size_t Simulation::getParticleCount() const {
    return particles.size();  // Bug: Not thread-safe
}

const std::vector<std::unique_ptr<Particle>>& Simulation::getParticles() const {
    return particles;
}

double Simulation::getTotalEnergy() const {
    // Bug: Race condition
    double total = 0.0;
    for (const auto& particle : particles) {
        total += particle->getEnergy();
    }
    return total;
}





void Simulation::updatePositions(double dt) {
    // Bug: Race condition
    for (auto& particle : particles) {
        double x = particle->getX() + particle->getVX() * dt;
        double y = particle->getY() + particle->getVY() * dt;
        particle->setPosition(x, y);
    }
}

void Simulation::handleCollisions() {
    // Bug: Potential deadlock

    
    for (size_t i = 0; i < particles.size(); ++i) {
        for (size_t j = i + 1; j < particles.size(); ++j) {
            if (particles[i]->isColliding(*particles[j])) {
                particles[i]->collide(*particles[j]);
            }
        }
    }
}

void Simulation::applyForces(double dt) {
    for (auto& particle : particles) {
        double force = containmentField->getContainmentForce(*particle);
        double px = particle->getX();
        double py = particle->getY();
        double dist = std::sqrt(px*px + py*py);
        double scale = (dist > 1e-6) ? -force / dist : 0.0; // Force towards origin

        double ax = scale * px;
        double ay = scale * py;

        double vx = particle->getVX() + ax * dt;
        double vy = particle->getVY() + ay * dt;

        particle->setVelocity(vx, vy);
    }
} 