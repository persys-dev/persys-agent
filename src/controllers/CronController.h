#ifndef CRON_CONTROLLER_H
#define CRON_CONTROLLER_H

#include <string>
#include <vector>

class CronController {
public:
    CronController();
    ~CronController();

    std::vector<std::string> listCronJobs();
    std::string addCronJob(const std::string &schedule, const std::string &command);
    std::string removeCronJob(const std::string &jobId);
    
private:
    std::string executeCronCommand(const std::string &command);
};

#endif
