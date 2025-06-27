#ifndef DOCKER_ROUTES_H
#define DOCKER_ROUTES_H

#include <crow.h>
#include "DockerController.h"
#include "Middleware.h"

namespace persys {
void initializeDockerRoutes(crow::App<persys::SignatureMiddleware>& app, DockerController& DockerController);
} // namespace persys

#endif // DOCKER_ROUTES_H