#include "sqlitepp/error_code.h"

namespace sqlitepp {
	namespace {
		struct sqlite_error_category_impl : std::error_category {
			const char* name() const noexcept override { return "sqlite"; }
			std::string message(int ev) const override {
				return sqlite3_errstr(ev);
			}
		};

		static const sqlite_error_category_impl g_sqlite_error_category{};
	} // namespace

	const std::error_category& sqlite_error_category() {
		return g_sqlite_error_category;
	}
} // namespace sqlitepp
