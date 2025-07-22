#include "arg.hpp"
#include "utils/util.hpp"
#include <string>
Args parse_args(int argc, char *argv[]) {
  Args args = {
      .input_file = "",
      .output_dir = "./output",
      .chunk_size = 100000,
      .num_threads = 4,
      .rep_val_threshold = 40,
      .zeta = 3,
      .dom_ratio = 0.6,
      .is_help = false,
  };

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      std::cout
          << "Usage: " << argv[0] << " [options] [file]\n"
          << "Options:\n"
          << "  -o <dir>      Set output directory (default ./output)\n"
          << "  -c <integer>  Chunk size (default 100000)\n"
          << "  -t <integer>  num of threads (default 4)\n"
          << "  -rt <integer> representative value threshold (default 40)\n"
          << "  -dt <float>   dominance ratio threshold (default 0.6)\n"
          << "  -z <integer>  zeta (default 3)\n";
      args.is_help = true;
    } else if (arg == "-o" && i + 1 < argc) {
      args.output_dir = argv[++i]; // 跳过下一个参数（文件名）
    } else if (arg == "-c" && i + 1 < argc) {
      args.chunk_size = std::stoul(argv[++i]); // 跳过下一个参数（文件名）
    } else if (arg == "-t" && i + 1 < argc) {
      args.num_threads = std::stoul(argv[++i]); // 跳过下一个参数（文件名）
    } else if (arg == "-rt" && i + 1 < argc) {
      args.rep_val_threshold =
          std::stoul(argv[++i]); // 跳过下一个参数（文件名）
    } else if (arg == "-dt" && i + 1 < argc) {
      args.dom_ratio = std::stod(argv[++i]); // 跳过下一个参数（文件名）
    } else if (arg == "-z" && i + 1 < argc) {
      args.zeta = std::stoul(argv[++i]); // 跳过下一个参数（文件名）
    } else {
      args.input_file = arg;
    }
  }
  if (args.is_help) {
    exit(0);
  } else if (args.input_file.empty()) {
    handle_error("input file not specified");
  } else if (args.output_dir[0] == '-') {
    handle_error("invalid output directory: ");
  } else if (args.chunk_size <= 0) {
    handle_error("chunk size must be positive");
  } else if (args.num_threads <= 0) {
    handle_error("num of threads must be positive");
  } else if (args.zeta <= 0) {
    handle_error("zeta must be positive");
  } else if (args.rep_val_threshold <= 0) {
    handle_error("representative value threshold must be positive");
  } else if (args.dom_ratio <= 0 || args.dom_ratio >= 1) {
    handle_error("dominance ratio must be in the range (0, 1)");
  }
  return args;
}