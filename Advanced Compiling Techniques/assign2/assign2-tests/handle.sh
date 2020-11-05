for file in `ls *.c`;
do
    clang -c -emit-llvm -g -O0 $file
done

cp ./*.bc ../
