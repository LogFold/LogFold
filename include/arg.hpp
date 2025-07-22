#ifndef LOGMD_ARG_HPP
#define LOGMD_ARG_HPP

#include <cstdlib>
#include <iostream>
#include <string>

struct Args {
  std::string input_file;
  std::string output_dir;
  unsigned int chunk_size;
  unsigned int num_threads;
  unsigned int rep_val_threshold;
  unsigned int zeta;
  double dom_ratio;
  bool is_help;
};

Args parse_args(int argc, char **argv);

#endif // LOGMD_ARG_HPP
