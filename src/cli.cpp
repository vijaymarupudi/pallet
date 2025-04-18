#include <cstdio>
#include <argparse/argparse.hpp>
#include "cliMain.hpp"

int main(int argc, char** argv) {
  argparse::ArgumentParser parser("cli");
  parser.add_argument("file").help("Lua file to execute");
  parser.parse_args(argc, argv);
  auto val = parser.get<std::string>("file");
  return cliMain(val.c_str());
}
