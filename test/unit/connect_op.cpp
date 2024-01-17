#include <boost/test/unit_test.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5/detail/internal_types.hpp>

#include <async_mqtt5/impl/connect_op.hpp>

#include <async_mqtt5/types.hpp>

#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(connect_op/*, *boost::unit_test::disabled()*/)

struct shared_test_data {
	error_code success{};
	error_code fail = asio::error::not_connected;

	const std::string connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	const std::string connack = encoders::encode_connack(
		true, reason_codes::success.value(), {}
	);
};

using test::after;
using std::chrono_literals::operator ""ms;

void run_unit_test(
	test::msg_exchange broker_side,
	asio::any_completion_handler<void(error_code)> op_handler
) {
	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);
	test::test_stream stream(executor);

	detail::mqtt_ctx mqtt_ctx;
	authority_path ap;
	auto eps = asio::ip::tcp::resolver(executor).resolve("127.0.0.1", "");

	auto handler = [&handlers_called, h = std::move(op_handler)](error_code ec) mutable {
		handlers_called++;
		std::move(h)(ec);
	};

	detail::connect_op<test::test_stream>(
		stream, std::move(handler), mqtt_ctx
	).perform(eps, ap);

	ioc.run_for(std::chrono::seconds(1));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(successfully_connect, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(2ms))
			.reply_with(connack, after(4ms));

	auto handler = [&](error_code ec) {
		BOOST_CHECK(ec == success);
	};

	run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(connection_refused, shared_test_data) {
	auto refuse_connack = encoders::encode_connack(
		true, reason_codes::server_unavailable.value(), {}
	);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(2ms))
			.reply_with(refuse_connack, after(4ms));

	auto handler = [&](error_code ec) {
		BOOST_CHECK(ec == asio::error::connection_refused);
	};

	run_unit_test(std::move(broker_side), std::move(handler));
}


BOOST_FIXTURE_TEST_CASE(access_denied, shared_test_data) {
	auto denied_connack = encoders::encode_connack(
		true, reason_codes::bad_username_or_password.value(), {}
	);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(2ms))
			.reply_with(denied_connack, after(4ms));

	auto handler = [&](error_code ec) {
		BOOST_CHECK(ec == asio::error::access_denied);
	};

	run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(fail_to_send_connect, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
		.complete_with(fail, after(2ms));

	auto handler = [&](error_code ec) {
		BOOST_CHECK(ec == fail);
	};

	run_unit_test(std::move(broker_side), std::move(handler));
}


BOOST_FIXTURE_TEST_CASE(receive_wrong_packet, shared_test_data) {
	// packets
	auto unexpected_packet = encoders::encode_puback(1, uint8_t(0x00), {});

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(2ms))
			.reply_with(unexpected_packet, after(3ms));

	auto handler = [&](error_code ec) {
		BOOST_CHECK(ec == asio::error::try_again);
	};

	run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(malformed_connack_varlen, shared_test_data) {
	// packets
	auto malformed_connack = std::string({ 0x20, -1 /* 0xFF */, -1, -1, -1 });

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(2ms))
			.reply_with(malformed_connack, after(3ms));

	auto handler = [&](error_code ec) {
		BOOST_CHECK(ec == asio::error::try_again);
	};

	run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(malformed_connack_rc, shared_test_data) {
	// packets
	auto malformed_connack = encoders::encode_connack(true, uint8_t(0x04), {});

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
		.complete_with(success, after(2ms))
		.reply_with(malformed_connack, after(3ms));

	auto handler = [&](error_code ec) {
		BOOST_CHECK(ec == client::error::malformed_packet);
	};

	run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(fail_reading_connack_payload, shared_test_data) {
	// packets
	connack_props cprops;
	cprops[prop::reason_string] = std::string(256, 'a');

	auto big_connack = encoders::encode_connack(
		true, uint8_t(0x00), cprops
	);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(2ms))
		.send(big_connack.substr(0, 5), after(5ms))
		.send(fail, after(7ms));

	auto handler = [&](error_code ec) {
		BOOST_CHECK(ec == fail);
	};

	run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(receive_unexpected_auth, shared_test_data) {
	auth_props aprops;
	aprops[prop::authentication_method] = "method";
	aprops[prop::authentication_data] = "data";
	auto auth = encoders::encode_auth(uint8_t(0x19), aprops);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(2ms))
		.send(auth, after(5ms));

	auto handler = [&](error_code ec) {
		BOOST_CHECK(ec == client::error::malformed_packet);
	};

	run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_AUTO_TEST_SUITE_END();
