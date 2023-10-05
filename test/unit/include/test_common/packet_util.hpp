#ifndef ASYNC_MQTT5_TEST_PACKET_UTIL_HPP
#define ASYNC_MQTT5_TEST_PACKET_UTIL_HPP

#include <unordered_map>
#include <string>

#include <async_mqtt5/impl/internal/codecs/message_decoders.hpp>
#include <async_mqtt5/impl/internal/codecs/message_encoders.hpp>
#include <async_mqtt5/detail/control_packet.hpp> 

namespace async_mqtt5::test {


inline qos_e extract_qos(uint8_t flags) {
	auto byte = (flags & 0b0110) >> 1;
	return qos_e(byte);
}

inline control_code_e extract_code(uint8_t control_byte) {
	using enum control_code_e;

	constexpr uint8_t mask = 0b11110000;
	constexpr uint8_t publish_bits = 0b0011;
	constexpr uint8_t special_mask = 0b00000010;
	constexpr control_code_e codes_with_non_zero_end[] = {
		pubrel, subscribe, unsubscribe
	};

	if ((control_byte >> 4) == publish_bits)
		return publish;
	if ((control_byte & mask) == control_byte)
		return control_code_e(control_byte & mask);

	for (const auto& special_code : codes_with_non_zero_end)
		if (control_byte == (uint8_t(special_code) | special_mask))
			return special_code;

	return no_packet;
}


inline std::string_view code_to_str(control_code_e code) {
	using enum control_code_e;

	switch (code) {
		case connect: return "CONNECT";
		case connack: return "CONNACK";
		case publish: return "PUBLISH";
		case puback: return "PUBACK";
		case pubrec: return "PUBREC";
		case pubrel: return "PUBREL";
		case pubcomp: return "PUBCOMP";
		case subscribe: return "SUBSCRIBE";
		case suback: return "SUBACK";
		case unsubscribe: return "UNSUBSCRIBE";
		case unsuback: return "UNSUBACK";
		case auth: return "AUTH";
		case disconnect: return "DISCONNECT";
		case pingreq: return "PINGREQ";
		case pingresp: return "PINGRESP";
	}
	return "UNKNOWN";
}

inline std::string to_readable_packet(
	std::string packet, error_code ec = {}, bool incoming = false
) {
	using enum control_code_e;

	auto control_byte = uint8_t(*packet.data());
	auto code = extract_code(control_byte);

	if (code == no_packet)
		return {};

	std::ostringstream stream;

	if (incoming)
		stream << "-> ";

	if (code == connect || code == connack || code == disconnect) {
		stream << code_to_str(code) << (ec ? " ec: " + ec.message() : "");
		return stream.str();
	}

	auto begin = ++packet.cbegin();
	auto varlen = decoders::type_parse(
		begin, packet.cend(), decoders::basic::varint_
	);

	if (code == publish) {
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
