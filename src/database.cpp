#include "sqlitepp/database.h"
#include "sqlitepp/error_code.h"
#include <cstdio>

namespace sqlitepp {
    database::database(const std::string& filename)
		: m_handle(nullptr)
	{
        if(SQLITE_VERSION_NUMBER != libversion_number())
            throw std::logic_error("version missmatch between library and header files");
        
		int res = sqlite3_open_v2(filename.c_str(), &m_handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
		throw_if_error(res, static_cast<sqlite3*>(nullptr));
		res = sqlite3_extended_result_codes(m_handle, 1);
		throw_if_error(res, m_handle);
	}

	database::~database() noexcept {
		sqlite3_close_v2(m_handle);
	}

    void database::exec(const std::string& query, std::function<void(int, char**, char**)> fn) {
		if(!fn) throw std::invalid_argument("invalid callback specified");
		int rc = sqlite3_exec(m_handle, query.c_str(), [](void* ud, int argc, char** argv, char** colnames) {
			auto f = static_cast<std::function<void(int, char**, char**)>*>(ud);
			(*f)(argc, argv, colnames);
			return 0;
		}, &fn, nullptr);
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
}