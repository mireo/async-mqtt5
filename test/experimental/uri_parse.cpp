#include <boost/spirit/home/x3.hpp>

template <typename T>
static constexpr auto to_(T& arg) {
	return [&](auto& ctx) { arg = boost::spirit::x3::_attr(ctx); };
}

template <typename T, typename Parser>
static constexpr auto as_(Parser&& p){
	return boost::spirit::x3::rule<struct _, T>{} = std::forward<Parser>(p);
}

void test_uri_parser(){
	struct authority_path { std::string host, port, path; };
	std::vector<authority_path> _servers;

	std::string brokers = "iot.fcluster.mireo.hr:1234, fc/nesto";
	std::string default_port = "8883";

	namespace x3 = boost::spirit::x3;

	std::string host, port, path;

	// loosely based on RFC 3986
	auto unreserved_ = x3::char_("-a-zA-Z_0-9._~");
	auto digit_ = x3::char_("0-9");
	auto separator_ = x3::char_(',');

	auto host_ = as_<std::string>(+unreserved_)[to_(host)];
	auto port_ = as_<std::string>(':' >> +digit_)[to_(port)];
	auto path_ = as_<std::string>('/' >> *unreserved_)[to_(path)];
	auto uri_ = *x3::omit[x3::space] >> (host_ >> *port_ >> *path_) >> 
		(*x3::omit[x3::space] >> x3::omit[separator_ | x3::eoi]);

	for (auto b = brokers.begin(); b != brokers.end(); ) {
		host.clear(); port.clear(); path.clear();
		if (phrase_parse(b, brokers.end(), uri_, x3::eps(false))) {
			_servers.push_back({ 
				std::move(host), port.empty() ? default_port : std::move(port),
				std::move(path)
			});
		}
		else b = brokers.end();
	}
}

