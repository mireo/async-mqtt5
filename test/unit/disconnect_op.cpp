#include <boost/test/unit_test.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <async_mqtt5/mqtt_client.hpp>

#include <async_mqtt5/impl/disconnect_op.hpp>

#include <async_mqtt5/detail/internal_types.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(disconnect_op/*, *boost::unit_test::disabled()*/)

void run_malformed_props_test(const disconnect_props& dprops) {
	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());

	auto handler = [&handlers_called](error_code ec) {
		++handlers_called;
		BOOST_TEST(ec == client::error::malformed_packet);
	};

	detail::disconnect_ctx ctx;
	ctx.props = dprops;

	detail::disconnect_op<
		client_service_type, detail::disconnect_ctx
	> { svc_ptr, std::move(ctx), std::move(handler) }
	.perform();

	ioc.run_for(std::chrono::milliseconds(500));
	BOOST_TEST(handlers_called == expected_handlers_called);
}


BOOST_AUTO_TEST_CASE(malformed_reason_string) {
	disconnect_props dprops;
	dprops[prop::reason_string] = std::string { 0x01 };

	run_malformed_props_test(dprops);
}

BOOST_AUTO_TEST_CASE(malformed_user_property_key) {
	disconnect_props dprops;
	dprops[prop::user_property].emplace_back(std::string { 0x01 }, "value");

	run_malformed_props_test(dprops);
}

BOOST_AUTO_TEST_CASE(malformed_user_property_value) {
	disconnect_props dprops;
	dprops[prop::user_property].emplace_back("key", std::string { 0x01 });

	run_malformed_props_test(dprops);
}

BOOST_AUTO_TEST_CASE(omit_props) {
	using test::after;
	using namespace std::chrono;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	connack_props co_props;
	co_props[prop::maximum_packet_size] = 20;

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(
		false, reason_codes::success.value(), co_props
	);

	disconnect_props props;
	props[prop::reason_string] = std::string(50, 'a');
	auto disconnect = encoders::encode_disconnect(
		reason_codes::normal_disconnection.value(), props
	);
	auto disconnect_no_props = encoders::encode_disconnect(
		reason_codes::normal_disconnection.value(), disconnect_props {}
	);

	error_code success {};

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(disconnect_no_props)
			.complete_with(success, after(0ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor);
	c.brokers("127.0.0.1")
		.async_run(asio::detached);

	asio::steady_timer timer(c.get_executor());
	timer.expires_after(50ms);
	timer.async_wait([&](error_code) {
		c.async_disconnect(
			disconnect_rc_e::normal_disconnection, props,
			[&](error_code ec) {
				handlers_called++;
				BOOST_TEST(!ec);
			}
		);
	});

	ioc.run_for(2s);
	BOOST_TEST(handlers_called == expected_handlers_called);
	BOOST_TEST(broker.received_all_expected());
}

BOOST_AUTO_TEST_SUITE_END()
