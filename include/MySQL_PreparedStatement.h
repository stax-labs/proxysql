#ifndef CLASS_MYSQL_PREPARED_STATEMENT_H
#define CLASS_MYSQL_PREPARED_STATEMENT_H

#include "proxysql.h"
#include "cpp.h"

/*
One of the main challenge in handling prepared statement (PS) is that a single
PS could be executed on multiple backends, and on each backend it could have a
different stmt_id.
For this reason ProxySQL returns to the client a stmt_id generated by the proxy
itself, and internally maps client's stmt_id with the backend stmt_id.

The implementation in ProxySQL is, simplified, the follow:
* when a client sends a MYSQL_COM_STMT_PREPARE, ProxySQL executes it to one of
  the backend
* the backend returns a stmt_id. This stmt_id is NOT returned to the client. The
  stmt_id returned from the backend is stored in MySQL_STMTs_local(), and
  MySQL_STMTs_local() is responsible for mapping the connection's MYSQL_STMT
  and a global_stmt_id
* the global_stmt_id is the stmt_id returned to the client
* the global_stmt_id is used to locate the relevant MySQL_STMT_Global_info() in
  MySQL_STMT_Manager()
* MySQL_STMT_Global_info() stores all metadata associated with a PS
* MySQL_STMT_Manager() is responsible for storing all MySQL_STMT_Global_info()
  in global structures accessible and shareble by all threads.

To summarie the most important classes:
* MySQL_STMT_Global_info() stores all metadata associated with a PS
* MySQL_STMT_Manager() stores all the MySQL_STMT_Global_info(), indexes using
  a global_stmt_id that iis the stmt_id generated by ProxySQL and returned to
  the client
* MySQL_STMTs_local() associate PS located in a backend connection to a
  global_stmt_id
*/


// class MySQL_STMTs_local associates a global statement ID with a local statement ID for a specific connection
class MySQL_STMTs_local {
	private:
	unsigned int num_entries;
	std::map<uint32_t, MYSQL_STMT *> m;
	public:
	MySQL_STMTs_local() {
		num_entries=0;
	}
	~MySQL_STMTs_local();
	// we declare it here to be inline
	void insert(uint32_t global_statement_id, MYSQL_STMT *stmt) {
		std::pair<std::map<uint32_t, MYSQL_STMT *>::iterator,bool> ret;
		ret=m.insert(std::make_pair(global_statement_id, stmt));
		if (ret.second==true) {
			num_entries++;
		}
	}
	// we declare it here to be inline
	MYSQL_STMT * find(uint32_t global_statement_id) {
		auto s=m.find(global_statement_id);
		if (s!=m.end()) {	// found
			return s->second;
		}
		return NULL;	// not found
	}
	bool erase(uint32_t global_statement_id);
	uint64_t compute_hash(unsigned int hostgroup, char *user, char *schema, char *query, unsigned int query_length);
};



// class MySQL_STMT_Global_info represents information about a MySQL Prepared Statement
// it is an internal representation of prepared statement
// it include all metadata associated with it
class MySQL_STMT_Global_info {
	private:
	void compute_hash();
  public:
  uint64_t hash;
  char *username;
  char *schemaname;
  char *query;
  unsigned int query_length;
	unsigned int hostgroup_id;
	int ref_count;
  uint32_t statement_id;
  uint16_t num_columns;
  uint16_t num_params;
  uint16_t warning_count;
	MYSQL_FIELD **fields;
	struct {
		int cache_ttl;
		int timeout;
		int delay;
	} properties;
	//MYSQL_BIND **params; // seems unused
	MySQL_STMT_Global_info(uint32_t id, unsigned int h, char *u, char *s, char *q, unsigned int ql, MYSQL_STMT *stmt, uint64_t _h);
	~MySQL_STMT_Global_info();
};

class MySQL_STMT_Manager {
	private:
	uint32_t next_statement_id;
	rwlock_t rwlock;
	std::map<uint32_t, MySQL_STMT_Global_info *> m;	// map using statement id
	std::map<uint64_t, MySQL_STMT_Global_info *> h;	// map using hashes
	public:
	MySQL_STMT_Manager();
	~MySQL_STMT_Manager();
	int ref_count(uint32_t statement_id, int cnt, bool lock=true);
	MySQL_STMT_Global_info * add_prepared_statement(unsigned int h, char *u, char *s, char *q, unsigned int ql, MYSQL_STMT *stmt, bool lock=true);
	MySQL_STMT_Global_info * add_prepared_statement(unsigned int h, char *u, char *s, char *q, unsigned int ql, MYSQL_STMT *stmt, int _cache_ttl, int _timeout, int _delay, bool lock=true);
	MySQL_STMT_Global_info * find_prepared_statement_by_stmt_id(uint32_t id, bool lock=true);
	MySQL_STMT_Global_info * find_prepared_statement_by_hash(uint64_t hash, bool lock=true);
	uint32_t total_prepared_statements() { return next_statement_id-1; }
};

#endif /* CLASS_MYSQL_PREPARED_STATEMENT_H */
