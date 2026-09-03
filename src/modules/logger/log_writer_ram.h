/****************************************************************************
 *
 *   Copyright (c) 2026 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#pragma once

#include <lib/variable_length_ringbuffer/VariableLengthRingbuffer.hpp>
#include <drivers/drv_hrt.h>

namespace px4
{
namespace logger
{

class LogWriterRam
{
public:
	LogWriterRam() = default;
	~LogWriterRam();

	/**
	 * Allocate the ring buffer and its per-record timestamp bookkeeping.
	 * @param buffer_size_bytes total byte budget for buffered record payloads
	 * @param max_record_size largest single record that can ever be written
	 * @param max_records maximum number of individually tracked (timestamped) records
	 * @param max_age records older than this are evicted automatically
	 * @return false if any allocation failed
	 */
	bool init(size_t buffer_size_bytes, size_t max_record_size, size_t max_records, hrt_abstime max_age);

	void write(const void *data, size_t data_len, hrt_abstime timestamp);

	size_t count() const { return _record_count; }

	/**
	 * Replay all currently buffered records in FIFO order, then
	 * clear the buffer.
	 * @param callback void(const void *data, size_t data_len)
	 */
	template<typename Callback>
	void drain(Callback callback)
	{
		while (_record_count > 0) {
			const size_t n = _ringbuffer.pop_front(_scratch, _scratch_len);

			if (n == 0) {
				break; // should not happen, buffer corrupted/empty
			}

			callback(_scratch, n);

			_timestamps_start = (_timestamps_start + 1) % _timestamps_capacity;
			_record_count--;
		}
	}

private:
	void evict_oldest_record();

	VariableLengthRingbuffer _ringbuffer;
	uint8_t *_scratch{nullptr};
	uint8_t *_evict_scratch{nullptr};
	size_t _scratch_len{0};

	hrt_abstime *_timestamps{nullptr}; // circular buffer of timestamps
	size_t _timestamps_capacity{0};
	size_t _timestamps_start{0}; // pointing to the oldest valid timestamp
	size_t _record_count{0};
	size_t _record_count_max{0}; // for debug stats
	size_t _max_len{0}; // for debug stats

	hrt_abstime _max_age{0};

	bool _initialized{false};
};

} // namespace logger
} // namespace px4
