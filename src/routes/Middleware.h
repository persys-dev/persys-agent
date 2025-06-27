#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include <crow.h>
#include <string>
#include <iostream>
#include "NodeController.h"

namespace persys {

struct SignatureMiddleware {
    struct context {};

    NodeController* nodeController = nullptr;

    void setNodeController(NodeController& nc) {
        nodeController = &nc;
    }

    void before_handle(crow::request& req, crow::response& res, context& /*ctx*/) {
        // Skip authentication for metrics endpoint
        if (req.url == "/metrics") {
            return;
        }

        if (!nodeController) {
            res.code = 500;
            res.write(crow::json::wvalue{{"error", "Middleware not initialized"}}.dump());
            res.end();
            return;
        }

        auto signature_it = req.headers.find("X-Scheduler-Signature");
        auto pubkey_it = req.headers.find("X-Scheduler-PublicKey");
        auto secret_it = req.headers.find("X-Shared-Secret");

        if (signature_it == req.headers.end() || pubkey_it == req.headers.end()) {
            res.code = 401;
            res.write(crow::json::wvalue{{"error", "Missing signature or public key headers"}}.dump());
            res.end();
            return;
        }

        std::string body = req.body;
        std::string signature = signature_it->second;
        std::string publicKeyHex = pubkey_it->second;

        bool isHandshake = req.url == "/api/v1/handshake";

        if (!nodeController->verifySignature(body, signature, publicKeyHex)) {
            if (secret_it != req.headers.end() && !secret_it->second.empty() && secret_it->second == nodeController->getSharedSecret()) {
                std::cout << "Signature verification failed, but shared secret matched: " << secret_it->second << std::endl;
            } else {
                std::cout << "verifySignature: Signature verification failed" << std::endl;
                res.code = 401;
                res.write(crow::json::wvalue{{"error", "Signature verification failed"}}.dump());
                res.end();
                return;
            }
        } else {
            std::cout << "verifySignature: Signature verified successfully" << std::endl;
        }

        std::string trustedKey = nodeController->loadPublicKey();
        std::cout << "Trusted key: " << (trustedKey.empty() ? "empty" : trustedKey.substr(0, 50) + "...") << std::endl;

        if (isHandshake) {
            if (!nodeController->savePublicKey(publicKeyHex)) {
                res.code = 500;
                res.write(crow::json::wvalue{{"error", "Failed to store public key"}}.dump());
                res.end();
                return;
            }
            std::cout << "Trusted public key stored: " << publicKeyHex.substr(0, 50) << "..." << std::endl;
        } else if (!trustedKey.empty() && trustedKey != publicKeyHex) {
            if (secret_it != req.headers.end() && !secret_it->second.empty() && secret_it->second == nodeController->getSharedSecret()) {
                std::cout << "Public key mismatch, but shared secret matched: " << secret_it->second << std::endl;
            } else {
                std::cout << "verifySignature: Public key does not match trusted key. Received: " << publicKeyHex.substr(0, 50) << "..., Trusted: " << trustedKey.substr(0, 50) << "..." << std::endl;
                res.code = 401;
                res.write(crow::json::wvalue{{"error", "Public key does not match trusted key"}}.dump());
                res.end();
                return;
            }
        }
    }

    void after_handle(crow::request& /*req*/, crow::response& /*res*/, context& /*ctx*/) {}
};

} // namespace persys

#endif // MIDDLEWARE_H