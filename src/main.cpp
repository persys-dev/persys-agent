#include "controllers/ComposeController.h"
#include "controllers/CronController.h"
#include "controllers/DockerController.h"
#include "controllers/SystemController.h"
#include "controllers/NodeController.h"
#include "controllers/SwarmController.h"
#include <crow.h>
#include <json/json.h>
#include "routes/HandshakeRoutes.h"
#include "routes/DockerRoutes.h"
#include "routes/ComposeRoutes.h"
#include "routes/CronRoutes.h"
#include "routes/SwarmRoutes.h"
#include "routes/Middleware.h"
#include <cstdlib>
#include <iostream>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include <sstream>
#include <map>

// Heartbeat function (unchanged)
bool sendHeartbeat(const std::string& centralUrl, const std::string& nodeId, const std::string& status, double availableCpu, int64_t availableMemory) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl" << std::endl;
        return false;
    }

    Json::Value heartbeat;
    heartbeat["nodeId"] = nodeId;
    heartbeat["status"] = status;
    heartbeat["availableCpu"] = availableCpu;
    heartbeat["availableMemory"] = availableMemory;

    Json::StreamWriterBuilder writer;
    std::string payload = Json::writeString(writer, heartbeat);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, (centralUrl + "/nodes/heartbeat").c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullptr);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (res != CURLE_OK || http_code != 200) {
        std::cerr << "Heartbeat failed for node " << nodeId << ": " << curl_easy_strerror(res) << ", HTTP " << http_code << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }

    std::cout << "Sent heartbeat for node " << nodeId << std::endl;
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return true;
}

// Heartbeat loop (unchanged)
void heartbeatLoop(const std::string& centralUrl, NodeController& nodeCtrl, SystemController& sysCtrl) {
    while (true) {
        Json::Value resources = sysCtrl.getSystemResources();
        std::string status = nodeCtrl.determineStatus(resources);
        double availableCpu = resources.get("available_cpu", 0.99).asDouble();
        int64_t availableMemory = resources.get("available_memory", 1354).asInt64();
        sendHeartbeat(centralUrl, nodeCtrl.getNodeId(), status, availableCpu, availableMemory);
        std::this_thread::sleep_for(std::chrono::minutes(4));
    }
}

// Registration with retry (unchanged)
bool registerWithRetry(NodeController& nodeCtrl, int maxRetries = 3, std::chrono::seconds delay = std::chrono::seconds(30)) {
    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        try {
            nodeCtrl.registerNode();
            if (nodeCtrl.isNodeReady()) {
                std::cout << "Node " << nodeCtrl.getNodeId() << " registered and ready for workloads\n";
                return true;
            } else {
                std::cout << "Node " << nodeCtrl.getNodeId() << " registered but not ready\n";
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "Registration attempt " << attempt << "/" << maxRetries << " failed: " << e.what() << std::endl;
        }
        if (attempt < maxRetries) {
            std::cout << "Retrying registration after " << delay.count() << " seconds\n";
            std::this_thread::sleep_for(delay);
        }
    }
    std::cerr << "Node registration failed after " << maxRetries << " attempts\n";
    return false;
}

// Prometheus metrics collection
std::string collectDockerMetrics(DockerController& dockerCtrl) {
    std::stringstream metrics;
    
    try {
        auto containers = dockerCtrl.listContainers();
        for (const auto& container : containers) {
            auto stats = dockerCtrl.getContainerStats(container["Id"].asString());
            
            // Container metrics
            metrics << "# HELP docker_container_cpu_usage_percent CPU usage percentage\n";
            metrics << "# TYPE docker_container_cpu_usage_percent gauge\n";
            metrics << "docker_container_cpu_usage_percent{container_id=\"" << container["Id"].asString() 
                   << "\",name=\"" << container["Names"][0].asString() << "\"} " 
                   << stats["cpu_percent"].asDouble() << "\n";
            
            metrics << "# HELP docker_container_memory_usage_bytes Memory usage in bytes\n";
            metrics << "# TYPE docker_container_memory_usage_bytes gauge\n";
            metrics << "docker_container_memory_usage_bytes{container_id=\"" << container["Id"].asString() 
                   << "\",name=\"" << container["Names"][0].asString() << "\"} " 
                   << stats["memory_usage"].asInt64() << "\n";
            
            metrics << "# HELP docker_container_memory_limit_bytes Memory limit in bytes\n";
            metrics << "# TYPE docker_container_memory_limit_bytes gauge\n";
            metrics << "docker_container_memory_limit_bytes{container_id=\"" << container["Id"].asString() 
                   << "\",name=\"" << container["Names"][0].asString() << "\"} " 
                   << stats["memory_limit"].asInt64() << "\n";
            
            metrics << "# HELP docker_container_network_rx_bytes Network received bytes\n";
            metrics << "# TYPE docker_container_network_rx_bytes gauge\n";
            metrics << "docker_container_network_rx_bytes{container_id=\"" << container["Id"].asString() 
                   << "\",name=\"" << container["Names"][0].asString() << "\"} " 
                   << stats["net_rx_bytes"].asInt64() << "\n";
            
            metrics << "# HELP docker_container_network_tx_bytes Network transmitted bytes\n";
            metrics << "# TYPE docker_container_network_tx_bytes gauge\n";
            metrics << "docker_container_network_tx_bytes{container_id=\"" << container["Id"].asString() 
                   << "\",name=\"" << container["Names"][0].asString() << "\"} " 
                   << stats["net_tx_bytes"].asInt64() << "\n";
        }
        
        // Docker daemon metrics
        auto info = dockerCtrl.getDockerInfo();
        metrics << "# HELP docker_daemon_containers_running Number of running containers\n";
        metrics << "# TYPE docker_daemon_containers_running gauge\n";
        metrics << "docker_daemon_containers_running " << info["ContainersRunning"].asInt() << "\n";
        
        metrics << "# HELP docker_daemon_containers_stopped Number of stopped containers\n";
        metrics << "# TYPE docker_daemon_containers_stopped gauge\n";
        metrics << "docker_daemon_containers_stopped " << info["ContainersStopped"].asInt() << "\n";
        
        metrics << "# HELP docker_daemon_containers_paused Number of paused containers\n";
        metrics << "# TYPE docker_daemon_containers_paused gauge\n";
        metrics << "docker_daemon_containers_paused " << info["ContainersPaused"].asInt() << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error collecting Docker metrics: " << e.what() << std::endl;
    }
    
    return metrics.str();
}

int main() {
    std::string centralUrl = std::getenv("CENTRAL_URL") ? std::getenv("CENTRAL_URL") : "http://localhost:8084";
    if (centralUrl == "" || centralUrl == "http://localhost:8084") {
        std::cerr << "Error: CENTRAL_URL environment variable not set" << std::endl;
        return 1;
    }
    int agentPort = std::getenv("AGENT_PORT") ? std::stoi(std::getenv("AGENT_PORT")) : 8080;

    // Initialize all controllers
    SystemController sysCtrl;
    NodeController nodeCtrl(centralUrl, sysCtrl, agentPort);
    SwarmController swarmCtrl;
    DockerController dockerCtrl;
    ComposeController composeCtrl;
    CronController cronCtrl;

    // Register node with retries
    if (!registerWithRetry(nodeCtrl)) {
        std::cerr << "Failed to register node, exiting\n";
        return 1;
    }

    // Set up Crow app with middleware (SignatureMiddleware is already included)
    crow::App<persys::SignatureMiddleware> app;

    app.get_middleware<persys::SignatureMiddleware>().setNodeController(nodeCtrl);

    // Add metrics endpoint before other routes to ensure it's not affected by middleware
    CROW_ROUTE(app, "/metrics")
    ([&dockerCtrl]() {
        return collectDockerMetrics(dockerCtrl);
    });

    // Initialize all routes
    persys::initializeHandshakeRoutes(app, nodeCtrl);
    persys::initializeDockerRoutes(app, dockerCtrl);
    persys::initializeComposeRoutes(app, composeCtrl);
    persys::initializeCronRoutes(app, cronCtrl);
    persys::initializeSwarmRoutes(app, swarmCtrl);

    // Health endpoint
    CROW_ROUTE(app, "/api/v1/health")
    ([&nodeCtrl, &sysCtrl]() {
        Json::Value resources = sysCtrl.getSystemResources();
        Json::Value health;
        health["nodeId"] = nodeCtrl.getNodeId();
        health["status"] = nodeCtrl.determineStatus(resources);
        health["availableCpu"] = resources.get("available_cpu", 0.99).asDouble();
        health["availableMemory"] = resources.get("available_memory", 1354).asInt64();
        health["timestamp"] = static_cast<long>(time(nullptr));

        Json::StreamWriterBuilder writer;
        return Json::writeString(writer, health);
    });

    CROW_CATCHALL_ROUTE(app)([](crow::response& res) {
        if (res.code == 404) {
            res.body = "The URL does not seem to be correct.";
        } else if (res.code == 405) {
            res.body = "The HTTP method does not seem to be correct.";
        }
        res.end();
    });

    // Start heartbeat thread
    std::thread heartbeatThread(heartbeatLoop, centralUrl, std::ref(nodeCtrl), std::ref(sysCtrl));

    // Run the app
    app.port(agentPort).multithreaded().run();

    // Clean up
    heartbeatThread.detach();
    return 0;
}