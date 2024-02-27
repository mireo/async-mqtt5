Async.MQTT5: A C++17 MQTT client based on Boost.Asio
===============================

Branch | Windows/Linux Build | Coverage | Documentation |
-------|---------------------|----------|---------------|
[`master`](https://github.com/mireo/async-mqtt5/tree/master) | [![build status](https://github.com/mireo/async-mqtt5/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/mireo/async-mqtt5/actions/workflows/ci.yml) | [![codecov](https://codecov.io/gh/mireo/async-mqtt5/branch/master/graph/badge.svg)](https://codecov.io/gh/mireo/async-mqtt5/branch/master) | [Documentation](https://spacetime.mireo.com/async-mqtt5/)

Async.MQTT5 is a professional, industrial-grade C++17 client built on [Boost.Asio](https://www.boost.org/doc/libs/1_82_0/doc/html/boost_asio.html). This Client is designed for publishing or receiving messages from an MQTT 5.0 compatible Broker. Async.MQTT5 represents a comprehensive implementation of the MQTT 5.0 protocol standard, offering full support for publishing or receiving messages with QoS 0, 1, and 2. 

Our clear intention is to include the Async.MQTT5 library into [Boost](https://www.boost.org/). We are actively working on it.

Motivation
---------
 The [MQTT](https://mqtt.org/) protocol is widely utilised for communication in various real-world scenarios, primarily serving as a reliable communication protocol for data transfer to and from IoT devices. While the MQTT protocol itself is relatively straightforward, integrating it into an application can be complex, especially due to the challenging implementation of message retransmission after a disconnect/reconnect sequence.

The aim of Async.MQTT5 is to provide a very simple asynchronous C++ interface for application developers. The internal Client's implementation manages network and MQTT protocol details. Notably, the Client does not expose connect functions (nor asynchronous connect functions); instead, network connectivity, MQTT handshake, and message retransmission are automatically handled within the Client.

The Async.MQTT5 interface aligns seamlessly with the Boost.Asio asynchronous model. The Client's asynchronous functions are compatible with all completion tokens supported by Boost.Asio. 

Features
---------
Async.MQTT5 is a library designed with the core belief that users should focus solely on their application logic, not the network complexities.
The library attempts to embody this belief with a range of key features designed to elevate the development experience: 

- **Complete TCP, TLS/SSL, and WebSocket support**
- **User-focused simplicity**: Providing an interface that is as simple as possible without compromising functionality.
- **Prioritised efficiency**: Utilising network and memory resources as efficiently as possible.
- **Minimal memory footprint**: Ensuring optimal performance in resource-constrained environments typical of IoT devices.
- **Automatic reconnect**: Automatically attempt to re-establish a connection in the event of a disconnection.
- **Fully Boost.Asio compliant**: The interfaces and implementation strategies are built upon the foundations of Boost.Asio. Boost.Asio and Boost.Beast users will have no issues understanding and integrating Async.MQTT5. Furthermore, Async.MQTT5 integrates well with any other library within the Boost.Asio ecosystem.
- **Custom allocators**: Support for custom allocators allows extra flexibility and control over the memory resources. Async.MQTT5 will use allocators associated with handlers from asynchronous functions to create instances of objects needed in the library implementation.
- **Per-Operation Cancellation**: All asynchronous operations support individual, targeted cancellation as per Asio's [Per-Operation Cancellation](https://www.boost.org/doc/libs/1_82_0/doc/html/boost_asio/overview/core/cancellation.html).
- **Completion Token**: All asynchronous functions support CompletionToken, allowing for versatile usage with callbacks, coroutines, futures, and more.
- **Full implementation of MQTT 5.0 specification**
- **Support for QoS 0, QoS 1, and QoS 2**
- **Custom authentication**: Async.MQTT5 defines an interface for your own custom authenticators to perform Enhanced Authentication.
- **High availability**: Async.MQTT5 supports listing multiple Brokers within the same cluster to which the Client can connect.
In the event of a connection failure with one Broker, the Client switches to the next in the list.
- **Offline buffering**: While offline, it automatically buffers all the packets to send when the connection is re-established. 

Using the library
---------

1. Download [Boost](https://www.boost.org/users/download/), and add it to your include path.
2. If you use SSL, download [OpenSSL](https://www.openssl.org/), link the library and add it to your include path.
3. Additionally, you can add Async.MQTT5's `include` folder to your include path.

You can compile the example below with the following command line on Linux:

    $ clang++ -std=c++17 <source-cpp-file> -o example -I<path-to-boost> -Iinclude -pthread

Usage and API
---------
The following example illustrates a simple scenario of configuring a Client and publishing a
"Hello World!" Application Message with `QoS` 0. 

```cpp
#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

int main() {
	boost::asio::io_context ioc;

	using client_type = async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket>;
	client_type c(ioc);

	c.credentials("<your-client-id>", "<client-username>", "<client-pwd>")
		.brokers("<your-mqtt-broker>", 1883)
		.async_run(boost::asio::detached);

	c.async_publish<async_mqtt5::qos_e::at_most_once>(
		"<topic>", "Hello world!",
		async_mqtt5::retain_e::no, async_mqtt5::publish_props {},
		[&c](async_mqtt5::error_code ec) {
			std::cout << ec.message() << std::endl;
			c.async_disconnect(boost::asio::detached); // disconnect and close the client
		}
	);
	
	ioc.run();
}
```
To see more examples, visit [Examples](https://spacetime.mireo.com/async-mqtt5/async_mqtt5/examples.html).

When to use
---------
 Async.MQTT5 might be suitable for you if any of the following statements is true:

- Your application uses Boost.Asio and requires integrating a MQTT Client.
- You require asynchronous access to an MQTT Broker.
- You are developing a higher-level component that requires a connection to an MQTT Broker.
- You require a dependable and resilient MQTT Client to manage all network-related issues automatically.

It may not be suitable for you if:
- You solely require synchronous access to an MQTT Broker.
- The MQTT Broker you connect to does not support the MQTT 5 version.


Requirements
---------
Async.MQTT5 is a header-only library. To use Async.MQTT5 it requires the following: 
- **C++17 capable compiler**
- **Boost 1.82 or later**. In addition to Asio, we use other header-only libraries such as Beast, Spirit, and more. 
- **OpenSSL**. Only if you require an SSL connection by using [boost::asio::ssl::stream](https://www.boost.org/doc/libs/1_82_0/doc/html/boost_asio/reference/ssl__stream.html).

Async.MQTT5 has been tested with the following compilers: 
- clang 12.0, 13.0, 14.0, 15.0 (Linux)
- GCC 10, 11, 12 (Linux)
- MSVC 14.37 - Visual Studio 2022 (Windows)

Contributing
---------
When contributing to this repository, please first discuss the change you wish to make via issue, email, or any other method with the owners of this repository before making a change.

You may merge a Pull Request once you have the sign-off from other developers, or you may request the reviewer to merge it for you.

License
---------

Copyright (c) 2001-2024 Mireo, EU

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS 
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Credits
--------- 

Maintained and authored by [Mireo](https://www.mireo.com).

<p align="center">
<a href="https://www.mireo.com"><img height="200" alt="Mireo" src="https://www.mireo.com/img/assets/mireo-logo.svg"></img></a>
</p>
