#pragma once
#include "sqlitepp/error_code.h"
#include <cstdint>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct sqlite3_stmt;
namespace sqlitepp {
	class result_iterator {
		sqlite3_stmt* m_handle;
		bool m_has_row;
		result_iterator(sqlite3_stmt* hdl)
			: m_handle(hdl), m_has_row(false) {}
		friend class statement;
		template <typename... Types>
		friend class stl_for_each_iterator;

		void get(size_t idx, double& val) const {
			if (idx >= column_count())
				throw_if_error(SQLITE_RANGE, m_handle);
			val = sqlite3_column_double(m_handle, idx);
		}

		void get(size_t idx, int64_t& val) const {
			if (idx >= column_count())
				throw_if_error(SQLITE_RANGE, m_handle);
			val = sqlite3_column_int64(m_handle, idx);
		}

		void get(size_t idx, std::string& val) const {
			if (idx >= column_count())
				throw_if_error(SQLITE_RANGE, m_handle);
			val = std::string(reinterpret_cast<const char*>(sqlite3_column_text(m_handle, idx)), sqlite3_column_bytes(m_handle, idx));
		}

		void get(size_t idx, std::vector<uint8_t>& val) const {
			if (idx >= column_count())
				throw_if_error(SQLITE_RANGE, m_handle);
			auto ptr = static_cast<const uint8_t*>(sqlite3_column_blob(m_handle, idx));
			auto size = sqlite3_column_bytes(m_handle, idx);
			val.reserve(size);
			val.assign(ptr, ptr + size);
		}

		template <typename Arg1, typename... Args>
		void get_all_impl(size_t idx, Arg1&& arg1, Args&&... args) const {
			this->get(idx, arg1);
			this->get_all_impl(idx + 1, std::forward<Args>(args)...);
		}

		void get_all_impl(size_t) const {
		}

		template <typename Tuple, size_t... I>
		void get_tuple_impl(Tuple& args, std::index_sequence<I...>) const {
			get_all(std::get<I>(args)...);
		}

	public:
		result_iterator(result_iterator&& o) : m_handle(o.m_handle), m_has_row(o.m_has_row) { o.m_handle = nullptr; }
		~result_iterator() {
			if (m_handle) sqlite3_reset(m_handle);
		}
		result_iterator(const result_iterator& o) = delete;
		result_iterator& operator=(const result_iterator& o) = delete;

		bool is_valid() const noexcept { return m_handle != nullptr; }
		bool next() {
			int ec = sqlite3_step(m_handle);
			throw_if_error(ec, m_handle);
			m_has_row = ec == SQLITE_ROW;
			return m_has_row;
		}
		bool done() const noexcept {
			return !m_has_row;
		}

		size_t column_count() const noexcept {
			return sqlite3_column_count(m_handle);
		}
		const char* column_name(size_t idx) const {
			if (idx >= column_count())
				throw_if_error(SQLITE_RANGE, m_handle);
			return sqlite3_column_name(m_handle, idx);
		}
		size_t column_index(const std::string& name) const {
			for (size_t i = 0; i < column_count(); i++) {
				auto col = column_name(i);
				if (!col || name.compare(col) != 0) continue;
				return i;
			}
			throw_if_error(SQLITE_RANGE, m_handle);
			abort(); // Never reached as the above always throws
		}
		int column_type(size_t idx) const {
			if (idx >= column_count())
				throw_if_error(SQLITE_RANGE, m_handle);
			return sqlite3_column_type(m_handle, idx);
		}
		bool column_is_null(size_t idx) const {
			return column_type(idx) == SQLITE_NULL;
		}
		double column_double(size_t idx) const {
			if (idx >= column_count())
				throw_if_error(SQLITE_RANGE, m_handle);
			return sqlite3_column_double(m_handle, idx);
		}
		int64_t column_int64(size_t idx) const {
			if (idx >= column_count())
				throw_if_error(SQLITE_RANGE, m_handle);
			return sqlite3_column_int64(m_handle, idx);
		}
		// TODO: Stringview ?
		std::pair<const char*, size_t> column_text(size_t idx) const {
			if (idx >= column_count())
				throw_if_error(SQLITE_RANGE, m_handle);
			return {reinterpret_cast<const char*>(sqlite3_column_text(m_handle, idx)), sqlite3_column_bytes(m_handle, idx)};
		}
		std::pair<const void*, size_t> column_blob(size_t idx) const {
			if (idx >= column_count())
				throw_if_error(SQLITE_RANGE, m_handle);
			return {sqlite3_column_blob(m_handle, idx), sqlite3_column_bytes(m_handle, idx)};
		}
		void column_blob(size_t idx, std::vector<uint8_t>& out) const {
			auto res = column_blob(idx);
			out.resize(res.second);
			std::copy(reinterpret_cast<const uint8_t*>(res.first), reinterpret_cast<const uint8_t*>(res.first) + res.second, out.data());
		}
		std::string column_string(size_t idx) const {
			auto txt = column_text(idx);
			return std::string(txt.first, txt.second);
		}

		int column_type(const std::string& name) const {
			return column_type(column_index(name));
		}
		bool column_is_null(const std::string& name) const {
			return column_is_null(column_index(name));
		}
		double column_double(const std::string& name) const {
			return column_double(column_index(name));
		}
		int64_t column_int64(const std::string& name) const {
			return column_int64(column_index(name));
		}
		// TODO: Stringview ?
		std::pair<const char*, size_t> column_text(const std::string& name) const {
			return column_text(column_index(name));
		}
		std::pair<const void*, size_t> column_blob(const std::string& name) const {
			return column_blob(column_index(name));
		}
		void column_blob(const std::string& name, std::vector<uint8_t>& out) const {
			column_blob(column_index(name), out);
		}
		std::string column_string(const std::string& name) const {
			return column_string(column_index(name));
		}

		template <typename... Args>
		void get_all(Args&&... args) const {
			this->get_all_impl(0, std::forward<Args>(args)...);
		}

		template <typename... Args>
		void get_tuple(std::tuple<Args...>& t) const {
			get_tuple_impl(t, std::index_sequence_for<Args...>{});
		}

		template <typename... Args>
		std::tuple<Args...> get_tuple() const {
			std::tuple<Args...> res;
			get_tuple(res);
			return res;
		}
	};

	template <typename... Types>
	class stl_for_each_iterator {
		result_iterator m_result;
		std::tuple<Types...> m_row;
		stl_for_each_iterator(sqlite3_stmt* iter)
			: m_result(iter), m_row() {
			if (m_result.is_valid()) {
				m_result.next();
				m_result.get_tuple(m_row);
			}
		}
		friend class statement;

	public:
		stl_for_each_iterator& operator++() {
			m_result.next();
			m_result.get_tuple(m_row);
			return *this;
		}

		const std::tuple<Types...>& operator*() {
			return m_row;
		}

		bool operator!=(const stl_for_each_iterator& other) {
			if (m_result.is_valid()) {
				if (other.m_result.is_valid()) throw std::logic_error("invalid use of stl_for_each_iterator");
				return !m_result.done();
			} else {
				if (!other.m_result.is_valid()) throw std::logic_error("invalid use of stl_for_each_iterator");
				return !other.m_result.done();
			}
		}
	};
} // namespace sqlitepp
