#include "SwarmController.h"
#include <array>
#include <stdexcept>
#include <sstream>

SwarmController::SwarmController() {}

std::string SwarmController::executeDockerCommand(const std::string& command) const {
    std::string fullCommand = "docker " + command + " 2>&1";
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(fullCommand.c_str(), "r");
    if (!pipe) throw std::runtime_error("Failed to run Docker command");
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

Json::Value SwarmController::getStatus() {
    std::string cmd = "info --format '{{json .Swarm}}'";
    std::string rawOutput = executeDockerCommand(cmd);
    Json::Value status(Json::objectValue);

    // Check for empty output or explicit Docker errors
    if (rawOutput.empty() || rawOutput.find("Error:") == 0) {  // Only trigger on actual error messages
        status["active"] = false;
        status["message"] = rawOutput.empty() ? "Unable to retrieve swarm status" : rawOutput;
        return status;
    }

    // Parse the JSON output
    Json::CharReaderBuilder builder;
    std::istringstream iss(rawOutput);
    std::string errs;
    Json::Value swarmData;
    if (!Json::parseFromStream(builder, iss, &swarmData, &errs)) {
        status["active"] = false;
        status["error"] = "Failed to parse swarm info: " + errs;
        status["raw_output"] = rawOutput;
        return status;
    }

    // Extract Swarm status
    std::string localNodeState = swarmData["LocalNodeState"].asString();
    status["active"] = (localNodeState == "active");
    status["local_node_state"] = localNodeState;
    status["node_id"] = swarmData["NodeID"].asString();
    status["control_available"] = swarmData["ControlAvailable"].asBool();
    status["managers"] = swarmData["Managers"].asInt();
    status["nodes"] = swarmData["Nodes"].asInt();

    if (!status["active"].asBool()) {
        status["message"] = "Node is not part of an active swarm";
    }

    return status;
}

std::string SwarmController::initSwarm() {
    std::string cmd = "swarm init";
    std::string result = executeDockerCommand(cmd);
    return result.find("Error") == std::string::npos ? "Swarm initialized successfully" : result;
}

std::string SwarmController::joinSwarm(const std::string& managerAddress, const std::string& token) {
    if (managerAddress.empty() || token.empty()) {
        return "Error: Manager address and token are required";
    }
    std::ostringstream cmd;
    cmd << "swarm join --token " << token << " " << managerAddress;
    std::string result = executeDockerCommand(cmd.str());
    return result.find("Error") == std::string::npos ? "Joined swarm successfully" : result;
}

std::string SwarmController::leaveSwarm() {
    std::string cmd = "swarm leave --force";
    std::string result = executeDockerCommand(cmd);
    return result.find("Error") == std::string::npos ? "Left swarm successfully" : result;
}

std::string SwarmController::deployStack(const std::string& stackName, const std::string& composeFile) {
    if (stackName.empty() || composeFile.empty()) {
        return "Error: Stack name and compose file path are required";
    }
    std::ostringstream cmd;
    cmd << "stack deploy -c " << composeFile << " " << stackName;
    std::string result = executeDockerCommand(cmd.str());
    return result.find("Error") == std::string::npos ? "Stack deployed successfully" : result;
}

std::string SwarmController::removeStack(const std::string& stackName) {
    if (stackName.empty()) {
        return "Error: Stack name is required";
    }
    std::string cmd = "stack rm " + stackName;
    std::string result = executeDockerCommand(cmd);
    return result.find("Error") == std::string::npos ? "Stack removed successfully" : result;
}