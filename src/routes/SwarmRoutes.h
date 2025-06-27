#ifndef SWARM_ROUTES_H
#define SWARM_ROUTES_H

#include <crow.h>
#include "SwarmController.h"
#include "Middleware.h"

namespace persys {
    void initializeSwarmRoutes(crow::App<persys::SignatureMiddleware>& app, SwarmController& swarmController);
}

#endif // SWARM_ROUTES_H