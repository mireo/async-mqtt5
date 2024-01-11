#include <boost/test/unit_test.hpp>

#include <boost/asio/io_context.hpp>

#include <async_mqtt5/mqtt_client.hpp>

#include <async_mqtt5/impl/subscribe_op.hpp>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(subscribe_op/*, *boost::unit_test::disabled()*/)

template <
	typename StreamType,
	typename TlsContext = std::monostate
>
class overrun_client : public detail::client_service<StreamType, TlsContext> {
public:
	overrun_client(const asio::any_io_executor& ex, const std::string& cnf) :
		detail::client_service<StreamType, TlsContext>(ex, cnf)
	{}

	uint16_t allocate_pid() {
		return 0;
	}
};

BOOST_AUTO_TEST_CASE(test_pid_overrun) {
	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = overrun_client<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor(), "");

	auto handler = [&handlers_called](error_code ec, std::vector<reason_code> rcs, auto) {
		++handlers_called;
		BOOST_CHECK(ec == client::error::pid_overrun);
		BOOST_ASSERT(rcs.size() == 1);
		BOOST_CHECK_EQUAL(rcs[0], reason_codes::empty);
	};

	detail::subscribe_op<
		client_service_type, decltype(handler)
	> { svc_ptr, std::move(handler) }
	.perform(
		{ { "topic", { qos_e::exactly_once } } }, subscribe_props {}
	);

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_invalid_topic_filters) {
	std::vector<std::string> invalid_topics = {
		"", "+topic", "#topic", "some/#/topic", "topic+",
		"$share//topic"
	};
	const int expected_handlers_called = static_cast<int>(invalid_topics.size());
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());

	for (const auto& topic: invalid_topics) {
		auto handler = [&handlers_called](error_code ec, std::vector<reason_code> rcs, auto) {
			++handlers_called;
			BOOST_CHECK(ec == client::error::invalid_topic);
			BOOST_ASSERT(rcs.size() == 1);
			BOOST_CHECK_EQUAL(rcs[0], reason_codes::empty);
		};

		detail::subscribe_op<
			client_service_type, decltype(handler)
		> { svc_ptr, std::move(handler) }
			.perform({{ topic, { qos_e::exactly_once } }}, subscribe_props {});
	}

	ioc.run_for(std::chrono::milliseconds(500));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}


BOOST_AUTO_TEST_CASE(test_malformed_packet) {
	std::vector<std::string> test_properties = {
		std::string(75000, 'a'), std::string(10, char(0x01))
	};

	const int expected_handlers_called = static_cast<int>(test_properties.size());
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());

	for (const auto& test_prop: test_properties) {
		auto handler = [&handlers_called](error_code ec, std::vector<reason_code> rcs, auto) {
			++handlers_called;
			BOOST_CHECK(ec == client::error::malformed_packet);
			BOOST_ASSERT(rcs.size() == 1);
			BOOST_CHECK_EQUAL(rcs[0], reason_codes::empty);
		};

		subscribe_props props;
		props[prop::user_property].push_back(test_prop);

		detail::subscribe_op<
			client_service_type, decltype(handler)
		> { svc_ptr, std::move(handler) }
			.perform({ { "topic", { qos_e::exactly_once } } }, props
		);
	}

	ioc.run_for(std::chrono::milliseconds(500));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_wildcard_subscriptions_not_supported) {
	std::vector<std::string> wildcard_topics = {
		"topic/#", "$share/grp/topic/#"
	};
	connack_props props;
	props[prop::wildcard_subscription_available] = uint8_t(0);

	int expected_handlers_called = static_cast<int>(wildcard_topics.size());
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(
		ioc.get_executor(), std::move(props)
	);
	BOOST_ASSERT(svc_ptr->connack_property(prop::wildcard_subscription_available) == 0);

	for (const auto& topic: wildcard_topics) {
		auto handler = [&handlers_called](error_code ec, std::vector<reason_code> rcs, auto) {
			++handlers_called;
			BOOST_CHECK(ec == client::error::wildcard_subscription_not_available);
			BOOST_ASSERT(rcs.size() == 1);
			BOOST_CHECK_EQUAL(rcs[0], reason_codes::empty);
		};

		detail::subscribe_op<
			client_service_type, decltype(handler)
		> { svc_ptr, std::move(handler) }
		.perform(
			{{ topic, { qos_e::exactly_once } }}, subscribe_props {}
		);
	}

	ioc.run_for(std::chrono::milliseconds(500));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_shared_subscriptions_not_supported) {
	connack_props props;
	props[prop::shared_subscription_available] = uint8_t(0);

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(
		ioc.get_executor(), std::move(props)
	);
	BOOST_ASSERT(svc_ptr->connack_property(prop::shared_subscription_available) == 0);

	auto handler = [&handlers_called](error_code ec, std::vector<reason_code> rcs, auto) {
		++handlers_called;
		BOOST_CHECK(ec == client::error::shared_subscription_not_available);
		BOOST_ASSERT(rcs.size() == 1);
		BOOST_CHECK_EQUAL(rcs[0], reason_codes::empty);
	};

	detail::subscribe_op<
		client_service_type, decltype(handler)
	> { svc_ptr, std::move(handler) }
	.perform(
		{{ "$share/group/topic", { qos_e::exactly_once } }}, subscribe_props {}
	);

	ioc.run_for(std::chrono::milliseconds(500));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_large_subscription_id) {
	uint8_t sub_id_available = 1;
	
	connack_props props;
	props[prop::subscription_identifier_available] = sub_id_available;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(
		ioc.get_executor(), std::move(props)
	);
	BOOST_ASSERT(svc_ptr->connack_property(prop::subscription_identifier_available) == sub_id_available);

	auto handler = [&handlers_called](error_code ec, std::vector<reason_code> rcs, auto) {
		++handlers_called;
		BOOST_CHECK(ec == client::error::subscription_identifier_not_available);
		BOOST_ASSERT(rcs.size() == 1);
		BOOST_CHECK_EQUAL(rcs[0], reason_codes::empty);
	};

	subscribe_props sub_props_big_id {};
	sub_props_big_id[prop::subscription_identifier] = std::numeric_limits<uint32_t>::max();

	detail::subscribe_op<
		client_service_type, decltype(handler)
	> { svc_ptr, std::move(handler) }
	.perform(
		{{ "topic", { qos_e::exactly_once } }}, sub_props_big_id
	);

	ioc.run_for(std::chrono::milliseconds(500));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_subscription_ids_not_supported) {
	uint8_t sub_id_available = 0;

	connack_props props;
	props[prop::subscription_identifier_available] = sub_id_available;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(
		ioc.get_executor(), std::move(props)
	);
	BOOST_ASSERT(svc_ptr->connack_property(prop::subscription_identifier_available) == sub_id_available);

	auto handler = [&handlers_called](error_code ec, std::vector<reason_code> rcs, auto) {
		++handlers_called;
		BOOST_CHECK(ec == client::error::subscription_identifier_not_available);
		BOOST_ASSERT(rcs.size() == 1);
		BOOST_CHECK_EQUAL(rcs[0], reason_codes::empty);
	};

	subscribe_props sub_props {};
	sub_props[prop::subscription_identifier] = 23;

	detail::subscribe_op<
		client_service_type, decltype(handler)
	> { svc_ptr, std::move(handler) }
	.perform(
		{{ "topic", { qos_e::exactly_once } }}, sub_props
	);

	ioc.run_for(std::chrono::milliseconds(500));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_packet_too_large) {
	int max_packet_sz = 10;

	connack_props props;
	props[prop::maximum_packet_size] = max_packet_sz;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(
		ioc.get_executor(), std::move(props)
	);
	BOOST_ASSERT(svc_ptr->connack_property(prop::maximum_packet_size) == max_packet_sz);

	auto handler = [&handlers_called](error_code ec, std::vector<reason_code> rcs, auto) {
		++handlers_called;
		BOOST_CHECK(ec == client::error::packet_too_large);
		BOOST_ASSERT(rcs.size() == 1);
		BOOST_CHECK_EQUAL(rcs[0], reason_codes::empty);
	};

	detail::subscribe_op<
		client_service_type, decltype(handler)
	> { svc_ptr, std::move(handler) }
	.perform(
		{ { "topic", { qos_e::exactly_once } } }, subscribe_props {}
	);

	ioc.run_for(std::chrono::milliseconds(500));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_resending) {
	using test::after;
	using std::chrono_literals::operator ""ms;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	// data
	std::vector<subscribe_topic> topics = {
		subscribe_topic { "topic", subscribe_options {} }
	};
	subscribe_props subscribe_props;

	std::vector<uint8_t> rcs = { reason_codes::granted_qos_0.value() };
	suback_props suback_props;

	// packets
	auto connect = encoders::encode_connect(
		"", std::nullopt, std::nullopt, 10, false, {}, std::nullopt
	);
	auto connack = encoders::encode_connack(false, reason_codes::success.value(), {});

	auto subscribe = encoders::encode_subscribe(1, topics, subscribe_props);
	auto suback = encoders::encode_suback(1, rcs, suback_props);

	test::msg_exchange broker_side;
	error_code success {};
	error_code fail = asio::error::not_connected;

	broker_side
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(subscribe)
			.complete_with(fail, after(0ms))
		.expect(connect)
			.complete_with(success, after(0ms))
			.reply_with(connack, after(0ms))
		.expect(subscribe)
			.complete_with(success, after(0ms))
			.reply_with(suback, after(0ms));

	asio::io_context ioc;
	auto executor = ioc.get_executor();
	auto& broker = asio::make_service<test::test_broker>(
		ioc, executor, std::move(broker_side)
	);

	using client_type = mqtt_client<test::test_stream>;
	client_type c(executor, "");
	c.brokers("127.0.0.1")
		.run();

	c.async_subscribe(
		topics, subscribe_props,
		[&](error_code ec, std::vector<reason_code> rcs, auto) {
			handlers_called++;

			BOOST_CHECK(!ec);
			BOOST_ASSERT(rcs.size() == 1);
			BOOST_CHECK_EQUAL(rcs[0], reason_codes::granted_qos_0);

			c.cancel();
		}
	);

	ioc.run_for(std::chrono::seconds(10));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
	BOOST_CHECK(broker.received_all_expected());
}

BOOST_AUTO_TEST_SUITE_END()
