#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace sqlitepp {
    class database;
    class result_iterator;
    template<typename... Types>
	class stl_for_each_iterator;
    class statement;
    namespace orm {
        struct class_info;
        struct field_info;
        class entity;
        enum class db_type;
        using db_text_type = std::string;
        using db_integer_type = int64_t;
        using db_real_type = double;
        using db_blob_type = std::vector<uint8_t>;
        using db_time_type = std::chrono::system_clock::time_point;
        struct db_null_type {
            explicit db_null_type() {}
            bool operator!=(const db_null_type&) const noexcept { return false; }
            bool operator==(const db_null_type&) const noexcept { return true; }
        };
        struct db_value : public std::variant<db_null_type, db_text_type, db_integer_type, db_real_type, db_blob_type> {
            using std::variant<db_null_type, db_text_type, db_integer_type, db_real_type, db_blob_type>::variant;

            template<typename U,
                typename std::enable_if<std::is_enum<U>::value>::type* = nullptr,
                typename std::enable_if<std::is_convertible<typename std::underlying_type<U>::type, db_integer_type>::value>::type* = nullptr>
            db_value(U val)
                : variant(static_cast<db_integer_type>(val))
            {}

            db_value(const char* str)
                : variant(db_text_type(str))
            {}

            db_value(const std::string& str)
                : variant(str)
            {}

            db_value(db_time_type time)
                : variant(static_cast<int64_t>(std::chrono::system_clock::to_time_t(time)))
            {}
        };
    }
}
