#pragma once
#include <cstdint>
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
			: m_handle(hdl), m_has_row(false)
		{}
		friend class statement;
		template<typename... Types> 
		friend class stl_for_each_iterator;

		void get(size_t idx, double& val) const;
		void get(size_t idx, int64_t& val) const;
		void get(size_t idx, std::string& val) const;
		void get(size_t idx, std::vector<uint8_t>& val) const;

		template<typename Arg1, typename... Args>
		void get_all_impl(size_t idx, Arg1&& arg1, Args&&... args) const {
			this->get(idx, arg1);
			this->get_all_impl(idx + 1, std::forward<Args>(args)...);
		}
		
		void get_all_impl(size_t) const {
		}

		template<typename Tuple, size_t... I>
		void get_tuple_impl(Tuple& args, std::index_sequence<I...>) const {
			get_all(std::get<I>(args)...);
		}
	public:
		result_iterator(result_iterator&& o);
		~result_iterator();
		result_iterator(const result_iterator& o) = delete;
		result_iterator& operator=(const result_iterator& o) = delete;

		bool is_valid() const noexcept;
		bool next();
		bool done() const noexcept;

		size_t column_count() const noexcept;
		const char* column_name(size_t idx) const;
		size_t column_index(const std::string& name) const;
		int column_type(size_t idx) const;
		bool column_is_null(size_t idx) const;
		double column_double(size_t idx) const;
		int64_t column_int64(size_t idx) const;
		// TODO: Stringview ?
		std::pair<const char*, size_t> column_text(size_t idx) const;
		std::pair<const void*, size_t> column_blob(size_t idx) const;
		void column_blob(size_t idx, std::vector<uint8_t>& out) const;
		std::string column_string(size_t idx) const;
		
		int column_type(const std::string& name) const;
		bool column_is_null(const std::string& name) const;
		double column_double(const std::string& name) const;
		int64_t column_int64(const std::string& name) const;
		// TODO: Stringview ?
		std::pair<const char*, size_t> column_text(const std::string& name) const;
		std::pair<const void*, size_t> column_blob(const std::string& name) const;
		void column_blob(const std::string& name, std::vector<uint8_t>& out) const;
		std::string column_string(const std::string& name) const;

		template<typename... Args>
		void get_all(Args&&... args) const {
			this->get_all_impl(0, std::forward<Args>(args)...);
		}

		template<typename... Args>
		void get_tuple(std::tuple<Args...>& t) const {
			get_tuple_impl(t, std::index_sequence_for<Args...>{});
		}

		template<typename... Args>
		std::tuple<Args...> get_tuple() const {
			std::tuple<Args...> res;
			get_tuple(res);
			return res;
		}
	};

	template<typename... Types>
	class stl_for_each_iterator {
		result_iterator m_result;
		std::tuple<Types...> m_row;
		stl_for_each_iterator(sqlite3_stmt* iter)
			: m_result(iter), m_row()
		{
			if(m_result.is_valid()) {
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
			if(m_result.is_valid()) {
				if(other.m_result.is_valid()) throw std::logic_error("invalid use of stl_for_each_iterator");
				return !m_result.done();
			} else {
				if(!other.m_result.is_valid()) throw std::logic_error("invalid use of stl_for_each_iterator");
				return !other.m_result.done();
			}
		}
	};
}
