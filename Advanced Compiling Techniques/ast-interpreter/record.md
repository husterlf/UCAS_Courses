CmakeLists.txt中添加：
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -std=c++14 -fPIC -w")

cmake命令：
% cmake -DLLVM_DIR=/usr/local/Cellar/llvm/10.0.1/
% make
