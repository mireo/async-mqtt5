#include <boost/test/unit_test.hpp>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <async_mqtt5/error.hpp>

#include <async_mqtt5/impl/client_service.hpp>
#include <async_mqtt5/impl/publish_send_op.hpp>

#include "test_common/test_service.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(publish_send_op/*, *boost::unit_test::disabled()*/)

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

	auto handler = [&handlers_called](error_code ec, reason_code rc, puback_props) {
		++handlers_called;
		BOOST_CHECK(ec == client::error::pid_overrun);
		BOOST_CHECK_EQUAL(rc, reason_codes::empty);
	};

	detail::publish_send_op<
		client_service_type, decltype(handler), qos_e::at_least_once
	> { svc_ptr, std::move(handler) }
		.perform(
			"test", "payload", retain_e::no, {}
		);

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_invalid_topic_names) {
	std::vector<std::string> invalid_topics = {
		"", "+", "#",
		"invalid+", "invalid#", "invalid/#", "invalid/+"
	};
	int expected_handlers_called = invalid_topics.size();
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());

	for (const auto& topic: invalid_topics) {
		auto handler = [&handlers_called](error_code ec) {
			++handlers_called;
			BOOST_CHECK(ec == client::error::invalid_topic);
		};

		detail::publish_send_op<
			client_service_type, decltype(handler), qos_e::at_most_once
		> { svc_ptr, std::move(handler) }
		.perform(topic, "payload", retain_e::no, {});
	}

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_malformed_packet) {
	std::string malformed_str = std::string { 0x01 };

	publish_props malfored_response_topic_props;
	malfored_response_topic_props[prop::response_topic] = "response#topic";

	publish_props utf8_payload_props;
	utf8_payload_props[prop::payload_format_indicator] = uint8_t(1);

	publish_props invalid_user_props;
	invalid_user_props[prop::user_property].push_back(malformed_str);

	publish_props malformed_content_type_props;
	malformed_content_type_props[prop::content_type] = malformed_str;

	publish_props out_of_range_subid_props;
	out_of_range_subid_props[prop::subscription_identifier] = 300'000'000;

	std::vector<publish_props> testing_props = {
		malfored_response_topic_props, utf8_payload_props,
		invalid_user_props, malformed_content_type_props,
		out_of_range_subid_props
	};

	int expected_handlers_called = testing_props.size();
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());

	for (const auto& props: testing_props) {
		auto handler = [&handlers_called](error_code ec) {
			++handlers_called;
			BOOST_CHECK(ec == client::error::malformed_packet);
		};

		detail::publish_send_op<
			client_service_type, decltype(handler), qos_e::at_most_once
		> { svc_ptr, std::move(handler) }
		.perform("topic", malformed_str, retain_e::no, props);
	}

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_packet_too_large) {
	connack_props props;
	props[prop::maximum_packet_size] = 10;

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(
		ioc.get_executor(), std::move(props)
	);

	auto handler = [&handlers_called](error_code ec, reason_code rc, puback_props) {
		++handlers_called;
		BOOST_CHECK(ec == client::error::packet_too_large);
		BOOST_CHECK_EQUAL(rc, reason_codes::empty);
	};

	detail::publish_send_op<
		client_service_type, decltype(handler), qos_e::at_least_once
	> { svc_ptr, std::move(handler) }
	.perform(
		"test", "payload", retain_e::no, {}
	);

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_qos_not_supported) {
	connack_props props;
	props[prop::maximum_qos] = uint8_t(0);

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(
		ioc.get_executor(), std::move(props)
	);

	auto handler = [&handlers_called](error_code ec, reason_code rc, puback_props) {
		++handlers_called;
		BOOST_CHECK(ec == client::error::qos_not_supported);
		BOOST_CHECK_EQUAL(rc, reason_codes::empty);
	};

	detail::publish_send_op<
		client_service_type, decltype(handler), qos_e::at_least_once
	> { svc_ptr, std::move(handler) }
	.perform(
		"test", "payload", retain_e::no, {}
	);

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_retain_not_available) {
	connack_props props;
	props[prop::retain_available] = uint8_t(0);

	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(
		ioc.get_executor(), std::move(props)
	);

	auto handler = [&handlers_called](error_code ec, reason_code rc, puback_props) {
		++handlers_called;
		BOOST_CHECK(ec == client::error::retain_not_available);
		BOOST_CHECK_EQUAL(rc, reason_codes::empty);
	};

	detail::publish_send_op<
		client_service_type, decltype(handler), qos_e::at_least_once
	> { svc_ptr, std::move(handler) }
	.perform(
		"test", "payload", retain_e::yes, {}
	);

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_topic_alias_maximum) {
	connack_props ta_allowed_props;
	ta_allowed_props[prop::topic_alias_maximum] = uint16_t(10);

	std::vector<connack_props> test_props = {
		ta_allowed_props, connack_props {} /* not allowed */
	};

	int expected_handlers_called = test_props.size();
	int handlers_called = 0;

	asio::io_context ioc;

	for (const auto& ca_props: test_props) {
		using client_service_type = test::test_service<asio::ip::tcp::socket>;
		auto svc_ptr = std::make_shared<client_service_type>(
			ioc.get_executor(), ca_props
		);

		auto handler = [&handlers_called](error_code ec) {
			++handlers_called;
			BOOST_CHECK(ec == client::error::topic_alias_maximum_reached);
		};

		publish_props props;
		props[prop::topic_alias] = uint16_t(12);

		detail::publish_send_op<
			client_service_type, decltype(handler), qos_e::at_most_once
		> { svc_ptr, std::move(handler) }
		.perform(
			"test", "payload", retain_e::yes, props
		);
	}

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_publish_immediate_cancellation) {
	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());
	asio::cancellation_signal cancel_signal;

	auto h = [&handlers_called](error_code ec, reason_code rc, puback_props) {
		++handlers_called;
		BOOST_CHECK(ec == asio::error::operation_aborted);
		BOOST_CHECK_EQUAL(rc, reason_codes::empty);
	};

	auto handler = asio::bind_cancellation_slot(cancel_signal.slot(), std::move(h));

	detail::publish_send_op<
		client_service_type, decltype(handler), qos_e::at_least_once
	> { svc_ptr, std::move(handler) }
	.perform(
		"test", "payload", retain_e::no, {}
	);

	cancel_signal.emit(asio::cancellation_type::terminal);
	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_publish_cancellation) {
	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());
	asio::cancellation_signal cancel_signal;

	auto h = [&handlers_called](error_code ec, reason_code rc, puback_props) {
		++handlers_called;
		BOOST_CHECK(ec == asio::error::operation_aborted);
		BOOST_CHECK_EQUAL(rc, reason_codes::empty);
	};

	auto handler = asio::bind_cancellation_slot(cancel_signal.slot(), std::move(h));

	asio::steady_timer timer(ioc.get_executor());
	timer.expires_after(std::chrono::milliseconds(60));
	timer.async_wait(
		[&cancel_signal](error_code) {
			cancel_signal.emit(asio::cancellation_type::terminal);
		}
	);

	detail::publish_send_op<
		client_service_type, decltype(handler), qos_e::at_least_once
	> { svc_ptr, std::move(handler) }
	.perform(
		"test", "payload", retain_e::no, {}
	);

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_SUITE_END()
