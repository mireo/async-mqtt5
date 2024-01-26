#include <boost/test/unit_test.hpp>

#include <boost/asio/io_context.hpp>

#include <async_mqtt5/mqtt_client.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/packet_util.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(send_publish/*, *boost::unit_test::disabled()*/)

struct shared_test_data {
	error_code success {};
	error_code fail = asio::error::not_connected;

	const std::string connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	const std::string connack = encoders::encode_connack(
		false, reason_codes::success.value(), {}
	);

	const std::string topic = "topic";
	const std::string payload = "payload";

	const std::string publish_qos0 = encoders::encode_publish(
		0, topic, payload, qos_e::at_most_once, retain_e::no, dup_e::no, {}
	);
	const std::string publish_qos1 = encoders::encode_publish(
		1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	const std::string publish_qos2 = encoders::encode_publish(
		1, topic, payload, qos_e::exactly_once, retain_e::no, dup_e::no, {}
	);

	const std::string puback = encoders::encode_puback(1, uint8_t(0x00), {});

	const std::string pubrec = encoders::encode_pubrec(1, uint8_t(0x00), {});
	const std::string pubrel = encoders::encode_pubrel(1, uint8_t(0x00), {});
	const std::string pubcomp = encoders::encode_pubcomp(1, uint8_t(0x00), {});
};

using test::after;
using namespace std::chrono;

template <qos_e qos>
void run_test(
	test::msg_exchange broker_side,
	asio::any_completion_handler<detail::on_publish_signature<qos>> op_handler
) {
	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.async_run(asio::detached);

	auto shared_data = shared_test_data();
	if constexpr (qos == qos_e::at_most_once)
		c.async_publish<qos>(
			shared_data.topic, shared_data.payload, retain_e::no, publish_props {},
			[&handlers_called, &c, h = std::move(op_handler)](error_code ec) mutable {
				++handlers_called;
				std::move(h)(ec);
				c.cancel();
			}
		);
	else
		c.async_publish<qos>(
			shared_data.topic, shared_data.payload, retain_e::no, publish_props {},
			[&handlers_called, &c, h = std::move(op_handler)]
			(error_code ec, reason_code rc, detail::on_publish_props_type<qos> props) mutable {
				++handlers_called;
				std::move(h)(ec, rc, props);
				c.cancel();
			}
		);

	ioc.run_for(2s);
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(send_publish_qos0, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos0)
			.complete_with(success, after(1ms));

	run_test<qos_e::at_most_once>(
		std::move(broker_side),
		[](error_code ec) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
		}
	);
}

BOOST_FIXTURE_TEST_CASE(send_publish_qos1, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos1)
			.complete_with(success, after(1ms))
			.reply_with(puback, after(2ms));

	run_test<qos_e::at_least_once>(
		std::move(broker_side),
		[](error_code ec, reason_code rc, puback_props) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_EQUAL(rc, reason_codes::success);
		}
	);
}

BOOST_FIXTURE_TEST_CASE(send_publish_qos2, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos2)
			.complete_with(success, after(1ms))
			.reply_with(pubrec, after(2ms))
		.expect(pubrel)
			.complete_with(success, after(1ms))
			.reply_with(pubcomp, after(2ms));

	run_test<qos_e::exactly_once>(
		std::move(broker_side),
		[](error_code ec, reason_code rc, pubcomp_props) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_EQUAL(rc, reason_codes::success);
		}
	);
}

BOOST_FIXTURE_TEST_CASE(fail_to_send_publish, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos1)
			.complete_with(fail, after(1ms))
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos1)
			.complete_with(success, after(1ms))
			.reply_with(puback, after(2ms));

	run_test<qos_e::at_least_once>(
		std::move(broker_side),
		[](error_code ec, reason_code rc, puback_props) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_EQUAL(rc, reason_codes::success);
		}
	);
}

BOOST_FIXTURE_TEST_CASE(fail_to_send_pubrel, shared_test_data) {
	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos2)
			.complete_with(success, after(1ms))
			.reply_with(pubrec, after(2ms))
		.expect(pubrel)
			.complete_with(fail, after(1ms))
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(pubrel)
			.complete_with(success, after(1ms))
			.reply_with(pubcomp, after(2ms));

	run_test<qos_e::exactly_once>(
		std::move(broker_side),
		[](error_code ec, reason_code rc, pubcomp_props) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_EQUAL(rc, reason_codes::success);
		}
	);
}

BOOST_FIXTURE_TEST_CASE(receive_malformed_puback, shared_test_data) {
	// packets
	auto publish_qos1_dup = encoders::encode_publish(
		1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::yes, {}
	);
	auto malformed_puback = encoders::encode_puback(1, uint8_t(0x04), {});

	auto disconnect = encoders::encode_disconnect(
		reason_codes::malformed_packet.value(),
		test::dprops_with_reason_string("Malformed PUBACK: invalid Reason Code")
	);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos1)
			.complete_with(success, after(1ms))
			.reply_with(malformed_puback, after(2ms))
		.expect(disconnect)
			.complete_with(success, after(1ms))
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos1_dup)
			.complete_with(success, after(1ms))
			.reply_with(puback, after(2ms));

	run_test<qos_e::at_least_once>(
		std::move(broker_side),
		[](error_code ec, reason_code rc, puback_props) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_EQUAL(rc, reason_codes::success);
		}
	);
}

BOOST_FIXTURE_TEST_CASE(receive_malformed_pubrec, shared_test_data) {
	// packets
	auto publish_qos2_dup = encoders::encode_publish(
		1, topic, payload, qos_e::exactly_once, retain_e::no, dup_e::yes, {}
	);

	auto malformed_pubrec = encoders::encode_pubrec(1, uint8_t(0x04), {});

	auto disconnect = encoders::encode_disconnect(
		reason_codes::malformed_packet.value(),
		test::dprops_with_reason_string("Malformed PUBREC: invalid Reason Code")
	);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos2)
			.complete_with(success, after(1ms))
			.reply_with(malformed_pubrec, after(2ms))
		.expect(disconnect)
			.complete_with(success, after(1ms))
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos2_dup)
			.complete_with(success, after(1ms))
			.reply_with(pubrec, after(2ms))
		.expect(pubrel)
			.complete_with(success, after(1ms))
			.reply_with(pubcomp, after(2ms));

	run_test<qos_e::exactly_once>(
		std::move(broker_side),
		[](error_code ec, reason_code rc, pubcomp_props) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_EQUAL(rc, reason_codes::success);
		}
	);
}

BOOST_FIXTURE_TEST_CASE(receive_malformed_pubcomp, shared_test_data) {
	// packets
	auto malformed_pubcomp = encoders::encode_pubcomp(1, uint8_t(0x04), {});

	auto disconnect = encoders::encode_disconnect(
		reason_codes::malformed_packet.value(),
		test::dprops_with_reason_string("Malformed PUBCOMP: invalid Reason Code")
	);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(publish_qos2)
			.complete_with(success, after(1ms))
			.reply_with(pubrec, after(2ms))
		.expect(pubrel)
			.complete_with(success, after(1ms))
			.reply_with(malformed_pubcomp, after(2ms))
		.expect(disconnect)
			.complete_with(success, after(1ms))
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms))
		.expect(pubrel)
			.complete_with(success, after(1ms))
			.reply_with(pubcomp, after(2ms));

	run_test<qos_e::exactly_once>(
		std::move(broker_side),
		[](error_code ec, reason_code rc, pubcomp_props) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_EQUAL(rc, reason_codes::success);
		}
	);
}

BOOST_FIXTURE_TEST_CASE(receive_pubrec_with_rc, shared_test_data) {
	// packets
	auto pubrec_with_rc = encoders::encode_pubrec(
		1, reason_codes::unspecified_error.value(), {}
	);

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(publish_qos2)
			.complete_with(success, after(0ms))
			.reply_with(pubrec_with_rc, after(0ms));

	run_test<qos_e::exactly_once>(
		std::move(broker_side),
		[](error_code ec, reason_code rc, pubcomp_props) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_EQUAL(rc, reason_codes::unspecified_error);
		}
	);
}

BOOST_FIXTURE_TEST_CASE(cancel_resending_publish, shared_test_data) {
	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	test::msg_exchange broker_side;
	broker_side
		.expect(connect)
			.complete_with(success, after(1ms))
			.reply_with(connack, after(2ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
		.async_run(asio::detached);

	asio::cancellation_signal cancel_signal;
	c.async_publish<qos_e::at_least_once>(
		topic, payload, retain_e::no, publish_props{},
		asio::bind_cancellation_slot(
			cancel_signal.slot(),
			[&handlers_called, &c](error_code ec, reason_code rc, puback_props) {
				++handlers_called;

				BOOST_CHECK(ec = asio::error::operation_aborted);
				BOOST_CHECK_EQUAL(rc, reason_codes::empty);

				c.cancel();
			}
		)
	);
	cancel_signal.emit(asio::cancellation_type::total);

	ioc.run_for(1s);
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_SUITE_END();
