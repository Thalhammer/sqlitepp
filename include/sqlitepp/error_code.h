#pragma once
#include <stdexcept>
#include <string>
#include <system_error>

#include <sqlite3.h>

namespace sqlitepp {
	enum class error_code : int {
		ok = SQLITE_OK,
		error = SQLITE_ERROR,
		internal = SQLITE_INTERNAL,
		perm = SQLITE_PERM,
		abort = SQLITE_ABORT,
		busy = SQLITE_BUSY,
		locked = SQLITE_LOCKED,
		nomem = SQLITE_NOMEM,
		readonly = SQLITE_READONLY,
		interrupt = SQLITE_INTERRUPT,
		ioerr = SQLITE_IOERR,
		corrupt = SQLITE_CORRUPT,
		notfound = SQLITE_NOTFOUND,
		full = SQLITE_FULL,
		cantopen = SQLITE_CANTOPEN,
		protocol = SQLITE_PROTOCOL,
		empty = SQLITE_EMPTY,
		schema = SQLITE_SCHEMA,
		toobig = SQLITE_TOOBIG,
		constraint = SQLITE_CONSTRAINT,
		mismatch = SQLITE_MISMATCH,
		misuse = SQLITE_MISUSE,
		nolfs = SQLITE_NOLFS,
		auth = SQLITE_AUTH,
		format = SQLITE_FORMAT,
		range = SQLITE_RANGE,
		notadb = SQLITE_NOTADB,
		notice = SQLITE_NOTICE,
		warning = SQLITE_WARNING,
		row = SQLITE_ROW,
		done = SQLITE_DONE,

		/* Extended codes */
		error_missing_collseq = SQLITE_ERROR_MISSING_COLLSEQ,
		error_retry = SQLITE_ERROR_RETRY,
		ioerr_read = SQLITE_IOERR_READ,
		ioerr_short_read = SQLITE_IOERR_SHORT_READ,
		ioerr_write = SQLITE_IOERR_WRITE,
		ioerr_fsync = SQLITE_IOERR_FSYNC,
		ioerr_dir_fsync = SQLITE_IOERR_DIR_FSYNC,
		ioerr_truncate = SQLITE_IOERR_TRUNCATE,
		ioerr_fstat = SQLITE_IOERR_FSTAT,
		ioerr_unlock = SQLITE_IOERR_UNLOCK,
		ioerr_rdlock = SQLITE_IOERR_RDLOCK,
		ioerr_delete = SQLITE_IOERR_DELETE,
		ioerr_blocked = SQLITE_IOERR_BLOCKED,
		ioerr_nomem = SQLITE_IOERR_NOMEM,
		ioerr_access = SQLITE_IOERR_ACCESS,
		ioerr_checkreservedlock = SQLITE_IOERR_CHECKRESERVEDLOCK,
		ioerr_lock = SQLITE_IOERR_LOCK,
		ioerr_close = SQLITE_IOERR_CLOSE,
		ioerr_dir_close = SQLITE_IOERR_DIR_CLOSE,
		ioerr_shmopen = SQLITE_IOERR_SHMOPEN,
		ioerr_shmsize = SQLITE_IOERR_SHMSIZE,
		ioerr_shmlock = SQLITE_IOERR_SHMLOCK,
		ioerr_shmmap = SQLITE_IOERR_SHMMAP,
		ioerr_seek = SQLITE_IOERR_SEEK,
		ioerr_delete_noent = SQLITE_IOERR_DELETE_NOENT,
		ioerr_mmap = SQLITE_IOERR_MMAP,
		ioerr_gettemppath = SQLITE_IOERR_GETTEMPPATH,
		ioerr_convpath = SQLITE_IOERR_CONVPATH,
		ioerr_vnode = SQLITE_IOERR_VNODE,
		ioerr_auth = SQLITE_IOERR_AUTH,
		ioerr_begin_atomic = SQLITE_IOERR_BEGIN_ATOMIC,
		ioerr_commit_atomic = SQLITE_IOERR_COMMIT_ATOMIC,
		ioerr_rollback_atomic = SQLITE_IOERR_ROLLBACK_ATOMIC,
		locked_sharedcache = SQLITE_LOCKED_SHAREDCACHE,
		busy_recovery = SQLITE_BUSY_RECOVERY,
		busy_snapshot = SQLITE_BUSY_SNAPSHOT,
		cantopen_notempdir = SQLITE_CANTOPEN_NOTEMPDIR,
		cantopen_isdir = SQLITE_CANTOPEN_ISDIR,
		cantopen_fullpath = SQLITE_CANTOPEN_FULLPATH,
		cantopen_convpath = SQLITE_CANTOPEN_CONVPATH,
		corrupt_vtab = SQLITE_CORRUPT_VTAB,
		readonly_recovery = SQLITE_READONLY_RECOVERY,
		readonly_cantlock = SQLITE_READONLY_CANTLOCK,
		readonly_rollback = SQLITE_READONLY_ROLLBACK,
		readonly_dbmoved = SQLITE_READONLY_DBMOVED,
		readonly_cantinit = SQLITE_READONLY_CANTINIT,
		readonly_directory = SQLITE_READONLY_DIRECTORY,
		abort_rollback = SQLITE_ABORT_ROLLBACK,
		constraint_check = SQLITE_CONSTRAINT_CHECK,
		constraint_commithook = SQLITE_CONSTRAINT_COMMITHOOK,
		constraint_foreignkey = SQLITE_CONSTRAINT_FOREIGNKEY,
		constraint_function = SQLITE_CONSTRAINT_FUNCTION,
		constraint_notnull = SQLITE_CONSTRAINT_NOTNULL,
		constraint_primarykey = SQLITE_CONSTRAINT_PRIMARYKEY,
		constraint_trigger = SQLITE_CONSTRAINT_TRIGGER,
		constraint_unique = SQLITE_CONSTRAINT_UNIQUE,
		constraint_vtab = SQLITE_CONSTRAINT_VTAB,
		constraint_rowid = SQLITE_CONSTRAINT_ROWID,
		notice_recover_wal = SQLITE_NOTICE_RECOVER_WAL,
		notice_recover_rollback = SQLITE_NOTICE_RECOVER_ROLLBACK,
		warning_autoindex = SQLITE_WARNING_AUTOINDEX,
		auth_user = SQLITE_AUTH_USER,
		ok_load_permanently = SQLITE_OK_LOAD_PERMANENTLY
	};

	const std::error_category& sqlite_error_category();

	inline std::error_code make_error_code(error_code e) {
		return {static_cast<int>(e), sqlite_error_category()};
	}

	inline void throw_if_error(int code, sqlite3* raw) {
		if (code != SQLITE_OK && code != SQLITE_ROW && code != SQLITE_DONE)
			throw std::system_error(make_error_code(static_cast<error_code>(code)), raw ? sqlite3_errmsg(raw) : "");
	}

	inline void throw_if_error(int code, sqlite3_stmt* raw) {
		if (code != SQLITE_OK && code != SQLITE_ROW && code != SQLITE_DONE)
			throw std::system_error(make_error_code(static_cast<error_code>(code)), raw ? sqlite3_errmsg(sqlite3_db_handle(raw)) : "");
	}
} // namespace sqlitepp

namespace std {
	template <>
	struct is_error_code_enum<sqlitepp::error_code> : true_type {};
} // namespace std
