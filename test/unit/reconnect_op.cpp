#include <boost/test/unit_test.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <async_mqtt5/impl/reconnect_op.hpp>

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(reconnect_op/*, *boost::unit_test::disabled()*/)

BOOST_AUTO_TEST_CASE(exponential_backoff) {
	using namespace std::chrono_literals;

	detail::exponential_backoff generator;

	auto first_iter = generator.generate();
	BOOST_TEST((first_iter >= 500ms && first_iter <= 1500ms));

	auto second_iter = generator.generate();
	BOOST_TEST((second_iter >= 1500ms && first_iter <= 2500ms));

	auto third_iter = generator.generate();
	BOOST_TEST((third_iter >= 3500ms && third_iter <= 4500ms));

	auto fourth_iter = generator.generate();
	BOOST_TEST((fourth_iter >= 7500ms && fourth_iter <= 8500ms));

	auto fifth_iter = generator.generate();
	BOOST_TEST((fifth_iter >= 15500ms && fourth_iter <= 16500ms));

	auto sixth_iter = generator.generate();
	BOOST_TEST((sixth_iter >= 15500ms && sixth_iter <= 16500ms));
}

BOOST_AUTO_TEST_SUITE_END();
