set -e

DATASET_NAMES=(
  "books_800M_uint64"
  "fb_200M_uint64"
  "osm_cellids_800M_uint64"
  "wiki_ts_200M_uint64"
  "gmm_k100_800M_uint64"
)

ROOT=$1
KEYSET_PATH=$2
RELOAD=$3
OUT_PATH=${ROOT}/out

mkdir -p ${OUT_PATH}
echo "${ROOT}"

echo "Start lmdb Benchmark"

for ((i = 0; i < ${#DATASET_NAMES[@]}; i++)) do
    for ((j = 0; j < 40; j++)) do
        dataset_name="${DATASET_NAMES[$i]}"
        echo ">>> ${dataset_name} ${j}"
	    bash ${RELOAD}
	    make kv_benchmark && ./kv_benchmark \
	        --key_path=${KEYSET_PATH}/${dataset_name}_ks_${j} \
	        --target_db_path=${ROOT}/storage/lmdb/${dataset_name} \
	        --out_path=${OUT_PATH}/${dataset_name}_out.txt
    done
done
