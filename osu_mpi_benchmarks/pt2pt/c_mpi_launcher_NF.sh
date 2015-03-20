num_procs=$1
file=$2
comp_file=${file%.*}
#echo "Compile: mpiCC -g -Wall -compile_info -o ${comp_file} ${file}"
#mpiCC -g -Wall -compile_info -o ${comp_file} ${file}
echo "Compile: mpiCC -o ${comp_file} ${file}"
mpiCC -o ${comp_file} ${file}
echo "Runing: ${comp_file}"
#--host {IP addresses} or --hosfile hosts.txt
#http://www.open-mpi.org/faq/?category=running
mpirun -np ${num_procs} ./${comp_file} arg1
