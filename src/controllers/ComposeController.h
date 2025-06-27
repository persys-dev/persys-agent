#ifndef COMPOSE_CONTROLLER_H
#define COMPOSE_CONTROLLER_H

#include <string>
#include <json/json.h>

class ComposeController {
public:
    std::string cloneRepository(const std::string& repoUrl, const std::string& branch, const std::string& authToken);
    std::string runCompose(const std::string& composeDir, const Json::Value& envVariables);
    std::string stopCompose(const std::string& composeDir);

private:
    bool directoryExists(const std::string& path) const;
    bool fileExists(const std::string& path) const;
    bool hasDockerfile(const std::string& dir) const;
    std::string getRepoNameFromUrl(const std::string& repoUrl) const;
    std::string executeCommand(const std::string& cmd) const;  // Declaration added
};

#endif // COMPOSE_CONTROLLER_H