#pragma once
#include <utility>
#include <cstdint>
#include <string>

struct sqlite3;
struct sqlite3_value;
struct sqlite3_context;
namespace sqlitepp {
    class inplace_context;
    class database;
    /**
     * This is a 'magic' class that allows us to treat sqlite_value structs in a oop fashion
     * simply by casting a 'sqlite_value*' to a 'inplace_value*' pointer.
     */
	class inplace_value {
        // if we need any of those we are doing it wrong.
        inplace_value() = delete;
        inplace_value(const inplace_value&) = delete;
        inplace_value(inplace_value&&) = delete;
        inplace_value operator=(const inplace_value&) = delete;
        inplace_value operator=(inplace_value&&) = delete;

        sqlite3_value* value_from_this() const noexcept;
        friend class inplace_context;
	public:
        static inplace_value* from_value(sqlite3_value* val);

        enum class type {
            integer = 1,
            real = 2,
            text = 3,
            blob = 4,
            null = 5
        };
        type get_type() const noexcept;
        type get_numeric_type() const noexcept;

        std::pair<const void*, size_t> as_blob() const noexcept;
        double as_double() const noexcept;
        int64_t as_int64() const noexcept;
        int32_t as_int32() const noexcept;
        void* as_pointer(const char* type) const noexcept;
        std::string as_string() const noexcept;
        std::wstring as_wstring() const noexcept;

        bool is_unchanged() const noexcept;
        bool is_from_bind() const noexcept;
	};

    /**
     * This is a 'magic' class that allows us to treat sqlite_context structs in a oop fashion
     * simply by casting a 'sqlite_context*' to a 'inplace_context*' pointer.
     */
	class inplace_context {
        // if we need any of those we are doing it wrong.
        inplace_context() = delete;
        inplace_context(const inplace_context&) = delete;
        inplace_context(inplace_context&&) = delete;
        inplace_context operator=(const inplace_context&) = delete;
        inplace_context operator=(inplace_context&&) = delete;

        sqlite3_context* context_from_this() const noexcept;
	public:
        static inplace_context* from_context(sqlite3_context* val);

        sqlite3* get_db_handle() const noexcept;
        database get_db() const noexcept;
        void* aggregate_context(size_t nbytes) noexcept;
        void* get_auxdata(int N) const noexcept;
        void set_auxdata(int N, void* ptr, void (*destructor)(void*)) noexcept;
        void* get_userdata() const noexcept;

        template<typename T, typename = typename std::enable_if<std::is_pod<T>::value>::type>
        T* aggregate_context() noexcept {
            return reinterpret_cast<T*>(aggregate_context(sizeof(T)));
        }
        template<typename T>
        T* get_auxdata(int N) const noexcept {
            return reinterpret_cast<T*>(get_auxdata(N));
        }
        template<typename T>
        void set_auxdata(int N, T* ptr, void (*destructor)(T*)) noexcept {
            set_auxdata(N, ptr, reinterpret_cast<void (*)(void*)>(destructor));
        }
        template<typename T>
        T* get_userdata() const noexcept {
            return reinterpret_cast<T*>(get_userdata());
        }

        void error(const std::string& e) noexcept;
        void error(const std::wstring& e) noexcept;
        void error_toobig() noexcept;
        void error_nomem() noexcept;
        void error_code(int errc) noexcept;

        void result_blob(const void* ptr, size_t len) noexcept;
        void result_double(double d) noexcept;
        void result_int(int64_t d) noexcept;
        void result_null() noexcept;
        void result_text(const std::string& val) noexcept;
        void result_text(const std::wstring& val) noexcept;
        void result_value(sqlite3_value* val) noexcept;
        void result_value(inplace_value* val) noexcept;
        void result_zeroblob(uint64_t num_bytes) noexcept;
        void result_pointer(void* ptr, const char* type, void(*destructor)(void*)) noexcept;
        void result_subtype(unsigned int n) noexcept;
	};
} // namespace sqlitepp