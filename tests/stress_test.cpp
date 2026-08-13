#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <atomic>
#include <chrono>
#include <sstream>
#include <fstream>
#include <filesystem>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

string toRESP(const vector<string>& tokens) {
    string resp = "*" + to_string(tokens.size()) + "\r\n";
    for (auto &t : tokens) {
        resp += "$" + to_string(t.size()) + "\r\n";
        resp += t + "\r\n";
    }
    return resp;
}

struct Stats {
    atomic<int> ops{0};
    atomic<int> errors{0};
    atomic<int> successes{0};
};

void clientThread(int id, int opsPerClient, Stats &stats, int seed) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        cerr << "WSAStartup failed in thread " << id << "\n";
        stats.errors++;
        return;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        cerr << "socket failed in thread " << id << "\n";
        stats.errors++;
        WSACleanup();
        return;
    }

    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(6379);
    inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);

    if (connect(sock, (sockaddr*)&serv, sizeof(serv)) == SOCKET_ERROR) {
        cerr << "connect failed in thread " << id << " err=" << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        stats.errors++;
        return;
    }

    mt19937 rng(seed);
    uniform_int_distribution<int> cmdDist(0,4);
    uniform_int_distribution<int> keyDist(1,100);
    uniform_int_distribution<int> valDist(1,100000);

    char recvbuf[4096];

    for (int i=0;i<opsPerClient;i++) {
        int cmd = cmdDist(rng);
        int k = keyDist(rng);
        int v = valDist(rng);
        vector<string> tokens;
        switch(cmd) {
            case 0: // SET
                tokens = {"SET", "key"+to_string(k), "val"+to_string(v)};
                break;
            case 1: // GET
                tokens = {"GET", "key"+to_string(k)};
                break;
            case 2: // DEL
                tokens = {"DEL", "key"+to_string(k)};
                break;
            case 3: // LPUSH
                tokens = {"LPUSH", "list"+to_string(k), "val"+to_string(v)};
                break;
            case 4: // RPOP
                tokens = {"RPOP", "list"+to_string(k)};
                break;
        }
        string req = toRESP(tokens);
        int sent = send(sock, req.c_str(), (int)req.size(), 0);
        if (sent == SOCKET_ERROR) {
            stats.errors++;
            break;
        }
        int recvd = recv(sock, recvbuf, sizeof(recvbuf)-1, 0);
        if (recvd > 0) {
            recvbuf[recvd] = '\0';
            stats.successes++;
        } else {
            stats.errors++;
            break;
        }
        stats.ops++;
    }

    closesocket(sock);
    WSACleanup();
}

int main() {
    const int clientCount = 100;
    const int opsPerClient = 500; // hundreds

    vector<thread> clients;
    Stats stats;

    cout << "Stress test starting: clients=" << clientCount << " ops/client=" << opsPerClient << "\n";
    cout.flush();
    auto start = chrono::high_resolution_clock::now();
    for (int i=0;i<clientCount;i++) {
        clients.emplace_back(clientThread, i, opsPerClient, ref(stats), 1000+i);
    }

    // Monitor thread responsiveness by checking stats increments
    atomic<bool> done{false};
    thread monitor([&](){
        int lastOps = 0;
        int stagnant = 0;
        int ticks = 0;
        while (!done) {
            this_thread::sleep_for(chrono::seconds(1));
            int current = stats.ops.load();
            if (++ticks % 5 == 0) {
                cerr << "Progress: ops=" << current << " errors=" << stats.errors.load() << " successes=" << stats.successes.load() << "\n";
                cerr.flush();
            }
            if (current == lastOps) {
                stagnant++;
            } else {
                stagnant = 0;
            }
            lastOps = current;
            // If stagnant for 10 seconds, print warning
            if (stagnant >= 10) {
                cerr << "Warning: no progress for 10s" << endl;
            }
        }
    });

    for (auto &t : clients) t.join();
    done = true;
    monitor.join();

    auto end = chrono::high_resolution_clock::now();
    double seconds = chrono::duration<double>(end - start).count();

    stringstream ss;
    ss << "Clients: " << clientCount << " Ops/client: " << opsPerClient << "\n";
    ss << "Total ops attempted: " << stats.ops.load() << "\n";
    ss << "Success responses: " << stats.successes.load() << "\n";
    ss << "Errors: " << stats.errors.load() << "\n";
    ss << "Duration(s): " << seconds << "\n";
    ss << "Throughput (ops/s): " << (stats.ops.load()/seconds) << "\n";

    cout << ss.str();

    // Also write a deterministic output file for retrieval
    {
        std::filesystem::create_directories("results");
        ofstream out("results/stress_result.txt");
        out << ss.str();
        out.close();
    }

    return 0;
}
