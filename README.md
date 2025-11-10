# SQLite3 VDBE VTAB RETURNING Bug

## Overview
This repository demonstrates a bug in SQLite3's VDBE codegen when using `RETURNING` on virtual tables.

When a query gets parsed, SQLite needs to initialize all referenced vtabs to discover their schema/indexes. That initialization can sometimes accidentally reset `db->aDb[1]` inside SQLite. This is where during parsing information about triggers (like `RETURNING`) gets stored. When that happens, all the trigger information gets lost. Because codegen is triggered after that, no VDBE for them will get generated.

# When is it possible for vtab initialization to reset `db->aDb[1]`?
When a vtab gets initialized, SQLite will execute `xConnect`/`xCreate`. If parsing a select statement, `xBestIndex` will then follow.

If any of those callbacks execute a query (for example, fetching some config from the database), then during VDBE execution SQLite may encounter `OP_TRANSACTION`. This operation includes a check for whether the current in-memory schema representation matches the physical on-disk schema. If not, it will reset `db->aDb[iDb]` and `db->aDb[1]`.

The best way to trigger this is to add another table using a different database connection.

## The PoC

1. Compile the POC:
   ```bash
   make
   ```
You can edit the Makefile and change `SQLITE3_PATH` to point to your custom sqlite3 amalgamation.

2. Run the POC:
   ```bash
   ./bug_poc
   ```

3. Observe the bug. `INSERT INTO minimal_vtab VALUES (1), (2), (3) RETURNING *` will return no rows despite the insert being successful.
   
By commenting out the call to `CREATE TABLE minimal_table (id INTEGER)`, notice that the bug is gone and 3 rows are returned.

# Possible fix
The simplest way is to check whether the schema version was changed during codegen in `static int sqlite3Prepare`. If it did, then just retry the prepare.
```
diff --git a/local-deps/libsqlite3-sys/sqlite3/sqlite3.c b/local-deps/libsqlite3-sys/sqlite3/sqlite3.c
index 6485e1c..b38fb18 100644
--- a/local-deps/libsqlite3-sys/sqlite3/sqlite3.c
+++ b/local-deps/libsqlite3-sys/sqlite3/sqlite3.c
@@ -146409,6 +146409,7 @@ static int sqlite3Prepare(
   int rc = SQLITE_OK;       /* Result code */
   int i;                    /* Loop counter */
   Parse sParse;             /* Parsing context */
+  int schema_cookies[db->nDb];
 
   /* sqlite3ParseObjectInit(&sParse, db); // inlined for performance */
   memset(PARSE_HDR(&sParse), 0, PARSE_HDR_SZ);
@@ -146478,6 +146479,10 @@ static int sqlite3Prepare(
     }
   }
 
+  for(i=0; i<db->nDb; i++) {
+    schema_cookies[i] = db->aDb[i].pSchema->schema_cookie;
+  }
+
 #ifndef SQLITE_OMIT_VIRTUALTABLE
   if( db->pDisconnect ) sqlite3VtabUnlockList(db);
 #endif
@@ -146505,6 +146510,13 @@ static int sqlite3Prepare(
   }
   assert( 0==sParse.nQueryLoop );
 
+  for(i=0; i<db->nDb; i++) {
+    // xConnect/xCreate/xBestIndex on vtabs can reset the schema accidentally
+    if(schema_cookies[i] != db->aDb[i].pSchema->schema_cookie) {
+      sParse.rc = SQLITE_SCHEMA;
+    }
+  }
+
   if( pzTail ){
     *pzTail = sParse.zTail;
   }
```
