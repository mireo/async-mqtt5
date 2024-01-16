#include <boost/test/unit_test.hpp>

#include <async_mqtt5.hpp>
#include <test_common/test_service.hpp>

using namespace async_mqtt5;

BOOST_AUTO_TEST_SUITE(session/*, *boost::unit_test::disabled()*/)

BOOST_AUTO_TEST_CASE(session_state_session_present) {
	detail::session_state session_state;

	BOOST_CHECK_EQUAL(session_state.session_present(), false);
	session_state.session_present(true);
	BOOST_CHECK_EQUAL(session_state.session_present(), true);
	session_state.session_present(false);
	BOOST_CHECK_EQUAL(session_state.session_present(), false);

	BOOST_CHECK_EQUAL(session_state.subscriptions_present(), false);
	session_state.subscriptions_present(true);
	BOOST_CHECK_EQUAL(session_state.subscriptions_present(), true);
	session_state.subscriptions_present(false);
	BOOST_CHECK_EQUAL(session_state.subscriptions_present(), false);
}

BOOST_AUTO_TEST_CASE(clear_waiting_on_pubrel) {
	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());
	svc_ptr->open_stream();

	decoders::publish_message pub_msg = std::make_tuple(
		"topic", int16_t(1), uint8_t(0b0100), publish_props {}, "payload"
	);

	detail::publish_rec_op<client_service_type> { svc_ptr }.perform(pub_msg);

	// let publish_rec_op reach wait_on_pubrel stage
	asio::steady_timer timer(ioc.get_executor());
	timer.expires_after(std::chrono::milliseconds(50));
	timer.async_wait([&svc_ptr](error_code) {
		BOOST_CHECK_EQUAL(svc_ptr.use_count(), 2);
		svc_ptr->update_session_state(); // session_present = false
		// publish_rec_op should complete
		BOOST_CHECK_EQUAL(svc_ptr.use_count(), 1);
	});

	ioc.run();
}


BOOST_AUTO_TEST_SUITE_END();
