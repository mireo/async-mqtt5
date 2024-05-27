//
// Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASYNC_MQTT5_SPINLOCK_HPP
#define ASYNC_MQTT5_SPINLOCK_HPP

#include <atomic>

namespace async_mqtt5::detail {

#if defined(_MSC_VER)
  /* prefer using intrinsics directly instead of winnt.h macro */
  /* http://software.intel.com/en-us/forums/topic/296168 */
  //#include <intrin.h>
  #if defined(_M_AMD64) || defined(_M_IX86)
  #pragma intrinsic(_mm_pause)
  #define __pause()  _mm_pause()
  /* (if pause not supported by older x86 assembler, "rep nop" is equivalent)*/
  /*#define __pause()  __asm rep nop */
  #elif defined(_M_IA64)
  #pragma intrinsic(__yield)
  #define __pause()  __yield()
  #else
  #define __pause()  YieldProcessor()
  #endif
#elif defined(__x86_64__) || defined(__i386__)
  #define __pause()  __asm__ __volatile__ ("pause")
#elif defined(__arm__) || defined(__arm64__) || defined(__aarch64__)
  #define __pause()  __asm__ __volatile__ ("yield")
#endif

// https://rigtorp.se/spinlock/

class spinlock {
	std::atomic<bool> lock_ { false };
public:
	void lock() noexcept {
		for (;;) {
			// Optimistically assume the lock is free on the first try
			if (!lock_.exchange(true, std::memory_order_acquire))
				return;
			// Wait for lock to be released without generating cache misses
			while (lock_.load(std::memory_order_relaxed)) __pause();
		}
	}

	bool try_lock() noexcept {
		// First do a relaxed load to check if lock is free in order to prevent
		// unnecessary cache misses if someone does while(!try_lock())
		return !lock_.load(std::memory_order_relaxed) &&
			!lock_.exchange(true, std::memory_order_acquire);
	}

	void unlock() noexcept {
		lock_.store(false, std::memory_order_release);
	}
};

} // end namespace async_mqtt5::detail

#endif // !ASYNC_MQTT5_SPINLOCK_HPP
