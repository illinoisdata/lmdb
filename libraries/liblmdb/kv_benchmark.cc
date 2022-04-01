/*
 * Benchmark from existing lmdb
 *
 * This source is derived from examples/mapped.cpp
 */

// #include <chrono>
// #include <iterator>
// #include <math.h>
// #include <vector>
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


double report_t(size_t t_idx, size_t &count_milestone, size_t &last_count_milestone, long long &last_elapsed, std::chrono::time_point<std::chrono::high_resolution_clock> start_t) {
    const double freq_mul = 1.1;
    auto curr_time = std::chrono::high_resolution_clock::now();
    long long time_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(curr_time - start_t).count();
    std::cout
        << "t = "<< time_elapsed << " ns: "
        << t_idx + 1 << " counts, tot "
        << (time_elapsed) / (t_idx + 1.0) << "/op, seg "
        << (time_elapsed - last_elapsed) / (t_idx + 1.0 - last_count_milestone) << "/op"
        << std::endl;
    last_elapsed = time_elapsed;
    last_count_milestone = count_milestone;
    count_milestone = ceil(((double) count_milestone) * freq_mul);  // next milestone to print
    return time_elapsed;
}

int main(int argc, char* argv[]) {
    auto flags = parse_flags(argc, argv);

    // extract paths
    std::string key_path = get_required(flags, "key_path");  // query keys and answers
    std::string target_db_path = get_required(flags, "target_db_path");  // path to save db file
    std::string out_path = get_required(flags, "out_path");  // output log
    fs::path target_path(target_db_path);

    // load keyset
    std::vector<uint64_t> queries;
    std::vector<uint64_t> expected_ans;
    {
        std::ifstream query_words_in(key_path);
        std::string line;
        while (std::getline(query_words_in, line)) {
            std::istringstream input;
            input.str(line);

            std::string key;
            std::string exp;
            input >> key;
            input >> exp;

            queries.push_back(std::stoull(key));
            expected_ans.push_back(std::stoull(exp));
        }   
    }

    // variables for milestone
    size_t last_count_milestone = 0;
    size_t count_milestone = 1;
    long long last_elapsed = 0;
    std::vector<double> timestamps;

    // start timer
    auto start_t = std::chrono::high_resolution_clock::now();

    // load lmdb
    int rc; // response variable
    MDB_env *env;
    MDB_dbi dbi;
    MDB_val lmdb_key; lmdb_key.mv_size = sizeof(uint64_t);
    MDB_val lmdb_data; lmdb_data.mv_size = sizeof(uint64_t);
    MDB_txn *txn;
    MDB_cursor *cursor;
    char kval[sizeof(uint64_t)];
    E(mdb_env_create(&env));
    E(mdb_env_set_mapsize(env, 1LL << 35));
    E(mdb_env_set_maxdbs(env, 1));
    E(mdb_env_open(env, target_db_path.c_str(), MDB_MAPASYNC|MDB_NOSYNC|MDB_NOMETASYNC, 0664));
    E(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
    E(mdb_dbi_open(txn, NULL, MDB_INTEGERKEY, &dbi));
    E(mdb_cursor_open(txn, dbi, &cursor));

    // issue queries and check answers
    for (size_t t_idx = 0; t_idx < queries.size(); t_idx++) {
        // query and answer
        uint64_t key = queries[t_idx];
        uint64_t answer = expected_ans[t_idx];  
        encode64(key, kval); lmdb_key.mv_size = sizeof(uint64_t); lmdb_key.mv_data = kval;

        // search
        E(mdb_cursor_get(cursor, &lmdb_key, &lmdb_data, MDB_SET_KEY));
        uint64_t rcv_key = decode64((unsigned char*) lmdb_key.mv_data);
        uint64_t rcv_rank = decode64((unsigned char*) lmdb_data.mv_data);

        // check with answer
        if (rcv_key != key || rcv_rank != answer) {
            printf("ERROR: incorrect rank: %lu (rcv_key= %lu), expected: %lu (key= %lu)\n", rcv_rank, rcv_key, answer, key);
        }

        if (t_idx + 1 == count_milestone || t_idx + 1 == queries.size()) {
            timestamps.push_back(report_t(t_idx, count_milestone, last_count_milestone, last_elapsed, start_t));    
        }
    }
    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);

    // write result to file
    {
        std::cout << "Writing timestamps to file " << out_path << std::endl;
        std::ofstream file_out;
        file_out.open(out_path, std::ios_base::app);
        for (const auto& timestamp : timestamps) {
            file_out << (long long) timestamp << ",";
        }
        file_out << std::endl;
        file_out.close();   
    }
    return 0;
}