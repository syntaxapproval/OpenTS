/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

/*
 * The web build's SocketClass. A page cannot send UDP, so the datagrams go to the page,
 * which carries them to the other players however it likes (a WebSocket relay, say) and
 * hands back what arrives. The page provides Module.opentsNet:
 *
 *   open(port)                    -> true when there is a network to join
 *   close()
 *   address()                     -> this machine's IPv4 address, network byte order
 *   broadcast()                   -> the broadcast address, network byte order
 *   send(ip, port, bytes)         a datagram; ip and port in network byte order
 *   receive()                     -> { ip, port, bytes } or null when nothing is waiting
 *
 * Addresses and ports pass through exactly as the game holds them (network byte order),
 * so the page can use them as keys without converting. Without Module.opentsNet the
 * socket never opens and the game reports that the network is unavailable.
 */

#include "always.h"

#include "netsocket.h"
#include "dbgprint.h"

#include <emscripten.h>

EM_JS(int, Web_Net_Open, (int port), {
	const net = Module.opentsNet;
	return net && net.open(port) ? 1 : 0;
});

EM_JS(void, Web_Net_Close, (void), {
	if (Module.opentsNet) Module.opentsNet.close();
});

EM_JS(uint32_t, Web_Net_Address, (void), {
	return Module.opentsNet ? Module.opentsNet.address() >>> 0 : 0;
});

EM_JS(uint32_t, Web_Net_Broadcast, (void), {
	return Module.opentsNet ? Module.opentsNet.broadcast() >>> 0 : 0;
});

EM_JS(int, Web_Net_Send, (uint32_t ip, int port, void const * buffer, int length), {
	if (!Module.opentsNet) return -1;
	Module.opentsNet.send(ip >>> 0, port, HEAPU8.slice(buffer, buffer + length));
	return length;
});

// Copies the next datagram into buffer and its sender into ip_out/port_out. Returns its
// length, -1 when nothing is waiting, or -2 when it was longer than the buffer (dropped,
// as a datagram socket drops one).
EM_JS(int, Web_Net_Receive, (void * buffer, int length, uint32_t * ip_out, uint16_t * port_out), {
	const packet = Module.opentsNet && Module.opentsNet.receive();
	if (!packet) return -1;
	if (packet.bytes.length > length) return -2;
	HEAPU8.set(packet.bytes, buffer);
	HEAPU32[ip_out >> 2] = packet.ip >>> 0;
	HEAPU16[port_out >> 1] = packet.port;
	return packet.bytes.length;
});

namespace {

class WebSocketClass : public SocketClass
{
	public:
		~WebSocketClass(void) override { Close(); }

		bool Open(unsigned short port) override
		{
			Close();
			Opened = Web_Net_Open(port) != 0;
			if (!Opened) {
				DebugString("The page offers no network to join\n");
			}
			return(Opened);
		}

		void Close(void) override
		{
			if (Opened) {
				Web_Net_Close();
				Opened = false;
			}
		}

		bool Is_Open(void) const override { return(Opened); }
		bool Set_Broadcast(bool) override { return(Opened); }
		bool Set_Buffer_Sizes(int, int) override { return(Opened); }
		void Clear_Error(void) override {}

		TransferResult Send_To(void const * buffer, int length, IPXAddressClass const & to) override
		{
			if (!Opened) return(TransferResult{SocketError::OTHER, 0});
			int const sent = Web_Net_Send(to.Get_IP(), to.Get_Port(), buffer, length);
			if (sent < 0) return(TransferResult{SocketError::OTHER, 0});
			return(TransferResult{SocketError::NONE, sent});
		}

		TransferResult Receive_From(void * buffer, int length, IPXAddressClass & from) override
		{
			if (!Opened) return(TransferResult{SocketError::OTHER, 0});
			uint32_t ip = 0;
			uint16_t port = 0;
			int const received = Web_Net_Receive(buffer, length, &ip, &port);
			if (received == -1) return(TransferResult{SocketError::WOULD_BLOCK, 0});
			if (received == -2) return(TransferResult{SocketError::MESSAGE_SIZE, 0});
			from.Set_Address(ip, port);
			return(TransferResult{SocketError::NONE, received});
		}

		bool Local_Interfaces(std::vector<InterfaceType> & interfaces) override
		{
			InterfaceType found = {};
			found.Address = Web_Net_Address();
			found.Broadcast = Web_Net_Broadcast();
			if (found.Address == 0) return(false);
			interfaces.push_back(found);
			return(true);
		}

	private:
		bool Opened = false;
};

}


std::unique_ptr<SocketClass> Socket_Create_Platform_Socket(void)
{
	return(std::make_unique<WebSocketClass>());
}
