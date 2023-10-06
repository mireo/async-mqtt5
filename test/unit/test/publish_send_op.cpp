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

namespace async_mqtt5::client {

inline std::ostream& operator<<(std::ostream& os, const error& err) {
	os << client_error_to_string(err);
	return os;
}

} // end namespace async_mqtt5::client


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

	auto handler = [&](error_code ec, reason_code rc, puback_props) {
		++handlers_called;
		BOOST_CHECK_EQUAL(ec, client::error::pid_overrun);
		BOOST_CHECK_EQUAL(rc, reason_codes::empty);
	};

	detail::publish_send_op<
		client_service_type, decltype(handler), qos_e::at_least_once
	> { svc_ptr, std::move(handler) }
		.perform(
			"test", "payload", retain_e::no, {}
		);

	ioc.run();
	BOOST_CHECK_EQUAL(
		handlers_called, expected_handlers_called
	);
}

BOOST_AUTO_TEST_CASE(test_publish_immediate_cancellation) {
	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());
	asio::cancellation_signal cancel_signal;

	auto h = [&](error_code ec, reason_code rc, puback_props) {
		++handlers_called;
		BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
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
	BOOST_CHECK_EQUAL(
		handlers_called, expected_handlers_called
	);
}

BOOST_AUTO_TEST_CASE(test_publish_cancellation) {
	constexpr int expected_handlers_called = 1;
	int handlers_called = 0;

	asio::io_context ioc;
	using client_service_type = test::test_service<asio::ip::tcp::socket>;
	auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());
	asio::cancellation_signal cancel_signal;

	auto h = [&](error_code ec, reason_code rc, puback_props) {
		++handlers_called;
		BOOST_CHECK_EQUAL(ec, asio::error::operation_aborted);
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
	BOOST_CHECK_EQUAL(
		handlers_called, expected_handlers_called
	);
}

BOOST_AUTO_TEST_SUITE_END()
