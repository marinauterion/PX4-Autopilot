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

#include "log_writer_ram.h"

#include <inttypes.h>
#include <px4_platform_common/log.h>

using namespace px4::logger;

LogWriterRam::~LogWriterRam()
{
	delete[] _scratch;
	delete[] _evict_scratch;
	delete[] _timestamps;
}

bool LogWriterRam::init(size_t buffer_size_bytes, size_t max_record_size, size_t max_records, hrt_abstime max_age)
{
	_max_age = max_age;

	_scratch_len = max_record_size;
	_scratch = new uint8_t[_scratch_len];
	_evict_scratch = new uint8_t[_scratch_len];

	_timestamps_capacity = max_records;
	_timestamps = new hrt_abstime[_timestamps_capacity];

	if (!_scratch || !_evict_scratch || !_timestamps || !_ringbuffer.allocate(buffer_size_bytes)) {
		delete[] _scratch;
		delete[] _evict_scratch;
		delete[] _timestamps;

		_scratch = nullptr;
		_evict_scratch = nullptr;
		_timestamps = nullptr;

		return false;
	}

	_initialized = true;
	return true;
}

void LogWriterRam::write(const void *data, size_t data_len, hrt_abstime timestamp)
{
	if (!_initialized || data_len > _scratch_len || data_len == 0
	    || (_record_count > 0 && timestamp < _timestamps[_timestamps_start])) {
		return;
	}

	// drop all outdated records
	while (_record_count > 0 && (timestamp - _timestamps[_timestamps_start]) > _max_age) {
		evict_oldest_record();
	}

	PX4_INFO("write record: max_len=%zu, timestamp=%" PRIu64 ", count_max=%zu", _max_len, timestamp, _record_count_max);

	// max_records limit reached - free first
	while (_record_count >= _timestamps_capacity && _record_count > 0) {
		PX4_ERR("max_records limit reached, evicting oldest record");
		evict_oldest_record();
	}

	bool written = _ringbuffer.push_back(static_cast<const uint8_t *>(data), data_len);

	if (data_len > _max_len) {
		_max_len = data_len;
	}

	// out of byte-buffer space: evict oldest record(s) and retry
	while (!written && _record_count > 0) {
		evict_oldest_record();
		written = _ringbuffer.push_back(static_cast<const uint8_t *>(data), data_len);
	}

	if (written) {
		const size_t idx = (_timestamps_start + _record_count) % _timestamps_capacity;
		_timestamps[idx] = timestamp;
		_record_count++;

		if (_record_count > _record_count_max) {
			_record_count_max = _record_count;
		}
	}
}

void LogWriterRam::evict_oldest_record()
{
	if (_record_count > 0) {
		_ringbuffer.pop_front(_evict_scratch, _scratch_len);
		_timestamps_start = (_timestamps_start + 1) % _timestamps_capacity;
		_record_count--;
	}
}
