#ifndef ASYNC_MQTT5_TEST_TEST_AUTHENTICATORS_HPP
#define ASYNC_MQTT5_TEST_TEST_AUTHENTICATORS_HPP

#include <boost/asio/dispatch.hpp>
#include <boost/asio/prepend.hpp>

#include <async_mqtt5/types.hpp>

#include <iostream>

namespace async_mqtt5::test {

namespace asio = boost::asio;

struct test_authenticator {
	test_authenticator() = default;

	template <typename CompletionToken>
	decltype(auto) async_auth(
		auth_step_e step, std::string data,
		CompletionToken&& token
	) {
		using error_code = boost::system::error_code;
		using Signature = void (error_code, std::string);

		auto initiate = [](auto handler, auth_step_e, std::string) {
			asio::dispatch(
				asio::prepend(std::move(handler), error_code {}, "")
			);
		};

		return asio::async_initiate<CompletionToken, Signature>(
			initiate, token, step, std::move(data)
		);
	}

	std::string_view method() const {
		return "method";
	}
};

template <auth_step_e fail_on_step>
struct fail_test_authenticator {
	fail_test_authenticator() = default;

	template <typename CompletionToken>
	decltype(auto) async_auth(
		auth_step_e step, std::string data,
		CompletionToken&& token
	) {
		using error_code = boost::system::error_code;
		using Signature = void (error_code, std::string);

		auto initiate = [](auto handler, auth_step_e step, std::string) {
			error_code ec;
			if (fail_on_step == step)
				ec = asio::error::no_recovery;

			asio::dispatch(
				asio::prepend(std::move(handler), ec, "")
			);
		};

		return asio::async_initiate<CompletionToken, Signature>(
			initiate, token, step, std::move(data)
		);
	}

	std::string_view method() const {
		return "method";
	}
};

} // end namespace async_mqtt5::test

#endif // ASYNC_MQTT5_TEST_TEST_AUTHENTICATORS_HPP
