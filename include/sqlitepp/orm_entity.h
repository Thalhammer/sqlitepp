#pragma once
#include <memory>

#include <sqlitepp/fwd.h>

namespace sqlitepp {
	namespace orm {
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
			void from_result(const sqlitepp::result_iterator& it);
			void insert();
			void update();

			friend std::vector<std::unique_ptr<entity>> select_multiple(database& db, const class_info& info, const std::string& where, std::vector<db_value> vals);
			friend std::unique_ptr<entity> select_one(database& db, const class_info& info, const std::string& where, std::vector<db_value> vals);

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
			bool is_modified() const;
			/**
             * \brief Reset the entity to its original values
             */
			void reset();
			/**
             * \brief Persist all changes to the database
             */
			void save();
			/**
             * \brief Remove the entity from the database
             */
			void remove();
		};
	} // namespace orm
} // namespace sqlitepp