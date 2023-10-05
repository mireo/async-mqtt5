#include <fmt/format.h>
#include <boost/asio/recycling_allocator.hpp>

#include "../../alloc/memory.h"
#include "../../alloc/string.h"
#include "../../alloc/vector.h"

namespace asio = boost::asio;

struct bx {
	pma::string s1, s2;

	bx(std::string v1, std::string v2, const pma::alloc<size_t>& alloc) :
		s1(v1.begin(), v1.end(), alloc),
		s2(v2.begin(), v2.end(), alloc)
	{}
};

void test_memory() {
	asio::recycling_allocator<char> base_alloc;
	pma::resource_adaptor<asio::recycling_allocator<char>> mem_res { base_alloc };
	auto alloc = pma::alloc<char>(&mem_res);

	pma::string s1 { "abcdefgrthoasofjasfasf", alloc };
	pma::string s2 { alloc }; 
	s2 = s1;

	//TODO: the commented lines do not compile on Windows
	//pma::vector<char> v1 { { 'a', 'b'}, alloc };
	pma::vector<char> v2 { alloc };
	//v2 = std::move(v1);
	//pma::vector<char> v3 = v2;
	//pma::vector<char> v4 = std::move(v3);
	//v1.swap(v2);

	bx vbx { "ABCD", "EFGH", alloc };

	fmt::print(stderr, "String = {}, is equal: {}\n", s2,
		s1.get_allocator() == vbx.s2.get_allocator()
	);

	//fmt::print(stderr, "Vector allocators are equal: {}\n", 
	//	v1.get_allocator() == v2.get_allocator()
	//);

	std::allocator_traits<decltype(alloc)>::rebind_alloc<std::string> char_alloc = 
		alloc;

}

