num_procs=$1
file=$2

comp_file=${file%.*}

echo  "Compile: mpiCC -o ${comp_file}  ${file}\n"

mpiCC -o ${comp_file}  ${file}

echo "Runing: ${comp_file}"

mpirun -np ${num_procs} ./${comp_file}


