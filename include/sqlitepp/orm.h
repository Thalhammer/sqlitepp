#pragma once
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>

#include <sqlitepp/database.h>
#include <sqlitepp/fwd.h>
#include <sqlitepp/orm.h>
#include <sqlitepp/result_iterator.h>
#include <sqlitepp/statement.h>

namespace sqlitepp {
	namespace orm {
		class entity;

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
			std::string name{};
			db_type type{db_type::integer};
			setter_fn_t setter{};
			getter_fn_t getter{};
			bool nullable{false};
			bool primary_key{false};
			bool row_id{false};
			enum {
				UNIQUE_ID_NONE = 0,
				UNIQUE_ID_DEFAULT = -1,
				UNIQUE_ID_SINGLE_FIELD = -2,
			};
			int unique_id{UNIQUE_ID_NONE};
			std::string fk_table{};
			std::string fk_field{};
			fk_action fk_del_action{fk_action::no_action};
			fk_action fk_update_action{fk_action::no_action};
			std::optional<db_value> default_value{};
		};

		struct class_info {
			typedef std::function<std::unique_ptr<entity>(database&)> create_fn_t;
			std::string table;
			std::string schema;
			bool is_temporary = false;
			std::vector<field_info> fields;

			field_info* get_field_by_name(const std::string& name) {
				for (auto& e : fields) {
					if (e.name == name) return &e;
				}
				return nullptr;
			}
			const field_info* get_field_by_name(const std::string& name) const {
				for (auto& e : fields) {
					if (e.name == name) return &e;
				}
				return nullptr;
			}
		};

		namespace detail {
			inline void bind_db_val(statement& s, size_t i, const db_value& val) {
				if (std::holds_alternative<db_null_type>(val))
					return;
				if (std::holds_alternative<db_blob_type>(val))
					return s.bind(i, std::get<db_blob_type>(val));
				if (std::holds_alternative<db_text_type>(val))
					return s.bind(i, std::get<db_text_type>(val));
				if (std::holds_alternative<db_integer_type>(val))
					return s.bind(i, std::get<db_integer_type>(val));
				if (std::holds_alternative<db_real_type>(val))
					return s.bind(i, std::get<db_real_type>(val));
				throw std::logic_error("unreachable");
			}
		} // namespace detail

		class entity {
			/**
             * \brief The rowid of this entity.
             * 
             * SQLITE always has a primary key, if none is specified
             * it autogenerates an special column called _rowid_ which acts like
             * INTEGER PRIMARY KEY and provides a unique id to every entity.
             * If the table defines an INTEGER PRIMARY KEY, then _rowid_ is an alias to it.
             * We use it to identify each entity of a particular table.
             * This value also allows checking if the entity is stored in the database or not.
             * If it was read from a table it will contain a positive number, if it is a new object
             * or was removed from the table it will be negative.
             * 
             * NOTE: If you read the same row into multiple entities and delete it using one of them
             * NOTE: the behaviour is undefined. Currently the _rowid_ is only changed on the one used
             * NOTE: to delete the row, but this may change in the future.
             */
			int64_t _rowid_{-1};
			sqlitepp::database& m_db;
			std::vector<db_value> m_db_vals{};

		public:
			void from_result(const sqlitepp::result_iterator& it) {
				auto& info = this->get_class_info();
				this->_rowid_ = it.column_int64(0);
				this->m_db_vals.resize(info.fields.size());
				for (size_t i = 1; i <= info.fields.size(); i++) {
					auto& e = info.fields[i - 1];
					db_value val{db_null_type{}};
					if (!it.column_is_null(i)) {
						switch (e.type) {
						case db_type::blob: {
							auto data = it.column_blob(i);
							val.emplace<db_blob_type>(data.second);
							std::copy(static_cast<const uint8_t*>(data.first),
									  static_cast<const uint8_t*>(data.first) + data.second,
									  std::get<db_blob_type>(val).data());
							break;
						}
						case db_type::text: val = it.column_string(i); break;
						case db_type::real: val = it.column_double(i); break;
						case db_type::integer: val = it.column_int64(i); break;
						}
					}
					e.setter(this, val);
					this->m_db_vals[i - 1] = val;
				}
			}

		private:
			void insert() {
				auto& info = this->get_class_info();
				std::string query = "INSERT INTO ";
				if (!info.schema.empty()) query += "`" + info.schema + "`";
				query += "`" + info.table + "` (";
				for (size_t i = 0; i < info.fields.size(); i++) {
					if (i != 0) query += ", ";
					query += "`" + info.fields[i].name + "`";
				}
				query += ") VALUES (";
				for (size_t i = 0; i < info.fields.size(); i++) {
					if (i != 0) query += ", ";
					query += "?";
				}
				query += ");";
				statement stmt(this->m_db, query);
				std::vector<db_value> vals = this->m_db_vals;
				vals.resize(info.fields.size());
				for (size_t i = 0; i < info.fields.size(); i++) {
					if (info.fields[i].row_id) continue;
					vals[i] = info.fields[i].getter(this);
					detail::bind_db_val(stmt, i + 1, vals[i]);
				}
				stmt.execute();
				this->_rowid_ = this->m_db.last_insert_rowid();
				for (size_t i = 0; i < info.fields.size(); i++) {
					auto& f = info.fields[i];
					if (!f.row_id) continue;
					f.setter(this, this->_rowid_);
					vals[i] = this->_rowid_;
				}
				this->m_db_vals = std::move(vals);
			}
			void update() {
				auto& info = this->get_class_info();
				std::string query = "UPDATE ";
				if (!info.schema.empty()) query += "`" + info.schema + "`";
				query += "`" + info.table + "` SET ";
				for (size_t i = 0; i < info.fields.size(); i++) {
					if (i != 0) query += ", ";
					query += "`" + info.fields[i].name + "` = ?";
				}
				query += " WHERE _rowid_ = ?;";
				statement stmt(this->m_db, query);
				std::vector<db_value> vals = this->m_db_vals;
				vals.resize(info.fields.size());
				for (size_t i = 0; i < info.fields.size(); i++) {
					auto val = info.fields[i].getter(this);
					detail::bind_db_val(stmt, i + 1, val);
					vals[i] = val;
				}
				this->m_db_vals = std::move(vals);
				stmt.bind(info.fields.size() + 1, this->_rowid_);
				stmt.execute();
			}

		public:
			explicit entity(sqlitepp::database& db)
				: m_db(db) {}
			virtual ~entity() {}

			/**
             * \brief Get class info of this entity
             * 
             * This should be valid at least for the duration this object exists.
             */
			virtual const class_info& get_class_info() const noexcept = 0;
			/**
             * \brief Check if this entity was changed
             */
			bool is_modified() const {
				auto& info = this->get_class_info();
				if (m_db_vals.size() != info.fields.size()) return true;
				for (size_t i = 0; i < m_db_vals.size(); i++) {
					auto dbval = info.fields[i].getter(this);
					if (dbval != m_db_vals[i]) return true;
				}
				return false;
			}
			/**
             * \brief Reset the entity to its original values
             */
			void reset() {
				auto& info = this->get_class_info();
				for (size_t i = 0; i < std::min(m_db_vals.size(), info.fields.size()); i++) {
					info.fields[i].setter(this, m_db_vals[i]);
				}
			}
			/**
             * \brief Persist all changes to the database
             */
			void save() {
				if (this->_rowid_ >= 0)
					this->update();
				else
					this->insert();
			}
			/**
             * \brief Remove the entity from the database
             */
			void remove() {
				if (this->_rowid_ < 0) return;
				auto& info = this->get_class_info();
				std::string query = "DELETE FROM ";
				if (!info.schema.empty()) query += "`" + info.schema + "`";
				query += "`" + info.table + "` WHERE _rowid_ = ?;";

				statement stmt(this->m_db, query);
				stmt.bind(1, this->_rowid_);
				stmt.execute();
				this->_rowid_ = -1;
				for (auto& f : info.fields) {
					if (f.row_id) f.setter(this, this->_rowid_);
				}
			}
		};

		inline constexpr auto primary_key(bool v = true) noexcept {
			return [v](class_info&, field_info& f) {
				f.primary_key = v;
			};
		}
		inline constexpr auto row_id(bool v = true) noexcept {
			return [v](class_info&, field_info& f) {
				f.primary_key = v;
				f.row_id = v;
			};
		}
		inline constexpr auto nullable(bool v = true) noexcept {
			return [v](class_info&, field_info& f) {
				f.nullable = v;
			};
		}
		inline constexpr auto unique_id(int id = field_info::UNIQUE_ID_SINGLE_FIELD) noexcept {
			return [id](class_info&, field_info& f) {
				f.unique_id = id;
			};
		}
		inline constexpr auto fk(const char* table, const char* field, fk_action del_action, fk_action update_action) noexcept {
			return [table, field, del_action, update_action](class_info&, field_info& f) {
				f.fk_table = table;
				f.fk_field = field;
				f.fk_del_action = del_action;
				f.fk_update_action = update_action;
			};
		}
		inline auto default_value(db_value val) {
			return [val](class_info&, field_info& f) {
				f.default_value = val;
			};
		}
		inline constexpr auto schema(const char* schema) noexcept {
			return [schema](class_info& c) { c.schema = schema; };
		}
		inline constexpr auto temporary(bool v = true) noexcept {
			return [v](class_info& c) { c.is_temporary = v; };
		}

		namespace detail {
			template <typename... TParams>
			struct invoke_all {
				template <typename Func1, typename... Func>
				static void invoke(TParams&&... params, Func1&& fn, Func&&... functs) {
					fn(std::forward<TParams>(params)...);
					invoke(std::forward<TParams>(params)..., std::forward<Func>(functs)...);
				}
				static void invoke(TParams&&...) {}
			};
		} // namespace detail

		template <typename T, typename std::enable_if<std::is_base_of<entity, T>::value>::type* = nullptr>
		class builder {
			class_info m_info = {};

		public:
			template <typename... TAttribute>
			builder(const std::string& table, TAttribute&&... attributes) {
				m_info.table = table;
				detail::invoke_all<class_info&>::invoke(m_info, std::forward<TAttribute>(attributes)...);
			}

			// Special overload for rowid columns
			template <typename Base, typename... TAttribute, typename std::enable_if<std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, const db_integer_type Base::*ptr, TAttribute&&... attributes) {
				return field(
					name,
					db_type::integer,
					[ptr](entity* e, const db_value& v) { *const_cast<db_integer_type*>(&(static_cast<T*>(e)->*ptr)) = std::get<db_integer_type>(v); },
					[ptr](const entity* e) -> db_value { return static_cast<db_integer_type>(static_cast<const T*>(e)->*ptr); },
					std::forward<TAttribute>(attributes)...);
			}

			/** =============   Normal fields    ================**/

			// Special overload for enum class columns
			template <typename U, typename Base, typename... TAttribute, typename std::enable_if<std::is_enum<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			typename std::enable_if<std::is_convertible<typename std::underlying_type<U>::type, db_integer_type>::value, builder&>::type
			field(const std::string& name, U Base::*ptr, TAttribute&&... attributes) {
				return field(
					name,
					db_type::integer, [ptr](entity* e, const db_value& v) { static_cast<T*>(e)->*ptr = static_cast<U>(std::get<db_integer_type>(v)); }, [ptr](const entity* e) -> db_value { return db_value{static_cast<db_integer_type>(static_cast<const T*>(e)->*ptr)}; }, std::forward<TAttribute>(attributes)...);
			}
			template <typename U, typename Base, typename... TAttribute, typename std::enable_if<!std::is_enum<U>::value && std::is_convertible<U, db_integer_type>::value && !std::is_floating_point<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, U Base::*ptr, TAttribute&&... attributes) {
				return field(
					name, db_type::integer, [ptr](entity* e, const db_value& v) { static_cast<T*>(e)->*ptr = static_cast<U>(std::get<db_integer_type>(v)); }, [ptr](const entity* e) -> db_value { return db_value{static_cast<db_integer_type>(static_cast<const T*>(e)->*ptr)}; }, std::forward<TAttribute>(attributes)...);
			}
			// Special overload for time values
			template <typename U, typename Base, typename... TAttribute, typename std::enable_if<std::is_same<U, std::chrono::system_clock::time_point>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, U Base::*ptr, TAttribute&&... attributes) {
				return field(
					name, db_type::integer, [ptr](entity* e, const db_value& v) { static_cast<T*>(e)->*ptr = std::chrono::system_clock::from_time_t(std::get<db_integer_type>(v)); }, [ptr](const entity* e) -> db_value { return db_value{static_cast<db_integer_type>(std::chrono::system_clock::to_time_t(static_cast<const T*>(e)->*ptr))}; }, std::forward<TAttribute>(attributes)...);
			}
			template <typename U, typename Base, typename... TAttribute, typename std::enable_if<std::is_convertible<U, db_text_type>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, U Base::*ptr, TAttribute&&... attributes) {
				return field(
					name, db_type::text, [ptr](entity* e, const db_value& v) { static_cast<T*>(e)->*ptr = std::get<db_text_type>(v); }, [ptr](const entity* e) -> db_value { return db_value{static_cast<db_text_type>(static_cast<const T*>(e)->*ptr)}; }, std::forward<TAttribute>(attributes)...);
			}
			template <typename U, typename Base, typename... TAttribute, typename std::enable_if<std::is_convertible<U, db_real_type>::value && std::is_floating_point<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, U Base::*ptr, TAttribute&&... attributes) {
				return field(
					name, db_type::real, [ptr](entity* e, const db_value& v) { static_cast<T*>(e)->*ptr = std::get<db_real_type>(v); }, [ptr](const entity* e) -> db_value { return db_value{static_cast<db_real_type>(static_cast<const T*>(e)->*ptr)}; }, std::forward<TAttribute>(attributes)...);
			}
			template <typename U, typename Base, typename... TAttribute, typename std::enable_if<std::is_convertible<U, db_blob_type>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, U Base::*ptr, TAttribute&&... attributes) {
				return field(
					name, db_type::blob, [ptr](entity* e, const db_value& v) { static_cast<T*>(e)->*ptr = std::get<db_blob_type>(v); }, [ptr](const entity* e) -> db_value { return db_value{static_cast<db_blob_type>(static_cast<const T*>(e)->*ptr)}; }, std::forward<TAttribute>(attributes)...);
			}

			/** =============   Optional fields    ================**/

			// Special overload for enum class columns
			template <typename U, typename Base, typename... TAttribute, typename std::enable_if<std::is_enum<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			typename std::enable_if<std::is_convertible<typename std::underlying_type<U>::type, db_integer_type>::value, builder&>::type
			field(const std::string& name, std::optional<U> Base::*ptr, TAttribute&&... attributes) {
				return field(
					name, db_type::integer, [ptr](entity* e, const db_value& v) {
                    if(std::holds_alternative<db_integer_type>(v))
                        static_cast<T*>(e)->*ptr = static_cast<U>(std::get<db_integer_type>(v));
                    else (static_cast<T*>(e)->*ptr).reset(); }, [ptr](const entity* e) -> db_value {
                    if((static_cast<const T*>(e)->*ptr).has_value())
                        return db_value{static_cast<db_integer_type>((static_cast<const T*>(e)->*ptr).value()) };
                    return db_value{db_null_type{}}; }, [](class_info&, field_info& fi) { fi.nullable = true; }, std::forward<TAttribute>(attributes)...);
			}
			template <typename U, typename Base, typename... TAttribute, typename std::enable_if<!std::is_enum<U>::value && std::is_convertible<U, db_integer_type>::value && !std::is_floating_point<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, std::optional<U> Base::*ptr, TAttribute&&... attributes) {
				return field(
					name, db_type::integer, [ptr](entity* e, const db_value& v) {
                    if(std::holds_alternative<db_integer_type>(v))
                        static_cast<T*>(e)->*ptr = static_cast<U>(std::get<db_integer_type>(v));
                    else (static_cast<T*>(e)->*ptr).reset(); }, [ptr](const entity* e) -> db_value {
                    if((static_cast<const T*>(e)->*ptr).has_value())
                        return db_value{static_cast<db_integer_type>((static_cast<const T*>(e)->*ptr).value()) };
                    return db_value{db_null_type{}}; }, [](class_info&, field_info& fi) { fi.nullable = true; }, std::forward<TAttribute>(attributes)...);
			}
			// Special overload for time values
			template <typename U, typename Base, typename... TAttribute, typename std::enable_if<std::is_same<U, std::chrono::system_clock::time_point>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, std::optional<U> Base::*ptr, TAttribute&&... attributes) {
				return field(
					name, db_type::integer, [ptr](entity* e, const db_value& v) {
                    if(std::holds_alternative<db_integer_type>(v))
                        static_cast<T*>(e)->*ptr = std::chrono::system_clock::from_time_t(std::get<db_integer_type>(v));
                    else (static_cast<T*>(e)->*ptr).reset(); }, [ptr](const entity* e) -> db_value {
                    if((static_cast<const T*>(e)->*ptr).has_value())
                        return db_value{static_cast<db_integer_type>(std::chrono::system_clock::to_time_t((static_cast<const T*>(e)->*ptr).value())) };
                    return db_value{db_null_type{}}; }, [](class_info&, field_info& fi) { fi.nullable = true; }, std::forward<TAttribute>(attributes)...);
			}
			template <typename U, typename Base, typename... TAttribute, typename std::enable_if<std::is_convertible<U, db_text_type>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, std::optional<U> Base::*ptr, TAttribute&&... attributes) {
				return field(
					name, db_type::text, [ptr](entity* e, const db_value& v) {
                    if(std::holds_alternative<db_text_type>(v))
                        static_cast<T*>(e)->*ptr = std::get<db_text_type>(v);
                    else (static_cast<T*>(e)->*ptr).reset(); }, [ptr](const entity* e) -> db_value {
                    if((static_cast<const T*>(e)->*ptr).has_value())
                        return db_value{static_cast<db_text_type>((static_cast<const T*>(e)->*ptr).value()) };
                    return db_value{db_null_type{}}; }, [](class_info&, field_info& fi) { fi.nullable = true; }, std::forward<TAttribute>(attributes)...);
			}
			template <typename U, typename Base, typename... TAttribute, typename std::enable_if<std::is_convertible<U, db_real_type>::value && std::is_floating_point<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, std::optional<U> Base::*ptr, TAttribute&&... attributes) {
				return field(
					name, db_type::real, [ptr](entity* e, const db_value& v) {
                    if(std::holds_alternative<db_real_type>(v))
                        static_cast<T*>(e)->*ptr = std::get<db_real_type>(v);
                    else (static_cast<T*>(e)->*ptr).reset(); }, [ptr](const entity* e) -> db_value {
                    if((static_cast<const T*>(e)->*ptr).has_value())
                        return db_value{static_cast<db_real_type>((static_cast<const T*>(e)->*ptr).value()) };
                    return db_value{db_null_type{}}; }, [](class_info&, field_info& fi) { fi.nullable = true; }, std::forward<TAttribute>(attributes)...);
			}
			template <typename U, typename Base, typename... TAttribute, typename std::enable_if<std::is_convertible<U, db_blob_type>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, std::optional<U> Base::*ptr, TAttribute&&... attributes) {
				return field(
					name, db_type::blob, [ptr](entity* e, const db_value& v) {
                    if(std::holds_alternative<db_blob_type>(v))
                        static_cast<T*>(e)->*ptr = std::get<db_blob_type>(v);
                    else (static_cast<T*>(e)->*ptr).reset(); }, [ptr](const entity* e) -> db_value {
                    if((static_cast<const T*>(e)->*ptr).has_value())
                        return db_value{static_cast<db_blob_type>((static_cast<const T*>(e)->*ptr).value()) };
                    return db_value{db_null_type{}}; }, [](class_info&, field_info& fi) { fi.nullable = true; }, std::forward<TAttribute>(attributes)...);
			}

			/** =============   Array fields    ================**/

			template <typename U, size_t Size, typename Base, typename... TAttribute, typename std::enable_if<std::is_convertible<U, db_integer_type>::value && !std::is_floating_point<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, std::array<U, Size> Base::*ptr, size_t index, TAttribute&&... attributes) {
				return field(
					name, db_type::integer, [ptr, index](entity* e, const db_value& v) { (static_cast<T*>(e)->*ptr)[index] = std::get<db_integer_type>(v); }, [ptr, index](const entity* e) -> db_value { return db_value{static_cast<db_integer_type>((static_cast<const T*>(e)->*ptr)[index])}; }, std::forward<TAttribute>(attributes)...);
			}
			// Special overload for time values
			template <typename U, size_t Size, typename Base, typename... TAttribute, typename std::enable_if<std::is_same<U, std::chrono::system_clock::time_point>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, std::array<U, Size> Base::*ptr, size_t index, TAttribute&&... attributes) {
				return field(
					name, db_type::integer, [ptr, index](entity* e, const db_value& v) { (static_cast<T*>(e)->*ptr)[index] = std::chrono::system_clock::from_time_t(std::get<db_integer_type>(v)); }, [ptr, index](const entity* e) -> db_value { return db_value{static_cast<db_integer_type>(std::chrono::system_clock::to_time_t((static_cast<const T*>(e)->*ptr)[index]))}; }, std::forward<TAttribute>(attributes)...);
			}
			template <typename U, size_t Size, typename Base, typename... TAttribute, typename std::enable_if<std::is_convertible<U, db_text_type>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, std::array<U, Size> Base::*ptr, size_t index, TAttribute&&... attributes) {
				return field(
					name, db_type::text, [ptr, index](entity* e, const db_value& v) { (static_cast<T*>(e)->*ptr)[index] = std::get<db_text_type>(v); }, [ptr, index](const entity* e) -> db_value { return db_value{static_cast<db_text_type>((static_cast<const T*>(e)->*ptr)[index])}; }, std::forward<TAttribute>(attributes)...);
			}
			template <typename U, size_t Size, typename Base, typename... TAttribute, typename std::enable_if<std::is_convertible<U, db_real_type>::value && std::is_floating_point<U>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, std::array<U, Size> Base::*ptr, size_t index, TAttribute&&... attributes) {
				return field(
					name, db_type::real, [ptr, index](entity* e, const db_value& v) { (static_cast<T*>(e)->*ptr)[index] = std::get<db_real_type>(v); }, [ptr, index](const entity* e) -> db_value { return db_value{static_cast<db_real_type>((static_cast<const T*>(e)->*ptr)[index])}; }, std::forward<TAttribute>(attributes)...);
			}
			template <typename U, size_t Size, typename Base, typename... TAttribute, typename std::enable_if<std::is_convertible<U, db_blob_type>::value && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, std::array<U, Size> Base::*ptr, size_t index, TAttribute&&... attributes) {
				return field(
					name, db_type::blob, [ptr, index](entity* e, const db_value& v) { (static_cast<T*>(e)->*ptr)[index] = std::get<db_blob_type>(v); }, [ptr, index](const entity* e) -> db_value { return db_value{static_cast<db_blob_type>((static_cast<const T*>(e)->*ptr)[index])}; }, std::forward<TAttribute>(attributes)...);
			}

			template <typename U, size_t Size, typename Base, typename... TAttribute, typename std::enable_if<(std::is_convertible<U, db_integer_type>::value || std::is_convertible<U, db_text_type>::value || std::is_convertible<U, db_real_type>::value || std::is_convertible<U, db_blob_type>::value || std::is_same<U, std::chrono::system_clock::time_point>::value) && std::is_base_of<Base, T>::value>::type* = nullptr>
			builder& field(const std::string& name, std::array<U, Size> Base::*ptr, TAttribute&&... attributes) {
				for (size_t i = 0; i < Size; i++) {
					this->field(name + std::to_string(i), ptr, i, std::forward<TAttribute>(attributes)...);
				}
				return *this;
			}

			template <typename... TAttribute>
			builder& field(const std::string& name, db_type type, field_info::setter_fn_t set, field_info::getter_fn_t get, TAttribute&&... attributes) {
				field_info i = {};
				i.name = name;
				i.type = type;
				i.getter = get;
				i.setter = set;
				detail::invoke_all<class_info&, field_info&>::invoke(m_info, i, std::forward<TAttribute>(attributes)...);
				m_info.fields.push_back(i);
				return *this;
			}

			class_info build() {
				return m_info;
			}
		};

		inline std::string generate_create_table(const class_info& info) {
			std::string res;
			if (info.is_temporary)
				res += "CREATE TEMPORARY TABLE ";
			else
				res += "CREATE TABLE ";
			if (!info.schema.empty()) res += "`" + info.schema + "`.";
			res += "`" + info.table + "` (\n";
			bool first = true;
			std::vector<std::string> pk_fields;
			std::map<int, std::vector<std::string>> unique_fields;
			for (auto& e : info.fields) {
				if (e.primary_key) pk_fields.push_back(e.name);
				if (e.unique_id > 0 || e.unique_id == field_info::UNIQUE_ID_DEFAULT) {
					unique_fields[e.unique_id].push_back(e.name);
				}
				if (!first) res += ",\n";
				first = false;
				res += "\t`" + e.name + "`";
				switch (e.type) {
				case db_type::text: res += " TEXT"; break;
				case db_type::integer: res += " INTEGER"; break;
				case db_type::real: res += " REAL"; break;
				case db_type::blob: res += " BLOB"; break;
				}
				if (!e.nullable) res += " NOT NULL";
				if (e.unique_id == field_info::UNIQUE_ID_SINGLE_FIELD) res += " UNIQUE";
			}
			if (!pk_fields.empty()) {
				res += ",\n\tPRIMARY KEY(";
				for (size_t i = 0; i < pk_fields.size(); i++) {
					if (i != 0) res += ", ";
					res += "`" + pk_fields[i] + "`";
				}
				res += ")";
			}
			for (auto& e : unique_fields) {
				res += ",\n\tUNIQUE(";
				for (size_t i = 0; i < e.second.size(); i++) {
					res += "`" + e.second[i] + "`";
					if (i != e.second.size() - 1) res += ", ";
				}
				res += ")";
			}
			for (auto& e : info.fields) {
				if (e.fk_table.empty()) continue;
				res += ",\n\tFOREIGN KEY (`" + e.name + "`) REFERENCES ";
				res += "`" + e.fk_table + "` (`" + e.fk_field + "`) ";
				switch (e.fk_del_action) {
				case fk_action::no_action: res += "ON DELETE NO ACTION"; break;
				case fk_action::restrict: res += "ON DELETE RESTRICT"; break;
				case fk_action::set_null: res += "ON DELETE SET NULL"; break;
				case fk_action::set_default: res += "ON DELETE SET DEFAULT"; break;
				case fk_action::cascade: res += "ON DELETE CASCADE"; break;
				}
				res += " ";
				switch (e.fk_update_action) {
				case fk_action::no_action: res += "ON UPDATE NO ACTION"; break;
				case fk_action::restrict: res += "ON UPDATE RESTRICT"; break;
				case fk_action::set_null: res += "ON UPDATE SET NULL"; break;
				case fk_action::set_default: res += "ON UPDATE SET DEFAULT"; break;
				case fk_action::cascade: res += "ON UPDATE CASCADE"; break;
				}
			}
			res += "\n);";
			return res;
		}

		inline int64_t remove(database& db, const class_info& info, const std::string& where = "", std::vector<db_value> vals = {}) {
			std::string query = "DELETE FROM ";
			if (!info.schema.empty()) query += "`" + info.schema + "`";
			query += "`" + info.table + "`";
			if (!where.empty()) query += " WHERE " + where;
			query += ";";

			auto nchanges = db.total_changes();
			sqlitepp::statement stmt(db, query);
			for (size_t i = 0; i < vals.size(); i++)
				detail::bind_db_val(stmt, i + 1, vals[i]);
			stmt.execute();
			return db.total_changes() - nchanges;
		}
		inline int64_t count(database& db, const class_info& info, const std::string& where = "", std::vector<db_value> vals = {}) {
			std::string query = "SELECT COUNT(*) FROM ";
			if (!info.schema.empty()) query += "`" + info.schema + "`";
			query += "`" + info.table + "`";
			if (!where.empty()) query += " WHERE " + where;
			query += ";";

			sqlitepp::statement stmt(db, query);
			for (size_t i = 0; i < vals.size(); i++)
				detail::bind_db_val(stmt, i + 1, vals[i]);
			auto it = stmt.iterator();
			if (!it.next()) return -1; // TODO: This should never happen :(
			return it.column_int64(0);
		}

		namespace detail {
			inline std::string build_select_all(const class_info& info, const std::string& where) {
				std::string query = "SELECT _rowid_ as _rowid_";
				for (auto& e : info.fields) {
					query += ", `" + e.name + "`";
				}
				query += " FROM ";
				if (!info.schema.empty()) query += "`" + info.schema + "`";
				query += "`" + info.table + "`";
				if (!where.empty()) {
					query += " WHERE " + where;
				}
				query += ";";
				return query;
			}
		} // namespace detail

		template <typename T>
		inline std::vector<std::unique_ptr<T>> select_multiple(database& db, const class_info& info, const std::string& where, std::vector<db_value> vals) {
			sqlitepp::statement stmt(db, detail::build_select_all(info, where));
			for (size_t i = 0; i < vals.size(); i++)
				detail::bind_db_val(stmt, i + 1, vals[i]);
			auto it = stmt.iterator();
			std::vector<std::unique_ptr<T>> res;
			while (it.next()) {
				auto e = std::make_unique<T>(db);
				e->from_result(it);
				res.emplace_back(std::move(e));
			}
			return res;
		}

		template <class T, typename std::enable_if<std::is_same<decltype(T::_class_info), const sqlitepp::orm::class_info>::value>::type* = nullptr>
		inline std::vector<std::unique_ptr<T>> select_multiple(database& db, const std::string& where, std::vector<db_value> vals) {
			return select_multiple<T>(db, T::_class_info, where, vals);
		}

		template <typename T, typename... Args>
		inline std::vector<std::unique_ptr<T>> select_multiple(database& db, const class_info& info, const std::string& where = "", Args&&... args) {
			sqlitepp::statement stmt(db, detail::build_select_all(info, where));
			stmt.bind_all<Args...>(std::forward<Args>(args)...);
			auto it = stmt.iterator();
			std::vector<std::unique_ptr<T>> res;
			while (it.next()) {
				auto e = std::make_unique<T>(db);
				e->from_result(it);
				res.emplace_back(std::move(e));
			}
			return res;
		}

		template <class T, typename... Args, typename std::enable_if<std::is_same<decltype(T::_class_info), const sqlitepp::orm::class_info>::value>::type* = nullptr>
		inline std::vector<std::unique_ptr<T>> select_multiple(database& db, const std::string& where = "", Args&&... args) {
			return select_multiple<T>(db, T::_class_info, where, std::forward<Args>(args)...);
		}

		template <typename T, typename... Args>
		inline size_t select_multiple_into(database& db, const class_info& info, std::vector<T>& rows, const std::string& where = "", Args&&... args) {
			sqlitepp::statement stmt(db, detail::build_select_all(info, where));
			stmt.bind_all<Args...>(std::forward<Args>(args)...);
			auto it = stmt.iterator();
			size_t num_rows = 0;
			while (it.next()) {
				auto& e = rows.emplace_back(db);
				num_rows++;
				e.from_result(it);
			}
			return num_rows;
		}

		template <class T, typename... Args, typename std::enable_if<std::is_same<decltype(T::_class_info), const sqlitepp::orm::class_info>::value>::type* = nullptr>
		inline size_t select_multiple_into(database& db, std::vector<T>& rows, const std::string& where = "", Args&&... args) {
			return select_multiple_into<T>(db, T::_class_info, rows, where, std::forward<Args>(args)...);
		}

		template <typename T>
		inline std::unique_ptr<T> select_one(database& db, const class_info& info, const std::string& where, std::vector<db_value> vals) {
			sqlitepp::statement stmt(db, detail::build_select_all(info, where));
			for (size_t i = 0; i < vals.size(); i++)
				detail::bind_db_val(stmt, i + 1, vals[i]);
			auto it = stmt.iterator();
			if (!it.next()) return nullptr;

			auto e = std::make_unique<T>(db);
			e->from_result(it);
			return e;
		}

		template <class T, typename std::enable_if<std::is_same<decltype(T::_class_info), const sqlitepp::orm::class_info>::value>::type* = nullptr>
		inline std::unique_ptr<T> select_one(database& db, const std::string& where, std::vector<db_value> vals) {
			return select_one<T>(db, T::_class_info, where, vals);
		}

		template <typename T, typename... Args>
		inline std::unique_ptr<T> select_one(database& db, const class_info& info, const std::string& where = "", Args&&... args) {
			sqlitepp::statement stmt(db, detail::build_select_all(info, where));
			stmt.bind_all<Args...>(std::forward<Args>(args)...);
			auto it = stmt.iterator();
			if (!it.next()) return nullptr;

			auto e = std::make_unique<T>(db);
			e->from_result(it);
			return e;
		}

		template <class T, typename... Args, typename std::enable_if<std::is_same<decltype(T::_class_info), const sqlitepp::orm::class_info>::value>::type* = nullptr>
		inline std::unique_ptr<T> select_one(database& db, const std::string& where, Args&&... args) {
			return select_one<T>(db, T::_class_info, where, std::forward<Args>(args)...);
		}
	} // namespace orm
} // namespace sqlitepp
