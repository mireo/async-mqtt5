#include <boost/test/unit_test.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <async_mqtt5.hpp>
#include <test_common/test_service.hpp>

using namespace async_mqtt5;

constexpr auto use_nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

BOOST_AUTO_TEST_SUITE(session/*, *boost::unit_test::disabled()*/)

BOOST_AUTO_TEST_CASE(session_state_session_present) {
	detail::session_state session_state;
	BOOST_CHECK_EQUAL(session_state.session_present(), false);
	session_state.session_present(true);
	BOOST_CHECK_EQUAL(session_state.session_present(), true);
	session_state.session_present(false);
	BOOST_CHECK_EQUAL(session_state.session_present(), false);
}

BOOST_AUTO_TEST_CASE(session_expired_in_channel) {
	asio::io_context ioc;

	using stream_type = asio::ip::tcp::socket;
	using client_type = mqtt_client<stream_type>;
	client_type c(ioc, "");

	c.credentials("tester", "", "")
		.brokers("mqtt.mireo.local", 1883)
		.run();

	co_spawn(ioc,
		[&]() -> asio::awaitable<void> {
			auto [ec, topic, payload, props] = co_await c.async_receive(use_nothrow_awaitable);
			BOOST_CHECK(ec == client::error::session_expired);
			BOOST_CHECK_EQUAL(topic, std::string {});
			BOOST_CHECK_EQUAL(payload, std::string {});
			c.cancel();
			co_return;
		},
		asio::detached
	);

	ioc.run();
}

BOOST_AUTO_TEST_CASE(clear_waiting_on_pubrel) {
	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());

	decoders::publish_message pub_msg = std::make_tuple(
		"topic", 1, 0b0100, publish_props {}, "payload"
	);

	detail::publish_rec_op<client_service_type> { svc_ptr }.perform(pub_msg);

	// let publish_rec_op reach wait_on_pubrel stage
	asio::steady_timer timer(ioc.get_executor());
	timer.expires_after(std::chrono::milliseconds(50));
	timer.async_wait([&svc_ptr, &handlers_called](error_code) {
		BOOST_CHECK_EQUAL(svc_ptr.use_count(), 2);
		svc_ptr->update_session_state(); // session_present = false
		// publish_rec_op should complete
		BOOST_CHECK_EQUAL(svc_ptr.use_count(), 1);

		svc_ptr->async_channel_receive(
		[&svc_ptr, &handlers_called](error_code ec, std::string topic, std::string payload, publish_props props) {
				handlers_called++;
				BOOST_CHECK(ec == client::error::session_expired);
				BOOST_CHECK_EQUAL(topic, std::string {});
				BOOST_CHECK_EQUAL(payload, std::string {});
				svc_ptr->cancel();
		});
	});

	ioc.run();
	BOOST_CHECK_EQUAL(
		handlers_called, expected_handlers_called
	);
}


BOOST_AUTO_TEST_SUITE_END();
