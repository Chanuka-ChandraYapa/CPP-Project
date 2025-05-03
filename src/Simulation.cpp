#include "../include/Simulation.h"
#include "../include/Config.h"
#include <algorithm>
#include <random>
#include <thread>
#include <iostream> // For debugging
#include <unordered_map> // For grid-based spatial partitioning

Simulation::Simulation(const Config& config)
    : fieldSize(config.field_size),
      timeStep(config.time_step),
      cellSize(config.cell_size),
      containmentField(std::make_unique<ContainmentField>(config)),
      threadManager(std::make_unique<ThreadManager>(config.initial_threads)),
      numThreads(config.initial_threads) {
    initializeParticles(config);
}

Simulation::~Simulation() {
    stop();
}

void Simulation::initializeParticles(const Config& config) {
    // Create a fixed seed for reproducible results in tests
    unsigned int seed = config.random_seed > 0 ? config.random_seed : std::random_device{}();
    
    particles.clear(); // Ensure vector is empty before initializing
    particles.resize(config.num_particles); // Resize vector to allow parallel initialization
    
    // Divide particles among threads for initialization
    const size_t particlesPerThread = config.num_particles / numThreads;
    const size_t remainingParticles = config.num_particles % numThreads;

    auto initializeParticleRange = [this, &config, seed](size_t start, size_t end, size_t threadId) {
        // One random generator per thread
        std::mt19937 gen(seed + threadId);
        std::uniform_real_distribution<> dis(-fieldSize/2, fieldSize/2);
        std::uniform_real_distribution<> vel_dis(-1.0, 1.0);
        
        for (size_t i = start; i < end; ++i) {
            particles[i] = std::make_unique<Particle>(
                dis(gen), dis(gen),
                config.initial_energy,
                config.particle_radius,
                config.max_energy
            );
            particles[i]->setVelocity(vel_dis(gen), vel_dis(gen));
        }
    };

    // Add tasks to initialize particles in parallel
    for (size_t i = 0; i < numThreads; ++i) {
        size_t start = i * particlesPerThread;
        size_t end = (i == numThreads - 1) ? 
            (start + particlesPerThread + remainingParticles) : 
            (start + particlesPerThread);
        
        threadManager->addTask([=]() {
            initializeParticleRange(start, end, i);
        });
    }

    // Wait for all initialization tasks to complete
    threadManager->waitForCompletion();
    std::cout << "Initialized " << particles.size() << " particles." << std::endl;
}

void Simulation::setContainmentField(std::unique_ptr<ContainmentField> field) {
    containmentField = std::move(field);
}

void Simulation::start() {
    if (running) return; // Prevent starting twice
    running = true;
    workerThreads.clear(); // Clear any old threads if stop wasn't called properly
    for (size_t i = 0; i < numThreads; ++i) {
        workerThreads.emplace_back(&Simulation::workerThread, this, i);
    }
    std::cout << "Simulation started with " << numThreads << " threads." << std::endl;
}

void Simulation::stop() {
    running = false; // Signal threads to stop
    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads.clear();
    std::cout << "Simulation stopped." << std::endl;
}

void Simulation::step() {
    updatePositions(timeStep);
    handleCollisions();
    applyForces(timeStep);
    removeEscapedParticles();
}

void Simulation::addParticle(std::unique_ptr<Particle> particle) {
    std::lock_guard<std::mutex> lock(particleMutex);
    particles.push_back(std::move(particle));
}

void Simulation::removeEscapedParticles() {
    std::lock_guard<std::mutex> lock(particleMutex);
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
    std::lock_guard<std::mutex> lock(particleMutex);
    return particles.size();
}

const std::vector<std::unique_ptr<Particle>>& Simulation::getParticles() const {
    return particles;
}

double Simulation::getTotalEnergy() const {
    const size_t totalParticles = particles.size();
    const size_t particlesPerThread = totalParticles / numThreads;
    const size_t remainingParticles = totalParticles % numThreads;
    
    // Vector to store partial sums from each thread
    std::vector<double> partialSums(numThreads, 0.0);
    
    auto calculatePartialSum = [this, &partialSums](size_t start, size_t end, size_t threadId) {
        double threadSum = 0.0;
        for (size_t i = start; i < end; ++i) {
            threadSum += particles[i]->getEnergy();
        }
        partialSums[threadId] = threadSum;
    };
    
    // Divide work among threads
    for (size_t i = 0; i < numThreads; ++i) {
        size_t start = i * particlesPerThread;
        size_t end = (i == numThreads - 1) ? 
            (start + particlesPerThread + remainingParticles) : 
            (start + particlesPerThread);
            
        threadManager->addTask([=]() {
            calculatePartialSum(start, end, i);
        });
    }
    
    threadManager->waitForCompletion();
    
    // Sum up all partial sums
    double total = 0.0;
    for (double sum : partialSums) {
        total += sum;
    }
    return total;
}

void Simulation::setNumThreads(size_t newNumThreads) {
    numThreads = newNumThreads;
    threadManager->setNumThreads(newNumThreads);
}

size_t Simulation::getNumThreads() const {
    return numThreads;
}

void Simulation::updatePositions(double dt) {
    const size_t totalParticles = particles.size();
    const size_t particlesPerThread = totalParticles / numThreads;
    const size_t remainingParticles = totalParticles % numThreads;

    auto updateParticleRange = [this, dt](size_t start, size_t end) {
        for (size_t i = start; i < end; ++i) {
            auto& particle = particles[i];
            double x = particle->getX() + particle->getVX() * dt;
            double y = particle->getY() + particle->getVY() * dt;
            particle->setPosition(x, y);
        }
    };

    // Divide work among threads
    for (size_t i = 0; i < numThreads; ++i) {
        size_t start = i * particlesPerThread;
        size_t end = (i == numThreads - 1) ? 
            (start + particlesPerThread + remainingParticles) : 
            (start + particlesPerThread);
        
        threadManager->addTask([=]() {
            updateParticleRange(start, end);
        });
    }

    // Wait for all position updates to complete
    threadManager->waitForCompletion();
}

void Simulation::handleCollisions() {
    // Use grid-based spatial partitioning for efficient collision detection
    std::lock_guard<std::mutex> lock1(simulationMutex);
    std::lock_guard<std::mutex> lock2(particleMutex);
    
    if (particles.empty()) {
        return; // No particles, no collisions to handle
    }
    
    // Create a mutex per particle to allow fine-grained locking
    std::vector<std::mutex> particleLocks(particles.size());
    
    // Determine cell size based on particle collision distance
    // For collision detection, particles use PARTICLE_RADIUS * 2.0
    // double particleRadius = 0.0;
    // if (!particles.empty()) {
    //     // Test collision distance between first particle and itself at different positions
    //     // This gives us the effective collision radius
    //     auto& p = particles[0];
    //     double originalX = p->getX();
    //     double originalY = p->getY();
        
    //     // Temporarily move particle to measure collision distance
    //     p->setPosition(originalX + 1.0, originalY);
    //     if (p->isColliding(*p)) {
    //         // If it collides with itself at distance 1.0, radius must be at least 0.5
    //         particleRadius = 0.5;
    //     } else {
    //         // Default to a reasonable value based on config (typically around 1.0)
    //         particleRadius = 1.0;
    //     }
        
    //     // Restore original position
    //     p->setPosition(originalX, originalY);
    // } else {
    //     // Default radius if no particles
    //     particleRadius = 1.0;
    // }
    
    // const double cellSize = 2.0; // Cell size = 2 * collision distance

    
    
    // Calculate grid dimensions based on field size and cell size
    const int gridDim = static_cast<int>(std::ceil(fieldSize / cellSize));
    
    // Lambda to get cell index from particle position
    auto getCellIndex = [gridDim, this](double x, double y) -> int {
        // Convert from world space to grid space (field is centered at origin)
        int gx = static_cast<int>((x + fieldSize/2) / cellSize);
        int gy = static_cast<int>((y + fieldSize/2) / cellSize);
        
        // Clamp to valid grid range
        gx = std::max(0, std::min(gx, gridDim - 1));
        gy = std::max(0, std::min(gy, gridDim - 1));
        
        return gy * gridDim + gx;
    };
    
    // Map from cell index to list of particle indices in that cell
    std::unordered_map<int, std::vector<size_t>> grid;
    
    // Assign particles to grid cells
    for (size_t i = 0; i < particles.size(); ++i) {
        const double x = particles[i]->getX();
        const double y = particles[i]->getY();
        const int idx = getCellIndex(x, y);
        grid[idx].push_back(i);
    }
    
    // Distribute grid cells among threads
    std::vector<int> cellIndices;
    for (const auto& cell : grid) {
        cellIndices.push_back(cell.first);
    }
    
    const size_t totalCells = cellIndices.size();
    const size_t cellsPerThread = totalCells > 0 ? totalCells / numThreads : 0;
    const size_t remainingCells = totalCells > 0 ? totalCells % numThreads : 0;
    
    // Process cells in parallel
    auto processCellRange = [this, &grid, &particleLocks, gridDim, &cellIndices](size_t startIdx, size_t endIdx) {
        for (size_t cellIdx = startIdx; cellIdx < endIdx; ++cellIdx) {
            int idx = cellIndices[cellIdx];
            int gx = idx % gridDim;
            int gy = idx / gridDim;
            
            // Check collisions with particles in the same cell
            const auto& currentCell = grid[idx];
            for (size_t i = 0; i < currentCell.size(); ++i) {
                size_t particleIdx1 = currentCell[i];
                
                // Check against other particles in the same cell
                for (size_t j = i + 1; j < currentCell.size(); ++j) {
                    size_t particleIdx2 = currentCell[j];
                    
                    if (particles[particleIdx1]->isColliding(*particles[particleIdx2])) {
                        // Lock both particles in a consistent order to prevent deadlocks
                        size_t first = std::min(particleIdx1, particleIdx2);
                        size_t second = std::max(particleIdx1, particleIdx2);
                        
                        // Acquire locks for both particles to ensure thread safety
                        std::lock(particleLocks[first], particleLocks[second]);
                        std::lock_guard<std::mutex> lockA(particleLocks[first], std::adopt_lock);
                        std::lock_guard<std::mutex> lockB(particleLocks[second], std::adopt_lock);
                        
                        // Now safely handle the collision
                        particles[first]->collide(*particles[second]);
                    }
                }
                
                // Check against particles in neighboring cells
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        // Skip the current cell (already checked above)
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = gx + dx;
                        int ny = gy + dy;
                        
                        // Skip invalid coordinates
                        if (nx < 0 || ny < 0 || nx >= gridDim || ny >= gridDim) continue;
                        
                        int neighborIdx = ny * gridDim + nx;
                        if (!grid.count(neighborIdx)) continue; // No particles in this cell
                        
                        const auto& neighborCell = grid[neighborIdx];
                        for (size_t j = 0; j < neighborCell.size(); ++j) {
                            size_t particleIdx2 = neighborCell[j];
                            
                            // Avoid duplicate checks by enforcing order
                            if (particleIdx1 < particleIdx2 && 
                                particles[particleIdx1]->isColliding(*particles[particleIdx2])) {
                                
                                // Lock both particles in a consistent order
                                size_t first = particleIdx1;  // Already know particleIdx1 < particleIdx2
                                size_t second = particleIdx2;
                                
                                // Acquire locks for both particles
                                std::lock(particleLocks[first], particleLocks[second]);
                                std::lock_guard<std::mutex> lockA(particleLocks[first], std::adopt_lock);
                                std::lock_guard<std::mutex> lockB(particleLocks[second], std::adopt_lock);
                                
                                // Handle collision
                                particles[first]->collide(*particles[second]);
                            }
                        }
                    }
                }
            }
        }
    };
    
    // Distribute work among threads
    for (size_t i = 0; i < numThreads && i < totalCells; ++i) {
        size_t startIdx = i * cellsPerThread;
        size_t endIdx = (i == numThreads - 1) ? 
            startIdx + cellsPerThread + remainingCells : 
            startIdx + cellsPerThread;
            
        threadManager->addTask([=]() {
            processCellRange(startIdx, endIdx);
        });
    }
    
    // Wait for all collision checks to complete
    threadManager->waitForCompletion();
}

void Simulation::applyForces(double dt) {
    // Each particle's force calculation is independent - perfect for parallelization
    const size_t totalParticles = particles.size();
    const size_t particlesPerThread = totalParticles / numThreads;
    const size_t remainingParticles = totalParticles % numThreads;

    auto applyForcesRange = [this, dt](size_t start, size_t end) {
        for (size_t i = start; i < end; ++i) {
            auto& particle = particles[i];
            double force = containmentField->getContainmentForce(*particle);
            double px = particle->getX();
            double py = particle->getY();
            double ax = (px>0) ? force : -force;
            double ay = (py>0) ? force : -force;
            // double dist = std::sqrt(px*px + py*py);
            // double scale = (dist > 1e-6) ? -force : 0.0; // Force towards origin

            // double ax = scale * px;
            // double ay = scale * py;
            
            double vx = particle->getVX() + ax * dt;
            double vy = particle->getVY() + ay * dt;
            
            particle->setVelocity(vx, vy);
        }
    };

    // Divide work among threads
    for (size_t i = 0; i < numThreads; ++i) {
        size_t start = i * particlesPerThread;
        size_t end = (i == numThreads - 1) ? 
            (start + particlesPerThread + remainingParticles) : 
            (start + particlesPerThread);
        
        threadManager->addTask([=]() {
            applyForcesRange(start, end);
        });
    }

    // Wait for all force calculations to complete
    threadManager->waitForCompletion();
}

void Simulation::workerThread(size_t threadId) {
    while (running) {
        // Bug: Improper thread synchronization
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
} 