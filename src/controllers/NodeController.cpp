#include "NodeController.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>
#include <stdexcept>
#include <fstream>
#include <pwd.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/sha.h>
#include <vector>
#include <string>

// Custom base64 decoder
static std::vector<char> base64_decode(const std::string& input) {
    static const unsigned char decode_table[] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    };

    std::vector<char> output;
    std::string cleaned_input;
    // Filter out invalid characters
    for (char c : input) {
        if (std::isalnum(c) || c == '+' || c == '/' || c == '=') {
            cleaned_input += c;
        } else {
            std::cerr << "Filtered invalid base64 character '" << c << "' (ASCII " << (int)c << ")" << std::endl;
        }
    }
    std::cerr << "Cleaned base64 input: " << cleaned_input << std::endl;

    size_t input_len = cleaned_input.size();
    // Validate length (must be multiple of 4)
    if (input_len % 4 != 0) {
        std::cerr << "Invalid base64 length: " << input_len << ", must be multiple of 4" << std::endl;
        return {};
    }

    size_t i = 0;
    while (i < input_len) {
        if (cleaned_input[i] == '=') {
            // Handle padding
            if (i < input_len - 2 || (i == input_len - 2 && cleaned_input[i + 1] != '=')) {
                std::cerr << "Invalid base64 padding at position " << i << std::endl;
                return {};
            }
            break;
        }

        if (i + 3 >= input_len) {
            std::cerr << "Incomplete base64 quartet at position " << i << std::endl;
            return {};
        }

        unsigned char a = decode_table[static_cast<unsigned char>(cleaned_input[i])];
        unsigned char b = decode_table[static_cast<unsigned char>(cleaned_input[i + 1])];
        unsigned char c = decode_table[static_cast<unsigned char>(cleaned_input[i + 2])];
        unsigned char d = decode_table[static_cast<unsigned char>(cleaned_input[i + 3])];

        if (a == 64) {
            std::cerr << "Invalid base64 character '" << cleaned_input[i] << "' at position " << i << std::endl;
            return {};
        }
        if (b == 64) {
            std::cerr << "Invalid base64 character '" << cleaned_input[i + 1] << "' at position " << i + 1 << std::endl;
            return {};
        }
        if (c == 64 && cleaned_input[i + 2] != '=') {
            std::cerr << "Invalid base64 character '" << cleaned_input[i + 2] << "' at position " << i + 2 << std::endl;
            return {};
        }
        if (d == 64 && cleaned_input[i + 3] != '=') {
            std::cerr << "Invalid base64 character '" << cleaned_input[i + 3] << "' at position " << i + 3 << std::endl;
            return {};
        }

        output.push_back((a << 2) | (b >> 4));
        if (cleaned_input[i + 2] != '=') {
            output.push_back((b << 4) | (c >> 2));
            if (cleaned_input[i + 3] != '=') {
                output.push_back((c << 6) | d);
            }
        }

        i += 4;
    }

    std::cerr << "Base64 decoded length: " << output.size() << std::endl;
    return output;
}

NodeController::NodeController(const std::string& centralUrl, SystemController& sysCtrl, int agentPort)
    : centralUrl_(centralUrl), isReady_(false), sysCtrl_(sysCtrl), agentPort_(agentPort) {
    nodeId_ = loadNodeId();
    if (nodeId_.empty()) {
        nodeId_ = generateNodeId();
        saveNodeId(nodeId_);
    }

    if (const char* portEnv = std::getenv("AGENT_PORT")) {
        try {
            agentPort_ = std::stoi(portEnv);
            if (agentPort_ <= 0 || agentPort_ > 65535) {
                std::cerr << "Invalid AGENT_PORT: " << portEnv << ", using default: " << agentPort << std::endl;
                agentPort_ = agentPort;
            }
        } catch (const std::exception& e) {
            std::cerr << "Invalid AGENT_PORT format: " << portEnv << ", using default: " << agentPort << std::endl;
            agentPort_ = agentPort;
        }
    } else {
        std::cerr << "AGENT_PORT not set, using default: " << agentPort_ << std::endl;
    }

    sharedSecret_ = std::getenv("AGENT_SECRET") ? std::getenv("AGENT_SECRET") : "";
    if (sharedSecret_.empty()) {
        std::cerr << "Warning: AGENT_SECRET not set; TOFU mode will be used unless shared secret is provided" << std::endl;
    }
}

std::string NodeController::generateNodeId() const {
    uuid_t uuid;
    uuid_generate_random(uuid);
    char uuidStr[37];
    uuid_unparse(uuid, uuidStr);
    return std::string(uuidStr);
}

std::string NodeController::loadNodeId() const {
    std::ifstream file("node_id.txt");
    if (file.is_open()) {
        std::string id;
        std::getline(file, id);
        file.close();
        return id;
    }
    return "";
}

void NodeController::saveNodeId(const std::string& id) const {
    std::ofstream file("node_id.txt");
    if (file.is_open()) {
        file << id;
        file.close();
    } else {
        throw std::runtime_error("Failed to save node ID to file");
    }
}

std::string NodeController::getDefaultInterface() const {
    std::ifstream routeFile("/proc/net/route");
    if (!routeFile.is_open()) {
        return "unknown";
    }

    std::string line;
    std::getline(routeFile, line);
    while (std::getline(routeFile, line)) {
        std::istringstream iss(line);
        std::string iface, dest, gateway;
        iss >> iface >> dest >> gateway;

        if (dest == "00000000" && gateway != "00000000") {
            routeFile.close();
            return iface;
        }
    }

    routeFile.close();
    return "unknown";
}

std::string NodeController::getExternalIp() const {
    std::string interface = getDefaultInterface();
    if (interface == "unknown") {
        return "unknown";
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return "unknown";
    }

    struct ifreq ifr;
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
        close(fd);
        return "unknown";
    }

    close(fd);
    return inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr);
}

std::string NodeController::getUsername() const {
    struct passwd* pw = getpwuid(geteuid());
    return pw ? std::string(pw->pw_name) : "unknown";
}

std::string NodeController::getHostname() const {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == -1) {
        return "unknown";
    }
    return std::string(hostname);
}

std::string NodeController::getOsName() const {
    struct utsname info;
    if (uname(&info) == -1) {
        return "unknown";
    }
    return std::string(info.sysname);
}

std::string NodeController::getKernelVersion() const {
    struct utsname info;
    if (uname(&info) == -1) {
        return "unknown";
    }
    return std::string(info.release);
}

std::string NodeController::determineStatus(const Json::Value& resources) const {
    const double CPU_THRESHOLD = 80.0;
    const double MEM_THRESHOLD = 80.0;
    const double DISK_THRESHOLD = 80.0;

    double cpu = resources["cpu_usage"].asDouble();
    double mem = resources["memory_usage"].asDouble();
    int disk = resources["disk_usage"].asInt();

    if (cpu > CPU_THRESHOLD || mem > MEM_THRESHOLD || disk > DISK_THRESHOLD) {
        return "busy";
    }
    return "active";
}

std::string NodeController::executeCommand(const std::string& cmd) const {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return "unknown";
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result.empty() ? "unknown" : result;
}

Json::Value NodeController::getHypervisorInfo() const {
    Json::Value hypervisor;

    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("vmx") != std::string::npos || line.find("svm") != std::string::npos) {
                hypervisor["type"] = "KVM";
                hypervisor["status"] = std::ifstream("/dev/kvm").good() ? "active" : "inactive";
                break;
            }
        }
        cpuinfo.close();
    }

    if (std::ifstream("/proc/xen").good()) {
        hypervisor["type"] = "Xen";
        hypervisor["status"] = "active";
    }

    std::string vbox_version = executeCommand("vboxmanage --version 2>/dev/null | tr -d '\n'");
    if (!vbox_version.empty() && vbox_version != "unknown") {
        hypervisor["type"] = "VirtualBox";
        hypervisor["version"] = vbox_version;
        hypervisor["status"] = "active";
    }

    if (hypervisor.isNull()) {
        hypervisor["type"] = "none";
        hypervisor["status"] = "n/a";
    }

    return hypervisor;
}

Json::Value NodeController::getContainerEngineInfo() const {
    Json::Value container;

    std::string docker_version = executeCommand("docker --version 2>/dev/null | tr -d '\n'");
    if (!docker_version.empty() && docker_version != "unknown") {
        container["type"] = "Docker";
        container["version"] = docker_version;
        container["status"] = executeCommand("systemctl is-active docker 2>/dev/null | tr -d '\n'") == "active" ? "active" : "inactive";
    }

    std::string podman_version = executeCommand("podman --version 2>/dev/null | tr -d '\n'");
    if (!podman_version.empty() && podman_version != "unknown") {
        container["type"] = "Podman";
        container["version"] = podman_version;
        container["status"] = "active";
    }

    if (container.isNull()) {
        container["type"] = "none";
        container["status"] = "n/a";
    }

    return container;
}

Json::Value NodeController::getDockerSwarmInfo() const {
    Json::Value swarm;

    std::string swarmState = executeCommand("docker info --format '{{.Swarm.LocalNodeState}}' 2>/dev/null | tr -d '\n'");
    if (swarmState.empty() || swarmState == "unknown" || swarmState == "inactive") {
        swarm["active"] = false;
        return swarm;
    }

    swarm["active"] = true;
    std::string nodeInspect = executeCommand("docker node inspect self --format '{{json .}}' 2>/dev/null");

    if (!nodeInspect.empty() && nodeInspect != "unknown") {
        Json::CharReaderBuilder builder;
        Json::Value nodeData;
        std::string errs;
        std::istringstream s(nodeInspect);
        if (Json::parseFromStream(builder, s, &nodeData, &errs)) {
            swarm["nodeId"] = nodeData["ID"].asString();
            swarm["role"] = nodeData["Spec"]["Role"].asString();
            swarm["status"] = nodeData["Status"]["State"].asString();
            if (nodeData["ManagerStatus"].isObject() && nodeData["ManagerStatus"]["Addr"].isString()) {
                swarm["managerAddress"] = nodeData["ManagerStatus"]["Addr"].asString();
            }
        } else {
            std::cerr << "Failed to parse docker node inspect: " << errs << std::endl;
        }
    }

    return swarm;
}

std::string NodeController::extractHostname(const std::string& url) const {
    // Simple URL parsing to extract hostname
    // Handle both http:// and https:// URLs
    std::string hostname = url;
    
    // Remove protocol
    size_t protocolPos = hostname.find("://");
    if (protocolPos != std::string::npos) {
        hostname = hostname.substr(protocolPos + 3);
    }
    
    // Remove path (everything after first slash)
    size_t pathPos = hostname.find('/');
    if (pathPos != std::string::npos) {
        hostname = hostname.substr(0, pathPos);
    }
    
    // Remove port if present
    size_t portPos = hostname.find(':');
    if (portPos != std::string::npos) {
        hostname = hostname.substr(0, portPos);
    }
    
    return hostname;
}

bool NodeController::isIpAddress(const std::string& hostname) const {
    // Check if the hostname is an IP address (IPv4 or IPv6)
    
    // IPv4 pattern: x.x.x.x where x is 0-255
    std::string ipv4Pattern = R"((\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3}))";
    
    // Simple IPv4 validation
    std::istringstream iss(hostname);
    std::string octet;
    int octetCount = 0;
    
    while (std::getline(iss, octet, '.')) {
        if (octetCount >= 4) return false; // Too many octets
        
        try {
            int value = std::stoi(octet);
            if (value < 0 || value > 255) return false; // Invalid octet range
        } catch (const std::exception&) {
            return false; // Not a number
        }
        
        octetCount++;
    }
    
    // Must have exactly 4 octets for IPv4
    if (octetCount != 4) return false;
    
    // Check if it's not all digits (which would indicate it's a hostname)
    for (char c : hostname) {
        if (c != '.' && !std::isdigit(c)) {
            return false; // Contains non-numeric characters, likely a hostname
        }
    }
    
    return true; // Looks like an IPv4 address
}

void NodeController::registerNode() {
    Json::Value resources = sysCtrl_.getSystemResources();
    Json::Value hypervisor = getHypervisorInfo();
    Json::Value container = getContainerEngineInfo();
    Json::Value swarm = getDockerSwarmInfo();

    Json::Value nodeData;
    nodeData["nodeId"] = nodeId_;
    nodeData["ipAddress"] = getExternalIp();
    nodeData["agentPort"] = agentPort_;
    nodeData["username"] = getUsername();
    nodeData["hostname"] = getHostname();
    nodeData["osName"] = getOsName();
    nodeData["kernelVersion"] = getKernelVersion();
    nodeData["status"] = determineStatus(resources);
    nodeData["timestamp"] = std::to_string(std::time(nullptr));
    nodeData["resources"] = resources;
    nodeData["hypervisor"] = hypervisor;
    nodeData["containerEngine"] = container;
    nodeData["dockerSwarm"] = swarm;
    nodeData["totalCpu"] = resources["total_cpu"].asDouble();
    nodeData["totalMemory"] = resources["total_memory"].asInt();
    nodeData["availableCpu"] = resources["available_cpu"].asDouble();
    nodeData["availableMemory"] = resources["available_memory"].asInt();
    nodeData["authConfig"]["sharedSecret"] = sharedSecret_;

    // Define supported labels with environment variables and fallbacks
    struct LabelConfig {
        std::string key;
        std::string envVar;
        std::string fallback;
    };
    std::vector<LabelConfig> labelConfigs = {
        {"env", "NODE_ENV", "prod"},
        {"region", "NODE_REGION", "us-west"},
        {"app", "NODE_APP", ""}
            // Add more labels here, e.g., {"role", "NODE_ROLE", ""}
    };

    // Set labels from environment variables
    for (const auto& config : labelConfigs) {
        const char* value = std::getenv(config.envVar.c_str());
        nodeData["labels"][config.key] = value && std::string(value) != "" ? value : config.fallback;
    }

    // Log labels for debugging
    std::cerr << "Registering node with labels: ";
    for (const auto& config : labelConfigs) {
        std::cerr << config.key << "=" << nodeData["labels"][config.key].asString();
        if (&config != &labelConfigs.back()) {
            std::cerr << ", ";
        }
    }
    std::cerr << std::endl;  

    Json::StreamWriterBuilder writer;
    std::string jsonPayload = Json::writeString(writer, nodeData);

    std::string url = centralUrl_ + "/nodes/register";
    
    // Log the URL being called
    std::cerr << "Attempting to register node with central service at: " << url << std::endl;
    
    // First, test DNS resolution (skip if IP address)
    std::string hostname = extractHostname(url);
    
    if (!isIpAddress(hostname)) {
        std::string dnsTestCmd = "nslookup " + hostname + " >/dev/null 2>&1";
        int dnsResult = std::system(dnsTestCmd.c_str());
        
        if (dnsResult != 0) {
            std::string errorMsg = "DNS resolution failed for hostname: " + hostname;
            std::cerr << "ERROR: " << errorMsg << std::endl;
            isReady_ = false;
            throw std::runtime_error(errorMsg);
        }
    } else {
        std::cerr << "Skipping DNS resolution for IP address: " << hostname << std::endl;
    }
    
    // Test if the server is reachable (basic connectivity)
    std::string connectivityTestCmd = "curl -s --connect-timeout 5 --max-time 10 " + url + " >/dev/null 2>&1";
    int connectivityResult = std::system(connectivityTestCmd.c_str());
    
    if (connectivityResult != 0) {
        std::string errorMsg = "Server not reachable or timeout: " + url;
        std::cerr << "ERROR: " << errorMsg << std::endl;
        isReady_ = false;
        throw std::runtime_error(errorMsg);
    }
    
    // Attempt the actual registration with detailed error capture
    std::string command = "curl -X POST -H \"Content-Type: application/json\" -d '"
                         + jsonPayload + "' " + url + " -w \"HTTP_CODE:%{http_code}\" -s";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::string errorMsg = "Failed to execute curl command for URL: " + url;
        std::cerr << "ERROR: " << errorMsg << std::endl;
        isReady_ = false;
        throw std::runtime_error(errorMsg);
    }
    
    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL) {
            result += buffer;
        }
    }
    int curlResult = pclose(pipe);
    
    // Extract HTTP status code from curl output
    size_t httpCodePos = result.find("HTTP_CODE:");
    int httpCode = -1;
    if (httpCodePos != std::string::npos) {
        std::string httpCodeStr = result.substr(httpCodePos + 10);
        httpCode = std::stoi(httpCodeStr);
    }
    
    if (curlResult == 0 && httpCode >= 200 && httpCode < 300) {
        std::cerr << "SUCCESS: Node registered successfully with HTTP code: " << httpCode << std::endl;
        isReady_ = (nodeData["status"].asString() == "active");
    } else {
        std::string errorMsg = "Registration failed for URL: " + url;
        if (httpCode > 0) {
            errorMsg += " (HTTP " + std::to_string(httpCode) + ")";
        }
        if (curlResult != 0) {
            errorMsg += " (curl exit code: " + std::to_string(curlResult) + ")";
        }
        std::cerr << "ERROR: " << errorMsg << std::endl;
        std::cerr << "Response: " << result << std::endl;
        isReady_ = false;
        throw std::runtime_error(errorMsg);
    }
}

std::string NodeController::getNodeId() const {
    return nodeId_;
}

bool NodeController::isNodeReady() const {
    return isReady_;
}

std::string NodeController::loadPublicKey() const {
    std::ifstream file("trusted_key.txt");
    if (!file.is_open()) {
        std::cerr << "Failed to open trusted_key.txt for reading" << std::endl;
        return "";
    }
    std::string key;
    std::getline(file, key); // Read the entire file (single line)
    file.close();
    if (!key.empty()) {
        std::cout << "Loaded public key from trusted_key.txt: " << key.substr(0, 50) << "..." << std::endl;
    }
    return key;
}

bool NodeController::savePublicKey(const std::string& publicKeyHex) const {
    std::ofstream file("trusted_key.txt");
    if (!file.is_open()) {
        std::cerr << "Failed to open /root/prod-agent/trusted_key.txt for writing" << std::endl;
        return false;
    }
    file << publicKeyHex;
    file.close();
    std::cout << "Saved public key to /root/prod-agent/trusted_key.txt: " << publicKeyHex.substr(0, 50) << "..." << std::endl;
    return true;
}

bool NodeController::verifySignature(const std::string& body, const std::string& signatureB64, const std::string& publicKeyHex) const {
    std::cerr << "verifySignature: signatureB64=" << signatureB64 << ", length=" << signatureB64.size() << std::endl;
    std::cerr << "verifySignature: publicKeyHex=" << publicKeyHex.substr(0, 50) << "..., length=" << publicKeyHex.size() << std::endl;

    std::vector<char> sigData = base64_decode(signatureB64);
    if (sigData.empty()) {
        std::cerr << "Failed to decode base64 signature" << std::endl;
        return false;
    }

    // Decode hex public key
    std::vector<unsigned char> keyData;
    for (size_t i = 0; i < publicKeyHex.length(); i += 2) {
        std::string byteString = publicKeyHex.substr(i, 2);
        try {
            unsigned char byte = (unsigned char)std::stoul(byteString, nullptr, 16);
            keyData.push_back(byte);
        } catch (const std::exception& e) {
            std::cerr << "Failed to decode hex at position " << i << ": " << e.what() << std::endl;
            return false;
        }
    }

    // Load public key
    BIO* bio = BIO_new_mem_buf(keyData.data(), keyData.size());
    if (!bio) {
        std::cerr << "Failed to create BIO" << std::endl;
        return false;
    }

    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) {
        std::cerr << "Failed to parse public key: " << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        return false;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!ctx) {
        EVP_PKEY_free(pkey);
        std::cerr << "Failed to create EVP context" << std::endl;
        return false;
    }

    if (EVP_PKEY_verify_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        std::cerr << "Failed to initialize verification" << std::endl;
        return false;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        std::cerr << "Failed to set RSA padding" << std::endl;
        return false;
    }

    if (EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        std::cerr << "Failed to set signature MD" << std::endl;
        return false;
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(body.c_str()), body.length(), hash);

    int result = EVP_PKEY_verify(ctx,
                                reinterpret_cast<const unsigned char*>(sigData.data()),
                                sigData.size(),
                                hash,
                                SHA256_DIGEST_LENGTH);

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    if (result != 1) {
        std::cerr << "Signature verification failed: " << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        return false;
    }

    return true;
}