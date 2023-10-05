import glob

Import('ctx')

ctx.Project('#/3rdParty/openssl')

sources = [
	'example/tcp.cpp',
	# commented out to speed up compiling
	# 'example/openssl-tls.cpp',
	# 'example/websocket-tcp.cpp',
	# 'example/websocket-tls.cpp',
	'example/src/run_examples.cpp',
]

test_sources = [
	# 'test/experimental/cancellation.cpp',
	# 'test/experimental/message_assembling.cpp',
	# 'test/experimental/memory.cpp',
	# 'test/experimental/mutex.cpp',
	# 'test/experimental/uri_parse.cpp',
	'test/unit/test/serialization.cpp',
	'test/unit/test/publish_send_op.cpp',
	'test/unit/test/client_broker.cpp',
	'test/unit/test/coroutine.cpp',
	'test/unit/src/run_tests.cpp'
]

includes = [
	'include',
	'#/3rdParty/openssl/include'
]

test_includes = [
	'include',
	'test/unit/include',
	'#/3rdParty/openssl/include'
]

libs = {
	'all': Split('openssl'),
}

defines = {
	'all' : ['BOOST_ALL_NO_LIB', 'BOOST_NO_TYPEID', '_REENTRANT'],
	'toolchain:llvm' : ['BOOST_FILESYSTEM_NO_CXX20_ATOMIC_REF'],
}

test_defines = {
	'all' : ['BOOST_ALL_NO_LIB', 'BOOST_NO_TYPEID', 'BOOST_TEST_NO_MAIN=1','_REENTRANT'],
	'toolchain:llvm' : ['BOOST_FILESYSTEM_NO_CXX20_ATOMIC_REF'],
}

# add ' -ftemplate-backtrace-limit=1' to cxxflags when necessary
cxxflags = {
	'all': Split('-fexceptions -frtti -Wall -Wno-unused-local-typedefs -ftemplate-backtrace-limit=1'),
}

frameworks = {
	'os:macos': Split('Security'),
}

ctx.Program(name='mqtt-examples',
	source=sources,
	includes=includes,
	defines=defines,
	CXXFLAGs=cxxflags,
	libraries=libs,
	frameworks=frameworks,
)

ctx.Program(name='mqtt-tests',
	source=test_sources,
	includes=test_includes,
	defines=test_defines,
	CXXFLAGs=cxxflags,
	frameworks=frameworks,
)
