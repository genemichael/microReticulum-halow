// Reticulum TCP client interface on lwIP sockets, no Arduino layer.
// HDLC-style framing as in Python RNS TCPClientInterface and this firmware's TCPInterface.h.
// Client mode only, one peer, dotted IPv4 address only.
#pragma once
#include <microReticulum/Interface.h>
#include <microReticulum/Bytes.h>
#include <microReticulum/Log.h>
#include <string>
#include <stdint.h>

class TcpClientInterface : public RNS::InterfaceImpl {
public:
	TcpClientInterface(const char* host, uint16_t port, const char* name = "TcpClientInterface");
	virtual ~TcpClientInterface();
	virtual bool start();
	virtual void stop();
	virtual void loop();
	bool connected() const { return _sock >= 0; }
	uint32_t rx_frames() const { return _rx_frames; }
	uint32_t tx_frames() const { return _tx_frames; }
	virtual inline std::string toString() const { return "TcpClientInterface[" + _host + ":" + std::to_string(_port) + "]"; }
protected:
	virtual bool send_outgoing(const RNS::Bytes& data);
private:
	static const uint16_t HW_MTU = 1064;
	bool try_connect();
	void drop(const char* why);
	bool write_all(const uint8_t* buf, size_t len);
	void deframe(uint8_t byte);
	std::string _host;
	uint16_t _port;
	int _sock = -1;
	bool _started = false;
	uint64_t _next_connect_ms = 0;
	uint32_t _backoff_ms = 5000;
	uint64_t _last_tx_ms = 0;
	uint32_t _rx_frames = 0;
	uint32_t _tx_frames = 0;
	bool _in_frame = false;
	bool _escape = false;
	bool _truncated = false;
	uint16_t _rxlen = 0;
	uint8_t _rxbuf[HW_MTU];
	uint8_t _frame_buf[HW_MTU * 2 + 4];
};
