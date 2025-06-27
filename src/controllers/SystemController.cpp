#include "SystemController.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <limits>
#include <array>
#include <stdexcept>

SystemController::SystemController() {}

SystemController::~SystemController() {}

Json::Value SystemController::getSystemResources() {
    Json::Value root;

    // Get CPU usage
    std::ifstream cpuFile("/proc/stat");
    std::string line;
    if (!cpuFile.is_open()) {
        std::cerr << "Failed to open /proc/stat" << std::endl;
        return root;
    }
    if (std::getline(cpuFile, line)) {
        std::istringstream iss(line);
        std::string cpu;
        long user, nice, system, idle;
        if (iss >> cpu >> user >> nice >> system >> idle) {
            root["cpu_usage"] = 100.0 * (user + nice + system) / (user + nice + system + idle);
        }
    }
    cpuFile.close();

    // Get CPU count for total_cpu
    std::ifstream cpuinfo("/proc/cpuinfo");
    int cpu_count = 0;
    if (cpuinfo.is_open()) {
        while (std::getline(cpuinfo, line)) {
            if (line.find("processor") == 0) cpu_count++;
        }
        cpuinfo.close();
    }
    root["total_cpu"] = cpu_count > 0 ? cpu_count : 4.0; // Fallback
    root["available_cpu"] = root["total_cpu"].asDouble() * (1.0 - root["cpu_usage"].asDouble() / 100.0);

    // Get Memory usage
    std::ifstream memFile("/proc/meminfo");
    long totalMem = 0, freeMem = 0, availableMem = 0, buffers = 0, cached = 0;
    if (memFile.is_open()) {
        std::string key;
        long value;
        while (memFile >> key >> value) {
            if (key == "MemTotal:") totalMem = value / 1024; // Convert to MB
            else if (key == "MemFree:") freeMem = value / 1024;
            else if (key == "MemAvailable:") availableMem = value / 1024;
            else if (key == "Buffers:") buffers = value / 1024;
            else if (key == "Cached:") cached = value / 1024;
            memFile.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        if (totalMem > 0) {
            root["total_memory"] = totalMem;
            if (availableMem > 0) {
                root["memory_usage"] = 100.0 * (totalMem - availableMem) / totalMem;
                root["available_memory"] = availableMem;
            } else {
                long approxAvailable = freeMem + buffers + cached;
                if (approxAvailable > totalMem) approxAvailable = totalMem;
                root["memory_usage"] = 100.0 * (totalMem - approxAvailable) / totalMem;
                root["available_memory"] = approxAvailable;
            }
        }
    } else {
        std::cerr << "Failed to open /proc/meminfo" << std::endl;
    }
    memFile.close();

    // Get Disk usage
    std::string diskUsage = executeShellCommand("df -h --output=pcent / | tail -1 | tr -d ' %'");
    try {
        if (!diskUsage.empty()) {
            root["disk_usage"] = std::stoi(diskUsage);
        } else {
            std::cerr << "Disk usage command returned empty result." << std::endl;
        }
    } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument in disk usage: " << e.what() << std::endl;
    } catch (const std::out_of_range& e) {
        std::cerr << "Disk usage value out of range: " << e.what() << std::endl;
    }

    return root;
}

std::string SystemController::executeShellCommand(const std::string &command) {
    std::array<char, 128> buffer;
    std::string result;
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to run command: " + command);
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}