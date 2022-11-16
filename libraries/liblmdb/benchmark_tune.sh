set -e

DATASET_NAMES=(
  "books_800M_uint64"
  # "fb_200M_uint64"
  # "osm_cellids_800M_uint64"
  # "wiki_ts_200M_uint64"
  # "gmm_k100_800M_uint64"
)
  
PAGE_SIZES=(
    "1024"
    "2048"
    "4096"
    "8192"
    "16384"
    "32768"
)

ROOT=$1
KEYSET_PATH=$2
RELOAD=$3
OUT_PATH=${ROOT}/out

mkdir -p ${OUT_PATH}
echo "${ROOT}"

echo "Start lmdb Benchmark"

for ((i = 0; i < ${#DATASET_NAMES[@]}; i++)) do
    for ((k = 0; k < ${#PAGE_SIZES[@]}; k++)) do
        for ((j = 0; j < 40; j++)) do
            dataset_name="${DATASET_NAMES[$i]}"
            page_size="${PAGE_SIZES[$k]}"
            echo ">>> ${dataset_name} ${j}"
            bash ${RELOAD}
            make kv_benchmark && LMDB_PAGESIZE=${page_size} ./kv_benchmark \
                --key_path=${KEYSET_PATH}/${dataset_name}_ks_${j} \
                --target_db_path=${ROOT}/storage/lmdb_${page_size}/${dataset_name} \
                --out_path=${OUT_PATH}/${dataset_name}_${page_size}_out.txt
        done
    done
done
