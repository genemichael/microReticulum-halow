#include "TcpClientInterface.h"
#include <microReticulum/Utilities/OS.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define HDLC_FLAG     0x7E
#define HDLC_ESC      0x7D
#define HDLC_ESC_MASK 0x20
#define TCP_CONNECT_TIMEOUT_S 5
#define TCP_RECONNECT_MAX_MS  60000
#define TCP_KEEPALIVE_MS      30000

using namespace RNS;

static uint64_t now_ms() { return Utilities::OS::ltime(); }

TcpClientInterface::TcpClientInterface(const char* host, uint16_t port, const char* name)
	: InterfaceImpl(name), _host(host), _port(port) {
	_IN = true;
	_OUT = true;
	_HW_MTU = HW_MTU;
	_FIXED_MTU = true;
	_bitrate = 1000000;
	_online = false;
}

TcpClientInterface::~TcpClientInterface() { stop(); }

bool TcpClientInterface::start() {
	_started = true;
	_next_connect_ms = 0;
	return true;
}

void TcpClientInterface::stop() {
	_started = false;
	drop("stopped");
}

void TcpClientInterface::drop(const char* why) {
	if (_sock >= 0) {
		close(_sock);
		_sock = -1;
		INFOF("TcpClientInterface: link to %s:%u closed (%s)", _host.c_str(), _port, why);
	}
	_online = false;
	_in_frame = false;
	_escape = false;
	_rxlen = 0;
}

bool TcpClientInterface::try_connect() {
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) { ERRORF("TcpClientInterface: socket() failed, errno %d", errno); return false; }
	struct timeval tv;
	tv.tv_sec = TCP_CONNECT_TIMEOUT_S;
	tv.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(_port);
	if (inet_pton(AF_INET, _host.c_str(), &addr.sin_addr) != 1) {
		ERRORF("TcpClientInterface: bad address %s (dotted IPv4 only)", _host.c_str());
		close(s);
		return false;
	}
	if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
		INFOF("TcpClientInterface: connect to %s:%u failed, errno %d", _host.c_str(), _port, errno);
		close(s);
		return false;
	}
	int one = 1;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
	_sock = s;
	_online = true;
	_last_tx_ms = now_ms();
	INFOF("TcpClientInterface: connected to %s:%u", _host.c_str(), _port);
	return true;
}

void TcpClientInterface::loop() {
	if (!_started) return;
	uint64_t now = now_ms();
	if (_sock < 0) {
		if (now < _next_connect_ms) return;
		if (try_connect()) {
			_backoff_ms = 5000;
		} else {
			_next_connect_ms = now_ms() + _backoff_ms;
			_backoff_ms = (_backoff_ms * 2 > TCP_RECONNECT_MAX_MS) ? TCP_RECONNECT_MAX_MS : _backoff_ms * 2;
		}
		return;
	}
	uint8_t buf[256];
	for (int rounds = 0; rounds < 16 && _sock >= 0; rounds++) {
		ssize_t n = recv(_sock, buf, sizeof(buf), 0);
		if (n > 0) {
			for (ssize_t i = 0; i < n; i++) deframe(buf[i]);
		} else if (n == 0) {
			drop("peer closed");
		} else {
			if (errno != EAGAIN && errno != EWOULDBLOCK) drop("recv error");
			break;
		}
	}
	// Empty frame as keepalive, as this firmware's TCPInterface does.
	if (_sock >= 0 && now - _last_tx_ms >= TCP_KEEPALIVE_MS) {
		const uint8_t ka[2] = { HDLC_FLAG, HDLC_FLAG };
		if (!write_all(ka, sizeof(ka))) drop("keepalive write failed");
		_last_tx_ms = now;
	}
}

void TcpClientInterface::deframe(uint8_t byte) {
	if (byte == HDLC_FLAG) {
		if (_in_frame && _rxlen > 0) {
			if (_truncated) {
				WARNINGF("TcpClientInterface: dropped oversized frame (>%d bytes)", HW_MTU);
			} else {
				Bytes data(_rxbuf, _rxlen);
				_rx_frames++;
				try { InterfaceImpl::handle_incoming(data); }
				catch (const std::exception& e) { ERRORF("TcpClientInterface: handle_incoming: %s", e.what()); }
			}
		}
		_in_frame = true;
		_escape = false;
		_truncated = false;
		_rxlen = 0;
	} else if (_in_frame) {
		if (_escape) {
			byte ^= HDLC_ESC_MASK;
			_escape = false;
		} else if (byte == HDLC_ESC) {
			_escape = true;
			return;
		}
		if (_rxlen < HW_MTU) _rxbuf[_rxlen++] = byte; else _truncated = true;
	}
}

// The socket is non-blocking for reads; a full send buffer is retried briefly, then the link is dropped.
bool TcpClientInterface::write_all(const uint8_t* buf, size_t len) {
	size_t off = 0;
	int stalls = 0;
	while (off < len) {
		ssize_t n = send(_sock, buf + off, len - off, 0);
		if (n > 0) { off += (size_t)n; stalls = 0; continue; }
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && stalls++ < 200) {
			Utilities::OS::sleep(0.01);
			continue;
		}
		return false;
	}
	return true;
}

bool TcpClientInterface::send_outgoing(const Bytes& data) {
	bool ok = false;
	try {
		if (_sock >= 0 && data.size() <= HW_MTU) {
			size_t flen = 0;
			_frame_buf[flen++] = HDLC_FLAG;
			for (size_t i = 0; i < data.size(); i++) {
				uint8_t b = data.data()[i];
				if (b == HDLC_FLAG || b == HDLC_ESC) {
					_frame_buf[flen++] = HDLC_ESC;
					_frame_buf[flen++] = b ^ HDLC_ESC_MASK;
				} else {
					_frame_buf[flen++] = b;
				}
			}
			_frame_buf[flen++] = HDLC_FLAG;
			ok = write_all(_frame_buf, flen);
			if (ok) { _tx_frames++; _last_tx_ms = now_ms(); }
			else drop("write failed");
		}
		InterfaceImpl::handle_outgoing(data);
	}
	catch (const std::exception& e) {
		ERRORF("TcpClientInterface: send_outgoing: %s", e.what());
		ok = false;
	}
	return ok;
}
