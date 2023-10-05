#ifndef ASYNC_MQTT5_REPLIES_HPP
#define ASYNC_MQTT5_REPLIES_HPP

#include <boost/asio/error.hpp>
#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/consign.hpp>

#include <async_mqtt5/error.hpp>
#include <async_mqtt5/detail/internal_types.hpp>

#include <async_mqtt5/detail/control_packet.hpp>

namespace async_mqtt5::detail {

namespace asio = boost::asio;

class replies {
	using signature = void (error_code, byte_citer, byte_citer);

	static constexpr auto max_reply_time = std::chrono::seconds(20);

	class handler_type : public asio::any_completion_handler<signature> {
		using base = asio::any_completion_handler<signature>;
		control_code_e _code;
		uint16_t _packet_id;
		std::chrono::time_point<std::chrono::system_clock> _ts;
	public:
		template <typename H>
		handler_type(control_code_e code, uint16_t pid, H&& handler) :
			base(std::forward<H>(handler)), _code(code), _packet_id(pid),
			_ts(std::chrono::system_clock::now())
		{}

		handler_type(handler_type&& other) noexcept :
			base(static_cast<base&&>(other)),
			_code(other._code), _packet_id(other._packet_id), _ts(other._ts)
		{}

		handler_type& operator=(handler_type&& other) noexcept {
			base::operator=(static_cast<base&&>(other));
			_code = other._code;
			_packet_id = other._packet_id;
			_ts = other._ts;
			return *this;
		}

		uint16_t packet_id() const noexcept {
			return _packet_id;
		}

		control_code_e code() const noexcept {
			return _code;
		}

		auto time() const noexcept {
			return _ts;
		}
	};

	using handlers = std::vector<handler_type>;
	handlers _handlers;

	struct fast_reply {
		control_code_e code;
		uint16_t packet_id;
		std::unique_ptr<std::string> packet;
	};
	using fast_replies = std::vector<fast_reply>;
	fast_replies _fast_replies;

public:
	template <typename CompletionToken>
	decltype(auto) async_wait_reply(
		control_code_e code, uint16_t packet_id, CompletionToken&& token
	) {
		auto dup_handler_ptr = find_handler(code, packet_id);
		if (dup_handler_ptr != _handlers.end()) {
			std::move(*dup_handler_ptr)(
				asio::error::operation_aborted, byte_citer {}, byte_citer {}
			);
			_handlers.erase(dup_handler_ptr);
		}

		auto freply = find_fast_reply(code, packet_id);
		if (freply == _fast_replies.end()) {
			auto initiate = [this](
				auto handler, control_code_e code, uint16_t packet_id
			) {
				_handlers.emplace_back(code, packet_id, std::move(handler));
			};
			return asio::async_initiate<CompletionToken, signature>(
				std::move(initiate), token, code, packet_id
			);
		}

		auto fdata = std::move(*freply);
		_fast_replies.erase(freply);

		byte_citer first = fdata.packet->cbegin(), last = fdata.packet->cend();
		auto with_packet = asio::consign(
			std::forward<CompletionToken>(token), std::move(fdata.packet)
		);
		auto initiate = [](auto handler, byte_citer first, byte_citer last) {
			auto ex = asio::get_associated_executor(handler);
			asio::post(ex, [h = std::move(handler), first, last]() mutable {
				std::move(h)(error_code {}, first, last);
			});
		};
		return asio::async_initiate<decltype(with_packet), signature>(
			std::move(initiate), with_packet, first, last
		);
	}

	void dispatch(
		error_code ec, control_code_e code, uint16_t packet_id,
		byte_citer first, byte_citer last
	) {
		auto handler_ptr = find_handler(code, packet_id);
		if (handler_ptr == _handlers.end()) {
			_fast_replies.push_back({
				code, packet_id, std::make_unique<std::string>(first, last)
			});
			return;
		}
		auto handler = std::move(*handler_ptr);
		_handlers.erase(handler_ptr);
		std::move(handler)(ec, first, last);
	}

	void resend_unanswered() {
		auto ua = std::move(_handlers);
		for (auto& h : ua)
			std::move(h)(asio::error::try_again, byte_citer {}, byte_citer {});
	}

	void cancel_unanswered() {
		auto ua = std::move(_handlers);
		for (auto& h : ua)
			std::move(h)(
				asio::error::operation_aborted,
				byte_citer {}, byte_citer {}
			);
	}

	bool any_expired() {
		auto now = std::chrono::system_clock::now();
		return std::any_of(
			_handlers.begin(), _handlers.end(),
			[now](const auto& h) {
				return now - h.time() > max_reply_time;
			}
		);
	}

	void clear_fast_replies() {
		_fast_replies.clear();
	}

private:
	handlers::iterator find_handler(control_code_e code, uint16_t packet_id) {
		return std::find_if(
			_handlers.begin(), _handlers.end(),
			[code, packet_id](const auto& h) {
				return h.code() == code && h.packet_id() == packet_id;
			}
		);
	}

	fast_replies::iterator find_fast_reply(
		control_code_e code, uint16_t packet_id
	) {
		return std::find_if(
			_fast_replies.begin(), _fast_replies.end(),
			[code, packet_id](const auto& f) {
				return f.code == code && f.packet_id == packet_id;
			}
		);
	}

};

} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_REPLIES_HPP
