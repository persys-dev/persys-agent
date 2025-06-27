#include "CronController.h"
#include <array>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>

CronController::CronController() {}

CronController::~CronController() {}

std::vector<std::string> CronController::listCronJobs() {
    std::vector<std::string> jobs;
    std::string result = executeCronCommand("crontab -l");

    std::istringstream stream(result);
    std::string line;
    while (std::getline(stream, line)) {
        jobs.push_back(line);
    }

    return jobs;
}

std::string CronController::addCronJob(const std::string &schedule, const std::string &command) {
    std::string newJob = schedule + " " + command;
    std::string tmpFile = "/tmp/cronjobs.txt";

    executeCronCommand("crontab -l > " + tmpFile);
    executeCronCommand("echo \"" + newJob + "\" >> " + tmpFile);
    return executeCronCommand("crontab " + tmpFile);
}

std::string CronController::removeCronJob(const std::string &jobId) {
    std::string tmpFile = "/tmp/cronjobs.txt";
    executeCronCommand("crontab -l | grep -v \"" + jobId + "\" > " + tmpFile);
    return executeCronCommand("crontab " + tmpFile);
}

std::string CronController::executeCronCommand(const std::string &command) {
    std::array<char, 128> buffer;
    std::string result;
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) throw std::runtime_error("Failed to run command");
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}
