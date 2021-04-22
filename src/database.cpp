#include "sqlitepp/database.h"
#include "sqlitepp/error_code.h"
#include "sqlitepp/statement.h"
#include <cstdio>

namespace sqlitepp {
	database::database(const std::string& filename)
		: m_handle(nullptr), m_do_close{true} {
		if (SQLITE_VERSION_NUMBER != libversion_number())
			throw std::logic_error("version missmatch between library and header files");

		int res = sqlite3_open_v2(filename.c_str(), &m_handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
		throw_if_error(res, static_cast<sqlite3*>(nullptr));
		res = sqlite3_extended_result_codes(m_handle, 1);
		throw_if_error(res, m_handle);
	}

	database::~database() noexcept {
		if (m_do_close && m_handle)
			sqlite3_close_v2(m_handle);
	}

	void database::exec(const std::string& query, std::function<void(int, char**, char**)> fn) {
		if (!fn) throw std::invalid_argument("invalid callback specified");
		int rc = sqlite3_exec(
			m_handle, query.c_str(), [](void* ud, int argc, char** argv, char** colnames) {
				auto f = static_cast<std::function<void(int, char**, char**)>*>(ud);
				(*f)(argc, argv, colnames);
				return 0;
			},
			&fn, nullptr);
		throw_if_error(rc, m_handle);
	}

	void database::exec(const std::string& query) {
		int rc = sqlite3_exec(m_handle, query.c_str(), nullptr, nullptr, nullptr);
		throw_if_error(rc, m_handle);
	}

	void database::interrupt() {
		sqlite3_interrupt(m_handle);
	}

	int64_t database::last_insert_rowid() const noexcept {
		return sqlite3_last_insert_rowid(m_handle);
	}

	size_t database::total_changes() const noexcept {
		return sqlite3_total_changes(m_handle);
	}

	bool database::has_table(const std::string& schema, const std::string& table) {
		// TODO: Escape weird schema names.
		// SQLITE allows everything as a table name, see:
		// https://stackoverflow.com/questions/3694276/what-are-valid-table-names-in-sqlite
		std::string query = "SELECT COUNT(*) FROM ";
		if (!schema.empty()) query += schema + ".";
		query += "sqlite_master WHERE name=? AND type='table'";
		statement stmt{*this, query};
		stmt.bind(1, table);
		for (auto& e : stmt.iterate<int64_t>()) {
			if (std::get<0>(e) == 1) return true;
			if (std::get<0>(e) == 0) return false;
			break;
		}
		// This should not be possible
		throw_if_error(SQLITE_INTERNAL, m_handle);
		return false; // never reached, but makes the compiler a tiny bit happier
	}

	std::set<std::string> database::get_tables(const std::string& schema) {
		std::string query = "SELECT name FROM " + schema + ".sqlite_master WHERE type='table'";
		statement stmt{*this, query};
		std::set<std::string> res;
		for (auto& e : stmt.iterate<std::string>()) {
			res.insert(std::get<0>(e));
		}
		return res;
	}

	std::set<std::pair<std::string, std::string>> database::get_schemas() {
		statement stmt{*this, "PRAGMA database_list"};
		std::set<std::pair<std::string, std::string>> res;
		for (auto& e : stmt.iterate<int64_t, std::string, std::string>()) {
			res.emplace(std::get<1>(e), std::get<2>(e));
		}
		return res;
	}

	int32_t database::get_application_id(const std::string& schema) {
		statement stmt{*this, "PRAGMA " + schema + ".application_id"};
		for (auto& e : stmt.iterate<int64_t>()) {
			return std::get<0>(e);
		}
		throw_if_error(SQLITE_INTERNAL, m_handle);
	}

	void database::set_application_id(int32_t id, const std::string& schema) {
		exec("PRAGMA " + schema + ".application_id=" + std::to_string(id));
	}

	int32_t database::get_user_version(const std::string& schema) {
		statement stmt{*this, "PRAGMA " + schema + ".user_version"};
		for (auto& e : stmt.iterate<int64_t>()) {
			return std::get<0>(e);
		}
		throw_if_error(SQLITE_INTERNAL, m_handle);
	}

	void database::set_user_version(int32_t version, const std::string& schema) {
		exec("PRAGMA " + schema + ".user_version=" + std::to_string(version));
	}

	void database::load_extension(const std::string& filename, const std::string& entry_point) {
		sqlite3_db_config(m_handle, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, 1, NULL);
		char* error = nullptr;
		auto res = sqlite3_load_extension(m_handle, filename.c_str(), entry_point.empty() ? nullptr : entry_point.c_str(), &error);
		std::string str_error = error;
		if (error) sqlite3_free(error);
		str_error = " " + str_error;
		if (res != SQLITE_OK)
			throw std::system_error(make_error_code(static_cast<error_code>(res)), sqlite3_errmsg(m_handle) + str_error);
	}

	void database::create_function(const std::string& name, int nArgs, encoding eTextRep, void* udata,
								   void (*func)(sqlite3_context*, int, sqlite3_value**), void (*udata_destruct)(void*)) {
		auto res = sqlite3_create_function_v2(m_handle, name.c_str(), nArgs, static_cast<int>(eTextRep), udata, func, nullptr, nullptr, udata_destruct);
		throw_if_error(res, m_handle);
	}

	void database::create_function(const std::string& name, int nArgs, encoding eTextRep, void* udata,
								   void (*step)(sqlite3_context*, int, sqlite3_value**), void (*final)(sqlite3_context*),
								   void (*udata_destruct)(void*)) {
		auto res = sqlite3_create_function_v2(m_handle, name.c_str(), nArgs, static_cast<int>(eTextRep), udata, nullptr, step, final, udata_destruct);
		throw_if_error(res, m_handle);
	}

	void database::create_function(const std::string& name, int nArgs, encoding eTextRep, std::function<void(inplace_context*, int, inplace_value**)> func) {
		struct my_info {
			std::function<void(inplace_context*, int, inplace_value**)> m_func;
		};
		auto mem = sqlite3_malloc(sizeof(my_info));
		if (!mem) throw_if_error(SQLITE_NOMEM, m_handle);
		auto info = new (mem) my_info{func};
		try {
			create_function(
				name, nArgs, eTextRep, info, [](sqlite3_context* ctx, int args, sqlite3_value** argv) {
                auto info = reinterpret_cast<my_info*>(sqlite3_user_data(ctx));
                if(!info) {
                    sqlite3_result_error_code(ctx, SQLITE_INTERNAL);
                    return;
                }
                info->m_func(reinterpret_cast<inplace_context*>(ctx), args, reinterpret_cast<inplace_value**>(argv)); }, [](void* ptr) {
                auto info = reinterpret_cast<my_info*>(ptr);
                if(info) info->~my_info();
                sqlite3_free(info); });
		} catch (...) {
			info->~my_info();
			sqlite3_free(info);
			throw;
		}
	}

	void database::create_function(const std::string& name, int nArgs, encoding eTextRep, std::function<void(inplace_context*, int, inplace_value**)> step, std::function<void(inplace_context*)> generateResult) {
		struct my_info {
			std::function<void(inplace_context*, int, inplace_value**)> m_step;
			std::function<void(inplace_context*)> m_final;
		};
		auto mem = sqlite3_malloc(sizeof(my_info));
		if (!mem) throw_if_error(SQLITE_NOMEM, m_handle);
		auto info = new (mem) my_info{step, generateResult};
		try {
			create_function(
				name, nArgs, eTextRep, info, [](sqlite3_context* ctx, int args, sqlite3_value** argv) {
                auto info = reinterpret_cast<my_info*>(sqlite3_user_data(ctx));
                if(!info) {
                    sqlite3_result_error_code(ctx, SQLITE_INTERNAL);
                    return;
                }
                info->m_step(reinterpret_cast<inplace_context*>(ctx), args, reinterpret_cast<inplace_value**>(argv)); }, [](sqlite3_context* ctx) {
                auto info = reinterpret_cast<my_info*>(sqlite3_user_data(ctx));
                if(!info) {
                    sqlite3_result_error_code(ctx, SQLITE_INTERNAL);
                    return;
                }
                info->m_final(reinterpret_cast<inplace_context*>(ctx)); }, [](void* ptr) {
                auto info = reinterpret_cast<my_info*>(ptr);
                if(info) info->~my_info();
                sqlite3_free(info); });
		} catch (...) {
			info->~my_info();
			sqlite3_free(info);
			throw;
		}
	}

	sqlite3* database::raw() const noexcept { return m_handle; }

	bool is_threadsafe() noexcept {
		return sqlite3_threadsafe() != 0;
	}

	std::string libversion() {
		return sqlite3_libversion();
	}

	std::string sourceid() {
		return sqlite3_sourceid();
	}

	int libversion_number() {
		return sqlite3_libversion_number();
	}
} // namespace sqlitepp