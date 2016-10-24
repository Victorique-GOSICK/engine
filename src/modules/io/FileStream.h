/**
 * @file
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <cstdarg>
#include <SDL_endian.h>
#include "core/Common.h"

namespace io {

class File;

/**
 * @brief Little endian file stream
 */
class FileStream {
private:
	int64_t _pos = 0;
	int64_t _size = 0;
	mutable SDL_RWops *_rwops;

public:
	FileStream(File* file);
	FileStream(SDL_RWops* rwops);
	virtual ~FileStream();

	inline int64_t remaining() const {
		return _size - _pos;
	}

	bool addBool(bool value);
	bool addByte(uint8_t byte);
	bool addShort(int16_t word);
	bool addInt(int32_t dword);
	bool addLong(int64_t dword);
	bool addFloat(float value);
	bool addString(const std::string& string);
	bool addFormat(const char *fmt, ...);

	/**
	 * @return A value of @c 0 indicates no error
	 */
	template<class Ret>
	int peek(Ret& val) const {
		const size_t bufSize = sizeof(Ret);
		if (remaining() < (int64_t)bufSize) {
			return -1;
		}
		uint8_t buf[bufSize];
		int amount = bufSize;
		SDL_RWseek(_rwops, _pos, RW_SEEK_SET);
		for (;;) {
			const int read = SDL_RWread(_rwops, &buf[bufSize - amount], amount, 1);
			if (read <= 0) {
				return -1;
			}
			amount -= read;
			if (amount == 0) {
				break;
			}
			// we should never get negative here
			core_assert_always(amount > 0);
		}
		const Ret *word = (const Ret*) (void*) buf;
		val = *word;
		return 0;
	}

	template<class Type>
	inline bool write(Type val) {
		SDL_RWseek(_rwops, _pos, RW_SEEK_SET);
		const size_t bufSize = sizeof(Type);
		uint8_t buf[bufSize];
		for (size_t i = 0; i < bufSize; ++i) {
			buf[i] = uint8_t(val >> (i * CHAR_BIT));
		}
		int amount = bufSize;
		do {
			const size_t written = SDL_RWwrite(_rwops, &buf[bufSize - amount], amount, 1);
			if (written == 0) {
				return false;
			}
			amount -= written;
			// we should never get negative here
			core_assert_always(amount > 0);
		} while (amount > 0);
		_pos += sizeof(val);
		return true;
	}

	template<class Ret>
	inline int read(Ret& val) {
		const int retVal = peek<Ret>(val);
		if (retVal == 0) {
			_pos += sizeof(val);
		}
		return retVal;
	}

	bool readBool();
	int readByte(int8_t& val);
	int readShort(int16_t& val);
	int readInt(int32_t& val);
	int readLong(int64_t& val);
	int readFloat(float& val);
	/**
	 * @brief Read a fixed-width string from a file. It may be null-terminated, but
	 * the position of the stream is still advanced by the given length
	 * @param[in] length The fixed length of the string in the file and the min length
	 * of the output buffer.
	 * @param{out] strbuff The output buffer
	 */
	bool readString(int length, char *strbuff);
	bool readFormat(const char *fmt, ...);

	int peekInt(int32_t& val) const;
	int peekShort(int16_t& val) const;
	int peekByte(int8_t& val) const;

	void append(const uint8_t *buf, size_t size);

	bool empty() const;

	int64_t skip(int64_t delta);

	// return the amount of bytes in the buffer
	int64_t size() const;

	int64_t pos() const;

	FileStream &operator<<(const uint8_t &x) {
		addByte(x);
		return *this;
	}

	FileStream &operator<<(const int16_t &x) {
		addShort(x);
		return *this;
	}

	FileStream &operator<<(const bool &x) {
		addBool(x);
		return *this;
	}

	FileStream &operator<<(const int32_t &x) {
		addInt(x);
		return *this;
	}

	FileStream &operator<<(const float &x) {
		addFloat(x);
		return *this;
	}

	FileStream &operator<<(const std::string &x) {
		addString(x);
		return *this;
	}
};

inline bool FileStream::empty() const {
	return _size <= 0;
}

inline void FileStream::append(const uint8_t *buf, size_t size) {
	// TODO: optimize
	for (std::size_t i = 0; i < size; ++i) {
		addByte(buf[i]);
	}
}

inline bool FileStream::addBool(bool value) {
	return addByte(value);
}

inline bool FileStream::readBool() {
	int8_t boolean;
	if (readByte(boolean) != 0) {
		return false;
	}
	return boolean != 0;
}

inline int64_t FileStream::skip(int64_t delta) {
	_pos += delta;
	if (_pos >= _size) {
		_pos = _size;
	} else if (_pos < 0) {
		_pos = 0;
	}
	return _pos;
}

inline int64_t FileStream::size() const {
	return _size;
}

inline int64_t FileStream::pos() const {
	return _pos;
}

}