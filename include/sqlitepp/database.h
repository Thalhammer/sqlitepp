#pragma once
#include <functional>
#include <set>
#include <string>

struct sqlite3;
struct sqlite3_context;
struct sqlite3_value;
namespace sqlitepp {
	enum class encoding {
		utf8 = 1,
		utf16le = 2,
		utf16be = 3,
		utf16 = 4,
	};

	class inplace_context;
	class inplace_value;
	class database {
		sqlite3* m_handle;
		bool m_do_close;
		friend class inplace_context;

		database(sqlite3* hdl, bool should_close)
			: m_handle(hdl), m_do_close(should_close) {}

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
		bool has_table(const std::string& table) { return has_table("", table); }
		bool has_table(const std::string& schema, const std::string& table);
		std::set<std::string> get_tables(const std::string& schema = "main");
		std::set<std::pair<std::string, std::string>> get_schemas();
		int32_t get_application_id(const std::string& schema = "main");
		void set_application_id(int32_t id, const std::string& schema = "main");
		int32_t get_user_version(const std::string& schema = "main");
		void set_user_version(int32_t version, const std::string& schema = "main");
		void load_extension(const std::string& filename, const std::string& entry_point = "");
		void create_function(const std::string& name, int nArgs, encoding eTextRep, void* udata, void (*func)(sqlite3_context*, int, sqlite3_value**), void (*udata_destruct)(void*));
		void create_function(const std::string& name, int nArgs, encoding eTextRep, void* udata, void (*step)(sqlite3_context*, int, sqlite3_value**), void (*final)(sqlite3_context*), void (*udata_destruct)(void*));
		void create_function(const std::string& name, int nArgs, encoding eTextRep, std::function<void(inplace_context*, int, inplace_value**)> func);
		void create_function(const std::string& name, int nArgs, encoding eTextRep, std::function<void(inplace_context*, int, inplace_value**)> step, std::function<void(inplace_context*)> generateResult);

		sqlite3* raw() const noexcept;
	};

	bool is_threadsafe() noexcept;
	std::string libversion();
	std::string sourceid();
	int libversion_number();
} // namespace sqlitepp