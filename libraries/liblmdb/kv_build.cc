/*
 * Build lmdb from sosd
 *
 * This source is derived from PGM-index::examples/mapped.cpp

export dataset_name="wiki_ts_200M_uint64"

export ROOT=$(pwd)
mkdir -p ${ROOT}/temp/lmdb
mkdir -p ${ROOT}/storage/lmdb
mkdir -p ${ROOT}/out

make kv_build && ./kv_build \
    --data_path=${ROOT}/data/${dataset_name} \
    --build_db_path=${ROOT}/temp/lmdb/${dataset_name} \
    --target_db_path=${ROOT}/storage/lmdb/${dataset_name}

make kv_benchmark && ./kv_benchmark \
    --key_path=${ROOT}/keyset/${dataset_name}_ks \
    --target_db_path=${ROOT}/storage/lmdb/${dataset_name} \
    --out_path=out/${dataset_name}_out.txt
 */

#include <bits/stdc++.h>
#include <sys/stat.h>
#include "lmdb.h"

#include "flags.h"

#ifndef __has_include
  static_assert(false, "__has_include not supported");
#else
#  if __cplusplus >= 201703L && __has_include(<filesystem>)
#    include <filesystem>
     namespace fs = std::filesystem;
#  elif __has_include(<experimental/filesystem>)
#    include <experimental/filesystem>
     namespace fs = std::experimental::filesystem;
#  elif __has_include(<boost/filesystem.hpp>)
#    include <boost/filesystem.hpp>
     namespace fs = boost::filesystem;
#  endif
#endif


// error handling
#define E(expr) CHECK((rc = (expr)) == MDB_SUCCESS, #expr)
#define RES(err, expr) ((rc = expr) == (err) || (CHECK(!rc, #expr), 0))
#define CHECK(test, msg) ((test) ? (void)0 : ((void)fprintf(stderr, \
  "%s:%d: %s: %s\n", __FILE__, __LINE__, msg, mdb_strerror(rc)), abort()))


/* Reuse code from benchmark */

bool global_verbose = true;

#define IF_VERBOSE(X) if (global_verbose) { X; }
#define OUT_VERBOSE(X) if (global_verbose) { std::cout << "# " << X << std::endl; }

template<typename T>
std::string to_metric(const T &x, int digits = 2, bool space = false) {
    static const char *prefix[] = {"y", "z", "a", "f", "p", "n", "µ", "m", "", "k", "M", "G", "T", "P", "E", "Z", "Y"};
    double value = x;
    if (value < 0.)
        value = -value;

    auto log_x = (int) std::log10(value);
    if (log_x > 0)
        log_x = (log_x / 3) * 3;
    else
        log_x = (-log_x + 3) / 3 * (-3);

    value *= std::pow(10, -log_x);
    if (value >= 1000.)
        value /= 1000.0, log_x += 3;

    if (std::fmod(value, 1.0) == 0.0)
        digits = 0;

    constexpr auto prefix_start = -24;
    const char *fmt = space ? (x < 0. ? "-%.*f %s" : "%.*f %s") : (x < 0. ? "-%.*f%s" : "%.*f%s");
    const char *chosen_prefix = prefix[(log_x - prefix_start) / 3];
    std::vector<char> buffer(100);
    int size = std::snprintf(&buffer[0], buffer.size(), fmt, digits, value, chosen_prefix);
    return std::string(&buffer[0], size);
}

template<typename T>
std::vector<T> read_data_binary(const std::string &filename, bool check_sorted) {
    OUT_VERBOSE("Reading " << filename)
    std::vector<T> data;
    try {
        std::fstream in(filename, std::ios::in | std::ios::binary);
        in.exceptions(std::ios::failbit | std::ios::badbit);
        struct stat fs;
        stat(filename.c_str(), &fs);
        size_t file_size = fs.st_size;
        data.resize(file_size / sizeof(T));
        in.read((char *) data.data(), file_size);
    } catch (std::ios_base::failure &e) {
        std::cerr << "Could not read the file. " << e.what() << std::endl;
        exit(1);
    }

    if (size_t(data[0]) == data.size() - 1)
        data.erase(data.begin()); // in some formats, the first element is the size of the dataset, ignore it

    if (check_sorted && !std::is_sorted(data.begin(), data.end())) {
        std::cerr << "Input data must be sorted." << std::endl;
        std::cerr << "Read: [";
        std::copy(data.begin(), std::min(data.end(), data.begin() + 10), std::ostream_iterator<T>(std::cerr, ", "));
        std::cout << "...]." << std::endl;
        exit(1);
    }

    IF_VERBOSE(std::cout << "# Read " << to_metric(data.size()) << " elements: [")
    IF_VERBOSE(std::copy(data.begin(),
                         std::min(data.end() - 1, data.begin() + 5),
                         std::ostream_iterator<T>(std::cout, ", ")))
    IF_VERBOSE(std::cout << "..., " << *(data.end() - 1) << "]" << std::endl)

    return data;
}

void build_lmdb(std::vector<uint64_t> &sosd_data, const std::string &filename) {
  // setting up lmdb
  int rc; // response variable
  MDB_env *env;
  MDB_dbi dbi;
  MDB_val key; key.mv_size = sizeof(uint64_t);
  MDB_val data; data.mv_size = sizeof(uint64_t);
  MDB_txn *txn;
  MDB_cursor *cursor;
  // MDB_stat mst;
  char kval[sizeof(uint64_t)];
  char dval[sizeof(uint64_t)];
  E(mdb_env_create(&env));
  E(mdb_env_set_mapsize(env, 1LL << 35));
  E(mdb_env_set_maxdbs(env, 1));
  E(mdb_env_open(env, filename.c_str(), MDB_MAPASYNC|MDB_NOSYNC|MDB_NOMETASYNC, 0664));

  // clearing all data
  printf("Clearing data\n");
  E(mdb_txn_begin(env, NULL, 0, &txn));
  E(mdb_dbi_open(txn, NULL, MDB_CREATE|MDB_INTEGERKEY, &dbi));
  E(mdb_cursor_open(txn, dbi, &cursor));
  int del_count = 0;
  while (!RES(MDB_NOTFOUND, mdb_cursor_get(cursor, &key, &data, MDB_NEXT))) {
    E(mdb_del(txn, dbi, &key, NULL));
    del_count++;
  }
  if (del_count) {
    printf("%d old pairs cleared\n", del_count);
  }
  mdb_cursor_close(cursor);
  E(mdb_txn_commit(txn));

  // begin adding all data
  printf("Inserting data\n");
  E(mdb_txn_begin(env, NULL, 0, &txn));
  E(mdb_dbi_open(txn, NULL, MDB_CREATE|MDB_INTEGERKEY, &dbi));
  E(mdb_cursor_open(txn, dbi, &cursor));
  int dup_count = 0;
  for (size_t i = 0; i < sosd_data.size(); i++) {
    if (i > 0 && sosd_data[i] == sosd_data[i-1]) {
      dup_count++;
    } else {
      encode64(sosd_data[i], kval); key.mv_size = sizeof(uint64_t); key.mv_data = kval;
      encode64(i, dval); data.mv_size = sizeof(uint64_t); data.mv_data = dval;
      if (RES(MDB_KEYEXIST, mdb_put(txn, dbi, &key, &data, MDB_APPEND))) {
        dup_count++;
      }
    }
    if (i % 1000000 == 0) {  // 10000 works
      E(mdb_txn_commit(txn)); 
      E(mdb_env_sync(env, 1));

      E(mdb_txn_begin(env, NULL, 0, &txn));
      E(mdb_cursor_open(txn, dbi, &cursor));
    }
    if (i % (sosd_data.size() / 10) == 0 || i == sosd_data.size() - 1) {
      printf("Adding rank= %lu: key= %lu\n", i, sosd_data[i]);
    }
  }
  if (dup_count) {
    printf("%d duplicates skipped\n", dup_count);
  }
  mdb_cursor_close(cursor);
  E(mdb_txn_commit(txn));
  // E(mdb_env_stat(env, &mst));

  // close db
  mdb_dbi_close(env, dbi);
  mdb_env_close(env);
  printf("Closed lmdb connection\n");
}


/* Build! */

int main(int argc, char* argv[]) {
    auto flags = parse_flags(argc, argv);

    // extract paths
    std::string data_path = get_required(flags, "data_path");  // sosd blob
    std::string build_db_path = get_required(flags, "build_db_path");  // path to build (e.g. locally)
    std::string target_db_path = get_required(flags, "target_db_path");  // path to save db file
    fs::path build_path(build_db_path);
    fs::path target_path(target_db_path);
    std::cout << "Removing tempporary DB directory " << build_path << std::endl;      
    fs::remove_all(build_db_path);
    if (!fs::is_directory(build_db_path) || !fs::exists(build_db_path)) {
      fs::create_directory(build_db_path);
    }

    // Load sosd blob
    std::vector<uint64_t> data = read_data_binary<uint64_t>(data_path, true);

    // Construct the disk-backed container
    build_lmdb(data, build_db_path);

    // Copy to target directory (slow random write)
    if (build_path != target_path) {
        std::cout << "Copying DB directory from " << build_path << " to " << target_path.string() << std::endl;      
        fs::copy(build_path, target_path, fs::copy_options::overwrite_existing | fs::copy_options::recursive);

        std::cout << "Removing tempporary DB directory " << build_path << std::endl;
        fs::remove_all(build_db_path);
    }
    return 0;
}