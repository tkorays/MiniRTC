/**
 * @file network_interface.h
 * @brief MiniRTC network interface implementation
 */

#ifndef MINIRTC_NETWORK_INTERFACE_H
#define MINIRTC_NETWORK_INTERFACE_H

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "minirtc/transport/transport.h"
#include "minirtc/transport/transport_types.h"

namespace minirtc {

// ============================================================================
// Network Interface Implementation
// ============================================================================

/// Network interface implementation
class NetworkInterface : public INetworkInterface {
 public:
  /// Constructor
  NetworkInterface();

  /// Destructor
  ~NetworkInterface() override;

  // ========================================================================
  // INetworkInterface Interface
  // ========================================================================

  /// Create socket
  TransportError Create(NetworkInterfaceType type,
                        const NetworkAddress& local_addr) override;

  /// Close socket
  void Close() override;

  /// Bind address
  TransportError Bind(const NetworkAddress& addr) override;

  /// Connect
  TransportError Connect(const NetworkAddress& addr) override;

  /// Listen
  TransportError Listen(int backlog) override;

  /// Accept
  TransportError Accept(std::shared_ptr<INetworkInterface>* client_socket,
                        NetworkAddress* client_addr) override;

  /// Send data
  TransportError SendTo(const uint8_t* data,
                        size_t size,
                        const NetworkAddress& to) override;

  /// Receive data
  TransportError ReceiveFrom(uint8_t* buffer,
                              size_t buffer_size,
                              size_t* received,
                              NetworkAddress* from) override;

  /// Set socket options
  TransportError SetOptions(const SocketOptions& options) override;

  /// Get local address
  NetworkAddress GetLocalAddress() const override;

  /// Get remote address
  NetworkAddress GetRemoteAddress() const override;

  /// Set callback
  void SetCallback(INetworkCallback* callback) override;

  /// Set non-blocking mode
  TransportError SetNonBlocking(bool enabled) override;

  /// Get socket file descriptor
  int GetSocketFd() const override;

  /// Get interface type
  NetworkInterfaceType GetType() const override;

  /// Check if socket is valid
  bool IsValid() const override;

 private:
  /// Platform-specific socket creation
  TransportError CreateSocket(NetworkInterfaceType type);

  /// Platform-specific socket close
  void CloseSocket();

  // Socket handle
  int socket_fd_ = -1;

  // Interface type
  NetworkInterfaceType type_ = NetworkInterfaceType::kUdpSocket;

  // Addresses
  NetworkAddress local_addr_;
  NetworkAddress remote_addr_;

  // Options
  SocketOptions options_;

  // Callback
  INetworkCallback* callback_ = nullptr;
};

// ============================================================================
// Network Interface Manager (Reserved)
// ============================================================================

/// Network interface manager
class NetworkInterfaceManager {
 public:
  /// Get singleton instance
  static NetworkInterfaceManager* Instance();

  /// Destructor
  ~NetworkInterfaceManager();

  /// Get all available interfaces
  std::vector<NetworkInterfaceInfo> GetInterfaces();

  /// Get interface by name
  std::shared_ptr<INetworkInterface> GetInterface(const std::string& name);

  /// Create socket on specific interface
  std::shared_ptr<INetworkInterface> CreateSocket(
      const std::string& interface_name,
      NetworkInterfaceType type,
      const NetworkAddress& local_addr);

 private:
  /// Constructor
  NetworkInterfaceManager() = default;
};

// ============================================================================
// Factory
// ============================================================================

/// Create network interface instance
std::shared_ptr<INetworkInterface> CreateNetworkInterface();

}  // namespace minirtc

#endif  // MINIRTC_NETWORK_INTERFACE_H
