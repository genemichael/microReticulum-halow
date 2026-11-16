// microReticulum transport node beside the HaLow mesh example, with one Reticulum TCP client
// interface to a Reticulum TCP server reachable over the mesh. No filesystem: the node has a new identity at each restart.
#include "TcpClientInterface.h"
#include <microStore/FileSystem.h>
#include <microStore/Adapters/NoopFileSystem.h>
#include <microReticulum.h>
#include <RNG.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include <time.h>
#include <stdio.h>

#include "sdkconfig.h"
#define RNS_PI_HOST          CONFIG_RNS_TCP_HOST
#define RNS_PI_PORT          CONFIG_RNS_TCP_PORT
#define RNS_ANNOUNCE_EVERY_S CONFIG_RNS_ANNOUNCE_INTERVAL_S
#define RNS_STATUS_EVERY_S   10

static RNS::Reticulum reticulum({RNS::Type::NONE});
static RNS::Interface tcp_interface(RNS::Type::NONE);
static RNS::Identity identity({RNS::Type::NONE});
static RNS::Destination destination({RNS::Type::NONE});

static bool mesh_has_address() {
	esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
	if (netif == NULL || !esp_netif_is_netif_up(netif)) return false;
	esp_netif_ip_info_t ip;
	if (esp_netif_get_ip_info(netif, &ip) != ESP_OK) return false;
	return ip.ip.addr != 0;
}

static void rns_task(void*) {
	printf("RNS: waiting for the mesh interface to be up with an address\n");
	while (!mesh_has_address()) vTaskDelay(pdMS_TO_TICKS(500));
	printf("RNS: mesh interface up, starting Reticulum\n");

	TcpClientInterface* tcp = nullptr;
	try {
		RNS::loglevel(RNS::LOG_INFO);

		// The library refuses to start with no filesystem registered. Register its own
		// do-nothing adapter: nothing is stored.
		microStore::FileSystem filesystem{microStore::Adapters::NoopFileSystem()};
		RNS::Utilities::OS::register_filesystem(filesystem);

		tcp = new TcpClientInterface(RNS_PI_HOST, RNS_PI_PORT);
		tcp_interface = tcp;
		tcp_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
		RNS::Transport::register_interface(tcp_interface);

		reticulum = RNS::Reticulum();   // calls RNG.begin()

		// Without the Arduino layer the Crypto library finds no hardware random source
		// (RNG.cpp selects it only when ESP32 is defined). Feed the chip's hardware
		// generator in here, after RNG.begin() and before any key is made.
		uint8_t seed[64];
		esp_fill_random(seed, sizeof(seed));
		RNG.stir(seed, sizeof(seed), sizeof(seed) * 8);

		reticulum.transport_enabled(true);
		reticulum.start();
		tcp_interface.start();

		identity = RNS::Identity();
		destination = RNS::Destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "rc32", "bench");
		destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);
		printf("RNS: destination rc32.bench hash %s\n", destination.hash().toHex().c_str());
	}
	catch (const std::exception& e) {
		printf("RNS: setup failed: %s\n", e.what());
		vTaskDelete(NULL);
		return;
	}

	bool was_connected = false;
	uint64_t last_announce_ms = 0;
	uint64_t last_status_ms = 0;
	while (true) {
		try {
			reticulum.loop();

			// Keep stirring hardware randomness in while running.
			uint32_t r = esp_random();
			RNG.stir((const uint8_t*)&r, sizeof(r), 0);

			uint64_t now = RNS::Utilities::OS::ltime();
			bool connected = tcp->connected();
			bool due = connected && (!was_connected || now - last_announce_ms >= RNS_ANNOUNCE_EVERY_S * 1000ULL);
			if (due) {
				destination.announce(RNS::bytesFromString("RC32-68"));
				last_announce_ms = now;
				printf("RNS: announce sent, destination %s\n", destination.hash().toHex().c_str());
			}
			was_connected = connected;

			if (now - last_status_ms >= RNS_STATUS_EVERY_S * 1000ULL) {
				last_status_ms = now;
				printf("RNS: status tcp_connected=%d tx_frames=%u rx_frames=%u heap_free=%u heap_min=%u\n",
					(int)connected, (unsigned)tcp->tx_frames(), (unsigned)tcp->rx_frames(),
					(unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
					(unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
			}
		}
		catch (const std::exception& e) {
			printf("RNS: loop exception: %s\n", e.what());
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

extern "C" void rns_task_start(void) {
	xTaskCreate(rns_task, "rns", 24576, NULL, 4, NULL);
}

// ESP-IDF 5.4.2 has no nanosleep(); the library's non-Arduino OS::sleep() calls it.
extern "C" int nanosleep(const struct timespec* req, struct timespec* rem) {
	(void)rem;
	uint32_t ms = (uint32_t)(req->tv_sec * 1000 + req->tv_nsec / 1000000);
	vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1));
	return 0;
}
