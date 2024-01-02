#include <boost/test/unit_test.hpp>

#include <boost/asio/use_awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/beast/websocket.hpp>

#include <async_mqtt5.hpp>

namespace boost::beast::websocket {

template <typename TeardownHandler>
void async_teardown(
	boost::beast::role_type /* role */,
	asio::ssl::stream<asio::ip::tcp::socket>& stream,
	TeardownHandler&& handler
) {
	return stream.async_shutdown(std::forward<TeardownHandler>(handler));
}

} // end namespace boost::beast::websocket

constexpr char mireo_ca[] =
	"-----BEGIN CERTIFICATE-----\n"
	"MIIDUTCCAjmgAwIBAgIUAzV59EhZA5MXluHNqRi9cBP0x9swDQYJKoZIhvcNAQEL\n"
	"BQAwGDEWMBQGA1UEAwwNTWlyZW8gUm9vdCBDQTAeFw0yMjA0MDcxMzM1MjlaFw0z\n"
	"MjA0MDQxMzM1MjlaMBgxFjAUBgNVBAMMDU1pcmVvIFJvb3QgQ0EwggEiMA0GCSqG\n"
	"SIb3DQEBAQUAA4IBDwAwggEKAoIBAQCin/qsHpdxT3iW0SEHhAcTfESyQcfwGtJE\n"
	"jcRrGEj36X6eahyY4AF+4Mlz2vWFeW52ayGXpQKn/z4tChdN80txdY77YmEX7XE0\n"
	"HHZYY6toNq/+mNX9h2HvB0GW+8+E0YfNN/HloTxDo3RT8+IovY9OSXt44vY4YtQK\n"
	"JbvZIm2Q8Iuv3vfNR05uFa4HcNqFhELh10jss0xG/54Y2NvB6xdKOZ8LRQuIX+Fu\n"
	"QRzMiqRFQPUJzWxbKF5I/MFiKWmAG0QNPDnlb8XtPmFTFCWY9X96wOpQOczrxT2+\n"
	"+vnTxPA3aTAkz7M4yUuocZQqTlbdfdGOSAENXavewdMCyy5bQsSLAgMBAAGjgZIw\n"
	"gY8wHQYDVR0OBBYEFLdUGYfJRf9mbM/fTav9U2vFI+TRMFMGA1UdIwRMMEqAFLdU\n"
	"GYfJRf9mbM/fTav9U2vFI+TRoRykGjAYMRYwFAYDVQQDDA1NaXJlbyBSb290IENB\n"
	"ghQDNXn0SFkDkxeW4c2pGL1wE/TH2zAMBgNVHRMEBTADAQH/MAsGA1UdDwQEAwIB\n"
	"BjANBgkqhkiG9w0BAQsFAAOCAQEAHm5d4YUP8BYcks10UCdswLtxbMUN99fNbnYo\n"
	"RMxx4EapwhEZFSNbIZvf1INJd5Po+hH5jteBeFVP+4zKqrhg3I8pjdC4josHmrhS\n"
	"28OjOFWp6xNJC43BHnLpc84bH0+XIEBbk7YA6H3GjpsZ7aJkhj/JPjjNq7bmyYN7\n"
	"1I9RK4PtIrNtUFbSsHZCZhf8Amtl8PrpktITECjfqCq+8uOAqP4McTIQ1JKwYy6f\n"
	"O6iu0eybJCFhWYENTUQyPi1VtEwOpWNLzaXBYdj69Xg8wA/J9RZIoqXWvtHv4rPF\n"
	"HGudMEIVB3y2vVLmujvQCqYPZWwbgpy5mN3F4uBNuZhTIwWRFg==\n"
	"-----END CERTIFICATE-----\n"
;


namespace async_mqtt5 {

template <typename StreamBase>
struct tls_handshake_type<asio::ssl::stream<StreamBase>> {
	static constexpr auto client = asio::ssl::stream_base::client;
	static constexpr auto server = asio::ssl::stream_base::server;
};

template <typename StreamBase>
void assign_tls_sni(
	const authority_path& ap,
	asio::ssl::context& /* ctx */,
	asio::ssl::stream<StreamBase>& stream
) {
	SSL_set_tlsext_host_name(stream.native_handle(), ap.host.c_str());
}

} // end namespace async_mqtt5

BOOST_AUTO_TEST_SUITE(coroutine/*, *boost::unit_test::disabled()*/)

using namespace async_mqtt5;
namespace asio = boost::asio;

constexpr auto use_nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

template<typename StreamType, typename TlsContext>
asio::awaitable<void> sanity_check(mqtt_client<StreamType, TlsContext>& c) {
	auto [ec_0] = co_await c.template async_publish<qos_e::at_most_once>(
		"test/mqtt-test", "hello world with qos0!", retain_e::yes, publish_props {},
		use_nothrow_awaitable
	);
	BOOST_CHECK(!ec_0);

	auto [ec_1, puback_rc, puback_props] = co_await c.template async_publish<qos_e::at_least_once>(
		"test/mqtt-test", "hello world with qos1!",
		retain_e::yes, publish_props {},
		use_nothrow_awaitable
	);
	BOOST_CHECK(!ec_1);
	BOOST_CHECK(!puback_rc);

	auto [ec_2, pubcomp_rc, pubcomp_props] = co_await c.template async_publish<qos_e::exactly_once>(
		"test/mqtt-test", "hello world with qos2!",
		retain_e::yes, publish_props {},
		use_nothrow_awaitable
	);
	BOOST_CHECK(!ec_2);
	BOOST_CHECK(!pubcomp_rc);

	std::vector<subscribe_topic> topics;
	topics.push_back(subscribe_topic{
		"test/mqtt-test", {
			qos_e::exactly_once,
			subscribe_options::no_local_e::no,
			subscribe_options::retain_as_published_e::retain,
			subscribe_options::retain_handling_e::send
		}
	});
	
	auto [sub_ec, sub_codes, sub_props] = co_await c.async_subscribe(
		topics, subscribe_props {}, use_nothrow_awaitable
	);
	BOOST_CHECK(!sub_ec);
	BOOST_CHECK(!sub_codes[0]);
	auto [rec, topic, payload, publish_props] = co_await c.async_receive(use_nothrow_awaitable);

	auto [unsub_ec, unsub_codes, unsub_props] = co_await c.async_unsubscribe(
		std::vector<std::string>{"test/mqtt-test"}, unsubscribe_props {},
		use_nothrow_awaitable
	);
	BOOST_CHECK(!unsub_ec);
	BOOST_CHECK(!unsub_codes[0]);

	co_await c.async_disconnect(use_nothrow_awaitable);
	co_return;
}


BOOST_AUTO_TEST_CASE(tcp_client_check) {
	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("tcp-tester", "", "")
		.brokers("emqtt.mireo.local", 1883)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.run();

	asio::steady_timer timer(ioc);
	timer.expires_after(std::chrono::seconds(5));

	timer.async_wait(
		[&](boost::system::error_code ec) {
			BOOST_CHECK_MESSAGE(ec, "Failed to receive all the expected replies!");
			c.cancel();
			ioc.stop();
		}
	);

	co_spawn(ioc, 
		[&]() -> asio::awaitable<void> {
			co_await sanity_check(c);
			timer.cancel();
		},
		asio::detached
	);

	ioc.run();
}

BOOST_AUTO_TEST_CASE(websocket_tcp_client_check) {
	asio::io_context ioc;

	using stream_type = boost::beast::websocket::stream<
		asio::ip::tcp::socket
	>;

	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("websocket-tcp-tester", "", "")
		.brokers("emqtt.mireo.local/mqtt", 8083)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.run();

	asio::steady_timer timer(ioc);
	timer.expires_after(std::chrono::seconds(5));

	timer.async_wait(
		[&](boost::system::error_code ec) {
			BOOST_CHECK_MESSAGE(ec, "Failed to receive all the expected replies!");
			c.cancel();
			ioc.stop();
		}
	);

	co_spawn(ioc,
		[&]() -> asio::awaitable<void> {
			co_await sanity_check(c);
			timer.cancel();
		},
		asio::detached
	);

	ioc.run();
}


BOOST_AUTO_TEST_CASE(openssl_tls_client_check) {
	asio::io_context ioc;

	using stream_type = asio::ssl::stream<asio::ip::tcp::socket>;
	asio::ssl::context tls_context(asio::ssl::context::tls_client);

	error_code ec;
	tls_context.add_certificate_authority(asio::buffer(mireo_ca), ec);
	tls_context.set_verify_mode(asio::ssl::verify_peer);

	using client_type = mqtt_client<stream_type, decltype(tls_context)>;
	client_type c(ioc, "", std::move(tls_context));

	c.credentials("openssl-tls-tester", "", "")
		.brokers("emqtt.mireo.local", 8883)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.run();

	asio::steady_timer timer(ioc);
	timer.expires_after(std::chrono::seconds(5));

	timer.async_wait(
		[&](boost::system::error_code ec) {
			BOOST_CHECK_MESSAGE(ec, "Failed to receive all the expected replies!");
			c.cancel();
			ioc.stop();
		}
	);

	co_spawn(ioc,
		 [&]() -> asio::awaitable<void> {
			 co_await sanity_check(c);
			 timer.cancel();
		 },
		 asio::detached
	);

	ioc.run();
}

BOOST_AUTO_TEST_CASE(websocket_tls_client_check) {
	asio::io_context ioc;

	using stream_type = boost::beast::websocket::stream<
		asio::ssl::stream<asio::ip::tcp::socket>
	>;

	error_code ec;
	asio::ssl::context tls_context(asio::ssl::context::tls_client);
	tls_context.add_certificate_authority(asio::buffer(mireo_ca), ec);
	tls_context.set_verify_mode(asio::ssl::verify_peer);

	using client_type = mqtt_client<stream_type, decltype(tls_context)>;
	client_type c(ioc, "", std::move(tls_context));

	c.credentials("websocket-tls-tester", "", "")
		.brokers("emqtt.mireo.local/mqtt", 8884)
		.will({ "test/mqtt-test", "Client disconnected!", qos_e::at_least_once })
		.run();

	asio::steady_timer timer(ioc);
	timer.expires_after(std::chrono::seconds(5));

	timer.async_wait(
		[&](boost::system::error_code ec) {
			BOOST_CHECK_MESSAGE(ec, "Failed to receive all the expected replies!");
			c.cancel();
			ioc.stop();
		}
	);

	co_spawn(ioc,
		[&]() -> asio::awaitable<void> {
			co_await sanity_check(c);
			timer.cancel();
		},
		asio::detached
	);

	ioc.run();
}

BOOST_AUTO_TEST_SUITE_END()

#endif
