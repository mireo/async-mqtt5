#include <boost/test/unit_test.hpp>

#include <boost/asio/io_context.hpp>

#include <async_mqtt5/error.hpp>

#include <async_mqtt5/impl/subscribe_op.hpp>

#include "test_common/test_service.hpp"

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(subscribe_op/*, *boost::unit_test::disabled()*/)

BOOST_AUTO_TEST_CASE(test_invalid_topic_filters) {
	std::vector<std::string> invalid_topics = {
		"", "+topic", "#topic", "some/#/topic", "topic+"
	};
	const int expected_handlers_called = invalid_topics.size();
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());

	for (const auto& topic: invalid_topics) {
		auto handler = [&handlers_called](error_code ec, auto, auto) {
			++handlers_called;
			BOOST_CHECK(ec == client::error::invalid_topic);
		};

		detail::subscribe_op<
			client_service_type, decltype(handler)
		> { svc_ptr, std::move(handler) }
			.perform({{ topic, { qos_e::exactly_once } }}, subscribe_props {});
	}

	ioc.run();
	BOOST_CHECK_EQUAL(handlers_called, expected_handlers_called);
}

BOOST_AUTO_TEST_SUITE_END()
