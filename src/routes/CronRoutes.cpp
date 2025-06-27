#include "CronRoutes.h"
#include <crow.h>
#include <json/json.h>


namespace persys {

void initializeCronRoutes(crow::App<persys::SignatureMiddleware>& app, CronController& cronController) {
    CROW_ROUTE(app, "/cron/list").methods("GET"_method)([&cronController](const crow::request& req) {
        std::vector<std::string> jobs = cronController.listCronJobs();
        crow::json::wvalue response;
        response["result"] = jobs;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/cron/add").methods("POST"_method)([&cronController](const crow::request& req) {
        Json::CharReaderBuilder builder;
        Json::Value jsonPayload;
        std::string errs;
        std::istringstream s(req.body);
        if (!Json::parseFromStream(builder, s, &jsonPayload, &errs)) {
            crow::json::wvalue response;
            response["error"] = "Invalid JSON: " + errs;
            return crow::response(400, response);
        }

        std::string schedule = jsonPayload["schedule"].asString();
        std::string command = jsonPayload["command"].asString();

        std::string result = cronController.addCronJob(schedule, command);
        crow::json::wvalue response;
        response["result"] = result;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/cron/remove/<string>").methods("POST"_method)([&cronController](const crow::request& req, const std::string& jobId) {
        std::string result = cronController.removeCronJob(jobId);
        crow::json::wvalue response;
        response["result"] = result;
        return crow::response(200, response);
    });
}

}