#ifndef SYSTEMCONTROLLER_H
#define SYSTEMCONTROLLER_H

#include <json/json.h>
#include <string>

class SystemController {
public:
    SystemController();
    ~SystemController();

    Json::Value getSystemResources();

private:
    std::string executeShellCommand(const std::string &command);  // Add the declaration here
};

#endif // SYSTEMCONTROLLER_H
