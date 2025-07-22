#include <arg.hpp>
#include <iostream>
#include <processor.hpp>

int main(int argc, char *argv[]) {
  // 解析命令行参数
  Args args = parse_args(argc, argv);

  columnar_subtoken_compress_logs(args);
}
