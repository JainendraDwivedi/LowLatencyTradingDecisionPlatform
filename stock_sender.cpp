#include<iostream>
#include<cstring>
#include<arpa/inet.h>
#include<unistd.h>
#include <sys/socket.h> // for socket(), sendto()
#include <vector>

struct StockData{
   char ticker[8]; // fixed 8 bytes ticker to send over udp
   float price;
   float high_52wk;
   float low_52wk;
};

int main(){
   int sock = socket(AF_INET, SOCK_STREAM,0); // tcp packet, domain IPv4
   if (sock < 0) {
    perror("socket failed");
    exit(1);
   }
   sockaddr_in serverAddr{};
   serverAddr.sin_family = AF_INET;
   serverAddr.sin_port = htons(9000);
   serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // using loopback address for
                                                        // now

   // ==== Input from user ====
   uint32_t numStocks;
   std::cout << "Enter number of stocks: ";
   std::cin >> numStocks;
   std::vector<StockData> stocks(numStocks);
   for (uint32_t i = 0; i < numStocks; ++i) {
      std::cout << "Stock " << i + 1 << ":\n";
      std::string ticker;
      std::cout << "  Ticker (max 7 chars): ";
      std::cin >> ticker;
      std::cout << "  Current price: ";
      std::cin >> stocks[i].price;
      std::cout << "  52-week high: ";
      std::cin >> stocks[i].high_52wk;
      std::cout << "  52-week low: ";
      std::cin >> stocks[i].low_52wk;
      std::memset(stocks[i].ticker, 0, sizeof(stocks[i].ticker)); // clear padding
      std::strncpy(stocks[i].ticker, ticker.c_str(), sizeof(stocks[i].ticker) - 1); // copying
                                                                                    // safely
      
   }
   size_t packetSize = sizeof(uint32_t)+numStocks*sizeof(StockData);
   char* packet = new char [packetSize];

   // packet structure ->
   // -------------------------------------------------------------
   // | numStocks (4B)   | stock[0] struct   | stock[1] ...       |
   // -------------------------------------------------------------

   std::memcpy(packet, &numStocks, sizeof(uint32_t)); // numStocks in starting of
                                                      // packet
   std::memcpy(packet + sizeof(uint32_t), stocks.data(), numStocks * sizeof(StockData));

   // We are using TCP packet
   if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
      perror("Connect failed");
      close(sock);
      return 1;
   }

   ssize_t sent = send(sock, packet, packetSize, 0);

   if (sent < 0) {
      perror("Send failed");
   }
   else{
      std::cout << "Packet sent (" << sent << " bytes)\n";
   }
   close(sock);
   delete[] packet;
   return 0;
}
