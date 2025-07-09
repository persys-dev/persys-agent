#ifndef NODE_CONTROLLER_H
#define NODE_CONTROLLER_H

#include "SystemController.h"
#include <crow.h>
#include <json/json.h>
#include <string>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

class NodeController {
public:
    NodeController(const std::string& centralUrl, SystemController& sysCtrl, int agentPort = 8080);
    void registerNode();
    std::string getNodeId() const;
    bool isNodeReady() const;
    bool verifySignature(const std::string& body, const std::string& signatureB64, const std::string& publicKeyHex) const;
    std::string loadPublicKey() const;
    std::string getSharedSecret() const { return sharedSecret_; }
    bool savePublicKey(const std::string& publicKeyHex) const;
    std::string determineStatus(const Json::Value& resources) const;
    
private:
    std::string generateNodeId() const;
    std::string loadNodeId() const;
    void saveNodeId(const std::string& id) const;
    std::string getDefaultInterface() const;
    std::string getExternalIp() const;
    std::string getUsername() const;
    std::string getHostname() const;
    std::string getOsName() const;
    std::string getKernelVersion() const;
    std::string executeCommand(const std::string& cmd) const;
    Json::Value getHypervisorInfo() const;
    Json::Value getContainerEngineInfo() const;
    Json::Value getDockerSwarmInfo() const;
    std::string extractHostname(const std::string& url) const;
    bool isIpAddress(const std::string& hostname) const;

    std::string centralUrl_;
    std::string nodeId_;
    bool isReady_;
    SystemController& sysCtrl_;
    std::string sharedSecret_;
    int agentPort_;
};

#endif // NODE_CONTROLLER_H