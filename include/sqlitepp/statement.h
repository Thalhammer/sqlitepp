#pragma once
#include <utility>
#include <vector>

#include <sqlitepp/result_iterator.h>

namespace sqlitepp {
	class database;
	class statement {
		database* m_db;
		sqlite3_stmt* m_handle;

		template<typename Arg1, typename... Args>
		void bind_all_impl(size_t idx, Arg1&& arg1, Args&&... args) {
			this->bind(idx, arg1);
			this->bind_all_impl(idx + 1, std::forward<Args>(args)...);
		}
		
		void bind_all_impl(size_t) {
		}

		template<typename Tuple, size_t... I>
		void bind_tuple_impl(const Tuple& args, std::index_sequence<I...>) {
			bind_all(std::get<I>(args)...);
		}
	public:
		statement(database& p, const std::string& query);
		statement(statement&& other);
		statement& operator=(statement&& other);

		statement(const statement& other) = delete;
		statement& operator=(const statement& other) = delete;

		~statement() noexcept;

		sqlite3_stmt* raw() const noexcept { return m_handle; }
		database& parent_database() const noexcept { return *m_db; }

		const char* query() const;
		bool is_readonly() const noexcept;

#ifdef __cpp_lib_string_view
		void bind(size_t idx, const std::string_view& str);
		void bind(size_t idx, const std::wstring_view& str);
		void bind(size_t idx, const std::string_view& data, bool is_blob);
#else
		void bind(size_t idx, const std::string& str);
		void bind(size_t idx, const std::wstring& str);
		void bind(size_t idx, const std::string& data, bool is_blob);
#endif

		void bind(size_t idx, const std::vector<uint8_t>& blob);
		void bind(size_t idx, std::nullptr_t);
		void bind(size_t idx, double val);
		void bind(size_t idx, int val);
		void bind(size_t idx, int64_t val);

		template<typename... Args>
		void bind_tuple(std::tuple<Args...>& t) {
			bind_tuple_impl(t, std::index_sequence_for<Args...>{});
		}

		template<typename... Args>
		void bind_all(Args&&... args) {
			this->bind_all_impl(1, std::forward<Args>(args)...);
		}

		size_t param_count() const noexcept;
		const char* param_name(size_t idx) const noexcept;
		size_t param_name(const char* name) const noexcept;
		size_t param_name(const std::string& name) const;

		result_iterator iterator();

		template<typename... Types>
		struct iterate_helper {
			statement* stmt;

			stl_for_each_iterator<Types...> begin() {
				return stl_for_each_iterator<Types...>(stmt->m_handle);
			}
			stl_for_each_iterator<Types...> end() {
				return stl_for_each_iterator<Types...>(nullptr);
			}
		};
		template<typename... Types>
		iterate_helper<Types...> iterate() {
			return iterate_helper<Types...>{this};
		}

		void execute();
	};
}