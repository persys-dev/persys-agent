#ifndef SWARM_CONTROLLER_H
#define SWARM_CONTROLLER_H

#include <string>
#include <json/json.h>

class SwarmController {
public:
    SwarmController();
    Json::Value getStatus();  // Existing method
    std::string initSwarm();  // Initialize a new Swarm
    std::string joinSwarm(const std::string& managerAddress, const std::string& token);  // Join an existing Swarm
    std::string leaveSwarm();  // Leave the Swarm
    std::string deployStack(const std::string& stackName, const std::string& composeFile);  // Deploy a stack
    std::string removeStack(const std::string& stackName);  // Remove a stack

private:
    std::string executeDockerCommand(const std::string& command) const;  // Helper to run Docker commands
};

#endif // SWARM_CONTROLLER_H