#include "DockerRoutes.h"
#include <json/json.h>
#include <sstream>
#include <thread>

namespace persys {

void initializeDockerRoutes(crow::App<persys::SignatureMiddleware>& app, DockerController& dockerController) {

    CROW_ROUTE(app, "/docker/run").methods("POST"_method)([&dockerController](const crow::request &req) {
        Json::CharReaderBuilder builder;
        Json::Value jsonPayload;
        std::string errs;
        std::istringstream s(req.body);
        if (!Json::parseFromStream(builder, s, &jsonPayload, &errs)) {
            crow::json::wvalue response;
            response["error"] = "Invalid JSON: " + errs;
            return crow::response(400, response);
        }

        std::string workloadId = jsonPayload["workloadId"].asString();
        std::string image = jsonPayload["image"].asString();
        std::string name = jsonPayload["name"].asString(); // This is now the workload ID
        std::string displayName = jsonPayload["displayName"].asString(); // Original name for display
        std::string command = jsonPayload["command"].asString();
        std::vector<std::string> ports;
        for (const auto &port : jsonPayload["ports"]) {
            ports.push_back(port.asString());
        }
        std::vector<std::string> envVars;
        if (jsonPayload["env"].isObject()) {
            for (const auto& key : jsonPayload["env"].getMemberNames()) {
                envVars.push_back(key + "=" + jsonPayload["env"][key].asString());
            }
        }
        std::vector<std::string> volumes;
        for (const auto &volume : jsonPayload["volumes"]) {
            volumes.push_back(volume.asString());
        }
        std::string network = jsonPayload["network"].asString();
        std::string restartPolicy = jsonPayload["restartPolicy"].asString();
        bool detach = jsonPayload["detach"].asBool();
        bool async = jsonPayload["async"].asBool();

        std::cout << "Docker Command: run -d --name " << name 
                  << " --label displayName=" << displayName
                  << " --restart=" << restartPolicy;
        for (const auto& port : ports) {
            std::cout << " -p " << port;
        }
        for (const auto& volume : volumes) {
            std::cout << " -v " << volume;
        }
        for (const auto& env : envVars) {
            std::cout << " -e \"" << env << "\"";
        }
        if (!network.empty()) {
            std::cout << " --network " << network;
        }
        if (!command.empty()) {
            std::cout << " " << command;
        }
        std::cout << " " << image << std::endl;

        // Prepare labels
        std::vector<std::string> labels;
        labels.push_back("displayName=" + displayName);
        labels.push_back("workloadId=" + workloadId);

        // Execute the Docker command (always async now, but return immediately)
        std::thread([&dockerController, image, name, ports, envVars, volumes, labels, network, restartPolicy, detach, command]() {
            try {
                std::string result = dockerController.startContainer(image, name, ports, envVars, volumes, labels, network, restartPolicy, detach, command);
                std::cout << "Container execution result for " << name << ": " << result << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Exception during container execution for " << name << ": " << e.what() << std::endl;
            }
        }).detach();

        // Return immediately
        crow::json::wvalue response;
        response["result"] = "Command queued for execution";
        response["workloadId"] = workloadId;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/docker/stop/<string>").methods("POST"_method)([&dockerController](const crow::request &req, const std::string &id) {
        std::string result = dockerController.stopContainer(id);
        crow::json::wvalue response;
        response["result"] = result;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/docker/list").methods("GET"_method)([&dockerController](const crow::request &req) {
        bool all = true;
        if (auto allParam = req.url_params.get("all")) {
            all = std::string(allParam) == "true";
        }

        Json::Value containers = dockerController.listContainers(all);
        crow::json::wvalue response;
        Json::StreamWriterBuilder writer;
        std::string jsonString = Json::writeString(writer, containers);
        response["result"] = crow::json::load(jsonString);
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/docker/remove/<string>").methods("POST"_method)([&dockerController](const crow::request &req, const std::string &id) {
        std::string result = dockerController.removeContainer(id);
        crow::json::wvalue response;
        response["result"] = result;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/docker/logs/<string>").methods("GET"_method)([&dockerController](const crow::request &req, const std::string &id) {
        std::string result = dockerController.getContainerLogs(id);
        crow::json::wvalue response;
        response["result"] = result;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/docker/images").methods("GET"_method)([&dockerController](const crow::request &req) {
        bool all = req.url_params.get("all") != nullptr && std::string(req.url_params.get("all")) == "true";
        Json::Value images = dockerController.listImages(all);
        crow::json::wvalue response;
        Json::StreamWriterBuilder writer;
        std::string jsonString = Json::writeString(writer, images);
        response["result"] = crow::json::load(jsonString);
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/docker/pull").methods("POST"_method)([&dockerController](const crow::request &req) {
        Json::CharReaderBuilder builder;
        Json::Value payload;
        std::istringstream s(req.body);
        std::string errs;
        if (!Json::parseFromStream(builder, s, &payload, &errs)) {
            crow::json::wvalue response;
            response["error"] = "Invalid JSON: " + errs;
            return crow::response(400, response);
        }
        std::string image = payload["image"].asString();
        std::string result = dockerController.pullImage(image);
        crow::json::wvalue response;
        response["result"] = result;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/docker/login").methods("POST"_method)([&dockerController](const crow::request &req) {
        Json::CharReaderBuilder builder;
        Json::Value payload;
        std::istringstream s(req.body);
        std::string errs;
        if (!Json::parseFromStream(builder, s, &payload, &errs)) {
            crow::json::wvalue response;
            response["error"] = "Invalid JSON: " + errs;
            return crow::response(400, response);
        }
        std::string registry = payload["registry"].asString();
        std::string username = payload["username"].asString();
        std::string password = payload["password"].asString();
        std::string result = dockerController.loginToRegistry(registry, username, password);
        crow::json::wvalue response;
        response["result"] = result;
        return crow::response(200, response);
    });
}

} // namespace persys