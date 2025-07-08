#ifndef DOCKER_CONTROLLER_H
#define DOCKER_CONTROLLER_H

#include <string>
#include <vector>
#include <json/json.h>

class DockerController {
public:
    DockerController();
    ~DockerController();

    std::string startContainer(const std::string &image,
                              const std::string &name,
                              const std::vector<std::string> &ports,
                              const std::vector<std::string> &envVars,
                              const std::vector<std::string> &volumes,
                              const std::vector<std::string> &labels,
                              const std::string &network,
                              const std::string &restartPolicy,
                              bool detach,
                              const std::string &command = "");

    std::string stopContainer(const std::string &containerId);
    Json::Value listContainers(bool all = false);
    std::string removeContainer(const std::string &containerId);
    std::string getContainerLogs(const std::string &containerId);
    // New functions
    Json::Value listImages(bool all = false);  // List all images, optionally including intermediates
    std::string pullImage(const std::string &image);  // Pull a public image
    std::string loginToRegistry(const std::string &registry,
                                const std::string &username,
                                const std::string &password);  // Login to a registry
    std::string pullPrivateImage(const std::string &image,
                                 const std::string &registry,
                                 const std::string &username,
                                 const std::string &password);  // Pull a private image
    Json::Value getContainerStats(const std::string &containerId); // Get stats for a specific container
    Json::Value getDockerInfo(); // Get general Docker daemon info

private:
    std::string executeDockerCommand(const std::string &command);
};

#endif // DOCKER_CONTROLLER_H
