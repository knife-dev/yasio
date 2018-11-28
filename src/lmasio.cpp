#include "ibinarystream.h"
#include "obinarystream.h"
#include "masio.h"
#include "lmasio.h"

void lua_open_masio(lua_State *L) {
  sol::state_view sol2(L);

#if 0
    auto t = sol2.create_named_table("simple_timer");
    // the simple timer implementation is here: https://github.com/halx99/x-studio365/blob/master/cocos2d-x-patch/cocos/editor-support/cocostudio/ext/SimpleTimer.h
    t.set_function("delay", simple_timer::delay);
    t.set_function("loop", simple_timer::loop);
    t.set_function("kill", simple_timer::kill);
#endif

  sol2.new_usertype<purelib::inet::io_hostent>(
      "io_hostent", 
      "address_", &purelib::inet::io_hostent::address_, 
      "port_", &purelib::inet::io_hostent::port_);

  sol2.new_usertype<purelib::inet::io_event>(
      "io_event", "channel_index", &purelib::inet::io_event::channel_index,
      "type", &purelib::inet::io_event::type, 
      "error_code",&purelib::inet::io_event::error_code, 
      "transport",&purelib::inet::io_event::transport, 
      "packet", [](purelib::inet::io_event *event) {
        return std::string(event->packet().data(), event->packet().size());
      });

  sol2.new_usertype<purelib::inet::async_socket_io>(
      "async_socket_io", "start_service", &purelib::inet::async_socket_io::start_service,
      "stop_service", &purelib::inet::async_socket_io::stop_service, 
      "set_callbacks",&purelib::inet::async_socket_io::set_callbacks,
      "dispatch_events", &purelib::inet::async_socket_io::dispatch_events,
      "open", &purelib::inet::async_socket_io::open,
      "write", [](purelib::inet::async_socket_io *aio,
         purelib::inet::transport_ptr transport, const char *s,
         size_t n) { aio->write(transport, std::vector<char>(s, s + n)); });

  // ##-- obinarystream
  sol2.new_usertype<obinarystream>(
      "obstream", 
      "push32", &obinarystream::push32,
      "pop32", static_cast<void (obinarystream ::*)()>(&obinarystream::pop32),
      "write_i8", &obinarystream::write_i<int8_t>,
      "write_i16", &obinarystream::write_i<int16_t>,
      "write_i32", &obinarystream::write_i<int32_t>,
      "write_i64", &obinarystream::write_i<int64_t>,
      "write_bytes", static_cast<size_t(obinarystream::*)(const std::string&)>(&obinarystream::write_bytes),
      "length", &obinarystream::length
      );
}