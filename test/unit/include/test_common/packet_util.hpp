#ifndef ASYNC_MQTT5_TEST_PACKET_UTIL_HPP
#define ASYNC_MQTT5_TEST_PACKET_UTIL_HPP

#include <string>

#include <async_mqtt5/detail/control_packet.hpp>

#include <async_mqtt5/impl/codecs/message_decoders.hpp>
#include <async_mqtt5/impl/codecs/message_encoders.hpp>

namespace async_mqtt5::test {

using error_code = boost::system::error_code;

inline qos_e extract_qos(uint8_t flags) {
	auto byte = (flags & 0b0110) >> 1;
	return qos_e(byte);
}

inline control_code_e extract_code(uint8_t control_byte) {
	constexpr uint8_t mask = 0b11110000;
	constexpr uint8_t publish_bits = 0b0011;
	constexpr uint8_t special_mask = 0b00000010;
	constexpr control_code_e codes_with_non_zero_end[] = {
		control_code_e::pubrel, control_code_e::subscribe,
		control_code_e::unsubscribe
	};

	if ((control_byte >> 4) == publish_bits)
		return control_code_e::publish;
	if ((control_byte & mask) == control_byte)
		return control_code_e(control_byte & mask);

	for (const auto& special_code : codes_with_non_zero_end)
		if (control_byte == (uint8_t(special_code) | special_mask))
			return special_code;

	return control_code_e::no_packet;
}


inline std::string_view code_to_str(control_code_e code) {
	switch (code) {
		case control_code_e::connect: return "CONNECT";
		case control_code_e::connack: return "CONNACK";
		case control_code_e::publish: return "PUBLISH";
		case control_code_e::puback: return "PUBACK";
		case control_code_e::pubrec: return "PUBREC";
		case control_code_e::pubrel: return "PUBREL";
		case control_code_e::pubcomp: return "PUBCOMP";
		case control_code_e::subscribe: return "SUBSCRIBE";
		case control_code_e::suback: return "SUBACK";
		case control_code_e::unsubscribe: return "UNSUBSCRIBE";
		case control_code_e::unsuback: return "UNSUBACK";
		case control_code_e::auth: return "AUTH";
		case control_code_e::disconnect: return "DISCONNECT";
		case control_code_e::pingreq: return "PINGREQ";
		case control_code_e::pingresp: return "PINGRESP";
	}
	return "UNKNOWN";
}

inline std::string to_readable_packet(
	std::string packet, error_code ec = {}, bool incoming = false
) {
	auto control_byte = uint8_t(*packet.data());
	auto code = extract_code(control_byte);

	if (code == control_code_e::no_packet)
		return {};

	std::ostringstream stream;

	if (incoming)
		stream << "-> ";

	if (
		code == control_code_e::connect || code == control_code_e::connack ||
		code == control_code_e::disconnect
	) {
		stream << code_to_str(code) << (ec ? " ec: " + ec.message() : "");
		return stream.str();
	}

	auto begin = ++packet.cbegin();
	auto varlen = decoders::type_parse(
		begin, packet.cend(), decoders::basic::varint_
	);

	if (code == control_code_e::publish) {
		auto publish = decoders::decode_publish(
			control_byte, *varlen, begin
		);
		auto& [topic, packet_id, flags, props, payload] = *publish;
		stream << code_to_str(code);
		stream << (packet_id ? " " + std::to_string(*packet_id) : "");
		return stream.str();
	}

	const auto packet_id = decoders::decode_packet_id(begin).value();
	stream << code_to_str(code) << " " << packet_id;
	return stream.str();
}

template <typename ConstBufferSequence>
std::vector<std::string> to_packets(const ConstBufferSequence& buffers) {
	std::vector<std::string> content;
	
	for (const auto& buff : buffers) {
		auto control_byte = *(const uint8_t*) buff.data();
		auto code = extract_code(control_byte);

		if (code == control_code_e::pingreq)
			continue;

		content.push_back({ (const char*)buff.data(), buff.size() });
	}

	return content;
}


} // end namespace async_mqtt5::test

#endif // ASYNC_MQTT5_TEST_PACKET_UTIL_HPP
