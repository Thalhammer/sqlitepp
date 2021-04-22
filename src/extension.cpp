#include "sqlitepp/extension.h"
#include "sqlitepp/database.h"

#include "sqlite3.h"

namespace sqlitepp {
	sqlite3_value* inplace_value::value_from_this() const noexcept {
		return reinterpret_cast<sqlite3_value*>(const_cast<inplace_value*>(this));
	}

	inplace_value* inplace_value::from_value(sqlite3_value* val) {
		return reinterpret_cast<inplace_value*>(val);
	}

	inplace_value::type inplace_value::get_type() const noexcept {
		return static_cast<type>(sqlite3_value_type(value_from_this()));
	}

	inplace_value::type inplace_value::get_numeric_type() const noexcept {
		return static_cast<type>(sqlite3_value_numeric_type(value_from_this()));
	}

	std::pair<const void*, size_t> inplace_value::as_blob() const noexcept {
		if (get_type() != type::blob)
			return {nullptr, 0};
		auto v = value_from_this();
		auto ptr = sqlite3_value_blob(v);
		auto size = sqlite3_value_bytes(v);
		return {ptr, size};
	}

	double inplace_value::as_double() const noexcept {
		if (get_type() != type::real) return 0;
		return sqlite3_value_double(value_from_this());
	}

	int64_t inplace_value::as_int64() const noexcept {
		if (get_type() != type::integer) return 0;
		return sqlite3_value_int64(value_from_this());
	}

	int32_t inplace_value::as_int32() const noexcept {
		if (get_type() != type::integer) return 0;
		return sqlite3_value_int(value_from_this());
	}

	void* inplace_value::as_pointer(const char* type) const noexcept {
		return sqlite3_value_pointer(value_from_this(), type);
	}

	std::string inplace_value::as_string() const noexcept {
		if (get_type() != type::text) return {};
		auto v = value_from_this();
		auto ptr = reinterpret_cast<const char*>(sqlite3_value_text(v));
		if (!ptr) return {};
		return std::string(ptr, sqlite3_value_bytes(v));
	}

	std::wstring inplace_value::as_wstring() const noexcept {
		if (get_type() != type::text) return {};
		auto v = value_from_this();
		auto ptr = reinterpret_cast<const wchar_t*>(sqlite3_value_text16(v));
		if (!ptr) return {};
		return std::wstring(ptr, sqlite3_value_bytes16(v) / 2);
	}

	bool inplace_value::is_unchanged() const noexcept {
		return sqlite3_value_nochange(value_from_this()) != 0;
	}

	bool inplace_value::is_from_bind() const noexcept {
#if SQLITE_VERSION_NUMBER > 3028000
		return sqlite3_value_frombind(value_from_this()) != 0;
#else
		return false;
#endif
	}

	sqlite3_context* inplace_context::context_from_this() const noexcept {
		return reinterpret_cast<sqlite3_context*>(const_cast<inplace_context*>(this));
	}

	inplace_context* inplace_context::from_context(sqlite3_context* val) {
		return reinterpret_cast<inplace_context*>(val);
	}

	sqlite3* inplace_context::get_db_handle() const noexcept {
		return sqlite3_context_db_handle(context_from_this());
	}

	database inplace_context::get_db() const noexcept {
		return database{get_db_handle(), false};
	}

	void* inplace_context::aggregate_context(size_t nbytes) noexcept {
		return sqlite3_aggregate_context(context_from_this(), nbytes);
	}

	void* inplace_context::get_auxdata(int N) const noexcept {
		return sqlite3_get_auxdata(context_from_this(), N);
	}

	void inplace_context::set_auxdata(int N, void* ptr, void (*destructor)(void*)) noexcept {
		sqlite3_set_auxdata(context_from_this(), N, ptr, destructor);
	}

	void* inplace_context::get_userdata() const noexcept {
		return sqlite3_user_data(context_from_this());
	}

	void inplace_context::error(const std::string& e) noexcept {
		sqlite3_result_error(context_from_this(), e.data(), e.size());
	}

	void inplace_context::error(const std::wstring& e) noexcept {
		sqlite3_result_error16(context_from_this(), e.data(), e.size());
	}

	void inplace_context::error_toobig() noexcept {
		sqlite3_result_error_toobig(context_from_this());
	}

	void inplace_context::error_nomem() noexcept {
		sqlite3_result_error_nomem(context_from_this());
	}

	void inplace_context::error_code(int errc) noexcept {
		sqlite3_result_error_code(context_from_this(), errc);
	}

	void inplace_context::result_blob(const void* ptr, size_t len) noexcept {
		sqlite3_result_blob64(context_from_this(), ptr, len, SQLITE_TRANSIENT);
	}

	void inplace_context::result_double(double d) noexcept {
		sqlite3_result_double(context_from_this(), d);
	}

	void inplace_context::result_int(int64_t d) noexcept {
		sqlite3_result_int64(context_from_this(), d);
	}

	void inplace_context::result_null() noexcept {
		sqlite3_result_null(context_from_this());
	}

	void inplace_context::result_text(const std::string& val) noexcept {
		sqlite3_result_text64(context_from_this(), val.data(), val.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
	}

	void inplace_context::result_text(const std::wstring& val) noexcept {
		sqlite3_result_text16(context_from_this(), val.data(), val.size(), SQLITE_TRANSIENT);
	}

	void inplace_context::result_value(sqlite3_value* val) noexcept {
		sqlite3_result_value(context_from_this(), val);
	}

	void inplace_context::result_value(inplace_value* val) noexcept {
		sqlite3_result_value(context_from_this(), val->value_from_this());
	}

	void inplace_context::result_zeroblob(uint64_t num_bytes) noexcept {
		sqlite3_result_zeroblob64(context_from_this(), num_bytes);
	}

	void inplace_context::result_pointer(void* ptr, const char* type, void (*destructor)(void*)) noexcept {
		sqlite3_result_pointer(context_from_this(), ptr, type, destructor);
	}

	void inplace_context::result_subtype(unsigned int n) noexcept {
		sqlite3_result_subtype(context_from_this(), n);
	}

} // namespace sqlitepp