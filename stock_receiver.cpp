#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint>
#include <thread>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <chrono> // for latency logging

struct StockData {
   char ticker[8];
   float price;
   float high_52wk;
   float low_52wk;
};

struct PortfolioEntry {
   std::string ticker;
   int quantity;
   float stop_loss;  // Sell if price falls below this
};

// === Static Portfolio ===
std::unordered_map<std::string, PortfolioEntry> portfolio = {
   {"AAPL", {"AAPL", 100, 90.0f}},
   {"GOOG", {"GOOG", 50, 110.0f}},
   {"ANET", {"ANET", 30, 70.0f}}
};

std::mutex logMutex; // mutex for thread-safe logging

// per stock thread logic
void analyzeStock(const StockData& stock, const PortfolioEntry& entry) {
   auto start = std::chrono::high_resolution_clock::now();

   std::string ticker(stock.ticker);
   float price = stock.price;
   float high = stock.high_52wk;
   float low = stock.low_52wk;
   // === Decision Conditions ===
   bool hitStopLoss = (price < entry.stop_loss);
   bool shouldSell = (price < high * 0.90f);
   bool shouldBuy = (price <= low * 1.05f);

   { 
      std::lock_guard<std::mutex> lock(logMutex);
      // critical section
      std::cout << "\nStock: " << ticker << " (You own " << entry.quantity << " shares)\n";
      std::cout << "  Price:       " << price << "\n";
      std::cout << "  52W High:    " << high << "\n";
      std::cout << "  52W Low:     " << low << "\n";
      std::cout << "  Stop Loss:   " << entry.stop_loss << "\n";

      std::cout << "  Action:      ";
      if (hitStopLoss) {
         std::cout << "SELL ❗ (Below stop-loss)\n";
      } else if (shouldSell) {
         std::cout << "SELL 📉 (Drop >10% from high)\n";
      } else if (shouldBuy) {
         std::cout << "BUY 🟢 (Near 52W low)\n";
      } else {
         std::cout << "HOLD 🔒\n";
      }
   }

   auto end = std::chrono::high_resolution_clock::now();
   auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

   {
      std::lock_guard<std::mutex> lock(logMutex);
      std::cout << "  [Latency]: " << latency << " μs\n";
   }
}

int main() {
   int listenSock = socket(AF_INET, SOCK_STREAM, 0);
   if (listenSock < 0) {
      perror("Socket creation failed");
      return 1;
   }
   sockaddr_in serverAddr{};
   serverAddr.sin_family = AF_INET;
   serverAddr.sin_port = htons(9000);
   serverAddr.sin_addr.s_addr = INADDR_ANY;

   if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
      perror("Bind failed");
      close(listenSock);
      return 1;
   }

   if (listen(listenSock, 1) < 0) {
      perror("Listen failed");
      close(listenSock);
      return 1;
   }
   std::cout << "Waiting for connection on port 9000...\n";
   sockaddr_in clientAddr{};
   socklen_t clientLen = sizeof(clientAddr);
   // this listens and creates a socket clientSock and listSock keeps listening for
   // other connections
   int clientSock = accept(listenSock, (sockaddr*)&clientAddr, &clientLen);
   if (clientSock < 0) {
      perror("Accept failed");
      close(listenSock);
      return 1;
   }
   std::cout << "Client connected from "
      << inet_ntoa(clientAddr.sin_addr) << ":"
      << ntohs(clientAddr.sin_port) << "\n";
   // === Read number of stocks ===
   uint32_t numStocks;
   ssize_t bytesRead = recv(clientSock, &numStocks, sizeof(numStocks), 0);
   if (bytesRead != sizeof(numStocks)) {
      std::cerr << "Failed to read number of stocks.\n";
      close(clientSock);
      close(listenSock);
      return 1;
   }
   std::cout << "Number of stocks: " << numStocks << "\n";
   // === Read all StockData entries ===
   size_t totalBytes = numStocks * sizeof(StockData);
   char* buffer = new char[totalBytes];

   size_t totalReceived = 0;
   while (totalReceived < totalBytes) {
      ssize_t r = recv(clientSock, buffer + totalReceived, totalBytes - totalReceived, 0);
      if (r <= 0) {
         std::cerr << "Error receiving stock data.\n";
         delete[] buffer;
         close(clientSock);
         close(listenSock);
         return 1;
      }
      totalReceived += r;
   }
   StockData* stocks = reinterpret_cast<StockData*>(buffer);

   std::vector<std::thread> threads; //  stores threads for each ticker
   for (uint32_t i = 0; i < numStocks; ++i) {
      std::string ticker(stocks[i].ticker);
      auto it = portfolio.find(ticker);
      if (it != portfolio.end()) {
         threads.emplace_back(analyzeStock, stocks[i], it->second);// emplace
                                                                   // directly
                                                                   // creates thread
                                                                   // inside vector
                                                                   // better than pb
      } else {
         std::lock_guard<std::mutex> lock(logMutex);
         std::cout << "\nStock: " << ticker << " is not in your portfolio — Ignored.\n";
      }
   }
   for (auto& t : threads) {
      t.join();
   }
   delete[] buffer;
   close(clientSock);
   close(listenSock);
   return 0;
}

// dont forget to compile with -pthread: g++ -std=c++17 -pthread -o receiver stock_receiver.cpp
