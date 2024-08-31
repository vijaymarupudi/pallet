#include <stdio.h>
#include "lua.hpp"
#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <string_view>
#include <string.h>
#include <sstream>
#include <vector>
#include <utility>
#include <optional>
#include <fstream>

struct State {
  std::vector<std::pair<std::string, std::string>> files;
  void add_file(const char* rootdir, const char* parent_dir_path, const char* file_name) {
    std::string key;
    if (strcmp(parent_dir_path, ".") == 0) {
      key = std::string(file_name);
    } else {
      key = std::string(parent_dir_path);
      key.append("/");
      key.append(file_name);
    }
    char buf[8192];
    snprintf(buf, 8192, "%s/%s/%s", rootdir, parent_dir_path, file_name);
    std::stringstream ss;
    std::ifstream file (buf, std::ios::in | std::ios::binary);
    ss << file.rdbuf();
    printf("Adding %s\n", key.c_str());
    files.push_back(std::pair(std::move(key), std::move(ss.str())));
  }
};

void process(State& state, const char* rootdir, const char* dirname) {
  char realdirname[8192];
  snprintf(realdirname, 8192, "%s/%s", rootdir, dirname);  
  
  DIR* dir = opendir(realdirname);
  struct dirent* ent;
  while ((ent = readdir(dir)) != nullptr) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) { continue; }
    if (ent->d_type & DT_REG) {
      state.add_file(rootdir, dirname, ent->d_name);
    } else if (ent->d_type & DT_DIR) {
      char newdirname[8192];
      if (strcmp(dirname, ".") == 0) {
        snprintf(newdirname, 8192, "%s", ent->d_name);
      } else {
        snprintf(newdirname, 8192, "%s/%s", dirname, ent->d_name);
      }
      process(state, rootdir, newdirname);
    }
  }
  closedir(dir);
}

struct WriterState {
  std::string data;
};

static int writer(lua_State* L, const void* data, size_t len, void* ud) {
  ((WriterState*)ud)->data.append((const char*)data, len);
  return 0;
}

static std::string l_compile_to_bytecode(lua_State* L, const std::string& luaCode) {
  WriterState state;
  luaL_loadstring(L, luaCode.c_str());
  lua_dump(L, writer, &state, 0);
  lua_pop(L, 1);
  return std::move(state.data);
}

int main(int argc, char** argv) {
  lua_State* L = luaL_newstate();
  const char* dirname = argv[1];
  const char* outfile = argv[2];
  State state;
  process(state, dirname, ".");
  std::vector<std::pair<std::string, std::string>> luaModules;
  std::vector<std::pair<std::string, std::string>> regularFiles;
  for (auto&& [fname, content] : state.files) {
    if (fname.ends_with(".lua")) {
      std::string modName = fname;
      for (size_t i = 0; i < modName.size(); i++) {
        if (modName[i] == '/') {
          modName[i] = '.';
        }
      }
      modName.resize(modName.size() - 4);
      std::string bytecode = l_compile_to_bytecode(L, content);
      printf("ModPack: %s (%llu)\n", modName.c_str(), bytecode.size());
      luaModules.push_back(std::pair(std::move(modName), std::move(bytecode)));
    } else {
      regularFiles.push_back(std::pair(std::move(fname), std::move(content)));
    }
  }

  std::string fileContents = R"(
struct filesystem_entry {
const char* filename;
const unsigned char* contents;
};
)";


  fileContents.append("filesystem_entry lua_modules[");
  fileContents.append(std::to_string(luaModules.size()));
  fileContents.append("] = ;");

  printf("%s", fileContents.c_str());

}
