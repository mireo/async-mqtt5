#include <boost/test/unit_test.hpp>

#include <boost/asio/io_context.hpp>

#include <async_mqtt5/impl/unsubscribe_op.hpp>

#include "test_common/test_service.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(unsubscribe_op/*, *boost::unit_test::disabled()*/)

BOOST_AUTO_TEST_CASE(test_pid_overrun) {
	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::overrun_client<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor(), "");

	auto handler = [&handlers_called](error_code ec, std::vector<reason_code> rcs, auto) {
		++handlers_called;
		BOOST_CHECK(ec == client::error::pid_overrun);
		BOOST_ASSERT(rcs.size() == 1);
		BOOST_CHECK_EQUAL(rcs[0], reason_codes::empty);
	};

	detail::unsubscribe_op<
		client_service_type, decltype(handler)
	> { svc_ptr, std::move(handler) }
	.perform({ "topic" }, unsubscribe_props{});

	ioc.run_for(std::chrono::milliseconds(500));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(test_invalid_topic_filters) {
	std::vector<std::string> invalid_topics = {
		"", "+topic", "#topic", "some/#/topic", "topic+"
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

		detail::unsubscribe_op<
			client_service_type, decltype(handler)
		> { svc_ptr, std::move(handler) }
		.perform({ topic }, unsubscribe_props{});
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

		unsubscribe_props props;
		props[prop::user_property].push_back(test_prop);

		detail::unsubscribe_op<
			client_service_type, decltype(handler)
		> { svc_ptr, std::move(handler) }
		.perform({ "topic" }, props);
	}

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

	detail::unsubscribe_op<
		client_service_type, decltype(handler)
	> { svc_ptr, std::move(handler) }
	.perform({ "topic" }, unsubscribe_props {});

	ioc.run_for(std::chrono::milliseconds(500));
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_SUITE_END()
