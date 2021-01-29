// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitService.h"

#include <absl/strings/match.h>
#include <absl/strings/str_format.h>
#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

#include "OrbitBase/Logging.h"
#include "OrbitGrpcServer.h"
#include "OrbitVersion/OrbitVersion.h"
#include "ProducerSideChannel/ProducerSideChannel.h"
#include "ProducerSideServer.h"

namespace orbit_service {

static std::string ReadStdIn() {
  int tmp = fgetc(stdin);
  if (tmp == -1) return "";

  std::string result;
  do {
    result += static_cast<char>(tmp);
    tmp = fgetc(stdin);
  } while (tmp != -1);

  return result;
}

static bool IsSshConnectionAlive(
    std::chrono::time_point<std::chrono::steady_clock> last_ssh_message,
    const int timeout_in_seconds) {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() -
                                                          last_ssh_message)
             .count() < timeout_in_seconds;
}

static std::unique_ptr<OrbitGrpcServer> CreateGrpcServer(uint16_t grpc_port) {
  std::string grpc_address = absl::StrFormat("127.0.0.1:%d", grpc_port);
  LOG("Starting gRPC server at %s", grpc_address);
  std::unique_ptr<OrbitGrpcServer> grpc_server = OrbitGrpcServer::Create(grpc_address);
  if (grpc_server == nullptr) {
    ERROR("Unable to start gRPC server");
    return nullptr;
  }
  LOG("gRPC server is running");
  return grpc_server;
}

static std::unique_ptr<ProducerSideServer> BuildAndStartProducerSideServer() {
  const std::filesystem::path unix_domain_socket_dir =
      std::filesystem::path{orbit_producer_side_channel::kProducerSideUnixDomainSocketPath}
          .parent_path();
  std::error_code error_code;
  std::filesystem::create_directories(unix_domain_socket_dir, error_code);
  if (error_code) {
    ERROR("Unable to create directory for socket for producer-side server: %s",
          error_code.message());
    return nullptr;
  }

  auto producer_side_server = std::make_unique<ProducerSideServer>();
  LOG("Starting producer-side server at %s",
      orbit_producer_side_channel::kProducerSideUnixDomainSocketPath);
  if (!producer_side_server->BuildAndStart(
          orbit_producer_side_channel::kProducerSideUnixDomainSocketPath)) {
    ERROR("Unable to start producer-side server");
    return nullptr;
  }
  LOG("Producer-side server is running");
  return producer_side_server;
}

void OrbitService::Run(std::atomic<bool>* exit_requested) {
  LOG("Running Orbit Service version %s", orbit_core::GetVersion());
#ifndef NDEBUG
  LOG("**********************************");
  LOG("Orbit Service is running in DEBUG!");
  LOG("**********************************");
#endif

  std::unique_ptr<OrbitGrpcServer> grpc_server = CreateGrpcServer(grpc_port_);
  if (grpc_server == nullptr) {
    return;
  }

  std::unique_ptr<ProducerSideServer> producer_side_server = BuildAndStartProducerSideServer();
  if (producer_side_server == nullptr) {
    return;
  }
  grpc_server->AddCaptureStartStopListener(producer_side_server.get());

  // Make stdin non-blocking.
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

  // Wait for exit_request or for the watchdog to expire.
  while (!(*exit_requested)) {
    std::string stdin_data = ReadStdIn();
    // If ssh sends EOF, end main loop.
    if (feof(stdin) != 0) break;

    if (IsSshWatchdogActive() || absl::StrContains(stdin_data, kStartWatchdogPassphrase)) {
      if (!stdin_data.empty()) {
        last_stdin_message_ = std::chrono::steady_clock::now();
      }

      if (!IsSshConnectionAlive(last_stdin_message_.value(), kWatchdogTimeoutInSeconds)) {
        break;
      }
    }

    std::this_thread::sleep_for(std::chrono::seconds{1});
  }

  producer_side_server->ShutdownAndWait();
  grpc_server->RemoveCaptureStartStopListener(producer_side_server.get());

  grpc_server->Shutdown();
  grpc_server->Wait();
}

}  // namespace orbit_service
