#include "sqlitepp/statement.h"
#include "sqlitepp/database.h"
#include "sqlitepp/error_code.h"

#include "sqlite3.h"

namespace sqlitepp {
	statement::statement(database& p, const std::string& query)
		: m_db(&p), m_handle(nullptr) {
		int res = sqlite3_prepare_v2(m_db->raw(), query.data(), query.size(), &m_handle, NULL);
		throw_if_error(res, m_handle);
	}

	statement::statement(statement&& other)
		: m_db(other.m_db), m_handle(other.m_handle) {
		other.m_handle = nullptr;
	}

	statement& statement::operator=(statement&& other) {
		if (m_handle) sqlite3_finalize(m_handle);
		m_db = other.m_db;
		m_handle = other.m_handle;
		other.m_handle = nullptr;
		return *this;
	}

	statement::~statement() noexcept {
		if (m_handle) sqlite3_finalize(m_handle);
	}

	const char* statement::query() const {
		return sqlite3_sql(m_handle);
	}

	bool statement::is_readonly() const noexcept {
		return sqlite3_stmt_readonly(m_handle) != 0;
	}

#ifdef __cpp_lib_string_view
	void statement::bind(size_t idx, const std::string_view& str) {
		int res = sqlite3_bind_text64(m_handle, idx, str.data(), str.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
		throw_if_error(res, m_handle);
	}

	void statement::bind(size_t idx, const std::wstring_view& str) {
		int res = sqlite3_bind_text64(m_handle, idx, reinterpret_cast<const char*>(str.data()), str.size(), SQLITE_TRANSIENT, SQLITE_UTF16);
		throw_if_error(res, m_handle);
	}

	void statement::bind(size_t idx, const std::string_view& data, bool is_blob) {
		if (is_blob) {
			int res = sqlite3_bind_blob64(m_handle, idx, data.data(), data.size(), SQLITE_TRANSIENT);
			throw_if_error(res, m_handle);
		} else {
			int res = sqlite3_bind_text64(m_handle, idx, data.data(), data.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
			throw_if_error(res, m_handle);
		}
	}
#else
	void statement::bind(size_t idx, const std::string& str) {
		int res = sqlite3_bind_text64(m_handle, idx, str.data(), str.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
		throw_if_error(res, m_handle);
	}

	void statement::bind(size_t idx, const std::wstring& str) {
		int res = sqlite3_bind_text64(m_handle, idx, reinterpret_cast<const char*>(str.data()), str.size(), SQLITE_TRANSIENT, SQLITE_UTF16);
		throw_if_error(res, m_handle);
	}

	void statement::bind(size_t idx, const std::string& data, bool is_blob) {
		if (is_blob) {
			int res = sqlite3_bind_blob64(m_handle, idx, data.data(), data.size(), SQLITE_TRANSIENT);
			throw_if_error(res, m_handle);
		} else {
			int res = sqlite3_bind_text64(m_handle, idx, data.data(), data.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
			throw_if_error(res, m_handle);
		}
	}
#endif

	void statement::bind(size_t idx, const std::vector<uint8_t>& blob) {
		int res = sqlite3_bind_blob64(m_handle, idx, blob.data(), blob.size(), SQLITE_TRANSIENT);
		throw_if_error(res, m_handle);
	}

	void statement::bind(size_t idx, std::nullptr_t) {
		int res = sqlite3_bind_null(m_handle, idx);
		throw_if_error(res, m_handle);
	}

	void statement::bind(size_t idx, double val) {
		int res = sqlite3_bind_double(m_handle, idx, val);
		throw_if_error(res, m_handle);
	}

	void statement::bind(size_t idx, int val) { bind(idx, static_cast<int64_t>(val)); }

	void statement::bind(size_t idx, int64_t val) {
		int res = sqlite3_bind_int64(m_handle, idx, val);
		throw_if_error(res, m_handle);
	}

	size_t statement::param_count() const noexcept { return sqlite3_bind_parameter_count(m_handle); }
	const char* statement::param_name(size_t idx) const noexcept { return sqlite3_bind_parameter_name(m_handle, idx); }
	size_t statement::param_name(const char* name) const noexcept { return sqlite3_bind_parameter_index(m_handle, name); }
	size_t statement::param_name(const std::string& name) const { return param_name(name.c_str()); }

	result_iterator statement::iterator() {
		return result_iterator(m_handle);
	}

	void statement::execute() {
		auto it = iterator();
		// Calling next() calls sqlite step, executing the statement
		it.next();
	}
} // namespace sqlitepp