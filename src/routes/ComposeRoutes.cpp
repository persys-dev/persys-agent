#include "ComposeRoutes.h"
#include <json/json.h>
#include <sstream>

namespace persys {

void initializeComposeRoutes(crow::App<persys::SignatureMiddleware>& app, ComposeController& composeController) {
    CROW_ROUTE(app, "/compose/run").methods("POST"_method)([&composeController](const crow::request &req) {
        Json::CharReaderBuilder builder;
        Json::Value jsonPayload;
        std::string errs;
        std::istringstream s(req.body);
        if (!Json::parseFromStream(builder, s, &jsonPayload, &errs)) {
            crow::json::wvalue response;
            response["error"] = "Invalid JSON: " + errs;
            return crow::response(400, response);
        }

        std::string composeDir = jsonPayload["composeDir"].asString();
        Json::Value envVariables = jsonPayload.get("envVariables", Json::Value(Json::nullValue));

        std::string result = composeController.runCompose(composeDir, envVariables);
        crow::json::wvalue response;
        response["result"] = result;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/compose/clone").methods("POST"_method)([&composeController](const crow::request &req) {
        Json::CharReaderBuilder builder;
        Json::Value jsonPayload;
        std::string errs;
        std::istringstream s(req.body);
        if (!Json::parseFromStream(builder, s, &jsonPayload, &errs)) {
            crow::json::wvalue response;
            response["error"] = "Invalid JSON: " + errs;
            return crow::response(400, response);
        }

        std::string repoUrl = jsonPayload.get("repoUrl", "").asString();
        std::string branch = jsonPayload.get("branch", "main").asString();
        std::string authToken = jsonPayload.get("authToken", "").asString();
        Json::Value envVariables = jsonPayload.get("envVariables", Json::Value(Json::nullValue));

        if (repoUrl.empty()) {
            crow::json::wvalue response;
            response["error"] = "Missing 'repoUrl' field";
            return crow::response(400, response);
        }

        crow::json::wvalue response;
        try {
            std::string composeDir = composeController.cloneRepository(repoUrl, branch, authToken);
            if (composeDir.empty()) {
                response["error"] = "Failed to clone or update repository";
                return crow::response(500, response);
            }
            std::string result = composeController.runCompose(composeDir, envVariables);
            response["result"] = result;
            response["composeDir"] = composeDir;
            return crow::response(200, response);
        } catch (const std::exception& e) {
            response["error"] = std::string("Internal server error: ") + e.what();
            return crow::response(500, response);
        }
    });

    CROW_ROUTE(app, "/compose/stop").methods("POST"_method)([&composeController](const crow::request &req) {
        Json::CharReaderBuilder builder;
        Json::Value jsonPayload;
        std::string errs;
        std::istringstream s(req.body);
        if (!Json::parseFromStream(builder, s, &jsonPayload, &errs)) {
            crow::json::wvalue response;
            response["error"] = "Invalid JSON: " + errs;
            return crow::response(400, response);
        }

        std::string composeDir = jsonPayload["composeDir"].asString();

        std::string result = composeController.stopCompose(composeDir);
        crow::json::wvalue response;
        response["result"] = result;
        return crow::response(200, response);
    });
}

} // namespace persys