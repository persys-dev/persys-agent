#ifndef HANDSHAKE_ROUTES_H
#define HANDSHAKE_ROUTES_H

#include <crow.h>
#include "NodeController.h"
#include "Middleware.h"

namespace persys {
    void initializeHandshakeRoutes(crow::App<persys::SignatureMiddleware>& app, NodeController& nodeController);
} // namespace persys

#endif // HANDSHAKE_ROUTES_H