#pragma once
#include <utility>
#include <vector>

#include <sqlitepp/result_iterator.h>

namespace sqlitepp {
	class database;
	class statement {
		database* m_db;
		sqlite3_stmt* m_handle;

		template <typename Arg1, typename... Args>
		void bind_all_impl(size_t idx, Arg1&& arg1, Args&&... args) {
			this->bind(idx, arg1);
			this->bind_all_impl(idx + 1, std::forward<Args>(args)...);
		}

		void bind_all_impl(size_t) {
		}

		template <typename Tuple, size_t... I>
		void bind_tuple_impl(const Tuple& args, std::index_sequence<I...>) {
			bind_all(std::get<I>(args)...);
		}

	public:
		statement(database& p, const std::string& query)
			: m_db(&p), m_handle(nullptr) {
			int res = sqlite3_prepare_v2(m_db->raw(), query.data(), query.size(), &m_handle, NULL);
			throw_if_error(res, m_handle);
		}
		statement(statement&& other)
			: m_db(other.m_db), m_handle(other.m_handle) {
			other.m_handle = nullptr;
		}
		statement& operator=(statement&& other) {
			if (m_handle) sqlite3_finalize(m_handle);
			m_db = other.m_db;
			m_handle = other.m_handle;
			other.m_handle = nullptr;
			return *this;
		}

		statement(const statement& other) = delete;
		statement& operator=(const statement& other) = delete;

		~statement() noexcept {
			if (m_handle) sqlite3_finalize(m_handle);
		}

		sqlite3_stmt* raw() const noexcept { return m_handle; }
		database& parent_database() const noexcept { return *m_db; }

		const char* query() const { return sqlite3_sql(m_handle); }
		bool is_readonly() const noexcept { return sqlite3_stmt_readonly(m_handle) != 0; }

#ifdef __cpp_lib_string_view
		void bind(size_t idx, const std::string_view& str) {
			int res = sqlite3_bind_text64(m_handle, idx, str.data(), str.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
			throw_if_error(res, m_handle);
		}
		void bind(size_t idx, const std::wstring_view& str) {
			int res = sqlite3_bind_text64(m_handle, idx, reinterpret_cast<const char*>(str.data()), str.size(), SQLITE_TRANSIENT, SQLITE_UTF16);
			throw_if_error(res, m_handle);
		}
		void bind(size_t idx, const std::string_view& data, bool is_blob) {
			if (is_blob) {
				// sqlite3 seems to threat binding a empty blob as NULL when the ptr is nullptr, so we give it some dummy pointer
				static char empty_byte;
				int res = sqlite3_bind_blob64(m_handle, idx, data.data() == nullptr ? &empty_byte : data.data(), data.size(), SQLITE_TRANSIENT);
				throw_if_error(res, m_handle);
			} else {
				int res = sqlite3_bind_text64(m_handle, idx, data.data(), data.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
				throw_if_error(res, m_handle);
			}
		}
#else
		void bind(size_t idx, const std::string& str) {
			int res = sqlite3_bind_text64(m_handle, idx, str.data(), str.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
			throw_if_error(res, m_handle);
		}
		void bind(size_t idx, const std::wstring& str) {
			int res = sqlite3_bind_text64(m_handle, idx, reinterpret_cast<const char*>(str.data()), str.size(), SQLITE_TRANSIENT, SQLITE_UTF16);
			throw_if_error(res, m_handle);
		}
		void bind(size_t idx, const std::string& data, bool is_blob) {
			if (is_blob) {
				// sqlite3 seems to threat binding a empty blob as NULL when the ptr is nullptr, so we give it some dummy pointer
				static char empty_byte;
				int res = sqlite3_bind_blob64(m_handle, idx, data.data() == nullptr ? &empty_byte : data.data(), data.size(), SQLITE_TRANSIENT);
				throw_if_error(res, m_handle);
			} else {
				int res = sqlite3_bind_text64(m_handle, idx, data.data(), data.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
				throw_if_error(res, m_handle);
			}
		}
#endif

		void bind(size_t idx, const std::vector<uint8_t>& blob) {
			// sqlite3 seems to threat binding a empty blob as NULL when the ptr is nullptr, so we give it some dummy pointer
			static uint8_t empty_byte;
			int res = sqlite3_bind_blob64(m_handle, idx, blob.data() == nullptr ? &empty_byte : blob.data(), blob.size(), SQLITE_TRANSIENT);
			throw_if_error(res, m_handle);
		}
		void bind(size_t idx, std::nullptr_t) {
			int res = sqlite3_bind_null(m_handle, idx);
			throw_if_error(res, m_handle);
		}
		void bind(size_t idx, double val) {
			int res = sqlite3_bind_double(m_handle, idx, val);
			throw_if_error(res, m_handle);
		}
		void bind(size_t idx, int val) { bind(idx, static_cast<int64_t>(val)); }
		void bind(size_t idx, int64_t val) {
			int res = sqlite3_bind_int64(m_handle, idx, val);
			throw_if_error(res, m_handle);
		}
		template <typename... Args>
		void bind_tuple(std::tuple<Args...>& t) { bind_tuple_impl(t, std::index_sequence_for<Args...>{}); }
		template <typename... Args>
		void bind_all(Args&&... args) { this->bind_all_impl(1, std::forward<Args>(args)...); }
		void clear_bindings() { throw_if_error(sqlite3_clear_bindings(m_handle), m_handle); }

		size_t param_count() const noexcept { return sqlite3_bind_parameter_count(m_handle); }
		const char* param_name(size_t idx) const noexcept { return sqlite3_bind_parameter_name(m_handle, idx); }
		size_t param_name(const char* name) const noexcept { return sqlite3_bind_parameter_index(m_handle, name); }
		size_t param_name(const std::string& name) const { return param_name(name.c_str()); }
		void reset() { throw_if_error(sqlite3_reset(m_handle), m_handle); }

		result_iterator iterator() { return result_iterator(m_handle); }

		template <typename... Types>
		struct iterate_helper {
			statement* stmt;

			stl_for_each_iterator<Types...> begin() {
				return stl_for_each_iterator<Types...>(stmt->m_handle);
			}
			stl_for_each_iterator<Types...> end() {
				return stl_for_each_iterator<Types...>(nullptr);
			}
		};
		template <typename... Types>
		iterate_helper<Types...> iterate() {
			return iterate_helper<Types...>{this};
		}

		void execute() {
			auto it = iterator();
			// Calling next() calls sqlite step, executing the statement
			it.next();
		}
	};
} // namespace sqlitepp
