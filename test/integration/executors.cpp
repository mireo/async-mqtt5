#include <boost/test/unit_test.hpp>

#include <chrono>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>

#include <async_mqtt5/impl/codecs/message_encoders.hpp>

#include <async_mqtt5.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(executors)

BOOST_AUTO_TEST_CASE(async_run) {
	using test::after;
	using namespace std::chrono;

	constexpr int expected_handlers_called = 9;
	int handlers_called = 0;

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(
		false, reason_codes::success.value(), {}
	);
	auto publish_0 = encoders::encode_publish(
		0, "t_0", "p_0", qos_e::at_most_once, retain_e::no, dup_e::no, {}
	);
	auto publish_1 = encoders::encode_publish(
		1, "t_1", "p_1", qos_e::at_least_once, retain_e::no, dup_e::no, {}
	);
	auto puback = encoders::encode_puback(
		1, reason_codes::success.value(), {}
	);
	auto publish_2 = encoders::encode_publish(
		2, "t_2", "p_2", qos_e::exactly_once, retain_e::no, dup_e::no, {}
	);
	auto pubrec = encoders::encode_pubrec(
		2, reason_codes::success.value(), {}
	);
	auto pubrel = encoders::encode_pubrel(
		2, reason_codes::success.value(), {}
	);
	auto pubcomp = encoders::encode_pubcomp(
		2, reason_codes::success.value(), {}
	);
	auto subscribe = encoders::encode_subscribe(
		3, std::vector<subscribe_topic> { { "t_0", {} } }, {}
	);
	auto suback = encoders::encode_suback(
		3, std::vector<uint8_t> { reason_codes::granted_qos_2.value() }, {}
	);
	auto unsubscribe = encoders::encode_unsubscribe(
		1, std::vector<std::string> { "t_0" }, {}
	);
	auto unsuback = encoders::encode_unsuback(
		1, std::vector<uint8_t> { reason_codes::success.value() }, {}
	);
	auto disconnect = encoders::encode_disconnect(
		reason_codes::normal_disconnection.value(), {}
	);

	test::msg_exchange broker_side;
	error_code success {};

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(subscribe, publish_0, publish_1, publish_2)
			.complete_with(success, after(0ms))
			.reply_with(puback, pubrec, suback, after(0ms))
		.expect(pubrel)
			.complete_with(success, after(0ms))
			.reply_with(pubcomp, after(0ms))
		.send(publish_0, after(50ms))
		.expect(unsubscribe)
			.complete_with(success, after(0ms))
			.reply_with(unsuback, after(0ms))
		.expect(publish_1)
			.complete_with(success, after(0ms))
		.expect(disconnect)
			.complete_with(success, after(0ms))
		;

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	auto strand = asio::make_strand(ioc);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.async_run(asio::bind_executor(
			strand,
			[&](error_code ec) {
				BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
				BOOST_CHECK(strand.running_in_this_thread());
				++handlers_called;
			}
		));

	c.async_publish<qos_e::at_most_once>(
		"t_0", "p_0", retain_e::no, {},
		[&](error_code ec) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK(strand.running_in_this_thread());
			++handlers_called;
		}
	);

	c.async_publish<qos_e::at_least_once>(
		"t_1", "p_1", retain_e::no, {},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			BOOST_CHECK(strand.running_in_this_thread());
			++handlers_called;
		}
	);

	c.async_publish<qos_e::exactly_once>(
		"t_2", "p_2", retain_e::no, {},
		[&](error_code ec, reason_code rc, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rc, rc.message());
			BOOST_CHECK(strand.running_in_this_thread());
			++handlers_called;
		}
	);

	c.async_subscribe(
		subscribe_topic { "t_0", {} }, {},
		[&](error_code ec, std::vector<reason_code> rcs, auto) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_MESSAGE(!rcs[0], rcs[0].message());
			BOOST_CHECK(strand.running_in_this_thread());
			++handlers_called;
		}
	);

	c.async_receive(
		[&](
			error_code ec,
			std::string rec_topic, std::string rec_payload,
			publish_props
		) {
			BOOST_CHECK_MESSAGE(!ec, ec.message());
			BOOST_CHECK_EQUAL("t_0", rec_topic);
			BOOST_CHECK_EQUAL("p_0",  rec_payload);
			BOOST_CHECK(strand.running_in_this_thread());
			++handlers_called;
			c.async_unsubscribe(
				"t_0", {},
				[&](error_code ec, std::vector<reason_code> rcs, auto) {
					BOOST_CHECK_MESSAGE(!ec, ec.message());
					BOOST_CHECK_MESSAGE(!rcs[0], rcs[0].message());
					BOOST_CHECK(strand.running_in_this_thread());
					++handlers_called;
					c.async_publish<qos_e::at_least_once>(
						"t_1", "p_1", retain_e::no, {},
						[&](error_code ec, reason_code rc, auto) {
							BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
							BOOST_CHECK_EQUAL(rc, reason_codes::empty);
							BOOST_CHECK(strand.running_in_this_thread());
							++handlers_called;
						}
					);
					c.async_disconnect(
						[&](error_code ec) {
							BOOST_CHECK_MESSAGE(!ec, ec.message());
							BOOST_CHECK(strand.running_in_this_thread());
							++handlers_called;
						}
					);
				}
			);
		}
	);

	ioc.run_for(500ms);
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_SUITE_END()
