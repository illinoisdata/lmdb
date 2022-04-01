/* mtest4.c - memory-mapped database tester/toy */
/*
 * Copyright 2011-2021 Howard Chu, Symas Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

/* Tests for sorted duplicate DBs with fixed-size keys */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lmdb.h"

#define E(expr) CHECK((rc = (expr)) == MDB_SUCCESS, #expr)
#define RES(err, expr) ((rc = expr) == (err) || (CHECK(!rc, #expr), 0))
#define CHECK(test, msg) ((test) ? (void)0 : ((void)fprintf(stderr, \
  "%s:%d: %s: %s\n", __FILE__, __LINE__, msg, mdb_strerror(rc)), abort()))

void encode64(uint64_t n, char* bytes) {
  bytes[0] = (n >> 56) & 0xFF;
  bytes[1] = (n >> 48) & 0xFF;
  bytes[2] = (n >> 40) & 0xFF;
  bytes[3] = (n >> 32) & 0xFF;;
  bytes[4] = (n >> 24) & 0xFF;
  bytes[5] = (n >> 16) & 0xFF;
  bytes[6] = (n >> 8) & 0xFF;
  bytes[7] = n & 0xFF;
}

void encode32(uint32_t n, char* bytes) {
  bytes[0] = (n >> 24) & 0xFF;
  bytes[1] = (n >> 16) & 0xFF;
  bytes[2] = (n >> 8) & 0xFF;
  bytes[3] = n & 0xFF;
}

uint64_t decode64(char* bytes) {
  return (((uint64_t) bytes[0]) << 56) +
    (((uint64_t) bytes[1]) << 48) +
    (((uint64_t) bytes[2]) << 40) +
    (((uint64_t) bytes[3]) << 32) +
    (((uint64_t) bytes[4]) << 24) +
    (((uint64_t) bytes[5]) << 16) +
    (((uint64_t) bytes[6]) << 8)  +
    bytes[7];
}

uint32_t decode32(char* bytes) {
  return (((uint32_t) bytes[0]) << 24) +
    (((uint32_t) bytes[1]) << 16) +
    (((uint32_t) bytes[2]) << 8)  +
    bytes[3];
}

int main(int argc,char * argv[])
{
  int i = 0, j = 0, rc;
  MDB_env *env;
  MDB_dbi dbi;
  MDB_val key, data;
  MDB_txn *txn;
  MDB_stat mst;
  MDB_cursor *cursor;
  int count;
  int *values;
  char sval[8];
  char kval[sizeof(uint32_t)];

  memset(sval, 0, sizeof(sval));

  count = 2048;
  values = (int *)malloc(count*sizeof(uint64_t));

  for(i = 0;i<count;i++) {
    values[i] = i*5;
  }

  E(mdb_env_create(&env));
  E(mdb_env_set_mapsize(env, 10485760));
  E(mdb_env_set_maxdbs(env, 4));
  E(mdb_env_open(env, "./testdb", MDB_FIXEDMAP|MDB_NOSYNC, 0664));

  E(mdb_txn_begin(env, NULL, 0, &txn));
  E(mdb_dbi_open(txn, "id4", MDB_CREATE|MDB_INTEGERKEY, &dbi));

  key.mv_size = sizeof(uint32_t);
  key.mv_data = kval;
  data.mv_size = sizeof(sval);
  data.mv_data = sval;

  /* Building */

  printf("Adding %d values\n", count);
  strcpy(kval, "001");
  for (int i=0;i<count;i++) {
    encode32(i / 2, kval);
    encode64(values[i], sval);
    key.mv_data = kval;
    data.mv_data = sval;
    if (RES(MDB_KEYEXIST, mdb_put(txn, dbi, &key, &data, MDB_NOOVERWRITE)))
      j++;
  }
  if (j) printf("%d duplicates skipped\n", j);
  E(mdb_txn_commit(txn));
  E(mdb_env_stat(env, &mst));

  /* Benchmarking */

  printf("Test read\n");
  E(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
  E(mdb_cursor_open(txn, dbi, &cursor));
  for (int i=0;i<10;i++) {
    encode32(i, kval); key.mv_data = kval;
    rc = mdb_cursor_get(cursor, &key, &data, MDB_SET_KEY);
    if (rc != 0) {
      printf("Unexpected rc= %d\n", rc);
    }
    printf("i= %d (%d): key= %p %d, data= %p %lu\n", i, decode32(kval),
      key.mv_data,  decode32((char*) key.mv_data),
      data.mv_data, decode64((char*) data.mv_data));
  }
  mdb_cursor_close(cursor);
  mdb_txn_abort(txn);


  /* Clean */

  printf("Deleting with cursor\n");
  E(mdb_txn_begin(env, NULL, 0, &txn));
  E(mdb_cursor_open(txn, dbi, &cursor));
  for (int i=0; i<count; i++) {
    if (RES(MDB_NOTFOUND, mdb_cursor_get(cursor, &key, &data, MDB_NEXT)))
      continue;
    E(mdb_del(txn, dbi, &key, NULL));
  }
  mdb_cursor_close(cursor);
  E(mdb_txn_commit(txn));

  free(values);
  mdb_dbi_close(env, dbi);
  mdb_env_close(env);
  printf("Closed connection\n");
  return 0;
}
