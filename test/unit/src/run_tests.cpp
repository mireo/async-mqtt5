#include <boost/test/unit_test.hpp>

#include <test_common/protocol_logging.hpp>

boost::unit_test::test_suite* init_tests(
	int /*argc*/, char* /*argv*/[]
) {
	async_mqtt5::test::logging_enabled() = true;
	return nullptr;
}

int main(int argc, char* argv[]) {
	return boost::unit_test::unit_test_main(&init_tests, argc, argv);
}

/*
* usage: ./mqtt-test [boost test --arg=val]*
* example: ./mqtt-test --log_level=test_suite
*
* all boost test parameters can be found here:
* https://www.boost.org/doc/libs/1_82_0/libs/test/doc/html/boost_test/runtime_config/summary.html
*/
