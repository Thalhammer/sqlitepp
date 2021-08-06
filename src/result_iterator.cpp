#include "sqlitepp/result_iterator.h"
#include "sqlitepp/error_code.h"
#include <sqlite3.h>
#include <cstring>

namespace sqlitepp {

	void result_iterator::get(size_t idx, double& val) const {
		if(idx >= column_count())
			throw_if_error(SQLITE_RANGE, m_handle);
		val = sqlite3_column_double(m_handle, idx);
	}

	void result_iterator::get(size_t idx, int64_t& val) const {
		if(idx >= column_count())
			throw_if_error(SQLITE_RANGE, m_handle);
		val = sqlite3_column_int64(m_handle, idx);
	}

	void result_iterator::get(size_t idx, std::string& val) const {
		if(idx >= column_count())
			throw_if_error(SQLITE_RANGE, m_handle);
		val = std::string(reinterpret_cast<const char*>(sqlite3_column_text(m_handle, idx)), sqlite3_column_bytes(m_handle, idx));
	}

	void result_iterator::get(size_t idx, std::vector<uint8_t>& val) const {
		if(idx >= column_count())
			throw_if_error(SQLITE_RANGE, m_handle);
		auto ptr = static_cast<const uint8_t*>(sqlite3_column_blob(m_handle, idx));
		auto size = sqlite3_column_bytes(m_handle, idx);
		val.reserve(size);
		val.assign(ptr, ptr + size);
	}

    result_iterator::result_iterator(result_iterator&& o)
		: m_handle(o.m_handle), m_has_row(o.m_has_row)
	{
		o.m_handle = nullptr;
	}

	result_iterator::~result_iterator() {
		if(m_handle) sqlite3_reset(m_handle);
	}

	bool result_iterator::is_valid() const noexcept { return m_handle != nullptr; }

	bool result_iterator::next() {
		int ec = sqlite3_step(m_handle);
		throw_if_error(ec, m_handle);
		m_has_row = ec == SQLITE_ROW;
		return m_has_row;
	}

	bool result_iterator::done() const noexcept {
		return !m_has_row;
	}
    
	size_t result_iterator::column_count() const noexcept {
		return sqlite3_column_count(m_handle);
	}

	const char* result_iterator::column_name(size_t idx) const {
		if(idx >= column_count())
			throw_if_error(SQLITE_RANGE, m_handle);
		return sqlite3_column_name(m_handle, idx);
	}

	size_t result_iterator::column_index(const std::string& name) const {
		for(size_t i = 0; i < column_count(); i++) {
			auto col = column_name(i);
			if(!col || name.compare(col) != 0) continue;
			return i;
		}
		throw_if_error(SQLITE_RANGE, m_handle);
		abort(); // Never reached as the above always throws
	}

	int result_iterator::column_type(size_t idx) const {
		if(idx >= column_count())
			throw_if_error(SQLITE_RANGE, m_handle);
		return sqlite3_column_type(m_handle, idx);
	}

	bool result_iterator::column_is_null(size_t idx) const {
		return column_type(idx) == SQLITE_NULL;
	}

	double result_iterator::column_double(size_t idx) const {
		if(idx >= column_count())
			throw_if_error(SQLITE_RANGE, m_handle);
		return sqlite3_column_double(m_handle, idx);
	}

	int64_t result_iterator::column_int64(size_t idx) const {
		if(idx >= column_count())
			throw_if_error(SQLITE_RANGE, m_handle);
		return sqlite3_column_int64(m_handle, idx);
	}

	// TODO: Stringview ?
	std::pair<const char*, size_t> result_iterator::column_text(size_t idx) const {
		if(idx >= column_count())
			throw_if_error(SQLITE_RANGE, m_handle);
		return { reinterpret_cast<const char*>(sqlite3_column_text(m_handle, idx)), sqlite3_column_bytes(m_handle, idx) };
	}

	std::pair<const void*, size_t> result_iterator::column_blob(size_t idx) const {
		if(idx >= column_count())
			throw_if_error(SQLITE_RANGE, m_handle);
		return { sqlite3_column_blob(m_handle, idx), sqlite3_column_bytes(m_handle, idx) };
	}

	void result_iterator::column_blob(size_t idx, std::vector<uint8_t>& out) const {
		auto res = column_blob(idx);
		out.resize(res.second);
		memcpy(out.data(), res.first, res.second);
	}

	std::string result_iterator::column_string(size_t idx) const {
	    auto txt = column_text(idx);
	    return std::string(txt.first, txt.second);
	}

	int result_iterator::column_type(const std::string& name) const {
		return column_type(column_index(name));
	}

	bool result_iterator::column_is_null(const std::string& name) const {
		return column_is_null(column_index(name));
	}

	double result_iterator::column_double(const std::string& name) const {
		return column_double(column_index(name));
	}

	int64_t result_iterator::column_int64(const std::string& name) const {
		return column_int64(column_index(name));
	}

		// TODO: Stringview ?
	std::pair<const char*, size_t> result_iterator::column_text(const std::string& name) const {
		return column_text(column_index(name));
	}

	std::pair<const void*, size_t> result_iterator::column_blob(const std::string& name) const {
		return column_blob(column_index(name));
	}

	void result_iterator::column_blob(const std::string& name, std::vector<uint8_t>& out) const {
		column_blob(column_index(name), out);
	}

	std::string result_iterator::column_string(const std::string& name) const {
		return column_string(column_index(name));
	}
}
