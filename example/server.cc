#include "../AdminCMDServer.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <signal.h>

class Server
{
public:
  struct AdminData
  {
    bool login;
  };
  using AdminServer = admincmd::AdminCMDServer<Server, AdminData>;
  using AdminConn = AdminServer::Connection;

  void run() {
    if (!admincmd_server.init(this, "127.0.0.1", 1234)) {
      std::cout << "admincmd_server init failed: " << admincmd_server.getLastError() << std::endl;
      return;
    }
    admincmd_help = "Server help:\n"
                    "login password\n"
                    "echo str\n"
                    "stop\n";

    running = true;
    admincmd_thr = std::thread([this]() {
      while (running.load(std::memory_order_relaxed)) {
        admincmd_server.poll();
        std::this_thread::yield();
      }
    });

    std::cout << "Server running..." << std::endl;
    admincmd_thr.join();
    std::cout << "Server stopped..." << std::endl;
  }

  void stop() { running = false; }

  std::string onAdminConnect(AdminConn& conn) {
    struct sockaddr_in addr;
    conn.getPeername(addr);
    std::cout << "admin connection from: " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << std::endl;

    conn.user_data.login = false;
    return admincmd_help;
  }

  void onAdminDisconnect(AdminConn& conn, const std::string& error) {
    struct sockaddr_in addr;
    conn.getPeername(addr);
    std::cout << "admin disconnect, error: " << error << ", login state: " << conn.user_data.login << std::endl;
  }

  std::string onAdminCMD(AdminConn& conn, int argc, const char** argv) {
    if (!strcmp(argv[0], "help")) {
      return admincmd_help;
    }

    if (!strcmp(argv[0], "login") && argc >= 2) {
      if (strcmp(argv[1], "123456")) {
        return "wrong password\n";
      }
      conn.user_data.login = true;
      return "login success\n";
    }
    if(!conn.user_data.login){
      return "must login first\n";
    }
    if (!strcmp(argv[0], "echo") && argc >= 2) {
      return std::string(argv[1]) + '\n';
    }
    if (!strcmp(argv[0], "stop")) {
      stop();
      return "";
    }
    return "invalid cmd, check help\n";
  }

private:
  AdminServer admincmd_server;
  std::thread admincmd_thr;
  std::string admincmd_help;
  std::atomic<bool> running;
};

Server server;

void my_handler(int s) {
  server.stop();
}

int main(int argc, char** argv) {
  struct sigaction sigIntHandler;

  sigIntHandler.sa_handler = my_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;

  sigaction(SIGINT, &sigIntHandler, NULL);

  server.run();
}
