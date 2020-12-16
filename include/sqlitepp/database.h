#pragma once
#include <functional>
#include <string>

struct sqlite3;
namespace sqlitepp {
	class database {
		sqlite3* m_handle;
	public:
		database(const std::string& filename = ":memory:");

		database(const database& other) = delete;
		database& operator=(const database& other) = delete;
		database(database&& other) = delete;
		database& operator=(database&& other) = delete;

		~database() noexcept;

		void exec(const std::string& query, std::function<void(int, char**, char**)> fn);
		void exec(const std::string& query);
		void interrupt();
		int64_t last_insert_rowid() const noexcept;
		size_t total_changes() const noexcept;
		sqlite3* raw() const noexcept;
	};

	bool is_threadsafe() noexcept;
	std::string libversion();
	std::string sourceid();
	int libversion_number();
}