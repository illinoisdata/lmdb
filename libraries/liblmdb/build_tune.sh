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
DATA_PATH=$2
echo "${ROOT}"

echo "Start Building lmdb"

for ((i = 0; i < ${#DATASET_NAMES[@]}; i++)) do
    for ((j = 0; j < ${#PAGE_SIZES[@]}; j++)) do
        dataset_name="${DATASET_NAMES[$i]}"
        page_size="${PAGE_SIZES[$j]}"
        echo ">>> ${dataset_name} ${j}"
        mkdir -p ${ROOT}/temp/lmdb_${page_size}/${dataset_name}
        mkdir -p ${ROOT}/storage/lmdb_${page_size}/${dataset_name}
        make kv_build && LMDB_PAGESIZE=${page_size} ./kv_build \
            --data_path=${DATA_PATH}/${dataset_name} \
            --build_db_path=${ROOT}/temp/lmdb_${page_size}/${dataset_name} \
            --target_db_path=${ROOT}/storage/lmdb_${page_size}/${dataset_name}
    done
done
