#pragma once
#include <functional>
#include <memory>
#include <map>
#include <chrono>
#include <optional>

#include <sqlitepp/database.h>
#include <sqlitepp/statement.h>
#include <sqlitepp/result_iterator.h>
#include <sqlitepp/orm_entity.h>
#include <sqlitepp/condition.h>

namespace sqlitepp {
    namespace orm {

        enum class db_type {
            text,
            integer,
            real,
            blob
        };

        enum class fk_action {
            no_action,
            restrict,
            set_null,
            set_default,
            cascade
        };

        struct field_info {
            typedef std::function<void(entity*, const db_value&)> setter_fn_t;
            typedef std::function<db_value(const entity*)> getter_fn_t;
            std::string name {};
            db_type type { db_type::integer };
            setter_fn_t setter {};
            getter_fn_t getter {};
            bool nullable { false };
            bool primary_key { false };
            bool row_id { false };
            enum {
                UNIQUE_ID_NONE = 0,
                UNIQUE_ID_DEFAULT = -1,
                UNIQUE_ID_SINGLE_FIELD = -2,
            };
            int unique_id { UNIQUE_ID_NONE };
            std::string fk_table {};
            std::string fk_field {};
            fk_action fk_del_action { fk_action::no_action };
            fk_action fk_update_action { fk_action::no_action };
            std::optional<db_value> default_value {};
        };
        
        struct class_info {
            typedef std::function<std::unique_ptr<entity>(database&)> create_fn_t;
            std::string table;
            std::string schema;
            bool is_temporary = false;
            std::vector<field_info> fields;
            create_fn_t create;

            field_info* get_field_by_name(const std::string& name) {
                for(auto& e : fields) {
                    if(e.name == name) return &e;
                }
                return nullptr;
            }
            const field_info* get_field_by_name(const std::string& name) const {
                for(auto& e : fields) {
                    if(e.name == name) return &e;
                }
                return nullptr;
            }
        };
        
        std::function<void(class_info&, field_info&)> primary_key(bool v = true);
        std::function<void(class_info&, field_info&)> row_id(bool v = true);
        std::function<void(class_info&, field_info&)> nullable(bool v = true);
        std::function<void(class_info&, field_info&)> unique_id(int id = field_info::UNIQUE_ID_SINGLE_FIELD);
        std::function<void(class_info&, field_info&)> fk(const std::string& table, const std::string& field, fk_action del_action, fk_action update_action);
        std::function<void(class_info&, field_info&)> default_value(db_value val);
        std::function<void(class_info&)> schema(std::string schema);
        std::function<void(class_info&)> temporary(bool v = true);
        
        template<typename T, typename std::enable_if<std::is_base_of<entity, T>::value>::type* = nullptr>
        class builder {
            class_info m_info = {};
        public:
            builder(const std::string& table, std::initializer_list<std::function<void(class_info&)>> attributes = {}) {
                m_info.table = table;
                m_info.create = [](database& db){ return std::make_unique<T>(db); };
                for(auto& e : attributes) {
                    e(m_info);
                }
            }
            
			// Special overload for rowid columns
            template<typename Base, typename std::enable_if<std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, const db_integer_type Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::integer, [ptr](entity* e, const db_value& v) {
                    *const_cast<db_integer_type*>(&(static_cast<T*>(e)->*ptr)) = std::get<db_integer_type>(v);
                }, [ptr](const entity* e) -> db_value {
                    return db_value{static_cast<db_integer_type>(static_cast<const T*>(e)->*ptr) };
                }, attributes);
            }


            /** =============   Normal fields    ================**/

            // Special overload for enum class columns
            template<typename U, typename Base, typename std::enable_if<std::is_enum<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            typename std::enable_if<std::is_convertible<typename std::underlying_type<U>::type, db_integer_type>::value, builder&>::type
            field(const std::string& name, U Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::integer, [ptr](entity* e, const db_value& v) {
                    static_cast<T*>(e)->*ptr = static_cast<U>(std::get<db_integer_type>(v));
                }, [ptr](const entity* e) -> db_value {
                    return db_value{static_cast<db_integer_type>(static_cast<const T*>(e)->*ptr) };
                }, attributes);
            }
            template<typename U, typename Base, typename std::enable_if<!std::is_enum<U>::value && std::is_convertible<U, db_integer_type>::value && !std::is_floating_point<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, U Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::integer, [ptr](entity* e, const db_value& v) {
                    static_cast<T*>(e)->*ptr = static_cast<U>(std::get<db_integer_type>(v));
                }, [ptr](const entity* e) -> db_value {
                    return db_value{static_cast<db_integer_type>(static_cast<const T*>(e)->*ptr) };
                }, attributes);
            }
            // Special overload for time values
            template<typename U, typename Base, typename std::enable_if<std::is_same<U, std::chrono::system_clock::time_point>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, U Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::integer, [ptr](entity* e, const db_value& v) {
                    static_cast<T*>(e)->*ptr = std::chrono::system_clock::from_time_t(std::get<db_integer_type>(v));
                }, [ptr](const entity* e) -> db_value {
                    return db_value{static_cast<db_integer_type>(std::chrono::system_clock::to_time_t(static_cast<const T*>(e)->*ptr)) };
                }, attributes);
            }
            template<typename U, typename Base, typename std::enable_if<std::is_convertible<U, db_text_type>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, U Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::text, [ptr](entity* e, const db_value& v) {
                    static_cast<T*>(e)->*ptr = std::get<db_text_type>(v);
                }, [ptr](const entity* e) -> db_value {
                    return db_value{static_cast<db_text_type>(static_cast<const T*>(e)->*ptr) };
                }, attributes);
            }
            template<typename U, typename Base, typename std::enable_if<std::is_convertible<U, db_real_type>::value && std::is_floating_point<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, U Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::real, [ptr](entity* e, const db_value& v) {
                    static_cast<T*>(e)->*ptr = std::get<db_real_type>(v);
                }, [ptr](const entity* e) -> db_value {
                    return db_value{static_cast<db_real_type>(static_cast<const T*>(e)->*ptr) };
                }, attributes);
            }
            template<typename U, typename Base, typename std::enable_if<std::is_convertible<U, db_blob_type>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, U Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::blob, [ptr](entity* e, const db_value& v) {
                    static_cast<T*>(e)->*ptr = std::get<db_blob_type>(v);
                }, [ptr](const entity* e) -> db_value {
                    return db_value{static_cast<db_blob_type>(static_cast<const T*>(e)->*ptr) };
                }, attributes);
            }
            
            /** =============   Optional fields    ================**/

            // Special overload for enum class columns
            template<typename U, typename Base, typename std::enable_if<std::is_enum<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            typename std::enable_if<std::is_convertible<typename std::underlying_type<U>::type, db_integer_type>::value, builder&>::type
            field(const std::string& name, std::optional<U> Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::integer, [ptr](entity* e, const db_value& v) {
                    if(std::holds_alternative<db_integer_type>(v))
                        static_cast<T*>(e)->*ptr = static_cast<U>(std::get<db_integer_type>(v));
                    else (static_cast<T*>(e)->*ptr).reset();
                }, [ptr](const entity* e) -> db_value {
                    if((static_cast<const T*>(e)->*ptr).has_value())
                        return db_value{static_cast<db_integer_type>((static_cast<const T*>(e)->*ptr).value()) };
                    return db_value{db_null_type{}};
                }, { [attributes](class_info& ci, field_info& fi) { fi.nullable = true; for(auto& e : attributes) e(ci, fi);} });
            }
            template<typename U, typename Base, typename std::enable_if<!std::is_enum<U>::value && std::is_convertible<U, db_integer_type>::value && !std::is_floating_point<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, std::optional<U> Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::integer, [ptr](entity* e, const db_value& v) {
                    if(std::holds_alternative<db_integer_type>(v))
                        static_cast<T*>(e)->*ptr = static_cast<U>(std::get<db_integer_type>(v));
                    else (static_cast<T*>(e)->*ptr).reset();
                }, [ptr](const entity* e) -> db_value {
                    if((static_cast<const T*>(e)->*ptr).has_value())
                        return db_value{static_cast<db_integer_type>((static_cast<const T*>(e)->*ptr).value()) };
                    return db_value{db_null_type{}};
                }, { [attributes](class_info& ci, field_info& fi) { fi.nullable = true; for(auto& e : attributes) e(ci, fi);} });
            }
            // Special overload for time values
            template<typename U, typename Base, typename std::enable_if<std::is_same<U, std::chrono::system_clock::time_point>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, std::optional<U> Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::integer, [ptr](entity* e, const db_value& v) {
                    if(std::holds_alternative<db_integer_type>(v))
                        static_cast<T*>(e)->*ptr = std::chrono::system_clock::from_time_t(std::get<db_integer_type>(v));
                    else (static_cast<T*>(e)->*ptr).reset();
                }, [ptr](const entity* e) -> db_value {
                    if((static_cast<const T*>(e)->*ptr).has_value())
                        return db_value{static_cast<db_integer_type>(std::chrono::system_clock::to_time_t((static_cast<const T*>(e)->*ptr).value())) };
                    return db_value{db_null_type{}};
                }, { [attributes](class_info& ci, field_info& fi) { fi.nullable = true; for(auto& e : attributes) e(ci, fi);} });
            }
            template<typename U, typename Base, typename std::enable_if<std::is_convertible<U, db_text_type>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, std::optional<U> Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::text, [ptr](entity* e, const db_value& v) {
                    if(std::holds_alternative<db_text_type>(v))
                        static_cast<T*>(e)->*ptr = std::get<db_text_type>(v);
                    else (static_cast<T*>(e)->*ptr).reset();
                }, [ptr](const entity* e) -> db_value {
                    if((static_cast<const T*>(e)->*ptr).has_value())
                        return db_value{static_cast<db_text_type>((static_cast<const T*>(e)->*ptr).value()) };
                    return db_value{db_null_type{}};
                }, { [attributes](class_info& ci, field_info& fi) { fi.nullable = true; for(auto& e : attributes) e(ci, fi);} });
            }
            template<typename U, typename Base, typename std::enable_if<std::is_convertible<U, db_real_type>::value && std::is_floating_point<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, std::optional<U> Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::real, [ptr](entity* e, const db_value& v) {
                    if(std::holds_alternative<db_real_type>(v))
                        static_cast<T*>(e)->*ptr = std::get<db_real_type>(v);
                    else (static_cast<T*>(e)->*ptr).reset();
                }, [ptr](const entity* e) -> db_value {
                    if((static_cast<const T*>(e)->*ptr).has_value())
                        return db_value{static_cast<db_real_type>((static_cast<const T*>(e)->*ptr).value()) };
                    return db_value{db_null_type{}};
                }, { [attributes](class_info& ci, field_info& fi) { fi.nullable = true; for(auto& e : attributes) e(ci, fi);} });
            }
            template<typename U, typename Base, typename std::enable_if<std::is_convertible<U, db_blob_type>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, std::optional<U> Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::blob, [ptr](entity* e, const db_value& v) {
                    if(std::holds_alternative<db_blob_type>(v))
                        static_cast<T*>(e)->*ptr = std::get<db_blob_type>(v);
                    else (static_cast<T*>(e)->*ptr).reset();
                }, [ptr](const entity* e) -> db_value {
                    if((static_cast<const T*>(e)->*ptr).has_value())
                        return db_value{static_cast<db_blob_type>((static_cast<const T*>(e)->*ptr).value()) };
                    return db_value{db_null_type{}};
                }, { [attributes](class_info& ci, field_info& fi) { fi.nullable = true; for(auto& e : attributes) e(ci, fi);} });
            }
            
            /** =============   Array fields    ================**/

            template<typename U, size_t Size, typename Base, typename std::enable_if<std::is_convertible<U, db_integer_type>::value && !std::is_floating_point<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, std::array<U,Size> Base::*ptr, size_t index, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::integer, [ptr, index](entity* e, const db_value& v) {
                    (static_cast<T*>(e)->*ptr)[index] = std::get<db_integer_type>(v);
                }, [ptr, index](const entity* e) -> db_value {
                    return db_value{static_cast<db_integer_type>((static_cast<const T*>(e)->*ptr)[index]) };
                }, attributes);
            }
            // Special overload for time values
            template<typename U, size_t Size, typename Base, typename std::enable_if<std::is_same<U, std::chrono::system_clock::time_point>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, std::array<U,Size> Base::*ptr, size_t index, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::integer, [ptr, index](entity* e, const db_value& v) {
                    (static_cast<T*>(e)->*ptr)[index] = std::chrono::system_clock::from_time_t(std::get<db_integer_type>(v));
                }, [ptr, index](const entity* e) -> db_value {
                    return db_value{static_cast<db_integer_type>(std::chrono::system_clock::to_time_t((static_cast<const T*>(e)->*ptr)[index])) };
                }, attributes);
            }
            template<typename U, size_t Size, typename Base, typename std::enable_if<std::is_convertible<U, db_text_type>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, std::array<U,Size> Base::*ptr, size_t index, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::text, [ptr, index](entity* e, const db_value& v) {
                    (static_cast<T*>(e)->*ptr)[index] = std::get<db_text_type>(v);
                }, [ptr, index](const entity* e) -> db_value {
                    return db_value{static_cast<db_text_type>((static_cast<const T*>(e)->*ptr)[index]) };
                }, attributes);
            }
            template<typename U, size_t Size, typename Base, typename std::enable_if<std::is_convertible<U, db_real_type>::value && std::is_floating_point<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, std::array<U,Size> Base::*ptr, size_t index, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::real, [ptr, index](entity* e, const db_value& v) {
                    (static_cast<T*>(e)->*ptr)[index] = std::get<db_real_type>(v);
                }, [ptr, index](const entity* e) -> db_value {
                    return db_value{static_cast<db_real_type>((static_cast<const T*>(e)->*ptr)[index]) };
                }, attributes);
            }
            template<typename U, size_t Size, typename Base, typename std::enable_if<std::is_convertible<U, db_blob_type>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, std::array<U,Size> Base::*ptr, size_t index, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                return field(name, db_type::blob, [ptr, index](entity* e, const db_value& v) {
                    (static_cast<T*>(e)->*ptr)[index] = std::get<db_blob_type>(v);
                }, [ptr, index](const entity* e) -> db_value {
                    return db_value{static_cast<db_blob_type>((static_cast<const T*>(e)->*ptr)[index]) };
                }, attributes);
            }

            template<typename U, size_t Size, typename Base, typename std::enable_if<
                (std::is_convertible<U, db_integer_type>::value
                || std::is_convertible<U, db_text_type>::value
                || std::is_convertible<U, db_real_type>::value
                || std::is_convertible<U, db_blob_type>::value
                || std::is_same<U, std::chrono::system_clock::time_point>::value)
				&& std::is_base_of<Base, T>::value>::type* = nullptr>
            builder& field(const std::string& name, std::array<U,Size> Base::*ptr, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes = {}) {
                for(size_t i = 0; i < Size; i++) {
                    this->field(name + std::to_string(i), ptr, i, attributes);
                }
                return *this;
            }
            
            builder& field(const std::string& name, db_type type, field_info::setter_fn_t set, field_info::getter_fn_t get, std::initializer_list<std::function<void(class_info&, field_info&)>> attributes) {
                field_info i = {};
                i.name = name;
                i.type = type;
                i.getter = get;
                i.setter = set;
                for(auto& e : attributes)
                    e(m_info, i);
                m_info.fields.push_back(i);
                return *this;
            }

            class_info build() {
                return m_info;
            }
        };

        std::string generate_create_table(const class_info& info);

        int64_t remove(database& db, const class_info& info, const std::string& where = "", std::vector<db_value> vals = {});
        int64_t count(database& db, const class_info& info, const std::string& where = "", std::vector<db_value> vals = {});
        template<size_t A,size_t B>
        inline int64_t remove(database& db, const class_info& info, const condition<A,B>& where) {
            auto p = where.as_partial();
            return remove(db, info, p.query, std::vector<db_value>(p.params.begin(), p.params.end()));
        }
        template<size_t A,size_t B>
        inline int64_t count(database& db, const class_info& info, const condition<A,B>& where) {
            auto p = where.as_partial();
            return count(db, info, p.query, std::vector<db_value>(p.params.begin(), p.params.end()));
        }

        std::vector<std::unique_ptr<entity>> select_multiple(database& db, const class_info& info, const std::string& where = "", std::vector<db_value> vals = {});
        std::unique_ptr<entity> select_one(database& db, const class_info& info, const std::string& where = "", std::vector<db_value> vals = {});

        template<typename T>
        inline std::vector<std::unique_ptr<T>> select_multiple(database& db, const class_info& info, const std::string& where = "", std::vector<db_value> vals = {}) {
            auto m = select_multiple(db, info, where, vals);
            std::vector<std::unique_ptr<T>> res;
            res.reserve(m.size());
            for(auto& e : m) {
                res.emplace_back(static_cast<T*>(e.release()));
            }
            return res;
        }

        template <class T, typename std::enable_if<std::is_same<decltype(T::_class_info), const sqlitepp::orm::class_info>::value>::type* = nullptr>
        inline std::vector<std::unique_ptr<T>> select_multiple(database& db, const std::string& where = "", std::vector<db_value> vals = {}) {
            return select_multiple<T>(db, T::_class_info, where, vals);
        }


        template<class T, size_t A, size_t B, typename std::enable_if<std::is_same<decltype(T::_class_info), const sqlitepp::orm::class_info>::value>::type* = nullptr>
        inline std::vector<std::unique_ptr<T>> select_multiple(database& db, const condition<A,B>& where) {
            auto p = where.as_partial();
            return select_multiple<T>(db, p.query, std::vector<db_value>(p.params.begin(), p.params.end()));
        }

        template<typename T>
        inline std::unique_ptr<T> select_one(database& db, const class_info& info, const std::string& where = "", std::vector<db_value> vals = {}) {
            return std::unique_ptr<T>(static_cast<T*>(select_one(db, info, where, vals).release()));
        }

        template <class T, typename std::enable_if<std::is_same<decltype(T::_class_info), const sqlitepp::orm::class_info>::value>::type* = nullptr>
        inline std::unique_ptr<T> select_one(database& db, const std::string& where = "", std::vector<db_value> vals = {}) {
            return select_one<T>(db, T::_class_info, where, vals);
        }

        template<class T, size_t A, size_t B, typename std::enable_if<std::is_same<decltype(T::_class_info), const sqlitepp::orm::class_info>::value>::type* = nullptr>
        inline std::unique_ptr<T> select_one(database& db, const condition<A,B>& where) {
            auto p = where.as_partial();
            return select_one<T>(db, p.query, std::vector<db_value>(p.params.begin(), p.params.end()));
        }

        inline std::vector<std::shared_ptr<entity>> select_multiple_shared(database& db, const class_info& info, const std::string& where = "", std::vector<db_value> vals = {}) {
            auto m = select_multiple(db, info, where, vals);
            std::vector<std::shared_ptr<entity>> res;
            res.reserve(m.size());
            for(auto& e : m) {
                res.emplace_back(std::move(e));
            }
            return res;
        }

        inline std::shared_ptr<entity> select_one_shared(database& db, const class_info& info, const std::string& where = "", std::vector<db_value> vals = {}) {
            return select_one(db, info, where, vals);
        }

        template<typename T>
        inline std::vector<std::unique_ptr<T>> select_multiple_shared(database& db, const class_info& info, const std::string& where = "", std::vector<db_value> vals = {}) {
            auto m = select_multiple(db, info, where, vals);
            std::vector<std::shared_ptr<T>> res;
            res.reserve(m.size());
            for(auto& e : m) {
                res.emplace_back(std::static_pointer_cast<T>(std::move(e)));
            }
            return res;
        }

        template <class T, typename std::enable_if<std::is_same<decltype(T::_class_info), const sqlitepp::orm::class_info>::value>::type* = nullptr>
        inline std::vector<std::shared_ptr<T>> select_multiple_shared(database& db, const std::string& where = "", std::vector<db_value> vals = {}) {
            return select_multiple_shared<T>(db, T::_class_info, where, vals);
        }

        template<typename T>
        inline std::shared_ptr<T> select_one_shared(database& db, const class_info& info, const std::string& where = "", std::vector<db_value> vals = {}) {
            return std::static_pointer_cast<T>(select_one(db, info, where, vals));
        }

        template <class T, typename std::enable_if<std::is_same<decltype(T::_class_info), const sqlitepp::orm::class_info>::value>::type* = nullptr>
        inline std::shared_ptr<T> select_one_shared(database& db, const std::string& where = "", std::vector<db_value> vals = {}) {
            return std::static_pointer_cast<T>(select_one(db, T::_class_info, where, vals));
        }
    }
}
