#ifndef ASYNC_MQTT5_TEST_PROTOCOL_LOGGING_HPP
#define ASYNC_MQTT5_TEST_PROTOCOL_LOGGING_HPP

#include <string>

namespace async_mqtt5::test {

inline bool& logging_enabled() {
	static bool enabled = false;
	return enabled;
}

inline void log(const std::string& message) {
	if (logging_enabled())
		fprintf(stderr, "%s\n", message.c_str());
}


} // end namespace async_mqtt5::test

#endif // ASYNC_MQTT5_TEST_PROTOCOL_LOGGING_HPP
