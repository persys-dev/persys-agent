#ifndef CRON_ROUTES_H
#define CRON_ROUTES_H

#include <crow.h>
#include "CronController.h"
#include "Middleware.h"

namespace persys {
    void initializeCronRoutes(crow::App<persys::SignatureMiddleware>& app, CronController& cronController);
}

#endif // CRON_ROUTES_H