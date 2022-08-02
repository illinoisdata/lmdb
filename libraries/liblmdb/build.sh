set -e
  
DATASET_NAMES=(
  "books_800M_uint64"
  "fb_200M_uint64"
  "osm_cellids_800M_uint64"
  "wiki_ts_200M_uint64"
  "gmm_k100_800M_uint64"
)

ROOT=$1
echo "${ROOT}"

echo "Start Building lmdb"

for ((i = 0; i < ${#DATASET_NAMES[@]}; i++)) do
    dataset_name="${DATASET_NAMES[$i]}"
    echo ">>> ${dataset_name} ${j}"
    mkdir -p ${ROOT}/temp/lmdb/${dataset_name}
    mkdir -p ${ROOT}/storage/lmdb/${dataset_name}
    make kv_build && ./kv_build \
        --data_path=${HOME}/data/${dataset_name} \
        --build_db_path=${ROOT}/temp/lmdb/${dataset_name} \
        --target_db_path=${ROOT}/storage/lmdb/${dataset_name}
done
