#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include "yasio/yasio.hpp"

#include "yasio/ibstream.hpp"
#include "yasio/obstream.hpp"

using namespace yasio;
using namespace yasio::inet;

void yasioTest()
{
  yasio::inet::io_hostent endpoints[] = {{"www.ip138.com", 80}};

  yasio::obstream obstest;
  obstest.push24();
  obstest.write_i(3.141592654);
  obstest.write_i(1.17723f);
  obstest.write_u24(0x112233);
  obstest.write_u24(16777217); // uint24 value overflow test
  obstest.write_i24(259);
  obstest.write_i24(-16);
  obstest.pop24();

  yasio::ibstream_view ibs(obstest.data(), static_cast<int>(obstest.length()));
  ibs.seek(3, SEEK_CUR);
  auto r1 = ibs.read_i<double>();
  auto f1 = ibs.read_i<float>();
  auto v1 = ibs.read_u24(); // should be 0x112233(1122867)
  auto v2 = ibs.read_u24(); // should be 1
  auto v3 = ibs.read_i24(); // should be 259
  auto v4 = ibs.read_i24(); // should be -16

  std::cout << r1 << ", " << f1 << ", " << v1 << ", " << v2 << ", " << v3 << ", " << v4 << "\n";

  io_service service(endpoints, YASIO_ARRAYSIZE(endpoints));

  resolv_fn_t resolv = [&](std::vector<ip::endpoint>& endpoints, const char* hostname,
                           unsigned short port) {
    return service.builtin_resolv(endpoints, hostname, port);
  };
  service.set_option(YOPT_S_RESOLV_FN, &resolv);

  std::vector<transport_handle_t> transports;

  deadline_timer udpconn_delay(service);
  deadline_timer udp_heartbeat(service);
  int total_bytes_transferred = 0;

  int max_request_count = 10;

  service.start_service([&](event_ptr&& event) {
    switch (event->kind())
    {
      case YEK_PACKET: {
        auto packet = std::move(event->packet());
        total_bytes_transferred += static_cast<int>(packet.size());
        fwrite(packet.data(), packet.size(), 1, stdout);
        fflush(stdout);
        break;
      }
      case YEK_CONNECT_RESPONSE:
        if (event->status() == 0)
        {
          auto transport = event->transport();
          if (event->cindex() == 0)
          {
            obstream obs;
            obs.write_bytes("GET /index.htm HTTP/1.1\r\n");

            obs.write_bytes("Host: www.ip138.com\r\n");

            obs.write_bytes("User-Agent: Mozilla/5.0 (Windows NT 10.0; "
                            "WOW64) AppleWebKit/537.36 (KHTML, like Gecko) "
                            "Chrome/51.0.2704.106 Safari/537.36\r\n");
            obs.write_bytes("Accept: */*;q=0.8\r\n");
            obs.write_bytes("Connection: Close\r\n\r\n");

            service.write(transport, std::move(obs.buffer()));
          }

          transports.push_back(transport);
        }
        break;
      case YEK_CONNECTION_LOST:
        printf("The connection is lost, %d bytes transferred\n", total_bytes_transferred);

        total_bytes_transferred = 0;
        if (--max_request_count > 0)
        {
          udpconn_delay.expires_from_now(std::chrono::seconds(1));
          udpconn_delay.async_wait_once([&]() {
            service.open(0);
          });
        }
        else
          service.stop_service();
        break;
    }
  });

  /*
  ** If after 5 seconds no data interaction at application layer,
  ** send a heartbeat per 10 seconds when no response, try 2 times
  ** if no response, then he connection will shutdown by driver.
  ** At windows will close with error: 10054
  */
  service.set_option(YOPT_S_TCP_KEEPALIVE, 5, 10, 2);

  std::this_thread::sleep_for(std::chrono::seconds(1));
  service.open(0); // open http client

  time_t duration = 0;
  while (service.is_running())
  {
    service.dispatch();
    if (duration >= 6000000)
    {
      for (auto transport : transports)
        service.close(transport);
      break;
    }
    duration += 50;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

int main(int, char**)
{
  yasioTest();

  return 0;
}
