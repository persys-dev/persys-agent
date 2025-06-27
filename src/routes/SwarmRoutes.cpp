#include "SwarmRoutes.h"
#include <json/json.h>
#include <sstream>

namespace persys {

void initializeSwarmRoutes(crow::App<persys::SignatureMiddleware>& app, SwarmController& swarmController) {
    CROW_ROUTE(app, "/api/swarm/status")
        .methods(crow::HTTPMethod::GET)([&swarmController](const crow::request& req) {
            Json::Value status = swarmController.getStatus();
            crow::json::wvalue response;
            Json::StreamWriterBuilder writer;
            std::string jsonString = Json::writeString(writer, status);
            response["result"] = crow::json::load(jsonString);
            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/api/swarm/init")
        .methods(crow::HTTPMethod::POST)([&swarmController](const crow::request& req) {
            std::string result = swarmController.initSwarm();
            crow::json::wvalue response;
            response["result"] = result;
            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/api/swarm/join")
        .methods(crow::HTTPMethod::POST)([&swarmController](const crow::request& req) {
            Json::CharReaderBuilder builder;
            Json::Value payload;
            std::istringstream s(req.body);
            std::string errs;
            if (!Json::parseFromStream(builder, s, &payload, &errs)) {
                crow::json::wvalue response;
                response["error"] = "Invalid JSON: " + errs;
                return crow::response(400, response);
            }
            std::string managerAddress = payload["managerAddress"].asString();
            std::string token = payload["token"].asString();
            std::string result = swarmController.joinSwarm(managerAddress, token);
            crow::json::wvalue response;
            response["result"] = result;
            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/api/swarm/leave")
        .methods(crow::HTTPMethod::POST)([&swarmController](const crow::request& req) {
            std::string result = swarmController.leaveSwarm();
            crow::json::wvalue response;
            response["result"] = result;
            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/api/swarm/deploy")
        .methods(crow::HTTPMethod::POST)([&swarmController](const crow::request& req) {
            Json::CharReaderBuilder builder;
            Json::Value payload;
            std::istringstream s(req.body);
            std::string errs;
            if (!Json::parseFromStream(builder, s, &payload, &errs)) {
                crow::json::wvalue response;
                response["error"] = "Invalid JSON: " + errs;
                return crow::response(400, response);
            }
            std::string stackName = payload["stackName"].asString();
            std::string composeFile = payload["composeFile"].asString();
            std::string result = swarmController.deployStack(stackName, composeFile);
            crow::json::wvalue response;
            response["result"] = result;
            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/api/swarm/remove")
        .methods(crow::HTTPMethod::POST)([&swarmController](const crow::request& req) {
            Json::CharReaderBuilder builder;
            Json::Value payload;
            std::istringstream s(req.body);
            std::string errs;
            if (!Json::parseFromStream(builder, s, &payload, &errs)) {
                crow::json::wvalue response;
                response["error"] = "Invalid JSON: " + errs;
                return crow::response(400, response);
            }
            std::string stackName = payload["stackName"].asString();
            std::string result = swarmController.removeStack(stackName);
            crow::json::wvalue response;
            response["result"] = result;
            return crow::response(200, response);
        });
}

} // namespace persys