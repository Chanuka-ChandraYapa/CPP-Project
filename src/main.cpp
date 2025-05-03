// ---- File: main.cpp ----

#include "../include/Simulation.h"
#include "../include/Config.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <map>
#include <fstream>        // For file reading
#include <stdexcept>      // For exceptions
#include <nlohmann/json.hpp> // JSON library

// --- Configuration Loading Function ---
Config loadConfig(const std::string& filename) {
    std::ifstream configFile(filename);
    if (!configFile.is_open()) {
        throw std::runtime_error("Could not open configuration file: " + filename);
    }

    json j;
    try {
        configFile >> j;
    } catch (json::parse_error& e) {
        throw std::runtime_error("Failed to parse configuration file: " + std::string(e.what()));
    }

    Config cfg;

    // Helper lambda to get values or throw
    auto get_or_throw = [&](const json& obj, const std::string& key) {
        if (!obj.contains(key)) {
            throw std::runtime_error("Missing required configuration key: " + key);
        }
        return obj.at(key);
    };
    
    // Helper lambda for nested keys
    auto get_nested_or_throw = [&](const json& root, const std::string& group, const std::string& key) {
         if (!root.contains(group) || !root.at(group).contains(key)) {
             throw std::runtime_error("Missing required configuration key: " + group + "." + key);
         }
         return root.at(group).at(key);
     };


    // Load Simulation settings
    cfg.num_particles = get_nested_or_throw(j, "simulation", "num_particles");
    cfg.field_size = get_nested_or_throw(j, "simulation", "field_size");
    cfg.initial_threads = get_nested_or_throw(j, "simulation", "initial_threads");
    cfg.time_step = get_nested_or_throw(j, "simulation", "time_step");

    // Load Particle settings
    cfg.initial_energy = get_nested_or_throw(j, "particle", "initial_energy");
    cfg.max_energy = get_nested_or_throw(j, "particle", "max_energy");
    cfg.particle_radius = get_nested_or_throw(j, "particle", "radius");


    // Load Containment Field settings
    cfg.initial_strength = get_nested_or_throw(j, "containment_field", "initial_strength");
    cfg.initial_decay_rate = get_nested_or_throw(j, "containment_field", "initial_decay_rate");
    cfg.field_grid_size = get_nested_or_throw(j, "containment_field", "grid_size");
    cfg.force_strength = get_nested_or_throw(j, "containment_field", "force_strength");

    // Load Rendering settings
    cfg.target_fps = get_nested_or_throw(j, "rendering", "target_fps");
    cfg.grid_width = get_nested_or_throw(j, "rendering", "grid_width");
    cfg.grid_height = get_nested_or_throw(j, "rendering", "grid_height");
    cfg.max_density_level = get_nested_or_throw(j, "rendering", "max_density_level");

    // Load Density Map (keys are strings in JSON, need conversion)
    const auto& density_map_json = get_nested_or_throw(j, "rendering", "density_map");
    if (!density_map_json.is_object()) {
         throw std::runtime_error("Configuration error: rendering.density_map must be an object.");
    }
    for (auto& [key_str, val] : density_map_json.items()) {
        try {
            int key = std::stoi(key_str);
            if (!val.is_string() || val.get<std::string>().length() != 1) {
                 throw std::runtime_error("Configuration error: density_map values must be single characters (strings).");
            }
            cfg.density_map[key] = val.get<std::string>()[0];
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error("Configuration error: density_map keys must be integers (in string format): " + key_str);
        } catch (const std::out_of_range& e) {
             throw std::runtime_error("Configuration error: density_map key out of range: " + key_str);
        }
    }
    if (cfg.density_map.empty()) {
        std::cerr << "Warning: rendering.density_map is empty in config file." << std::endl;
    }

    return cfg;
}

// --- Update renderASCII to use Config ---
void renderASCII(const std::vector<std::unique_ptr<Particle>>& particles, double fieldSize, const Config& cfg) {
    // Initialize empty grid to store counts
    std::vector<std::vector<int>> gridCounts(cfg.grid_height, std::vector<int>(cfg.grid_width, 0));

    // Map particles to grid cells and count
    for (const auto& particle : particles) {
        double x = particle->getX();
        double y = particle->getY();

        // Convert to grid coordinates
        int col = static_cast<int>((x + fieldSize/2) * cfg.grid_width / fieldSize);
        int row = static_cast<int>((y + fieldSize/2) * cfg.grid_height / fieldSize);

        // Clamp values to grid bounds
        col = std::clamp(col, 0, cfg.grid_width - 1);
        row = std::clamp(row, 0, cfg.grid_height - 1);

        gridCounts[row][col]++;
    }

    // Clear screen and render
    std::cout << "\033[2J\033[H";  // ANSI escape codes for clear screen

    // Top border
    std::cout << '+' << std::string(cfg.grid_width, '-') << "+\n";

    // Render grid based on counts
    for (int i = 0; i < cfg.grid_height; ++i) {
        std::cout << '|'; // Left border
        for (int j = 0; j < cfg.grid_width; ++j) {
            int count = gridCounts[i][j];
            if (count == 0) {
                std::cout << ' ';
            } else {
                // Find the character for the density, capping at MAX_DENSITY_LEVEL
                int level = std::min(count, cfg.max_density_level);
                auto it = cfg.density_map.find(level);
                std::cout << (it != cfg.density_map.end() ? it->second : ' '); // Use found char or space
            }
        }
        std::cout << "|\n"; // Right border
    }

    // Bottom border
    std::cout << '+' << std::string(cfg.grid_width, '-') << "+\n";

    std::cout << std::flush;
}

int main() {
    try {
        // --- Load Configuration ---
        const std::string configFilename = "config.json";
        Config config = loadConfig(configFilename);
        std::cout << "Configuration loaded from " << configFilename << std::endl;

        // --- Pass Config to Simulation and Components ---
        Simulation simulation(config);

        simulation.start();

        // Main loop
        const double FRAME_TIME = 1.0 / config.target_fps;

        while (simulation.getParticleCount() > 0) {
            auto frameStart = std::chrono::high_resolution_clock::now();

            // Update simulation
            simulation.step();

            // Render particles using config
            renderASCII(simulation.getParticles(), config.field_size, config);

            // Frame rate control
            auto frameEnd = std::chrono::high_resolution_clock::now();
            auto frameDuration = std::chrono::duration<double>(frameEnd - frameStart).count();

            if (frameDuration < FRAME_TIME) {
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(FRAME_TIME - frameDuration)
                );
            }

            // Print stats
            static int frameCount = 0;
             if (++frameCount % 30 == 0) {
                 // Recalculate actual FPS for printing
                 auto now = std::chrono::high_resolution_clock::now();
                 static auto lastStatTime = now;
                 auto elapsed = std::chrono::duration<double>(now - lastStatTime).count();
                 double actualFps = (elapsed > 1e-6) ? (30.0 / elapsed) : 0.0; // 30 frames since last stat
                 lastStatTime = now;

                std::cout << "\nParticles: " << simulation.getParticleCount()
                          << " | Energy: " << simulation.getTotalEnergy()
                          << " | FPS: " << actualFps << std::endl;
            }
        }

        simulation.stop();
        std::cout << "Simulation ended. All particles escaped.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
} 