# GG - C++ Systems Programming Toolkit

A collection of high-performance C++ libraries designed for real-time trading systems, built from the ground up with minimal dependencies.

## Libraries

| Library | Description | Type |
|---------|-------------|------|
| [observer](./observer) | Thread-affine event system (Observer pattern) | Header-only |
| [ggnet](./ggnet) | HTTP/1.1 + WebSocket client with epoll | Header-only |
| [gg-ws](./gg-ws) | Production WebSocket with memory pools and CPU affinity | Compiled |
| [terminal-gui](./terminal-gui) | Immediate-mode terminal dashboard | Header-only |

## Design Principles

- **Minimal dependencies** - Only OpenSSL where TLS is needed
- **Zero-allocation hot paths** - Pre-allocated buffers, memory pools
- **Linux-native** - Direct epoll, CPU affinity, no abstraction layers
- **Header-only where possible** - Easy integration

## observer

Thread-affine event system where callbacks execute on the subscriber's thread, not the emitter's. Features type-safe events via templates and RAII auto-unsubscribe.

```cpp
gg::EventBus bus;
auto sub = bus.subscribe<PriceUpdate>([](const PriceUpdate& e) {
    // Runs on THIS thread, not the emitter's
});
bus.emit(PriceUpdate{67234.50});
bus.poll();
```

## ggnet

Header-only networking library with HTTP/1.1 (keep-alive, connection warmup) and WebSocket (RFC 6455) on native Linux epoll. Includes a built-in zero-allocation JSON parser.

```cpp
ggnet::EpollLoop loop;
ggnet::WsClient ws(loop);
ws.onMessage([](std::string_view msg) { /* ... */ });
ws.connect("wss://stream.binance.com:9443/ws/btcusdt@trade");
loop.run();
```

## gg-ws

Production WebSocket library with memory pools for allocation-free operation, CPU affinity for core pinning, heartbeat management, and thread-safe message queues. Includes unit tests.

## terminal-gui

Immediate-mode terminal dashboard library optimized for 60fps. Dirty checking renders only changed cells, with zero allocations in the main loop and full UTF-8 support.

```cpp
tgui::init(tgui::Charset::Unicode);
tgui::begin_frame();
tgui::box_begin("CPU");
tgui::progress_bar(45, 100, 20);
tgui::box_end();
tgui::end_frame();
```

## Requirements

- Linux
- C++17
- OpenSSL (for ggnet and gg-ws)
- xmake (for gg-ws build)

## License

MIT
