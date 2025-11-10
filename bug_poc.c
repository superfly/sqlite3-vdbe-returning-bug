#include <assert.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Virtual table structure */
typedef struct {
  sqlite3_vtab base;
} minimal_vtab;

/* Virtual table cursor structure */
typedef struct {
  sqlite3_vtab_cursor base;
} minimal_cursor;

/* xConnect - called when connecting to the virtual table */
static int vtabConnect(sqlite3 *db, void *pAux, int argc,
                       const char *const *argv, sqlite3_vtab **ppVtab,
                       char **pzErr) {
  printf("[minimal_vtab] xConnect called\n");

  /* Prepare and execute statement in xConnect */
  printf("[minimal_vtab] Preparing statement in xConnect...\n");
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v3(db, "SELECT * FROM test_table", -1,
                              SQLITE_PREPARE_PERSISTENT, &stmt, NULL);
  if (rc == SQLITE_OK) {
    printf("[minimal_vtab] Executing prepared statement from xConnect...\n");
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
      printf("[minimal_vtab] Got row: %s\n", sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
  } else {
    printf("[minimal_vtab] Failed to prepare statement in xConnect: %s\n",
           sqlite3_errmsg(db));
  }

  /* Declare the virtual table schema */
  rc = sqlite3_declare_vtab(db, "CREATE TABLE x(id INTEGER)");
  if (rc != SQLITE_OK) {
    return rc;
  }

  /* Allocate virtual table structure */
  minimal_vtab *vtab = sqlite3_malloc(sizeof(minimal_vtab));
  if (!vtab) {
    return SQLITE_NOMEM;
  }
  memset(vtab, 0, sizeof(minimal_vtab));

  *ppVtab = (sqlite3_vtab *)vtab;
  return SQLITE_OK;
}

/* xDisconnect */
static int vtabDisconnect(sqlite3_vtab *pVtab) {
  sqlite3_free(pVtab);
  return SQLITE_OK;
}

/* xBestIndex */
static int vtabBestIndex(sqlite3_vtab *pVtab, sqlite3_index_info *pInfo) {
  return SQLITE_OK;
}

/* xOpen - open a cursor */
static int vtabOpen(sqlite3_vtab *pVtab, sqlite3_vtab_cursor **ppCursor) {
  minimal_cursor *cursor = sqlite3_malloc(sizeof(minimal_cursor));
  if (!cursor) {
    return SQLITE_NOMEM;
  }
  memset(cursor, 0, sizeof(minimal_cursor));
  *ppCursor = (sqlite3_vtab_cursor *)cursor;
  return SQLITE_OK;
}

/* xClose - close a cursor */
static int vtabClose(sqlite3_vtab_cursor *pCursor) {
  sqlite3_free(pCursor);
  return SQLITE_OK;
}

/* xFilter */
static int vtabFilter(sqlite3_vtab_cursor *pCursor, int idxNum,
                      const char *idxStr, int argc, sqlite3_value **argv) {
  return SQLITE_OK;
}

/* xNext */
static int vtabNext(sqlite3_vtab_cursor *pCursor) { return SQLITE_OK; }

/* xEof */
static int vtabEof(sqlite3_vtab_cursor *pCursor) { return 1; /* Always EOF */ }

/* xColumn */
static int vtabColumn(sqlite3_vtab_cursor *pCursor, sqlite3_context *ctx,
                      int i) {
  sqlite3_result_null(ctx);
  return SQLITE_OK;
}

/* xRowid */
static int vtabRowid(sqlite3_vtab_cursor *pCursor, sqlite3_int64 *pRowid) {
  *pRowid = 0;
  return SQLITE_OK;
}

/* xUpdate */
static int vtabUpdate(sqlite3_vtab *pVtab, int argc, sqlite3_value **argv,
                      sqlite3_int64 *pRowid) {
  return SQLITE_OK;
}

/* Virtual table module definition */
static sqlite3_module minimal_module = {
    0,              /* iVersion */
    NULL,           /* xCreate */
    vtabConnect,    /* xConnect */
    vtabBestIndex,  /* xBestIndex */
    vtabDisconnect, /* xDisconnect */
    NULL,           /* xDestroy */
    vtabOpen,       /* xOpen */
    vtabClose,      /* xClose */
    vtabFilter,     /* xFilter */
    vtabNext,       /* xNext */
    vtabEof,        /* xEof */
    vtabColumn,     /* xColumn */
    vtabRowid,      /* xRowid */
    vtabUpdate,     /* xUpdate */
    NULL,           /* xBegin */
    NULL,           /* xSync */
    NULL,           /* xCommit */
    NULL,           /* xRollback */
    NULL,           /* xFindFunction */
    NULL,           /* xRename */
};

int main(void) {
  sqlite3 *db;
  char *err_msg = NULL;
  int rc;

  /* Remove existing database file */
  unlink("/tmp/poc.sqlite");

  /* Open database */
  rc = sqlite3_open("/tmp/poc.sqlite", &db);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[main] Cannot open database: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  printf("[main] Database opened\n");

  /* Create test table */
  rc = sqlite3_exec(db, "CREATE TABLE test_table (name TEXT)", NULL, NULL,
                    &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[main] Failed to create table: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return 1;
  }

  /* Insert test data */
  rc = sqlite3_exec(db, "INSERT INTO test_table VALUES ('test_value')", NULL,
                    NULL, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[main] Failed to insert data: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return 1;
  }

  printf("[main] Test table created and populated\n");

  /* Register virtual table module with db as pClientData */
  rc =
      sqlite3_create_module_v2(db, "minimal_vtab", &minimal_module, NULL, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[main] Failed to create module: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }

  printf("[main] Virtual table module registered\n");

  /* Open another connection to alter the schema */
  sqlite3 *db2;
  rc = sqlite3_open("/tmp/poc.sqlite", &db2);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[main] Failed to open second connection: %s\n",
            sqlite3_errmsg(db2));
    sqlite3_close(db);
    return 1;
  }
  printf("[main] Opened second database connection\n");

  /* Alter schema using another connection */
  rc = sqlite3_exec(db2, "CREATE TABLE minimal_table (id INTEGER)", NULL, NULL,
                    &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[main] Failed to alter schema: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db2);
    sqlite3_close(db);
    return 1;
  }
  printf("[main] Altered schema on second connection\n");

  /* Close second connection */
  sqlite3_close(db2);
  printf("[main] Closed second database connection\n");

  printf("[main] Preparing INSERT with RETURNING\n");
  /* Insert rows into vtab with RETURNING clause */
  sqlite3_stmt *insert_stmt;
  rc = sqlite3_prepare_v3(
      db, "INSERT INTO minimal_vtab VALUES (1), (2), (3) RETURNING *", -1, 0,
      &insert_stmt, NULL);
  if (rc != SQLITE_OK) { 
    fprintf(stderr, "[main] Failed to prepare INSERT with RETURNING: %s\n",
            sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }
  printf("[main] Prepared INSERT with RETURNING\n");

  printf("[main] Executing INSERT with RETURNING:\n");
  int row_count = 0;
  while ((rc = sqlite3_step(insert_stmt)) == SQLITE_ROW) {
    printf("[main]   Returned row: %s\n", sqlite3_column_text(insert_stmt, 0));
    row_count++;
  }

  printf("[main] Row count: %d\n", row_count);
  assert(row_count == 3 && "Expected 3 rows to be returned");
  printf("[main] Assertion passed: %d rows returned\n", row_count);

  if (rc != SQLITE_DONE) {
    fprintf(stderr, "[main] Error during INSERT execution: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(insert_stmt);
    sqlite3_close(db);
    return 1;
  }

  sqlite3_finalize(insert_stmt);
  printf("[main] Rows inserted with RETURNING\n");

  /* Cleanup */
  sqlite3_close(db);

  printf("[main] Done\n");
  return 0;
}
