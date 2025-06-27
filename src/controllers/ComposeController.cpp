#include "ComposeController.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <algorithm>

std::string ComposeController::getRepoNameFromUrl(const std::string& repoUrl) const {
    // Extract the last part of the URL (e.g., "myrepo.git" from "https://github.com/user/myrepo.git")
    size_t lastSlash = repoUrl.find_last_of('/');
    if (lastSlash == std::string::npos) return "unknown_repo";
    std::string repoPart = repoUrl.substr(lastSlash + 1);
    // Remove ".git" suffix if present
    size_t gitPos = repoPart.find(".git");
    if (gitPos != std::string::npos) {
        repoPart = repoPart.substr(0, gitPos);
    }
    return repoPart.empty() ? "unknown_repo" : repoPart;
}

bool ComposeController::directoryExists(const std::string& path) const {
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

bool ComposeController::fileExists(const std::string& path) const {
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

bool ComposeController::hasDockerfile(const std::string& dir) const {
    // Check for Dockerfile in the directory or immediate subdirectories
    std::string dockerfilePath = dir + "/Dockerfile";
    if (fileExists(dockerfilePath)) return true;

    // Simple check for subdirectories (not recursive for simplicity)
    std::string command = "find " + dir + " -maxdepth 2 -name Dockerfile";
    std::string result = executeCommand(command);
    return !result.empty();
}

std::string ComposeController::executeCommand(const std::string& cmd) const {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

std::string ComposeController::cloneRepository(const std::string& repoUrl, const std::string& branch, const std::string& authToken) {
    std::string repoName = getRepoNameFromUrl(repoUrl);
    std::string repoDir = "./" + repoName;  // Use relative path with repo name
    std::string command;

    if (directoryExists(repoDir)) {
        // Directory exists, perform git pull
        command = "cd " + repoDir + " && git pull origin " + branch;
        if (!authToken.empty()) {
            // Rewrite URL with token for pull
            std::string authUrl = "https://" + authToken + "@" + repoUrl.substr(8);
            command = "cd " + repoDir + " && git fetch " + authUrl + " " + branch + " && git reset --hard FETCH_HEAD";
        }
    } else {
        // Directory doesn’t exist, clone the repo
        command = "git clone --branch " + branch + " " + repoUrl + " " + repoDir;
        if (!authToken.empty()) {
            std::string authUrl = "https://" + authToken + "@" + repoUrl.substr(8);
            command = "git clone --branch " + branch + " " + authUrl + " " + repoDir;
        }
    }

    int result = std::system(command.c_str());
    return result == 0 ? repoDir : "";
}

std::string ComposeController::runCompose(const std::string& composeDir, const Json::Value& envVariables) {
    std::string envCmd;
    if (envVariables.isObject()) {
        for (const auto& key : envVariables.getMemberNames()) {
            envCmd += key + "=" + envVariables[key].asString() + " ";
        }
    }

    // Look for docker-compose.yml or docker-compose.yaml
    std::string composeFile = composeDir + "/docker-compose.yml";
    if (!fileExists(composeFile)) {
        composeFile = composeDir + "/docker-compose.yaml";
        if (!fileExists(composeFile)) {
            return "No docker-compose file found in " + composeDir;
        }
    }

    // Check if --build is needed
    std::string buildOption = hasDockerfile(composeDir) ? "--build" : "";
    std::string command = envCmd + "docker compose -f " + composeFile + " up -d " + buildOption;
    int result = std::system(command.c_str());
    return result == 0 ? "Compose started successfully" : "Failed to start compose";
}

std::string ComposeController::stopCompose(const std::string& composeDir) {
    // Look for docker-compose.yml or docker-compose.yaml
    std::string composeFile = composeDir + "/docker-compose.yml";
    if (!fileExists(composeFile)) {
        composeFile = composeDir + "/docker-compose.yaml";
        if (!fileExists(composeFile)) {
            return "No docker-compose file found in " + composeDir;
        }
    }

    std::string command = "docker compose -f " + composeFile + " down";
    int result = std::system(command.c_str());
    return result == 0 ? "Compose stopped successfully" : "Failed to stop compose";
}