#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Config {
    // Simulation
    size_t num_particles;
    double field_size;
    double cell_size;
    size_t initial_threads;
    double time_step;
    unsigned int random_seed;

    // Particle
    double initial_energy;
    double max_energy;
    double particle_radius;

    // Containment Field
    double initial_strength;
    double initial_decay_rate;
    size_t field_grid_size;
    double force_strength;

    // Rendering
    double target_fps;
    int grid_width;
    int grid_height;
    std::map<int, char> density_map;
    int max_density_level;

    // Load config from JSON file
    void load_from_file(const std::string& filename) {
        std::ifstream f(filename);
        if (!f) throw std::runtime_error("Could not open config file: " + filename);
        json j;
        f >> j;

        num_particles = j.at("num_particles");
        field_size = j.at("field_size");
        cell_size = j.at("cell_size");
        initial_threads = j.at("initial_threads");
        time_step = j.at("time_step");
        random_seed = j.at("random_seed");

        initial_energy = j.at("initial_energy");
        max_energy = j.at("max_energy");
        particle_radius = j.at("particle_radius");

        initial_strength = j.at("initial_strength");
        initial_decay_rate = j.at("initial_decay_rate");
        field_grid_size = j.at("field_grid_size");
        force_strength = j.at("force_strength");

        target_fps = j.at("target_fps");
        grid_width = j.at("grid_width");
        grid_height = j.at("grid_height");
        max_density_level = j.at("max_density_level");

        for (auto& [k, v] : j.at("density_map").items()) {
            density_map[std::stoi(k)] = v.get<std::string>()[0];
        }
    }
};
