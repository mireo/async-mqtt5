#include <boost/test/unit_test.hpp>

#include <test_common/protocol_logging.hpp>

namespace {

void setup_mqtt() {
	async_mqtt5::test::logging_enabled() = true;
}

}

#ifdef BOOST_TEST_DYN_LINK
static bool init_tests() {
	setup_mqtt();
	return true;
}

int main(int argc, char* argv[]) {
	return boost::unit_test::unit_test_main(&init_tests, argc, argv);
}
#else
boost::unit_test::test_suite* init_unit_test_suite(int, char** const) {
	setup_mqtt();
	return nullptr;
}
#endif

/*
* usage: ./mqtt-test [boost test --arg=val]*
* example: ./mqtt-test --log_level=test_suite
*
* all boost test parameters can be found here:
* https://www.boost.org/doc/libs/1_82_0/libs/test/doc/html/boost_test/runtime_config/summary.html
*/
