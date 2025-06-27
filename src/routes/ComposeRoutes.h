#ifndef COMPOSE_ROUTES_H
#define COMPOSE_ROUTES_H

#include <crow.h>
#include "ComposeController.h"
#include "Middleware.h"

namespace persys {
void initializeComposeRoutes(crow::App<persys::SignatureMiddleware>& app, ComposeController& composeController);
} // namespace persys

#endif // COMPOSE_ROUTES_H