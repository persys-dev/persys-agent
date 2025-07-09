#include "DockerController.h"
#include <array>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <set> // Added for std::set
#include <ctime> // Added for time functions
#include <map>
#include <mutex>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

DockerController::DockerController() {}

DockerController::~DockerController() {}

std::string DockerController::executeDockerCommand(const std::string &command) {
    std::string fullCommand = "docker " + command + " 2>&1";
    std::array<char, 128> buffer;
    std::string result;
    FILE *pipe = popen(fullCommand.c_str(), "r");
    if (!pipe) throw std::runtime_error("Failed to run command");
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

std::string DockerController::startContainer(const std::string &image, 
                                             const std::string &name, 
                                             const std::vector<std::string> &ports, 
                                             const std::vector<std::string> &envVars, 
                                             const std::vector<std::string> &volumes, 
                                             const std::vector<std::string> &labels,
                                             const std::string &network, 
                                             const std::string &restartPolicy, 
                                             bool detach,
                                             const std::string &command) {
    std::ostringstream cmd;
    cmd << "run ";

    if (detach) cmd << "-d ";
    if (!name.empty()) cmd << "--name " << name << " ";
    if (!network.empty()) cmd << "--network " << network << " ";
    cmd << "--restart=" << restartPolicy << " ";

    for (const auto &port : ports) {
        cmd << "-p " << port << " ";
    }
    
    for (const auto &env : envVars) {
        cmd << "-e \"" << env << "\" ";
    }

    for (const auto &volume : volumes) {
        cmd << "-v " << volume << " ";
    }

    for (const auto &label : labels) {
        cmd << "--label " << label << " ";
    }

    cmd << image << " ";
    if (!command.empty()) {
        cmd << command;
    }
    printf("Docker Command: %s", cmd.str().c_str());
    return executeDockerCommand(cmd.str());
}

std::string DockerController::stopContainer(const std::string &containerId) {
    return executeDockerCommand("stop " + containerId);
}

// Add a struct to track state for each workload/container
struct WorkloadState {
    std::string status; // e.g., Pulling, ImagePullBackOff, ContainerCreating, Running, Failed
    std::string reason; // e.g., network error, auth error, etc.
    time_t lastUpdate;
};

static std::map<std::string, WorkloadState> workloadStates;
static std::mutex workloadStatesMutex;

std::map<std::string, time_t> pendingWorkloads; // workloadID -> launch time
std::mutex pendingWorkloadsMutex;

std::map<pid_t, std::string> runningDockerRuns; // pid -> workloadId
std::mutex runningDockerRunsMutex;

Json::Value DockerController::listContainers(bool all) {
    // Use --format to get structured output, one line per container
    std::string format = "--format '{{.ID}}\t{{.Names}}\t{{.Image}}\t{{.Status}}\t{{.Ports}}'";
    std::string cmd = "ps " + std::string(all ? "-a" : "") + " " + format;
    std::string rawOutput = executeDockerCommand(cmd);

    Json::Value containers(Json::arrayValue);
    std::set<std::string> knownNames;
    if (!rawOutput.empty()) {
        std::istringstream iss(rawOutput);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            std::istringstream lineStream(line);
            std::string id, names, image, status, ports;
            std::getline(lineStream, id, '\t');
            std::getline(lineStream, names, '\t');
            std::getline(lineStream, image, '\t');
            std::getline(lineStream, status, '\t');
            std::getline(lineStream, ports, '\t');
            Json::Value container;
            container["id"] = id;
            container["names"] = names;
            container["image"] = image;
            container["status"] = status;
            container["ports"] = ports;
            // Add reason if tracked
            {
                std::lock_guard<std::mutex> lock(workloadStatesMutex);
                if (workloadStates.count(names)) {
                    container["reason"] = workloadStates[names].reason;
                }
            }
            containers.append(container);
            knownNames.insert(names);
        }
    }

    // --- Pending workloads: check for running docker run processes ---
    {
        std::lock_guard<std::mutex> lock(pendingWorkloadsMutex);
        for (auto it = pendingWorkloads.begin(); it != pendingWorkloads.end(); ) {
            const std::string& workloadId = it->first;
            time_t launchTime = it->second;
            bool found = false;
            for (const auto& c : containers) {
                if (c["names"].asString() == workloadId) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Check for running docker run process
                std::string psOutput;
                std::array<char, 256> buffer;
                std::string grepCmd = "ps aux | grep 'docker run' | grep -- '" + workloadId + "' | grep -v grep";
                FILE* pipe = popen(grepCmd.c_str(), "r");
                if (pipe) {
                    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                        psOutput += buffer.data();
                    }
                    pclose(pipe);
                }
                double minutes = difftime(time(nullptr), launchTime) / 60.0;
                if (!psOutput.empty()) {
                    Json::Value synthetic;
                    synthetic["id"] = "";
                    synthetic["names"] = workloadId;
                    synthetic["image"] = "";
                    synthetic["status"] = "Pulling";
                    synthetic["reason"] = "docker run in progress";
                    synthetic["sinceMinutes"] = minutes;
                    synthetic["ports"] = "";
                    containers.append(synthetic);
                    std::cout << "[Agent] Workload " << workloadId << " is being pulled/created (" << minutes << " min)" << std::endl;
                    ++it;
                } else if (minutes > 35) { // 35 min grace period
                    std::cout << "[Agent] Workload " << workloadId << " pending for " << minutes << " min, removing from pendingWorkloads" << std::endl;
                    it = pendingWorkloads.erase(it);
                } else {
                    ++it;
                }
            } else {
                // Container is now visible, remove from pending
                it = pendingWorkloads.erase(it);
            }
        }
    }

    // After collecting containers from docker ps, enhance with docker inspect
    for (Json::Value& c : containers) {
        std::string name = c["names"].asString();
        if (name.empty()) continue;
        std::string inspectCmd = "inspect " + name + " --format '{{json .State}}'";
        std::string inspectOut = executeDockerCommand(inspectCmd);
        if (!inspectOut.empty()) {
            Json::Value state;
            Json::CharReaderBuilder builder;
            std::string errs;
            std::istringstream s(inspectOut);
            if (Json::parseFromStream(builder, s, &state, &errs)) {
                // Use .State fields for more accurate status
                if (state.isMember("Running") && state["Running"].asBool()) {
                    c["status"] = "Running";
                } else if (state.isMember("Paused") && state["Paused"].asBool()) {
                    c["status"] = "Paused";
                } else if (state.isMember("Restarting") && state["Restarting"].asBool()) {
                    c["status"] = "Restarting";
                } else if (state.isMember("Dead") && state["Dead"].asBool()) {
                    c["status"] = "Dead";
                } else if (state.isMember("Status")) {
                    std::string st = state["Status"].asString();
                    if (st == "created") {
                        c["status"] = "ContainerCreating";
                    } else if (st == "exited") {
                        c["status"] = "Exited";
                    } else if (st == "removing") {
                        c["status"] = "Removing";
                    } else if (st == "dead") {
                        c["status"] = "Dead";
                    } else if (st == "running") {
                        c["status"] = "Running";
                    }
                }
                if (state.isMember("Error") && !state["Error"].asString().empty()) {
                    c["status"] = "ImagePullBackOff";
                    c["reason"] = state["Error"].asString();
                }
            }
        }
    }

    // --- Track running docker run processes ---
    {
        std::lock_guard<std::mutex> lock(runningDockerRunsMutex);
        for (auto it = runningDockerRuns.begin(); it != runningDockerRuns.end(); ) {
            pid_t pid = it->first;
            const std::string& workloadId = it->second;
            // Check if process is still running
            if (kill(pid, 0) == 0) { // process exists
                // Only add if not already in containers
                bool found = false;
                for (const auto& c : containers) {
                    if (c["names"].asString() == workloadId) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    Json::Value synthetic;
                    synthetic["id"] = "";
                    synthetic["names"] = workloadId;
                    synthetic["image"] = "";
                    synthetic["status"] = "Pulling";
                    synthetic["reason"] = "docker run in progress (tracked by PID)";
                    synthetic["ports"] = "";
                    containers.append(synthetic);
                    std::cout << "[Agent] Workload " << workloadId << " is being pulled/created (PID " << pid << ")" << std::endl;
                }
                ++it;
            } else {
                // Process is gone, remove from map
                it = runningDockerRuns.erase(it);
            }
        }
    }

    // Fallback: For any docker run process in ps aux, parse --name and match to a workload
    std::string psOutput;
    {
        std::array<char, 256> buffer;
        FILE* pipe = popen("ps aux | grep 'docker run' | grep -v grep", "r");
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                psOutput += buffer.data();
            }
            pclose(pipe);
        }
    }
    std::istringstream psStream(psOutput);
    std::string psLine;
    while (std::getline(psStream, psLine)) {
        size_t namePos = psLine.find("--name");
        if (namePos != std::string::npos) {
            std::istringstream iss(psLine.substr(namePos));
            std::string dummy, containerName;
            iss >> dummy >> containerName;
            if (!containerName.empty()) {
                bool found = false;
                for (const auto& c : containers) {
                    if (c["names"].asString() == containerName) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    Json::Value synthetic;
                    synthetic["id"] = "";
                    synthetic["names"] = containerName;
                    synthetic["image"] = "";
                    synthetic["status"] = "Pulling";
                    synthetic["reason"] = "docker run in progress (ps aux fallback)";
                    synthetic["ports"] = "";
                    containers.append(synthetic);
                    std::cout << "[Agent] Container " << containerName << " is being pulled/created (ps aux fallback)" << std::endl;
                }
            }
        }
    }

    return containers;
}

std::string DockerController::removeContainer(const std::string &containerId) {
    return executeDockerCommand("rm " + containerId);
}

std::string DockerController::getContainerLogs(const std::string &containerId) {
    return executeDockerCommand("logs " + containerId);
}

Json::Value DockerController::listImages(bool all) {
    std::string format = "--format '{{.ID}}\t{{.Repository}}\t{{.Tag}}\t{{.Size}}'";
    std::string cmd = "images " + std::string(all ? "-a" : "") + " " + format;
    std::string rawOutput = executeDockerCommand(cmd);

    Json::Value images(Json::arrayValue);
    if (rawOutput.empty()) {
        return images;
    }

    std::istringstream iss(rawOutput);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        std::istringstream lineStream(line);
        std::string id, repository, tag, size;
        std::getline(lineStream, id, '\t');
        std::getline(lineStream, repository, '\t');
        std::getline(lineStream, tag, '\t');
        std::getline(lineStream, size, '\t');

        Json::Value image;
        image["id"] = id;
        image["repository"] = repository;
        image["tag"] = tag;
        image["size"] = size;

        images.append(image);
    }

    return images;
}

std::string DockerController::pullImage(const std::string &image) {
    if (image.empty()) {
        return "Error: Image name cannot be empty";
    }
    std::string cmd = "pull " + image;
    std::string result = executeDockerCommand(cmd);
    return result.find("Error") == std::string::npos ? "Image pulled successfully" : result;
}

std::string DockerController::loginToRegistry(const std::string &registry, 
                                              const std::string &username, 
                                              const std::string &password) {
    if (registry.empty() || username.empty() || password.empty()) {
        return "Error: Registry, username, and password are required";
    }
    std::ostringstream cmd;
    cmd << "login " << registry << " -u " << username << " -p " << password;
    std::string result = executeDockerCommand(cmd.str());
    return result.find("Login Succeeded") != std::string::npos ? "Login successful" : result;
}

std::string DockerController::pullPrivateImage(const std::string &image, 
                                                const std::string &registry, 
                                                const std::string &username, 
                                                const std::string &password) {
    // First, attempt to login
    std::string loginResult = loginToRegistry(registry, username, password);
    if (loginResult != "Login successful") {
        return "Failed to login to registry: " + loginResult;
    }

    // Then pull the image
    std::string fullImage = registry + "/" + image;
    std::string pullResult = pullImage(fullImage);
    return pullResult;
}

Json::Value DockerController::getContainerStats(const std::string &containerId) {
    // Use docker stats --no-stream --format to get stats for a single container
    std::string format = "--format \"{{.CPUPerc}}\t{{.MemUsage}}\t{{.MemLimit}}\t{{.NetIO}}\"";
    std::string cmd = "stats --no-stream --format '{{.CPUPerc}}\t{{.MemUsage}}\t{{.MemLimit}}\t{{.NetIO}}' " + containerId;
    std::string output = executeDockerCommand(cmd);

    Json::Value stats;
    if (output.empty()) {
        return stats;
    }
    std::istringstream iss(output);
    std::string line;
    if (std::getline(iss, line)) {
        std::istringstream lineStream(line);
        std::string cpu, memUsage, memLimit, netIO;
        std::getline(lineStream, cpu, '\t');
        std::getline(lineStream, memUsage, '\t');
        std::getline(lineStream, memLimit, '\t');
        std::getline(lineStream, netIO, '\t');

        // Parse CPU percentage (e.g., "0.05%")
        try {
            stats["cpu_percent"] = std::stod(cpu.substr(0, cpu.size() - 1));
        } catch (...) {
            stats["cpu_percent"] = 0.0;
        }

        // Parse memory usage and limit (e.g., "12.34MiB / 128MiB")
        auto parseMemory = [](const std::string &memStr) -> int64_t {
            double value = 0.0;
            std::string unit;
            std::istringstream ss(memStr);
            ss >> value >> unit;
            if (unit.find("GiB") != std::string::npos) return static_cast<int64_t>(value * 1024 * 1024 * 1024);
            if (unit.find("MiB") != std::string::npos) return static_cast<int64_t>(value * 1024 * 1024);
            if (unit.find("KiB") != std::string::npos) return static_cast<int64_t>(value * 1024);
            if (unit.find("B") != std::string::npos) return static_cast<int64_t>(value);
            return static_cast<int64_t>(value);
        };
        size_t slash = memUsage.find('/');
        if (slash != std::string::npos) {
            std::string used = memUsage.substr(0, slash);
            stats["memory_usage"] = parseMemory(used);
        } else {
            stats["memory_usage"] = 0;
        }
        stats["memory_limit"] = parseMemory(memLimit);

        // Parse network IO (e.g., "1.23kB / 4.56kB")
        size_t netSlash = netIO.find('/');
        auto parseNet = [](const std::string &netStr) -> int64_t {
            double value = 0.0;
            std::string unit;
            std::istringstream ss(netStr);
            ss >> value >> unit;
            if (unit.find("GB") != std::string::npos) return static_cast<int64_t>(value * 1024 * 1024 * 1024);
            if (unit.find("MB") != std::string::npos) return static_cast<int64_t>(value * 1024 * 1024);
            if (unit.find("kB") != std::string::npos) return static_cast<int64_t>(value * 1024);
            if (unit.find("B") != std::string::npos) return static_cast<int64_t>(value);
            return static_cast<int64_t>(value);
        };
        if (netSlash != std::string::npos) {
            std::string rx = netIO.substr(0, netSlash);
            std::string tx = netIO.substr(netSlash + 1);
            stats["net_rx_bytes"] = parseNet(rx);
            stats["net_tx_bytes"] = parseNet(tx);
        } else {
            stats["net_rx_bytes"] = 0;
            stats["net_tx_bytes"] = 0;
        }
    }
    return stats;
}

Json::Value DockerController::getDockerInfo() {
    // Use docker info --format to get running, stopped, and paused containers
    std::string cmd = "info --format '{{json .}}'";
    std::string output = executeDockerCommand(cmd);
    Json::Value info;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream iss(output);
    if (!Json::parseFromStream(builder, iss, &info, &errs)) {
        // Fallback: try to parse from plain text output
        output = executeDockerCommand("info");
        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.find("Running:") != std::string::npos) {
                info["ContainersRunning"] = std::stoi(line.substr(line.find(":") + 1));
            } else if (line.find("Paused:") != std::string::npos) {
                info["ContainersPaused"] = std::stoi(line.substr(line.find(":") + 1));
            } else if (line.find("Stopped:") != std::string::npos) {
                info["ContainersStopped"] = std::stoi(line.substr(line.find(":") + 1));
            }
        }
    }
    return info;
}

