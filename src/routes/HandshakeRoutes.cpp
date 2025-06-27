#include "HandshakeRoutes.h"
#include <crow.h>
#include <json/json.h>

namespace persys{
    void initializeHandshakeRoutes(crow::App<persys::SignatureMiddleware>& app, NodeController& nodeController) {
        CROW_ROUTE(app, "/api/v1/handshake").methods("POST"_method)([&nodeController](const crow::request& req) {
            Json::Value payload;
            Json::CharReaderBuilder builder;
            std::string errs;
            std::istringstream s(req.body);
            if (!Json::parseFromStream(builder, s, &payload, &errs)) {
                return crow::response(400, "{\"error\": \"Invalid JSON payload\"}");
            }

            std::string schedulerId = payload["schedulerId"].asString();
            std::string timestamp = payload["timestamp"].asString();

            std::cout << "Received handshake from scheduler: " << schedulerId << ", timestamp: " << timestamp << std::endl;

            Json::Value response;
            response["message"] = "Handshake successful";
            response["nodeId"] = nodeController.getNodeId();
            Json::StreamWriterBuilder writer;
            return crow::response(200, Json::writeString(writer, response));
        });
    }
} // namespace persys